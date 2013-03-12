/*
    Copyright (C) 2009-2013 Paul Davis
    Authors: Sampo Savolainen, Jannis Pohlmann

<<<<<<< HEAD
#include <glibmm/threads.h>

#include "pbd/abstract_ui.h"

#include <cwiid.h>
=======
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
>>>>>>> master

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

<<<<<<< HEAD
#define ENSURE_WIIMOTE_THREAD(slot) \
	if (Glib::Threads::Thread::self() != main_thread) {	\
		slot_mutex.lock();\
		slot_list.push_back(slot);\
		slot_cond.signal();\
		slot_mutex.unlock();\
		return;\
	} 
=======
*/
>>>>>>> master

#ifndef ardour_wiimote_control_protocol_h
#define ardour_wiimote_control_protocol_h

<<<<<<< HEAD
class WiimoteControlProtocol : public ARDOUR::ControlProtocol {
  public:
    WiimoteControlProtocol (ARDOUR::Session &);
    virtual ~WiimoteControlProtocol ();
    
    static bool probe();
    
    int set_active (bool yn);
    XMLNode& get_state();
    int set_state(const XMLNode&);
    
    void wiimote_callback(cwiid_wiimote_t *, int, union cwiid_mesg [], 
			  struct timespec *);
    
  private:
    
    void wiimote_main();
    volatile bool main_thread_quit;
    volatile bool restart_discovery;
    
    Glib::Threads::Thread *main_thread;
    
    void update_led_state();
    
    bool callback_thread_registered_for_ardour;
    
    static uint16_t button_state;
    
    cwiid_wiimote_t *wiimote_handle;
    
    Glib::Threads::Cond slot_cond;
    Glib::Threads::Mutex slot_mutex;
    
    std::list< sigc::slot<void> > slot_list;
    
    sigc::connection transport_state_conn;
    sigc::connection record_state_conn;
=======
#include <cwiid.h>

#include "pbd/abstract_ui.h"
#include "ardour/types.h"
#include "control_protocol/control_protocol.h"

struct WiimoteControlUIRequest : public BaseUI::BaseRequestObject {
public:
	WiimoteControlUIRequest () {}
	~WiimoteControlUIRequest () {}
};

class WiimoteControlProtocol
	: public ARDOUR::ControlProtocol
	, public AbstractUI<WiimoteControlUIRequest>
{
public:
	WiimoteControlProtocol (ARDOUR::Session &);
	virtual ~WiimoteControlProtocol ();

	static bool probe ();
	int set_active (bool yn);

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

	void start_wiimote_discovery ();
	void stop_wiimote_discovery ();

	void wiimote_callback (int mesg_count, union cwiid_mesg mesg[]);

protected:
	void do_request (WiimoteControlUIRequest*);
	int start ();
	int stop ();

	void thread_init ();

	bool connect_idle ();
	bool connect_wiimote ();

	void update_led_state ();

protected:
	PBD::ScopedConnectionList session_connections;
	cwiid_wiimote_t* wiimote;
	GSource *idle_source;
	uint16_t button_state;
	bool callback_thread_registered;
>>>>>>> master
};

#endif  /* ardour_wiimote_control_protocol_h */

