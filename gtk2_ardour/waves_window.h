/*	Copyright (C) 2014 Waves Audio Ltd.
    This program is free software; you can redistribute it and/or modify    it under the terms of the GNU General Public License as published by    the Free Software Foundation; either version 2 of the License, or    (at your option) any later version.
    This program is distributed in the hope that it will be useful,    but WITHOUT ANY WARRANTY; without even the implied warranty of    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License    along with this program; if not, write to the Free Software    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.*/
#ifndef __waves_window_h__#define __waves_window_h__#include <string>
#include <gtkmm.h>
#include "waves_ui.h"

class WavesWindow : public Gtk::Window {  public:	WavesWindow (Gtk::WindowType window_type);	WavesWindow (Gtk::WindowType window_type, std::string layout_script);	Gtk::VBox& get_vbox (const char* id) { return _children.get_vbox (id); }
	Gtk::HBox& get_hbox (const char* id) { return _children.get_hbox (id); }
	Gtk::Layout& get_layout (const char* id) { return _children.get_layout (id); }
	Gtk::Label& get_label (const char* id) { return _children.get_label (id); }
	Gtk::ComboBoxText& get_combo_box_text (const char* id) { return _children.get_combo_box_text (id); }
	WavesButton& get_waves_button (const char* id) { return _children.get_waves_button (id); }
	Gtk::Adjustment& get_adjustment (const char* id) { return _children.get_adjustment (id); }
  private:	WavesUI::WidgetMap _children;};
#endif // __waves_window_h__
