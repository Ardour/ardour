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
#include <gtk--/window.h>

#include "keyboard_target.h"

namespace ARDOUR {
	class Session;
};

/*
 * This virtual parent class is so that each dialog box uses the
 * same mechanism to declare its closing, and to have a common
 * method of connecting and disconnecting from a Session.
 */
class ArdourDialog : public Gtk::Window, public KeyboardTarget
{
  public:
	ArdourDialog (string name);
	~ArdourDialog();

	bool within_hiding() const { return _within_hiding; }

	void run ();
	void stop (int);
	void close ();
	void set_keyboard_input (bool yn);
	void set_hide_on_stop (bool yn);
	int  run_status();

	gint enter_notify_event_impl (GdkEventCrossing*);
	gint leave_notify_event_impl (GdkEventCrossing*);
	gint unmap_event_impl (GdkEventAny *);

	ARDOUR::Session *session;

	virtual void set_session (ARDOUR::Session* s) {
		session = s;
	}

	virtual void session_gone () {
		set_session (0);
	}

	void quit ();
	void wm_close();
	void wm_doi ();
	gint wm_close_event (GdkEventAny *);
	gint wm_doi_event (GdkEventAny *);
	gint wm_doi_event_stop (GdkEventAny *);

  private:
	int  _run_status;
	bool _within_hiding;
	bool kbd_input;
	bool running;
	bool hide_on_stop;
};

#endif // __ardour_dialog_h__
