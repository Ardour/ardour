/*
    Copyright (C) 2012 Paul Davis

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

#include <set>
#include <boost/shared_ptr.hpp>
#include <glibmm/threads.h>
#include <sigc++/signal.h>

#include "pbd/signals.h"

#include "ardour/session_handle.h"

namespace ARDOUR {

class AutomationControl;

class AutomationWatch : public sigc::trackable, public ARDOUR::SessionHandlePtr, public PBD::ScopedConnectionList {
  public:
    static AutomationWatch& instance();

    void add_automation_watch (boost::shared_ptr<ARDOUR::AutomationControl>);
    void remove_automation_watch (boost::shared_ptr<ARDOUR::AutomationControl>);
    void set_session (ARDOUR::Session*);

    gint timer ();

  private:
    typedef std::set<boost::shared_ptr<ARDOUR::AutomationControl> > AutomationWatches;

    AutomationWatch ();
    ~AutomationWatch();

    static AutomationWatch* _instance;
    Glib::Threads::Thread*  _thread;
    bool                    _run_thread;
    AutomationWatches        automation_watches;
    Glib::Threads::Mutex     automation_watch_lock;
    PBD::ScopedConnection    transport_connection;

    void transport_state_change ();
    void remove_weak_automation_watch (boost::weak_ptr<ARDOUR::AutomationControl>);
    void thread ();
};

}
