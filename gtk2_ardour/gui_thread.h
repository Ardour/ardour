#ifndef __ardour_gtk_gui_thread_h__
#define __ardour_gtk_gui_thread_h__

#define ENSURE_GUI_THREAD(slot) \
     if (!Gtkmmext::UI::instance()->caller_is_gui_thread()) {\
	Gtkmmext::UI::instance()->call_slot ((slot));\
        return;\
     }

#endif /* __ardour_gtk_gui_thread_h__ */
