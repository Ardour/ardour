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

#ifndef __midiport_manager_h__
#define __midiport_manager_h__

#include <list>

#include <string>

#include "pbd/rcu.h"

#include "midi++/types.h"
#include "midi++/port.h"

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"

namespace ARDOUR {

class MidiPort;
class Port;

class LIBARDOUR_API MidiPortManager {
  public:
    MidiPortManager();
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

    MIDI::Port* midi_input_port () const { return _midi_input_port; }
    MIDI::Port* midi_output_port () const { return _midi_output_port; }
    MIDI::Port* mmc_input_port () const { return _mmc_input_port; }
    MIDI::Port* mmc_output_port () const { return _mmc_output_port; }
    
    /* Ports used for synchronization. These have their I/O handled inside the
     * process callback.
     */

    boost::shared_ptr<MidiPort> mtc_input_port() const { return _mtc_input_port; }
    boost::shared_ptr<MidiPort> mtc_output_port() const { return _mtc_output_port; }
    boost::shared_ptr<MidiPort> midi_clock_input_port() const { return _midi_clock_input_port; }
    boost::shared_ptr<MidiPort> midi_clock_output_port() const { return _midi_clock_output_port; }
    
    void set_midi_port_states (const XMLNodeList&);
    std::list<XMLNode*> get_midi_port_states () const;

    PBD::Signal0<void> PortsChanged;

  protected:
    /* asynchronously handled ports: MIDI::Port */
    MIDI::Port* _midi_input_port;
    MIDI::Port* _midi_output_port;
    MIDI::Port* _mmc_input_port;
    MIDI::Port* _mmc_output_port;
    /* these point to the same objects as the 4 members above,
       but cast to their ARDOUR::Port base class
    */
    boost::shared_ptr<Port> _midi_in;
    boost::shared_ptr<Port> _midi_out;
    boost::shared_ptr<Port> _mmc_in;
    boost::shared_ptr<Port> _mmc_out;

    /* synchronously handled ports: ARDOUR::MidiPort */
    boost::shared_ptr<MidiPort> _mtc_input_port;
    boost::shared_ptr<MidiPort> _mtc_output_port;
    boost::shared_ptr<MidiPort> _midi_clock_input_port;
    boost::shared_ptr<MidiPort> _midi_clock_output_port;

    void create_ports ();

};

} // namespace MIDI

#endif  // __midi_port_manager_h__
