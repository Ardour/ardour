/*
 * Copyright (C) 2000-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2007-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

string
PortInsert::name_and_id_new_insert (Session& s, uint32_t& bitslot)
{
	bitslot = s.next_insert_id ();
	return string_compose (_("insert %1"), bitslot);
}

PortInsert::PortInsert (Session& s, boost::shared_ptr<Pannable> pannable, boost::shared_ptr<MuteMaster> mm)
	: IOProcessor (s, true, true, name_and_id_new_insert (s, _bitslot), "", DataType::AUDIO, true)
	, _out (new Delivery (s, _output, pannable, mm, _name, Delivery::Insert))
{
	_mtdm = 0;
	_latency_detect = false;
	_latency_flush_samples = 0;
	_measured_latency = 0;
}

PortInsert::~PortInsert ()
{
	_session.unmark_insert_id (_bitslot);
	delete _mtdm;
}

void
PortInsert::set_pre_fader (bool p)
{
	Processor::set_pre_fader (p);
	_out->set_pre_fader (p);
}

void
PortInsert::start_latency_detection ()
{
	delete _mtdm;
	_mtdm = new MTDM (_session.sample_rate());
	_latency_flush_samples = 0;
	_latency_detect = true;
	_measured_latency = 0;
}

void
PortInsert::stop_latency_detection ()
{
	_latency_flush_samples = effective_latency() + _session.engine().samples_per_cycle();
	_latency_detect = false;
}

void
PortInsert::set_measured_latency (samplecnt_t n)
{
	_measured_latency = n;
}

samplecnt_t
PortInsert::latency() const
{
	/* because we deliver and collect within the same cycle,
	   all I/O is necessarily delayed by at least samples_per_cycle().

	   if the return port for insert has its own latency, we
	   need to take that into account too.
	*/

	if (_measured_latency == 0) {
		return _session.engine().samples_per_cycle() + _input->latency();
	} else {
		return _measured_latency;
	}
}

void
PortInsert::run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nframes, bool)
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

			outbuf.set_written (true);
		}

		return;

	} else if (_latency_flush_samples) {

		/* wait for the entire input buffer to drain before picking up input again so that we can't
		 * hear the remnants of whatever MTDM pumped into the pipeline.
		 */

		silence (nframes, start_sample);

		if (_latency_flush_samples > nframes) {
			_latency_flush_samples -= nframes;
		} else {
			_latency_flush_samples = 0;
		}

		return;
	}

	if (!check_active()) {
		/* deliver silence */
		silence (nframes, start_sample);
		return;
	}

	_out->run (bufs, start_sample, end_sample, speed, nframes, true);
	_input->collect_input (bufs, nframes, ChanCount::ZERO);
}

XMLNode&
PortInsert::state ()
{
	XMLNode& node = IOProcessor::state ();
	node.set_property ("type", "port");
	node.set_property ("bitslot", _bitslot);
	node.set_property ("latency", _measured_latency);
	node.set_property ("block-size", _session.get_block_size());

	return node;
}

int
PortInsert::set_state (const XMLNode& node, int version)
{
	XMLNodeList nlist = node.children();
	XMLNodeIterator niter;
	XMLPropertyList plist;

	const XMLNode* insert_node = &node;

	// legacy sessions: search for child Redirect node
	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		if ((*niter)->name() == "Redirect") {
			insert_node = *niter;
			break;
		}
	}

	IOProcessor::set_state (*insert_node, version);

	std::string type_str;
	if (!node.get_property ("type", type_str)) {
		error << _("XML node describing port insert is missing the `type' field") << endmsg;
		return -1;
	}

	if (type_str != "port") {
		error << _("non-port insert XML used for port plugin insert") << endmsg;
		return -1;
	}

	uint32_t blocksize = 0;
	node.get_property ("block-size", blocksize);

	//if the jack period is the same as when the value was saved, we can recall our latency..
	if ( (_session.get_block_size() == blocksize) ) {
		node.get_property ("latency", _measured_latency);
	}

	if (!node.property ("ignore-bitslot")) {
		uint32_t bitslot;
		if (node.get_property ("bitslot", bitslot)) {
			_session.unmark_insert_id (_bitslot);
			_bitslot = bitslot;
			_session.mark_insert_id (_bitslot);
		} else {
			_bitslot = _session.next_insert_id();
		}
	}

	return 0;
}

ARDOUR::samplecnt_t
PortInsert::signal_latency() const
{
	/* because we deliver and collect within the same cycle,
	 * all I/O is necessarily delayed by at least samples_per_cycle().
	 *
	 * if the return port for insert has its own latency, we
	 * need to take that into account too.
	 */

	if (_measured_latency == 0) {
		return _session.engine().samples_per_cycle()
		       + _input->connected_latency (false)
		       + _output->connected_latency (true);
	} else {
		return _measured_latency;
	}
}

/** Caller must hold process lock */
bool
PortInsert::configure_io (ChanCount in, ChanCount out)
{
#ifndef PLATFORM_WINDOWS
	assert (!AudioEngine::instance()->process_lock().trylock());
#endif

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
PortInsert::can_support_io_configuration (const ChanCount& in, ChanCount& out)
{
	out = in;
	return true;
}

bool
PortInsert::set_name (const std::string& new_name)
{
	string unique_name = validate_name (new_name, string_compose (_("insert %1"), _bitslot));

	if (unique_name.empty ()) {
		return false;
	}

	return IOProcessor::set_name (unique_name);
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
