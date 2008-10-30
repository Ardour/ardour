/*
    Copyright (C) 2002 Paul Davis

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

#ifndef __ardour_dialog_h__
#define __ardour_dialog_h__

#include <ardour/ardour.h>
#include <gtkmm/window.h>
#include <gtkmm/dialog.h>

namespace ARDOUR {
	class Session;
}

/*
 * This virtual parent class is so that each dialog box uses the
 * same mechanism to declare its closing, and to have a common
 * method of connecting and disconnecting from a Session.
 */
class ArdourDialog : public Gtk::Dialog
{
  public:
	ArdourDialog (std::string title, bool modal = false, bool use_separator = false);
	ArdourDialog (Gtk::Window& parent, std::string title, bool modal = false, bool use_separator = false);
	~ArdourDialog();

	static int close_all_current_dialogs (int response);

	bool on_key_press_event (GdkEventKey *);
	bool on_enter_notify_event (GdkEventCrossing*);
	bool on_leave_notify_event (GdkEventCrossing*);
	void on_unmap ();
	void on_show ();

	ARDOUR::Session *session;

	virtual void set_session (ARDOUR::Session* s) {
		session = s;
	}

	virtual void session_gone () {
		set_session (0);
	}

	static void close_all_dialogs () { CloseAllDialogs(); }

  private:
	static sigc::signal<void> CloseAllDialogs;
};

#endif // __ardour_dialog_h__

