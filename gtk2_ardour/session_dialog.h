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

#ifndef __gtk2_ardour_session_dialog_h__
#define __gtk2_ardour_session_dialog_h__

#include "pbd/signals.h"

#include <string>
#include "waves_dialog.h"

#include <gdkmm/pixbuf.h>
#include <gtkmm/label.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/expander.h>
#include <gtkmm/box.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/filechooserbutton.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/treeview.h>
#include <gtkmm/treestore.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/table.h>
#include <gtkmm/frame.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/liststore.h>
#include <gtkmm/combobox.h>

#include "ardour/utils.h"
#include "ardour/engine_state_controller.h"
#include "ardour/template_utils.h"

#include "ardour_dialog.h"
#include "window_manager.h"

class TracksControlPanel;

class EngineControl;
#define MAX_RECENT_SESSION_COUNT 10
#define MAX_RECENT_TEMPLATE_COUNT 10
class SessionDialog : public WavesDialog {
  public:
    SessionDialog (WM::Proxy<TracksControlPanel>& system_configuration_dialog,
				   bool require_new,
		           const std::string& session_name,
				   const std::string& session_path, 
				   const std::string& template_name,
				   bool cancel_not_quit);

	~SessionDialog ();

  private:
	WavesButton& _quit_button;
	WavesButton& _new_session_button;
	WavesButton& _new_session_with_template_button;
	WavesButton& _open_selected_button;
	WavesButton& _open_saved_session_button;
	WavesButton& _system_configuration_button;
	WavesButton* _recent_session_button[MAX_RECENT_SESSION_COUNT];
	WavesButton* _recent_template_button[MAX_RECENT_TEMPLATE_COUNT];
	Gtk::Label& _session_details_label_1;
   	Gtk::Label& _session_details_label_2;
   	Gtk::Label& _session_details_label_3;
 	Gtk::Label& _session_details_label_4;
	WM::Proxy<TracksControlPanel>& _system_configuration_dialog;
  
#include "session_dialog.logic.h"
};

#endif /* __gtk2_ardour_session_dialog_h__ */
