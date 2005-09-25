#ifndef __ardour_gtk_stop_signal_h__
#define __ardour_gtk_stop_signal_h__

#include <gtkmm.h>
#include <gtk/gtksignal.h>

static inline gint
stop_signal (Gtk::Widget& widget, const char *signal_name)
{
	gtk_signal_emit_stop_by_name (GTK_OBJECT(widget.gobj()), signal_name);
	return TRUE;
}

#endif /* __ardour_gtk_stop_signal_h__ */
