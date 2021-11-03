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

#include "pbd/stateful.h"

#include "ardour/audioengine.h"
#include "ardour/boost_debug.h"
#include "ardour/debug.h"
#include "ardour/disk_reader.h"
#include "ardour/session.h"
#include "ardour/rc_configuration.h"
#include "ardour/transport_master_manager.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

const std::string TransportMasterManager::state_node_name = X_("TransportMasters");
TransportMasterManager* TransportMasterManager::_instance = 0;

TransportMasterManager::TransportMasterManager()
	: _master_speed (0)
	, _master_position (0)
	, _session (0)
	, _master_invalid_this_cycle (false)
	, disk_output_blocked (false)
	, master_dll_initstate (0)
{
}

TransportMasterManager::~TransportMasterManager ()
{
	Glib::Threads::RWLock::WriterLock lm (lock);
	_current_master.reset ();
	_transport_masters.clear ();
}

TransportMasterManager&
TransportMasterManager::create ()
{
	assert (!_instance);

	_instance = new TransportMasterManager;

	XMLNode* tmm_node = Config->transport_master_state ();

	if (tmm_node) {
		_instance->set_state (*tmm_node, Stateful::current_state_version);
	} else {
		_instance->set_default_configuration ();
	}

	return *_instance;
}

int
TransportMasterManager::set_default_configuration ()
{
	try {

		clear ();

		/* setup default transport masters. Most people will never need any
		   others
		*/

		add (Engine, X_("JACK Transport"), false);
		add (MTC, X_("MTC"), false);
		add (LTC, X_("LTC"), false);
		add (MIDIClock, X_("MIDI Clock"), false);

	} catch (...) {
		return -1;
	}

	_current_master = _transport_masters.back();
	return 0;
}

void
TransportMasterManager::set_session (Session* s)
{
	/* Called by AudioEngine in process context, synchronously with it's
	 * own "adoption" of the Session. The call will occur before the first
	 * call to ::pre_process_transport_masters().
	 */

	Glib::Threads::RWLock::ReaderLock lm (lock);

	maybe_restore_tc_format ();

	config_connection.disconnect ();

	_session = s;

	for (TransportMasters::iterator tm = _transport_masters.begin(); tm != _transport_masters.end(); ++tm) {
		(*tm)->set_session (s);
	}

	if (_session) {
		_session->config.ParameterChanged.connect_same_thread (config_connection, boost::bind (&TransportMasterManager::parameter_changed, this, _1));
	}

}

void
TransportMasterManager::parameter_changed (std::string const & what)
{
	if (what == "external-sync") {
		if (!_session->config.get_external_sync()) {
			/* disabled */
			unblock_disk_output ();
		}
	}
}

TransportMasterManager&
TransportMasterManager::instance()
{
	if (!_instance) {
		fatal << string_compose (_("programming error:%1"), X_("TransportMasterManager::instance() called without an instance!")) << endmsg;
		abort (); /* NOTREACHED */
	}
	return *_instance;
}

void
TransportMasterManager::destroy()
{
	delete _instance;
	_instance = 0;
}

