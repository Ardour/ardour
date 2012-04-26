/*
    Copyright (C) 1998 Paul Barton-Davis
    
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

#ifndef __midi_manager_h__
#define __midi_manager_h__

#include <list>

#include <string>

#include "pbd/rcu.h"

#include "midi++/types.h"
#include "midi++/port.h"

namespace MIDI {

class MachineControl;	

class Manager {
  public:
	~Manager ();
	
	/** Signal the start of an audio cycle.
	 * This MUST be called before any reading/writing for this cycle.
	 * Realtime safe.
	 */
	void cycle_start (pframes_t nframes);
	
	/** Signal the end of an audio cycle.
	 * This signifies that the cycle began with @ref cycle_start has ended.
	 * This MUST be called at the end of each cycle.
	 * Realtime safe.
	 */
	void cycle_end ();

	MachineControl* mmc () const { return _mmc; }
	Port *mtc_input_port() const { return _mtc_input_port; }
	Port *mtc_output_port() const { return _mtc_output_port; }
	Port *midi_input_port() const { return _midi_input_port; }
	Port *midi_output_port() const { return _midi_output_port; }
	Port *midi_clock_input_port() const { return _midi_clock_input_port; }
	Port *midi_clock_output_port() const { return _midi_clock_output_port; }

	Port* add_port (Port *);
	void remove_port (Port *);

	Port* port (std::string const &);

	void set_port_states (std::list<XMLNode*>);

	typedef std::list<Port *> PortList;

	boost::shared_ptr<const PortList> get_midi_ports() const { return _ports.reader (); } 

	static void create (jack_client_t* jack);
	
	static Manager *instance () {
		return theManager;
	}
	static void destroy ();

	void reestablish (jack_client_t *);
	void reconnect ();

	PBD::Signal0<void> PortsChanged;

  private:
	/* This is a SINGLETON pattern */
	
	Manager (jack_client_t *);
	static Manager *theManager;

	MIDI::MachineControl*   _mmc;
	MIDI::Port*         _mtc_input_port;
	MIDI::Port*         _mtc_output_port;
	MIDI::Port*         _midi_input_port;
	MIDI::Port*         _midi_output_port;
	MIDI::Port*         _midi_clock_input_port;
	MIDI::Port*         _midi_clock_output_port;

	SerializedRCUManager<PortList> _ports;
};

} // namespace MIDI

#endif  // __midi_manager_h__
