/*
    Copyright (C) 2000,2007 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <string>

#include "pbd/xml++.h"

#include "ardour/audio_port.h"
#include "ardour/audioengine.h"
#include "ardour/delivery.h"
#include "ardour/io.h"
#include "ardour/mtdm.h"
#include "ardour/port_insert.h"
#include "ardour/session.h"
#include "ardour/types.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

string
PortInsert::name_and_id_new_insert (Session& s, uint32_t& bitslot)
{
	bitslot = s.next_insert_id ();
	return string_compose (_("insert %1"), bitslot+ 1);
}

PortInsert::PortInsert (Session& s, boost::shared_ptr<Pannable> pannable, boost::shared_ptr<MuteMaster> mm)
	: IOProcessor (s, true, true, name_and_id_new_insert (s, _bitslot), "", DataType::AUDIO, true)
	, _out (new Delivery (s, _output, pannable, mm, _name, Delivery::Insert))
{
        _mtdm = 0;
        _latency_detect = false;
        _latency_flush_frames = false;
        _measured_latency = 0;
}

PortInsert::~PortInsert ()
{
        _session.unmark_insert_id (_bitslot);
        delete _mtdm;
}

void
PortInsert::start_latency_detection ()
{
	delete _mtdm;
        _mtdm = new MTDM (_session.frame_rate());
        _latency_flush_frames = false;
        _latency_detect = true;
        _measured_latency = 0;
}

void
PortInsert::stop_latency_detection ()
{
        _latency_flush_frames = signal_latency() + _session.engine().frames_per_cycle();
        _latency_detect = false;
}

void
PortInsert::set_measured_latency (framecnt_t n)
{
        _measured_latency = n;
}

framecnt_t
PortInsert::latency() const
{
	/* because we deliver and collect within the same cycle,
	   all I/O is necessarily delayed by at least frames_per_cycle().

	   if the return port for insert has its own latency, we
	   need to take that into account too.
	*/

	if (_measured_latency == 0) {
		return _session.engine().frames_per_cycle() + _input->latency();
	} else {
		return _measured_latency;
	}
}

void
PortInsert::run (BufferSet& bufs, framepos_t start_frame, framepos_t end_frame, pframes_t nframes, bool)
{
	if (_output->n_ports().n_total() == 0) {
		return;
	}

        if (_latency_detect) {

                if (_input->n_ports().n_audio() != 0) {

                        AudioBuffer& outbuf (_output->ports().nth_audio_port(0)->get_audio_buffer (nframes));
                        Sample* in = _input->ports().nth_audio_port(0)->get_audio_buffer (nframes).data();
                        Sample* out = outbuf.data();

                        _mtdm->process (nframes, in, out);
			
                        outbuf.set_is_silent (false);
                        outbuf.set_written (true);
                }

                return;

        } else if (_latency_flush_frames) {

                /* wait for the entire input buffer to drain before picking up input again so that we can't
                   hear the remnants of whatever MTDM pumped into the pipeline.
                */

                silence (nframes);

                if (_latency_flush_frames > nframes) {
                        _latency_flush_frames -= nframes;
                } else {
                        _latency_flush_frames = 0;
                }

                return;
        }

	if (!_active && !_pending_active) {
		/* deliver silence */
		silence (nframes);
		goto out;
	}

	_out->run (bufs, start_frame, end_frame, nframes, true);
	_input->collect_input (bufs, nframes, ChanCount::ZERO);

  out:
	_active = _pending_active;
}

XMLNode&
PortInsert::get_state(void)
{
	return state (true);
}

XMLNode&
PortInsert::state (bool full)
{
	XMLNode& node = IOProcessor::state(full);
	char buf[32];
	node.add_property ("type", "port");
	snprintf (buf, sizeof (buf), "%" PRIu32, _bitslot);
	node.add_property ("bitslot", buf);
        snprintf (buf, sizeof (buf), "%" PRId64, _measured_latency);
        node.add_property("latency", buf);
        snprintf (buf, sizeof (buf), "%u", _session.get_block_size());
        node.add_property("block_size", buf);

	return node;
}

int
PortInsert::set_state (const XMLNode& node, int version)
{
	XMLNodeList nlist = node.children();
	XMLNodeIterator niter;
	XMLPropertyList plist;
	const XMLProperty *prop;

	const XMLNode* insert_node = &node;

	// legacy sessions: search for child Redirect node
	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		if ((*niter)->name() == "Redirect") {
			insert_node = *niter;
			break;
		}
	}

	IOProcessor::set_state (*insert_node, version);

	if ((prop = node.property ("type")) == 0) {
		error << _("XML node describing port insert is missing the `type' field") << endmsg;
		return -1;
	}

	if (prop->value() != "port") {
		error << _("non-port insert XML used for port plugin insert") << endmsg;
		return -1;
	}

        uint32_t blocksize = 0;
        if ((prop = node.property ("block_size")) != 0) {
                sscanf (prop->value().c_str(), "%u", &blocksize);
        }

        //if the jack period is the same as when the value was saved, we can recall our latency..
        if ( (_session.get_block_size() == blocksize) && (prop = node.property ("latency")) != 0) {
                uint32_t latency = 0;
                sscanf (prop->value().c_str(), "%u", &latency);
                _measured_latency = latency;
        }

	if (!node.property ("ignore-bitslot")) {
		if ((prop = node.property ("bitslot")) == 0) {
			_bitslot = _session.next_insert_id();
		} else {
			_session.unmark_insert_id (_bitslot);
			sscanf (prop->value().c_str(), "%" PRIu32, &_bitslot);
			_session.mark_insert_id (_bitslot);
		}
	}

	return 0;
}

ARDOUR::framecnt_t
PortInsert::signal_latency() const
{
	/* because we deliver and collect within the same cycle,
	   all I/O is necessarily delayed by at least frames_per_cycle().

	   if the return port for insert has its own latency, we
	   need to take that into account too.
	*/

        if (_measured_latency == 0) {
                return _session.engine().frames_per_cycle() + _input->signal_latency();
        } else {
                return _measured_latency;
        }
}

/** Caller must hold process lock */
bool
PortInsert::configure_io (ChanCount in, ChanCount out)
{
	assert (!AudioEngine::instance()->process_lock().trylock());

	/* for an insert, processor input corresponds to IO output, and vice versa */

	if (_input->ensure_io (in, false, this) != 0) {
		return false;
	}

	if (_output->ensure_io (out, false, this) != 0) {
		return false;
	}

	return Processor::configure_io (in, out);
}

bool
PortInsert::can_support_io_configuration (const ChanCount& in, ChanCount& out) const
{
	out = in;
	return true;
}

bool
PortInsert::set_name (const std::string& name)
{
	bool ret = Processor::set_name (name);

	ret = (ret && _input->set_name (name) && _output->set_name (name));

	return ret;
}

void
PortInsert::activate ()
{
	IOProcessor::activate ();

	_out->activate ();
}

void
PortInsert::deactivate ()
{
	IOProcessor::deactivate ();

	_out->deactivate ();
}