// Called from AudioEngine::process_callback() BEFORE Session::process() is called. Each transport master has processed any incoming data for this cycle,
// and this method computes the transport speed that Ardour should use to get into and remain in sync with the master.
//
double
TransportMasterManager::pre_process_transport_masters (pframes_t nframes, samplepos_t now)
{
	Glib::Threads::RWLock::ReaderLock lm (lock, Glib::Threads::TRY_LOCK);

	if (!lm.locked()) {
		return 1.0;
	}

	boost::optional<samplepos_t> session_pos;

	if (_session) {
		session_pos = _session->audible_sample();
	}

	if (Config->get_run_all_transport_masters_always()) {
		for (TransportMasters::iterator tm = _transport_masters.begin(); tm != _transport_masters.end(); ++tm) {
			if ((*tm)->check_collect()) {
				(*tm)->pre_process (nframes, now, session_pos);
			}
		}
	}

	if (!_session) {
		return 1.0;
	}

	/* if we're not running ALL transport masters, but still have a current
	 * one, then we should run that one all the time so that we know
	 * precisely where it is when we starting chasing it ...
	 */

	if (!Config->get_run_all_transport_masters_always() && _current_master) {
		_current_master->pre_process (nframes, now, session_pos);
	}

	if (!_session->config.get_external_sync()) {
		maybe_restore_tc_format ();
		DEBUG_TRACE (DEBUG::Slave, string_compose ("no external sync, use session actual speed of %1\n", _session->actual_speed() ? _session->actual_speed() : 1.0));
		return _session->actual_speed () ? _session->actual_speed() : 1.0;
	}

	/* --- NOT REACHED UNLESS CHASING (i.e. _session->config.get_external_sync() is true ------*/

	if (!_current_master->ok()) {
		/* stop */
		_session->request_stop (false, false, _current_master->request_type());
		DEBUG_TRACE (DEBUG::Slave, "no roll2 - master has failed\n");
		_master_invalid_this_cycle = true;
		return 1.0;
	}

	if (!_current_master->locked()) {
		DEBUG_TRACE (DEBUG::Slave, "no roll4 - not locked\n");
		_master_invalid_this_cycle = true;
		return 1.0;
	}

	double engine_speed;
	samplepos_t ignore1, ignore2;

	if (!_current_master->speed_and_position (_master_speed, _master_position, ignore1, ignore2, now)) {
		return 1.0;
	}

	DEBUG_TRACE (DEBUG::Slave, string_compose ("Current master at %1 moving at %2\n", _master_position, _master_speed));

	if (_current_master->sample_clock_synced()) {

		/* No master DLL required. Speed identified by the master is
		 * our speed, quantized to {1.0, 0.0, -1.0}
		 */

		if (_master_speed > 0.0f) {
			engine_speed = 1.0f;
		} else if (_master_speed < 0.0f) {
			engine_speed = -1.0f;
		} else {
			engine_speed = 0.0f;
		}

		DEBUG_TRACE (DEBUG::Slave, string_compose ("S-clock synced master speed %1 used as %2\n", _master_speed, engine_speed));

	} else if (_master_speed != 0.0) {

		samplepos_t delta = _master_position;

		if (_session->compute_audible_delta (delta)) {

			if (master_dll_initstate == 0) {

				init_transport_master_dll (_master_speed, _master_position);
				DEBUG_TRACE (DEBUG::Slave, string_compose ("initializing master DLL, will be %1 next process cycle\n", master_dll_initstate));

				return _master_speed;
			}

			/* compute delta or "error" between the computed master_position for
			 * this cycle and the current session position.
			 *
			 * Remember: ::speed_and_position() is being called in process context
			 * but returns the predicted speed+position for the start of this process cycle,
			 * not just the most recent timestamp received by the current master object.
			 */

			DEBUG_TRACE (DEBUG::Slave, string_compose ("master DLL: delta = %1 (%2 vs %3) res: %4\n", delta, _master_position, _session->transport_sample(), _current_master->resolution()));

			if (delta > _current_master->resolution()) {

				// init_transport_master_dll (_master_speed, _master_position);

				if (!_session->actively_recording()) {
					DEBUG_TRACE (DEBUG::Slave, string_compose ("slave delta %1 greater than slave resolution %2 => no disk output\n", delta, _current_master->resolution()));
					/* run routes as normal, but no disk output */
					block_disk_output ();
				} else {
					unblock_disk_output ();
				}
			} else {
				unblock_disk_output ();
			}

			/* inject DLL with new data */

			DEBUG_TRACE (DEBUG::Slave, string_compose ("feed master DLL t0 %1 t1 %2 e %3 %4 e2 %5 sess %6\n", t0, t1, delta, _master_position, e2, _session->transport_sample()));

			const double e = delta;

			t0 = t1;
			t1 += b * e + e2;
			e2 += c * e;

			engine_speed = (t1 - t0) / nframes;

			DEBUG_TRACE (DEBUG::Slave, string_compose ("slave @ %1 speed %2 cur delta %3 matching speed %4\n", _master_position, _master_speed, delta, engine_speed));

			/* provide a .1% deadzone to lock the speed */
			if (fabs (engine_speed - 1.0) <= 0.001) {
				engine_speed = 1.0;
			}

			/* speed is set, we're locked, and good to go */
			DEBUG_TRACE (DEBUG::Slave, string_compose ("%1: computed speed-to-follow-master as %2\n", _current_master->name(), engine_speed));

		} else {

			/* session has not finished with latency compensation yet, so we cannot compute the
			   difference between the master and the session.
			*/
			engine_speed = 1.0;
		}

	} else {

		engine_speed = 1.0;
	}

	_master_invalid_this_cycle = false;

	maybe_set_tc_format ();

	DEBUG_TRACE (DEBUG::Slave, string_compose ("computed resampling ratio as %1 with position = %2 and speed = %3\n", engine_speed, _master_position, _master_speed));

	return engine_speed;
}

