/*
 * Copyright (C) 2012-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017-2019 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_automation_watch_h__
#define __ardour_automation_watch_h__

#include <set>
#include <boost/shared_ptr.hpp>
#include <glibmm/threads.h>
#include <sigc++/signal.h>

#include "pbd/signals.h"

#include "ardour/session_handle.h"
#include "ardour/types.h"

namespace ARDOUR {

class AutomationControl;

class LIBARDOUR_API AutomationWatch : public sigc::trackable, public ARDOUR::SessionHandlePtr
{
public:
	static AutomationWatch& instance();

	void add_automation_watch (boost::shared_ptr<ARDOUR::AutomationControl>);
	void remove_automation_watch (boost::shared_ptr<ARDOUR::AutomationControl>);
	void transport_stop_automation_watches (ARDOUR::samplepos_t);
	void set_session (ARDOUR::Session*);

	gint timer ();

private:
	typedef std::set<boost::shared_ptr<ARDOUR::AutomationControl> > AutomationWatches;
	typedef std::map<boost::shared_ptr<ARDOUR::AutomationControl>, PBD::ScopedConnection> AutomationConnection;

	AutomationWatch ();
	~AutomationWatch();

	static AutomationWatch* _instance;
	Glib::Threads::Thread*  _thread;
	samplepos_t             _last_time;
	bool                    _run_thread;
	AutomationWatches        automation_watches;
	AutomationConnection     automation_connections;
	Glib::Threads::Mutex     automation_watch_lock;
	PBD::ScopedConnection    transport_connection;

	void transport_state_change ();
	void remove_weak_automation_watch (boost::weak_ptr<ARDOUR::AutomationControl>);
	void thread ();
};

} /* namespace */

#endif // __ardour_automation_watch_h__
