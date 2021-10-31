/*
 * Copyright (C) 2013-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __libardour_port_manager_h__
#define __libardour_port_manager_h__

#include <exception>
#include <map>
#include <string>
#include <vector>

#include <stdint.h>

#include <boost/shared_ptr.hpp>

#include "pbd/natsort.h"
#include "pbd/rcu.h"
#include "pbd/ringbuffer.h"
#include "pbd/g_atomic_compat.h"

#include "ardour/chan_count.h"
#include "ardour/midiport_manager.h"
#include "ardour/monitor_port.h"
#include "ardour/port.h"

namespace ARDOUR {

class PortEngine;
class AudioBackend;
class Session;

class CircularSampleBuffer;
class CircularEventBuffer;

class LIBARDOUR_API PortManager
{
public:
	struct DPM {
		DPM ()
		{
			reset ();
		}
		void reset ()
		{
			level = 0;
			peak  = 0;
		}
		Sample level;
		Sample peak;
	};

	struct MPM {
		MPM ()
		{
			reset ();
		}
		void reset ()
		{
			memset (chn_active, 0, sizeof (float) * 17);
		}
		bool active (int chn) const {
			if (chn < 0 || chn > 16) {
				return false;
			}
			return chn_active[chn] > 0.1;
		}
		/* 0..15: MIDI Channel Event, 16: System Common Message */
		float chn_active[17];
	};

	struct SortByPortName {
		bool operator() (std::string const& a, std::string const& b) const {
			return PBD::naturally_less (a.c_str (), b.c_str ());
		}
	};

	typedef std::map<std::string, boost::shared_ptr<Port>, SortByPortName> Ports;
	typedef std::list<boost::shared_ptr<Port> >                            PortList;

	typedef boost::shared_ptr<CircularSampleBuffer> AudioPortScope;
	typedef boost::shared_ptr<CircularEventBuffer>  MIDIPortMonitor;
	typedef boost::shared_ptr<DPM>                  AudioPortMeter;
	typedef boost::shared_ptr<MPM>                  MIDIPortMeter;

	struct AudioInputPort {
		AudioInputPort (samplecnt_t);
		AudioPortScope scope;
		AudioPortMeter meter;
	};

	struct MIDIInputPort {
		MIDIInputPort (samplecnt_t);
		MIDIPortMonitor monitor;
		MIDIPortMeter   meter;
	};

	typedef std::map<std::string, AudioInputPort, SortByPortName> AudioInputPorts;
	typedef std::map<std::string, MIDIInputPort, SortByPortName>  MIDIInputPorts;

	PortManager ();
	virtual ~PortManager () {}

	PortEngine& port_engine ();

	uint32_t    port_name_size () const;
	std::string my_name () const;

#ifndef NDEBUG
	void list_cycle_ports () const;
	void list_all_ports () const;
#endif

	/* Port registration */

	boost::shared_ptr<Port> register_input_port (DataType, const std::string& portname, bool async = false, PortFlags extra_flags = PortFlags (0));
	boost::shared_ptr<Port> register_output_port (DataType, const std::string& portname, bool async = false, PortFlags extra_flags = PortFlags (0));
	int                     unregister_port (boost::shared_ptr<Port>);

	/* Port connectivity */

	int connect (const std::string& source, const std::string& destination);
	int disconnect (const std::string& source, const std::string& destination);
	int disconnect (boost::shared_ptr<Port>);
	int disconnect (std::string const&);
	int reestablish_ports ();
	int reconnect_ports ();

	bool connected (const std::string&);
	bool physically_connected (const std::string&);
	int  get_connections (const std::string&, std::vector<std::string>&);

	/* Naming */

	boost::shared_ptr<Port> get_port_by_name (const std::string&);
	void                    port_renamed (const std::string&, const std::string&);
	std::string             make_port_name_relative (const std::string& name) const;
	std::string             make_port_name_non_relative (const std::string& name) const;
	std::string             get_pretty_name_by_name (const std::string& portname) const;
	std::string             short_port_name_from_port_name (std::string const& full_name) const;
	bool                    port_is_mine (const std::string& fullname) const;

	static bool port_is_virtual_piano (std::string const&);
	static bool port_is_control_only (std::string const&);
	static bool port_is_physical_input_monitor_enable (std::string const&);

	/* other Port management */

	bool      port_is_physical (const std::string&) const;
	void      get_physical_outputs (DataType      type, std::vector<std::string>&,
	                                MidiPortFlags include = MidiPortFlags (0),
	                                MidiPortFlags exclude = MidiPortFlags (0));
	void      get_physical_inputs (DataType      type, std::vector<std::string>&,
	                               MidiPortFlags include = MidiPortFlags (0),
	                               MidiPortFlags exclude = MidiPortFlags (0));

	ChanCount n_physical_outputs () const;
	ChanCount n_physical_inputs () const;

	int get_ports (const std::string& port_name_pattern, DataType type, PortFlags flags, std::vector<std::string>&);
	int get_ports (DataType, PortList&);

	void set_port_pretty_name (std::string const&, std::string const&);

	void remove_all_ports ();

	void clear_pending_port_deletions ();

	virtual void add_pending_port_deletion (Port*) = 0;

	PBD::RingBuffer<Port*>& port_deletions_pending ()
	{
		return _port_deletions_pending;
	}

	bool check_for_ambiguous_latency (bool log = false) const;

	/* per-Port monitoring */

	bool can_request_input_monitoring () const;
	void request_input_monitoring (const std::string&, bool) const;
	void ensure_input_monitoring (const std::string&, bool) const;

	class PortRegistrationFailure : public std::exception
	{
	public:
		PortRegistrationFailure (std::string const& why = "")
			: reason (why) {}

		~PortRegistrationFailure () throw () {}

		const char* what () const throw ()
		{
			return reason.c_str ();
		}

	private:
		std::string reason;
	};

	/* the port engine will invoke these callbacks when the time is right */

	void registration_callback ();
	int  graph_order_callback ();
	void connect_callback (const std::string&, const std::string&, bool connection);

	bool port_remove_in_progress () const
	{
		return _port_remove_in_progress;
	}

	MidiPortFlags midi_port_metadata (std::string const&);

	void get_configurable_midi_ports (std::vector<std::string>&, bool for_input);
	void get_midi_selection_ports (std::vector<std::string>&);
	void add_midi_port_flags (std::string const&, MidiPortFlags);
	void remove_midi_port_flags (std::string const&, MidiPortFlags);

	/** Emitted if the list of ports to be used for MIDI selection tracking changes */
	PBD::Signal0<void> MidiSelectionPortsChanged;
	/** Emitted if anything other than the selection property for a MIDI port changes */
	PBD::Signal0<void> MidiPortInfoChanged;
	/** Emitted if pretty-name of a port changed */
	PBD::Signal1<void, std::string> PortPrettyNameChanged;

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

	PBD::Signal3<void, DataType, std::vector<std::string>, bool> PhysInputChanged;

	/* Input port meters and monitors */
	void reset_input_meters ();

	AudioInputPorts audio_input_ports () const;
	MIDIInputPorts  midi_input_ports () const;

	MonitorPort& monitor_port () {
		return _monitor_port;
	}

