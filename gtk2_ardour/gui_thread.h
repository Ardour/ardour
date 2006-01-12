#ifndef __ardour_gtk_gui_thread_h__
#define __ardour_gtk_gui_thread_h__

#include <gtkmm2ext/gtk_ui.h>

#define ENSURE_GUI_THREAD(slot) \
     if (!Gtkmm2ext::UI::instance()->caller_is_gui_thread()) {\
	Gtkmm2ext::UI::instance()->call_slot ((slot));\
        return;\
     }

#endif /* __ardour_gtk_gui_thread_h__ */
