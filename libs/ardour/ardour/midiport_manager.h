/*
 * Copyright (C) 1998-2018 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __midiport_manager_h__
#define __midiport_manager_h__

#include <list>

#include <string>

#include "pbd/rcu.h"

#include "midi++/port.h"
#include "midi++/types.h"

#include "ardour/libardour_visibility.h"
#include "ardour/midi_port.h"
#include "ardour/types.h"

namespace ARDOUR {

class AsyncMIDIPort;
class MidiPort;
class Port;

class LIBARDOUR_API MidiPortManager
{
public:
	MidiPortManager ();
	virtual ~MidiPortManager ();

	/* Ports used for control. These are read/written to outside of the
     * process callback (asynchronously with respect to when data
     * actually arrives).
     *
     * More detail: we do actually read/write data for these ports
     * inside the process callback, but incoming data is only parsed
     * and outgoing data is only generated *outside* the process
     * callback.
     */

	boost::shared_ptr<ARDOUR::Port> mmc_input_port () const
	{
		return boost::dynamic_pointer_cast<MidiPort> (_mmc_in);
	}
	boost::shared_ptr<ARDOUR::Port> mmc_output_port () const
	{
		return boost::dynamic_pointer_cast<MidiPort> (_mmc_out);
	}

	boost::shared_ptr<ARDOUR::Port> scene_input_port () const
	{
		return boost::dynamic_pointer_cast<MidiPort> (_scene_in);
	}
	boost::shared_ptr<ARDOUR::Port> scene_output_port () const
	{
		return boost::dynamic_pointer_cast<MidiPort> (_scene_out);
	}

	/* Ports used to send synchronization. These have their output handled inside the
	 * process callback.
	 */

	boost::shared_ptr<MidiPort> mtc_output_port () const
	{
		return _mtc_output_port;
	}
	boost::shared_ptr<MidiPort> midi_clock_output_port () const
	{
		return _midi_clock_output_port;
	}

	/* Virtual MIDI keyboard output */
	boost::shared_ptr<AsyncMIDIPort> vkbd_output_port () const;

	void                set_midi_port_states (const XMLNodeList&);
	std::list<XMLNode*> get_midi_port_states () const;

	void set_public_latency (bool playback);

protected:
	/* asynchronously handled ports: ARDOUR::AsyncMIDIPort */
	boost::shared_ptr<Port> _mmc_in;
	boost::shared_ptr<Port> _mmc_out;
	boost::shared_ptr<Port> _scene_in;
	boost::shared_ptr<Port> _scene_out;
	boost::shared_ptr<Port> _vkbd_out;

	/* synchronously handled ports: ARDOUR::MidiPort */
	boost::shared_ptr<MidiPort> _mtc_output_port;
	boost::shared_ptr<MidiPort> _midi_clock_output_port;

	void create_ports ();
};

} // namespace ARDOUR

#endif
