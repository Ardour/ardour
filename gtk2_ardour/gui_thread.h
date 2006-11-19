#ifndef __ardour_gtk_gui_thread_h__
#define __ardour_gtk_gui_thread_h__

#include <gtkmm2ext/gtk_ui.h>
#include <pbd/crossthread.h>

#define ENSURE_GUI_THREAD(slot) \
     if (!Gtkmm2ext::UI::instance()->caller_is_ui_thread()) {\
	Gtkmm2ext::UI::instance()->call_slot ((slot));\
        return;\
     }

#define GTK_SAFE(theSlot) crossthread_safe (Gtkmm2ext::UI::instance()->thread_id(),\
					    *Gtkmm2ext::UI::instance(), \
					    (theSlot))

#endif /* __ardour_gtk_gui_thread_h__ */
