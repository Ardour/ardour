/*
    Copyright (C) 2014 Waves Audio Ltd.

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

#ifndef __gtk2_session_lock_dialog_h__
#define __gtk2_session_lock_dialog_h__

#include <string>
#include "waves_dialog.h"

//#include <gdkmm/pixbuf.h>
//#include <gtkmm/label.h>
//#include <gtkmm/drawingarea.h>
//#include <gtkmm/expander.h>
//#include <gtkmm/box.h>
//#include <gtkmm/radiobutton.h>
//#include <gtkmm/filechooserbutton.h>
//#include <gtkmm/scrolledwindow.h>
//#include <gtkmm/treeview.h>
//#include <gtkmm/treestore.h>
//#include <gtkmm/checkbutton.h>
//#include <gtkmm/table.h>
//#include <gtkmm/frame.h>
//#include <gtkmm/spinbutton.h>
//#include <gtkmm/liststore.h>
//#include <gtkmm/combobox.h>

//#include "ardour/utils.h"

class EngineControl;
#define MAX_RECENT_SESSION_COUNTS 10
class SessionLockDialog : public WavesDialog {
  public:

    SessionLockDialog ();
	~SessionLockDialog ();
    bool on_key_press_event (GdkEventKey*);
    
    void on_show ();
    void on_hide ();

  private:
	WavesButton& _ok_button;
	void on_ok(WavesButton*);
};

#endif /* __gtk2_session_lock_dialog_h__ */
