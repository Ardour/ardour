/*
    Copyright (C) 2009 Paul Davis 

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

#include <algorithm>

#include <pbd/xml++.h>

#include <ardour/internal_send.h>
#include <ardour/session.h>
#include <ardour/port.h>
#include <ardour/audio_port.h>
#include <ardour/buffer_set.h>
#include <ardour/meter.h>
#include <ardour/panner.h>
#include <ardour/io.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;

InternalSend::InternalSend (Session& s, Placement p, boost::shared_ptr<IO> dst)
	: IOProcessor (s, string_compose (_(">%1"), dst->name()), p, 
		       -1, -1, -1, -1,
		       DataType::AUDIO, false)
	, destination (dst)
{
	_metering = false;

	destination->input_changed.connect (mem_fun (*this, &InternalSend::destination_io_config_changed));

	destination_io_config_changed (ConfigurationChanged, this);

	ProcessorCreated (this); /* EMIT SIGNAL */
}

InternalSend::~InternalSend ()
{
	GoingAway ();
}

void
InternalSend::destination_io_config_changed (IOChange c, void* src)
{
	if (!(c & ConfigurationChanged)) {
		return;
	}

	_io->disconnect_outputs (this);

	_io->ensure_io (ChanCount::ZERO, destination->n_inputs(), false, this);

	PortSet::const_iterator us (_io->outputs().begin());
	PortSet::const_iterator them (destination->inputs().begin ());

	for (; us != _io->outputs().end() && them != destination->inputs().end(); ++us, ++them) {
		(const_cast<Port*>(&(*us)))->connect (const_cast<Port*>(&(*them)));
	}
}

XMLNode&
InternalSend::get_state(void)
{
	fatal << X_("InternalSend::get_state() called - should never happen") << endmsg;
	/*NOTREACHED*/
	return *(new XMLNode ("foo"));
}

int
InternalSend::set_state(const XMLNode& node)
{
	fatal << X_("InternalSend::set_state() called - should never happen") << endmsg;
	/*NOTREACHED*/
	return 0;
}

void
InternalSend::run_in_place (BufferSet& bufs, nframes_t start_frame, nframes_t end_frame, nframes_t nframes, nframes_t offset)
{
	if (active()) {

		// we have to copy the input, because IO::deliver_output may alter the buffers
		// in-place, which a send must never do. otherwise its gain settings will
		// affect the signal seen later in the parent Route.

		// BufferSet& sendbufs = _session.get_mix_buffers(bufs.count());

		// sendbufs.read_from (bufs, nframes);
		// assert(sendbufs.count() == bufs.count());

		_io->deliver_output (bufs, start_frame, end_frame, nframes, offset);

		if (_metering) {
			if (_io->effective_gain() == 0) {
				_io->peak_meter().reset();
			} else {
				_io->peak_meter().run_in_place(_io->output_buffers(), start_frame, end_frame, nframes, offset);
			}
		}

	} else {
		_io->silence (nframes, offset);
		if (_metering) {
			_io->peak_meter().reset();
		}
	}
}

void
InternalSend::set_metering (bool yn)
{
	_metering = yn;

	if (!_metering) {
		/* XXX possible thread hazard here */
		_io->peak_meter().reset();
	}
}

bool
InternalSend::can_support_io_configuration (const ChanCount& in, ChanCount& out_is_ignored) const
{
	/* number of outputs is fixed (though mutable by changing the I/O configuration
	   of the destination)
	*/

	cerr << "IS: testing I/O config in=" << in.n_audio() << " out=" << out_is_ignored.n_audio() << endl;

	if (in == _io->n_outputs()) {
		return 1;
	}

	return -1;
}

bool
InternalSend::configure_io (ChanCount in, ChanCount out)
{
	cerr << "Configure IS for in " << in.n_audio() << " out = " << out.n_audio() << endl;

	/* we're transparent no matter what.  fight the power. */

	if (out != in) {
		return false;
	}

	_io->set_output_maximum (in);
	_io->set_output_minimum (in);
	_io->set_input_maximum (ChanCount::ZERO);
	_io->set_input_minimum (ChanCount::ZERO);

	out = _io->n_outputs();

	Processor::configure_io(in, out);

	_io->reset_panner();

	return true;
}

ChanCount
InternalSend::output_streams() const
{
	// this method reflects the idea that from the perspective of the Route's ProcessorList, 
	// a send is just a passthrough. that doesn't match what the Send actually does with its 
	// data, but since what it does is invisible to the Route, it appears to be a passthrough.
	
	return _io->n_outputs ();
}

ChanCount
InternalSend::input_streams() const
{
	return _configured_input;
}


void
InternalSend::expect_inputs (const ChanCount& expected)
{
	if (expected != expected_inputs) {
		expected_inputs = expected;
		_io->reset_panner ();
	}
}

void
InternalSend::activate ()
{
	Processor::activate ();
}

void
InternalSend::deactivate ()
{
	Processor::deactivate ();
}