void
TransportMasterManager::maybe_restore_tc_format ()
{
	if (_session && _session_tc_format) {
		_session->config.set_timecode_format (*_session_tc_format);
	}
	_session_tc_format.reset ();
}

void
TransportMasterManager::maybe_set_tc_format ()
{
	if (!Config->get_timecode_sync_frame_rate() || !_session) {
		return;
	}
	boost::shared_ptr<TimecodeTransportMaster> tcm;
	if ((tcm = boost::dynamic_pointer_cast<TimecodeTransportMaster>(_current_master)) == 0) {
		return;
	}

	if (!tcm->apparent_timecode_format_valid ()) {
		return;
	}

	Timecode::TimecodeFormat stf = _session->config.get_timecode_format();
	Timecode::TimecodeFormat mtf = tcm->apparent_timecode_format ();

	if (stf == mtf) {
		return;
	}

	/* save session's original TC */
	if (!_session_tc_format) {
		_session_tc_format = stf;
	}

	warning << string_compose(_("Transport master adjusted framerate from %1 to %2."),
			Timecode::timecode_format_name(stf),
			Timecode::timecode_format_name(mtf))
		<< endmsg;

	_session->config.set_timecode_format (mtf);
}

void
TransportMasterManager::init_transport_master_dll (double speed, samplepos_t pos)
{
	/* the bandwidth of the DLL is a trade-off,
	 * because the max-speed of the transport in ardour is
	 * limited to +-8.0, a larger bandwidth would cause oscillations
	 *
	 * But this is only really a problem if the user performs manual
	 * seeks while transport is running and slaved to some timecode-y master.
	 */

	AudioEngine* ae = AudioEngine::instance();

	double const omega = 2.0 * M_PI * double(ae->samples_per_cycle()) / 2.0 / double(ae->sample_rate());
	b = 1.4142135623730950488 * omega;
	c = omega * omega;

	const int direction = (speed >= 0.0 ? 1 : -1);

	master_dll_initstate = direction;

	e2 = double (direction * ae->samples_per_cycle());
	t0 = double (pos);
	t1 = t0 + e2;

	DEBUG_TRACE (DEBUG::Slave, string_compose ("[re-]init ENGINE DLL %1 %2 %3 from %4 %5\n", t0,  t1, e2, speed, pos));
}

int
TransportMasterManager::add (SyncSource type, std::string const & name, bool removeable)
{
	int ret = 0;
	boost::shared_ptr<TransportMaster> tm;

	DEBUG_TRACE (DEBUG::Slave, string_compose ("adding new transport master, type %1 name %2 removeable %3\n", enum_2_string (type), name, removeable));

	{
		Glib::Threads::RWLock::WriterLock lm (lock);

		for (TransportMasters::const_iterator t = _transport_masters.begin(); t != _transport_masters.end(); ++t) {
			if ((*t)->name() == name) {
				error << string_compose (_("There is already a transport master named \"%1\" - not duplicated"), name) << endmsg;
				return -1;
			}
		}

		tm = TransportMaster::factory (type, name, removeable);

		if (!tm) {
			return -1;
		}

		BOOST_MARK_TMM (tm);

		ret = add_locked (tm);
	}

	if (ret == 0) {
		Added (tm);
	}

	return ret;
}

