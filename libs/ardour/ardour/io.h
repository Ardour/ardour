/*
    Copyright (C) 2000 Paul Davis

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

#ifndef __ardour_io_h__
#define __ardour_io_h__

#include <string>
#include <vector>
#include <cmath>
#include <jack/jack.h>

#include <glibmm/threads.h>

#include "pbd/fastlog.h"
#include "pbd/undo.h"
#include "pbd/statefuldestructible.h"
#include "pbd/controllable.h"

#include "ardour/ardour.h"
#include "ardour/automation_control.h"
#include "ardour/bundle.h"
#include "ardour/chan_count.h"
#include "ardour/data_type.h"
#include "ardour/latent.h"
#include "ardour/port_set.h"
#include "ardour/session_object.h"
#include "ardour/libardour_visibility.h"
#include "ardour/types.h"
#include "ardour/utils.h"
#include "ardour/buffer_set.h"

class XMLNode;

namespace ARDOUR {

class Amp;
class AudioEngine;
class AudioPort;
class Bundle;
class MidiPort;
class PeakMeter;
class Port;
class Processor;
class Session;
class UserBundle;

/** A collection of ports (all input or all output) with connections.
 *
 * An IO can contain ports of varying types, making routes/inserts/etc with
 * varied combinations of types (eg MIDI and audio) possible.
 */
class LIBARDOUR_API IO : public SessionObject, public Latent
{
  public:
	static const std::string state_node_name;

	enum Direction {
		Input,
		Output
	};

        IO (Session&, const std::string& name, Direction, DataType default_type = DataType::AUDIO, bool sendish = false);
        IO (Session&, const XMLNode&, DataType default_type = DataType::AUDIO, bool sendish = false);

	virtual ~IO();

	Direction direction() const { return _direction; }

	DataType default_type() const         { return _default_type; }
	void     set_default_type(DataType t) { _default_type = t; }

	bool active() const { return _active; }
	void set_active(bool yn) { _active = yn; }

	bool set_name (const std::string& str);

	virtual void silence (framecnt_t);
	void increment_port_buffer_offset (pframes_t offset);

	int ensure_io (ChanCount cnt, bool clear, void *src);

        int connect_ports_to_bundle (boost::shared_ptr<Bundle>, bool exclusive, void *);
	int disconnect_ports_from_bundle (boost::shared_ptr<Bundle>, void *);

	BundleList bundles_connected ();

	boost::shared_ptr<Bundle> bundle () { return _bundle; }

	int add_port (std::string connection, void *src, DataType type = DataType::NIL);
	int remove_port (boost::shared_ptr<Port>, void *src);
	int connect (boost::shared_ptr<Port> our_port, std::string other_port, void *src);
	int disconnect (boost::shared_ptr<Port> our_port, std::string other_port, void *src);
	int disconnect (void *src);
	bool connected_to (boost::shared_ptr<const IO>) const;
	bool connected_to (const std::string&) const;
	bool connected () const;
	bool physically_connected () const;

	framecnt_t signal_latency () const { return 0; }
	framecnt_t latency () const;

	PortSet& ports() { return _ports; }
	const PortSet& ports() const { return _ports; }

	bool has_port (boost::shared_ptr<Port>) const;

	boost::shared_ptr<Port> nth (uint32_t n) const {
		if (n < _ports.num_ports()) {
			return _ports.port(n);
		} else {
			return boost::shared_ptr<Port> ();
		}
	}

	boost::shared_ptr<Port> port_by_name (const std::string& str) const;

	boost::shared_ptr<AudioPort> audio(uint32_t n) const;
	boost::shared_ptr<MidiPort>  midi(uint32_t n) const;

	const ChanCount& n_ports ()  const { return _ports.count(); }

	/* The process lock will be held on emission of this signal if
	 * IOChange contains ConfigurationChanged.  In other cases,
	 * the process lock status is undefined.
	 */
	PBD::Signal2<void, IOChange, void *> changed;

