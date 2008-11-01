#ifndef ardour_wiimote_control_protocol_h
#define ardour_wiimote_control_protocol_h

#include <ardour/types.h>
#include <control_protocol/control_protocol.h>

#include <glibmm/thread.h>

#include <cwiid.h>


namespace ARDOUR {
        class Session;
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
		
		void main_thread();

		bool thread_quit;
		bool thread_registered_for_ardour;

		Glib::Thread *thread;
		static uint16_t button_state;
};


#endif  /* ardour_wiimote_control_protocol_h */

