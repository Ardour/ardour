/*
 * Copyright (C) 2018-2019 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_transport_master_manager_h__
#define __ardour_transport_master_manager_h__

#include <string>

#include <boost/noncopyable.hpp>

#include "ardour/transport_master.h"
#include "ardour/types.h"

namespace ARDOUR {

class UI_TransportMaster;

class LIBARDOUR_API TransportMasterManager : public boost::noncopyable
{
  public:
	static TransportMasterManager& create ();
	~TransportMasterManager ();

	int set_default_configuration ();
	void restart ();
	void engine_stopped ();

	static TransportMasterManager& instance();
	static void destroy();
	/* this method is not thread-safe and is intended to be used only
	 * very early in application-lifetime to check if the TMM has
	 * been created yet. Do not use in other code.
	 */
	static bool exists() { return _instance != 0; }

	typedef std::list<boost::shared_ptr<TransportMaster> > TransportMasters;

	int add (SyncSource type, std::string const & name, bool removeable = true);
	int remove (std::string const & name);
	void clear ();

	PBD::Signal1<void,boost::shared_ptr<TransportMaster> > Added;
	PBD::Signal1<void,boost::shared_ptr<TransportMaster> > Removed; // null argument means "clear"

	double pre_process_transport_masters (pframes_t, samplepos_t session_transport_position);

	double get_current_speed_in_process_context() const { return _master_speed; }
	samplepos_t get_current_position_in_process_context() const { return _master_position; }

	boost::shared_ptr<TransportMaster> current() const { return _current_master; }
	int set_current (boost::shared_ptr<TransportMaster>);
	int set_current (SyncSource);
	int set_current (std::string const &);

	PBD::Signal2<void,boost::shared_ptr<TransportMaster>, boost::shared_ptr<TransportMaster> > CurrentChanged;

	int set_state (XMLNode const &, int);
	XMLNode& get_state();

	void set_session (Session*);
	Session* session() const { return _session; }

	bool master_invalid_this_cycle() const { return _master_invalid_this_cycle; }

	boost::shared_ptr<TransportMaster> master_by_type (SyncSource src) const;
	boost::shared_ptr<TransportMaster> master_by_name (std::string const &) const;
	boost::shared_ptr<TransportMaster> master_by_port (boost::shared_ptr<Port> const &p) const;

	TransportMasters const & transport_masters() const { return _transport_masters; }

	static const std::string state_node_name;

	void block_disk_output ();
	void unblock_disk_output ();
	void reinit (double speed, samplepos_t pos);

  private:
	TransportMasterManager();

	TransportMasters      _transport_masters;
	mutable Glib::Threads::RWLock  lock;
	double                _master_speed;
	samplepos_t           _master_position;

	boost::shared_ptr<TransportMaster> _current_master;
	Session* _session;

	bool _master_invalid_this_cycle;
	bool disk_output_blocked;

	/* a DLL to chase the transport master, calculate playback speed
	 * by matching Ardour's current playhead position against
	 * the position of the transport-master */
	double t0; // PH position at the beginning of this cycle
	double t1; // expected PH position if next cycle
	double e2; // second order loop error
	double bandwidth; // DLL filter bandwidth
	double b, c, omega; // DLL filter coefficients

	void init_transport_master_dll (double speed, samplepos_t pos);
	int master_dll_initstate; // play-direction -1, +1, or 0: not initialized

	static TransportMasterManager* _instance;

	/* original TC format in case the slave changed it */
	boost::optional<Timecode::TimecodeFormat> _session_tc_format;
	void maybe_restore_tc_format ();
	void maybe_set_tc_format ();

	int add_locked (boost::shared_ptr<TransportMaster>);
	double compute_matching_master_speed (pframes_t nframes, samplepos_t, bool& locate_required);
	int set_current_locked (boost::shared_ptr<TransportMaster>);

	PBD::ScopedConnection config_connection;
	void parameter_changed (std::string const & what);
};

} // namespace ARDOUR

#endif /* __ardour_transport_master_manager_h__ */
