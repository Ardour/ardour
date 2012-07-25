#ifndef ardour_wiimote_control_protocol_h
#define ardour_wiimote_control_protocol_h

#include "ardour/types.h"
#include "control_protocol/control_protocol.h"

#include <glibmm/threads.h>

#include "pbd/abstract_ui.h"

#include <cwiid.h>


namespace ARDOUR {
        class Session;
}

#define ENSURE_WIIMOTE_THREAD(slot) \
	if (Glib::Threads::Thread::self() != main_thread) {	\
		slot_mutex.lock();\
		slot_list.push_back(slot);\
		slot_cond.signal();\
		slot_mutex.unlock();\
		return;\
	} 


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
};


#endif  /* ardour_wiimote_control_protocol_h */