protected:
	boost::shared_ptr<AudioBackend> _backend;

	SerializedRCUManager<Ports> _ports;

	bool                   _port_remove_in_progress;
	PBD::RingBuffer<Port*> _port_deletions_pending;

	boost::shared_ptr<Port> register_port (DataType type, const std::string& portname, bool input, bool async = false, PortFlags extra_flags = PortFlags (0));
	void                    port_registration_failure (const std::string& portname);

	/** List of ports to be used between \ref cycle_start() and \ref cycle_end() */
	boost::shared_ptr<Ports> _cycle_ports;

	void silence (pframes_t nframes, Session* s = 0);
	void silence_outputs (pframes_t nframes);
	void check_monitoring ();
	/** Signal the start of an audio cycle.
	 * This MUST be called before any reading/writing for this cycle.
	 * Realtime safe.
	 */
	void cycle_start (pframes_t nframes, Session* s = 0);

	/** Signal the end of an audio cycle.
	 * This signifies that the cycle began with @ref cycle_start has ended.
	 * This MUST be called at the end of each cycle.
	 * Realtime safe.
	 */
	void cycle_end (pframes_t nframes, Session* s = 0);

	void cycle_end_fade_out (gain_t, gain_t, pframes_t, Session* s = 0);

	static std::string port_info_file ();
	static std::string midi_port_info_file ();

	void filter_midi_ports (std::vector<std::string>&, MidiPortFlags, MidiPortFlags);

	void set_port_buffer_sizes (pframes_t);

private:
	void run_input_meters (pframes_t, samplecnt_t);
	void set_pretty_names (std::vector<std::string> const&, DataType, bool);
	void fill_midi_port_info_locked ();
	void load_port_info ();
	void save_port_info ();
	void update_input_ports (bool);

	MonitorPort _monitor_port;

	struct PortID {
		PortID (boost::shared_ptr<AudioBackend>, DataType, bool, std::string const&);
		PortID (XMLNode const&, bool old_midi_format = false);

		std::string backend;
		std::string device_name;
		std::string port_name;
		DataType    data_type;
		bool        input;

		XMLNode& state () const;

		bool operator< (PortID const& o) const {
			if (backend != o.backend) {
				return backend < o.backend;
			}
			if (device_name != o.device_name) {
				return device_name < o.device_name;
			}
			if (port_name != o.port_name) {
				return PBD::naturally_less (port_name.c_str (), o.port_name.c_str ());
			}
			if (input != o.input) {
				return input;
			}
			return (uint32_t) data_type < (uint32_t) o.data_type;
		}

		bool operator== (PortID const& o) const {
			if (backend != o.backend) {
				return false;
			}
			if (device_name != o.device_name) {
				return false;
			}
			if (port_name != o.port_name) {
				return false;
			}
			if (input != o.input) {
				return false;
			}
			if (data_type != o.data_type) {
				return false;
			}
			return true;
		}
	};

	struct PortMetaData {
		PortMetaData () : properties (MidiPortFlags (0)) {}
		PortMetaData (XMLNode const&);

		std::string pretty_name;
		MidiPortFlags properties;
	};

	typedef std::map<PortID, PortMetaData> PortInfo;

	mutable Glib::Threads::Mutex _port_info_mutex;
	PortInfo                     _port_info;
	bool                         _midi_info_dirty;

	SerializedRCUManager<AudioInputPorts> _audio_input_ports;
	SerializedRCUManager<MIDIInputPorts>  _midi_input_ports;
	GATOMIC_QUAL gint                     _reset_meters;
};

} // namespace ARDOUR

#endif /* __libardour_port_manager_h__ */
