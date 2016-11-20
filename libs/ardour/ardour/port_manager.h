/*
  Copyright (C) 2013 Paul Davis

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

#ifndef __libardour_port_manager_h__
#define __libardour_port_manager_h__

#include <vector>
#include <string>
#include <exception>
#include <map>

#include <stdint.h>

#include <boost/shared_ptr.hpp>

#include "pbd/rcu.h"
#include "pbd/ringbuffer.h"

#include "ardour/chan_count.h"
#include "ardour/midiport_manager.h"
#include "ardour/port.h"

namespace ARDOUR {

class PortEngine;
class AudioBackend;
class Session;

class LIBARDOUR_API PortManager
{
  public:
	typedef std::map<std::string,boost::shared_ptr<Port> > Ports;
	typedef std::list<boost::shared_ptr<Port> > PortList;

	PortManager ();
	virtual ~PortManager() {}

	PortEngine& port_engine();

	uint32_t port_name_size() const;
	std::string my_name() const;

	/* Port registration */

	boost::shared_ptr<Port> register_input_port (DataType, const std::string& portname, bool async = false, PortFlags extra_flags = PortFlags (0));
	boost::shared_ptr<Port> register_output_port (DataType, const std::string& portname, bool async = false, PortFlags extra_flags = PortFlags (0));
	int unregister_port (boost::shared_ptr<Port>);

	/* Port connectivity */

	int  connect (const std::string& source, const std::string& destination);
	int  disconnect (const std::string& source, const std::string& destination);
	int  disconnect (boost::shared_ptr<Port>);
	int  disconnect (std::string const &);
	int  reestablish_ports ();
	int  reconnect_ports ();

	bool  connected (const std::string&);
	bool  physically_connected (const std::string&);
	int   get_connections (const std::string&, std::vector<std::string>&);

	/* Naming */

	boost::shared_ptr<Port> get_port_by_name (const std::string &);
	void                    port_renamed (const std::string&, const std::string&);
	std::string             make_port_name_relative (const std::string& name) const;
	std::string             make_port_name_non_relative (const std::string& name) const;
	std::string             get_pretty_name_by_name (const std::string& portname) const;
	bool                    port_is_mine (const std::string& fullname) const;

	static bool port_is_control_only (std::string const &);

	/* other Port management */

	bool      port_is_physical (const std::string&) const;
	void      get_physical_outputs (DataType type, std::vector<std::string>&,
	                                MidiPortFlags include = MidiPortFlags (0),
	                                MidiPortFlags exclude = MidiPortFlags (0));
	void      get_physical_inputs (DataType type, std::vector<std::string>&,
	                               MidiPortFlags include = MidiPortFlags (0),
	                               MidiPortFlags exclude = MidiPortFlags (0));
	ChanCount n_physical_outputs () const;
	ChanCount n_physical_inputs () const;

	int get_ports (const std::string& port_name_pattern, DataType type, PortFlags flags, std::vector<std::string>&);
	int get_ports (DataType, PortList&);

	void remove_all_ports ();
	void clear_pending_port_deletions ();
	virtual void add_pending_port_deletion (Port*) = 0;
	RingBuffer<Port*>& port_deletions_pending () { return _port_deletions_pending; }

	/* per-Port monitoring */

	bool can_request_input_monitoring () const;
	void request_input_monitoring (const std::string&, bool) const;
	void ensure_input_monitoring (const std::string&, bool) const;

	class PortRegistrationFailure : public std::exception {
	                                        public:
		PortRegistrationFailure (std::string const & why = "")
			: reason (why) {}

		~PortRegistrationFailure () throw () {}

		const char *what() const throw () { return reason.c_str(); }

	                                        private:
		std::string reason;
	};

	/* the port engine will invoke these callbacks when the time is right */

	void registration_callback ();
	int graph_order_callback ();
	void connect_callback (const std::string&, const std::string&, bool connection);

	bool port_remove_in_progress() const { return _port_remove_in_progress; }

	struct MidiPortInformation {
		std::string   pretty_name;
		bool          input;
		MidiPortFlags properties;

		MidiPortInformation () : input (false) , properties (MidiPortFlags (0)) {}
	};

	void fill_midi_port_info ();

	MidiPortInformation midi_port_information (std::string const&);
	void get_known_midi_ports (std::vector<std::string>&);
	void get_midi_selection_ports (std::vector<std::string>&);
	void add_midi_port_flags (std::string const&, MidiPortFlags);
	void remove_midi_port_flags (std::string const&, MidiPortFlags);
	void set_midi_port_pretty_name (std::string const&, std::string const&);

	/** Emitted if the list of ports to be used for MIDI selection tracking changes */
	PBD::Signal0<void> MidiSelectionPortsChanged;
	/** Emitted if anything other than the selection property for a MIDI port changes */
	PBD::Signal0<void> MidiPortInfoChanged;

	/** Emitted if the backend notifies us of a graph order event */
	PBD::Signal0<void> GraphReordered;

	/** Emitted if a Port is registered or unregistered */
	PBD::Signal0<void> PortRegisteredOrUnregistered;

	/** Emitted if a Port is connected or disconnected.
	 *  The Port parameters are the ports being connected / disconnected, or 0 if they are not known to Ardour.
	 *  The std::string parameters are the (long) port names.
	 *  The bool parameter is true if ports were connected, or false for disconnected.
	 */
	PBD::Signal5<void, boost::weak_ptr<Port>, std::string, boost::weak_ptr<Port>, std::string, bool> PortConnectedOrDisconnected;

  protected:
	boost::shared_ptr<AudioBackend> _backend;
	SerializedRCUManager<Ports> ports;
	bool _port_remove_in_progress;
	RingBuffer<Port*> _port_deletions_pending;

	boost::shared_ptr<Port> register_port (DataType type, const std::string& portname, bool input, bool async = false, PortFlags extra_flags = PortFlags (0));
	void port_registration_failure (const std::string& portname);

	/** List of ports to be used between ::cycle_start() and ::cycle_end()
	 */
	boost::shared_ptr<Ports> _cycle_ports;

	void fade_out (gain_t, gain_t, pframes_t);
	void silence (pframes_t nframes, Session *s = 0);
	void silence_outputs (pframes_t nframes);
	void check_monitoring ();
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
	void cycle_end (pframes_t nframes);

	typedef std::map<std::string,MidiPortInformation> MidiPortInfo;

	mutable Glib::Threads::Mutex midi_port_info_mutex;
	MidiPortInfo midi_port_info;

	static std::string midi_port_info_file ();
	bool midi_info_dirty;
	void save_midi_port_info ();
	void load_midi_port_info ();
	void fill_midi_port_info_locked ();

	void filter_midi_ports (std::vector<std::string>&, MidiPortFlags, MidiPortFlags);
};



} // namespace

#endif /* __libardour_port_manager_h__ */