int
TransportMasterManager::add_locked (boost::shared_ptr<TransportMaster> tm)
{
	if (!tm) {
		return -1;
	}


	if (_session) {
		tm->set_session (_session);
	}

	_transport_masters.push_back (tm);
	return 0;
}

int
TransportMasterManager::remove (std::string const & name)
{
	int ret = -1;
	boost::shared_ptr<TransportMaster> tm;

	{
		Glib::Threads::RWLock::WriterLock lm (lock);

		for (TransportMasters::iterator t = _transport_masters.begin(); t != _transport_masters.end(); ++t) {
			if ((*t)->name() == name) {
				if (!(*t)->removeable()) {
					return -1;
				}
				tm = *t;
				_transport_masters.erase (t);
				ret = 0;
				break;
			}
		}
	}

	if (ret == 0) {
		Removed (tm);
	}

	return ret;
}

int
TransportMasterManager::set_current_locked (boost::shared_ptr<TransportMaster> c)
{
	if (c) {
		if (find (_transport_masters.begin(), _transport_masters.end(), c) == _transport_masters.end()) {
			warning << string_compose (X_("programming error: attempt to use unknown transport master \"%1\"\n"), c->name());
			return -1;
		}
	}

	maybe_restore_tc_format ();

	if (!c->usable()) {
		return -1;
	}

	/* this is called from within the process() call stack, but *after* the
	 * call to ::pre_process_transport_masters()
	 */

	_current_master = c;
	_master_speed = 0;
	_master_position = 0;
	_master_invalid_this_cycle = true;

	master_dll_initstate = 0;

	unblock_disk_output ();

	if (c && c->type() == Engine) {

		/* We cannot sync with an already moving JACK transport mechanism, so
		 * stop it before we start.
		 */

		AudioEngine::instance()->transport_stop ();
	}

	DEBUG_TRACE (DEBUG::Slave, string_compose ("current transport master set to %1\n", (c ? c->name() : string ("none"))));

	return 0;
}

int
TransportMasterManager::set_current (boost::shared_ptr<TransportMaster> c)
{
	int ret = -1;
	boost::shared_ptr<TransportMaster> old (_current_master);

	{
		Glib::Threads::RWLock::WriterLock lm (lock);
		ret = set_current_locked (c);
	}

	if (ret == 0) {
		CurrentChanged (old, _current_master); // EMIT SIGNAL
	}

	return ret;
}

int
TransportMasterManager::set_current (SyncSource ss)
{
	int ret = -1;
	boost::shared_ptr<TransportMaster> old (_current_master);

	{
		Glib::Threads::RWLock::WriterLock lm (lock);

		for (TransportMasters::iterator t = _transport_masters.begin(); t != _transport_masters.end(); ++t) {
			if ((*t)->type() == ss) {
				ret = set_current_locked (*t);
				break;
			}
		}
	}

	if (ret == 0) {
		CurrentChanged (old, _current_master); // EMIT SIGNAL
	}

	return ret;
}


int
TransportMasterManager::set_current (std::string const & str)
{
	int ret = -1;
	boost::shared_ptr<TransportMaster> old (_current_master);

	{
		Glib::Threads::RWLock::WriterLock lm (lock);

		for (TransportMasters::iterator t = _transport_masters.begin(); t != _transport_masters.end(); ++t) {
			if ((*t)->name() == str) {
				ret = set_current_locked (*t);
				break;
			}
		}
	}

	if (ret == 0) {
		CurrentChanged (old, _current_master); // EMIT SIGNAL
	}

	return ret;
}


void
TransportMasterManager::clear ()
{
	{
		Glib::Threads::RWLock::WriterLock lm (lock);
		_current_master.reset ();
		_transport_masters.clear ();
	}

	Removed (boost::shared_ptr<TransportMaster>());
}