	virtual XMLNode& state (bool full);
	XMLNode& get_state (void);
	int set_state (const XMLNode&, int version);
	int set_state_2X (const XMLNode&, int, bool);
	static void prepare_for_reset (XMLNode&, const std::string&);

	class BoolCombiner {
	public:

		typedef bool result_type;

		template <typename Iter>
		result_type operator() (Iter first, Iter last) const {
			bool r = false;
			while (first != last) {
				if (*first) {
					r = true;
				}
				++first;
			}

			return r;
		}
	};

	/** Emitted when the port count is about to change.  Objects
	 *  can attach to this, and return `true' if they want to prevent
	 *  the change from happening.
	 */
	PBD::Signal1<bool, ChanCount, BoolCombiner> PortCountChanging;

	static int disable_connecting ();
	static int enable_connecting ();

	static PBD::Signal1<void, ChanCount> PortCountChanged; // emitted when the number of ports changes

	static std::string name_from_state (const XMLNode&);
	static void set_name_in_state (XMLNode&, const std::string&);

	/* we have to defer/order port connection. this is how we do it.
	*/

	static PBD::Signal0<int> ConnectingLegal;
	static bool              connecting_legal;

	XMLNode *pending_state_node;
	int pending_state_node_version;
	bool pending_state_node_in;

	/* three utility functions - this just seems to be simplest place to put them */

	void collect_input (BufferSet& bufs, pframes_t nframes, ChanCount offset);
	void process_input (boost::shared_ptr<Processor>, framepos_t start_frame, framepos_t end_frame, pframes_t nframes);
	void copy_to_outputs (BufferSet& bufs, DataType type, pframes_t nframes, framecnt_t offset);

	/* AudioTrack::deprecated_use_diskstream_connections() needs these */

	int set_ports (const std::string& str);

  private:
	mutable Glib::Threads::Mutex io_lock;

  protected:
	PortSet   _ports;
	Direction _direction;
	DataType _default_type;
	bool     _active;
        bool     _sendish;

  private:
	int connecting_became_legal ();
	PBD::ScopedConnection connection_legal_c;

	boost::shared_ptr<Bundle> _bundle; ///< a bundle representing our ports

	struct UserBundleInfo {
		UserBundleInfo (IO*, boost::shared_ptr<UserBundle> b);
		boost::shared_ptr<UserBundle> bundle;
		PBD::ScopedConnection changed;
	};

	std::vector<UserBundleInfo*> _bundles_connected; ///< user bundles connected to our ports

	static int parse_io_string (const std::string&, std::vector<std::string>& chns);
	static int parse_gain_string (const std::string&, std::vector<std::string>& chns);

	int ensure_ports (ChanCount, bool clear, void *src);

	void check_bundles_connected ();

	void bundle_changed (Bundle::Change);

	int get_port_counts (const XMLNode& node, int version, ChanCount& n, boost::shared_ptr<Bundle>& c);
	int get_port_counts_2X (const XMLNode& node, int version, ChanCount& n, boost::shared_ptr<Bundle>& c);
	int create_ports (const XMLNode&, int version);
	int make_connections (const XMLNode&, int, bool);
	int make_connections_2X (const XMLNode &, int, bool);

	boost::shared_ptr<Bundle> find_possible_bundle (const std::string &desired_name);

	int ensure_ports_locked (ChanCount, bool clear, bool& changed);

	std::string build_legal_port_name (DataType type);
	int32_t find_port_hole (const char* base);

	void setup_bundle ();
	std::string bundle_channel_name (uint32_t, uint32_t, DataType) const;

	BufferSet _buffers;
	void disconnect_check (boost::shared_ptr<ARDOUR::Port>, boost::shared_ptr<ARDOUR::Port>);
};

} // namespace ARDOUR

#endif /*__ardour_io_h__ */