int
TransportMasterManager::set_state (XMLNode const & node, int version)
{
	assert (node.name() == state_node_name);

	XMLNodeList const & children = node.children();

	{
		Glib::Threads::RWLock::WriterLock lm (lock);

		_current_master.reset ();
#if 0
		boost_debug_list_ptrs ();
#endif

		/* TramsportMasters live for the entire life of the
		 * program. TransportMasterManager::set_state() should only be
		 * called at the start of the program, and there should be no
		 * transport masters at that time.
		 */

		assert (_transport_masters.empty());

		for (XMLNodeList::const_iterator c = children.begin(); c != children.end(); ++c) {

			boost::shared_ptr<TransportMaster> tm = TransportMaster::factory (**c);

			if (!tm) {
				continue;
			}

			BOOST_MARK_TMM (tm);

			if (add_locked (tm)) {
				continue;
			}

			/* we know it is the last thing added to the list of masters */

			_transport_masters.back()->set_state (**c, version);
		}
	}

	/* fallback choice, lives on until ::restart() is called after the
	 * engine is running.
	 */

	set_current (MTC);

	return 0;
}

XMLNode&
TransportMasterManager::get_state ()
{
	XMLNode* node = new XMLNode (state_node_name);

	if (_current_master) {
		node->set_property (X_("current"), _current_master->name());
	}

	Glib::Threads::RWLock::ReaderLock lm (lock);

	for (TransportMasters::iterator t = _transport_masters.begin(); t != _transport_masters.end(); ++t) {
		node->add_child_nocopy ((*t)->get_state());
	}

	return *node;
}

boost::shared_ptr<TransportMaster>
TransportMasterManager::master_by_type (SyncSource src) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	for (TransportMasters::const_iterator tm = _transport_masters.begin(); tm != _transport_masters.end(); ++tm) {
		if ((*tm)->type() == src) {
			return *tm;
		}
	}

	return boost::shared_ptr<TransportMaster> ();
}

boost::shared_ptr<TransportMaster>
TransportMasterManager::master_by_port (boost::shared_ptr<Port> const &p) const
{
	Glib::Threads::RWLock::ReaderLock lm (lock);

	for (TransportMasters::const_iterator tm = _transport_masters.begin(); tm != _transport_masters.end(); ++tm) {
		if ((*tm)->port() == p) {
			return *tm;
		}
	}

	return boost::shared_ptr<TransportMaster> ();

}

void
TransportMasterManager::engine_stopped ()
{
	DEBUG_TRACE (DEBUG::Slave, "engine stopped, reset all transport masters\n");
	{
		Glib::Threads::RWLock::ReaderLock lm (lock);

		for (TransportMasters::const_iterator tm = _transport_masters.begin(); tm != _transport_masters.end(); ++tm) {
			(*tm)->reset (false);
		}
	}
}

void
TransportMasterManager::restart ()
{
	XMLNode* node;

	if ((node = Config->transport_master_state()) != 0) {

		{
			Glib::Threads::RWLock::ReaderLock lm (lock);

			for (TransportMasters::const_iterator tm = _transport_masters.begin(); tm != _transport_masters.end(); ++tm) {
				(*tm)->connect_port_using_state ();
				(*tm)->reset (false);
			}
		}

		/* engine is running, connections are viable ... try to set current */

		std::string current_master;

		if (node->get_property (X_("current"), current_master)) {

			/* may fal if current_master is not usable */

			set_current (current_master);
		}

	} else {
		if (TransportMasterManager::instance().set_default_configuration ()) {
			error << _("Cannot initialize transport master manager") << endmsg;
			/* XXX now what? */
		}
	}
}

void
TransportMasterManager::block_disk_output ()
{
	if (!disk_output_blocked) {
		//DiskReader::inc_no_disk_output ();
		disk_output_blocked = true;
	}
}

void
TransportMasterManager::unblock_disk_output ()
{
	if (disk_output_blocked) {
		//DiskReader::dec_no_disk_output ();
		disk_output_blocked = false;
	}
}

void
TransportMasterManager::reinit (double speed, samplepos_t pos)
{
	init_transport_master_dll (speed, pos);
}
