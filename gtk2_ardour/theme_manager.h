/*
    Copyright (C) 2000-2007 Paul Davis

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

#ifndef __ardour_gtk_color_manager_h__
#define __ardour_gtk_color_manager_h__

#include <gtkmm/treeview.h>
#include <gtkmm/treestore.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/colorselection.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/button.h>
#include <gtkmm/scale.h>
#include <gtkmm/rc.h>

#include "canvas/canvas.h"

#include "ardour_window.h"

#include "ui_config.h"

namespace ArdourCanvas {
	class Container;
	class ScrollGroup;
}

class ThemeManager : public ArdourWindow
{
  public:
	ThemeManager();
	~ThemeManager();

	int save (std::string path);
	void setup_theme ();
	void reset_canvas_colors();

	void on_dark_theme_button_toggled ();
	void on_light_theme_button_toggled ();
	void on_flat_buttons_toggled ();
	void on_blink_rec_arm_toggled ();
        void on_region_color_toggled ();
        void on_show_clip_toggled ();
        void on_waveform_gradient_depth_change ();
        void on_timeline_item_gradient_depth_change ();
	void on_all_dialogs_toggled ();
	void on_icon_set_changed ();

  private:
	Gtk::Notebook notebook;

	struct ColorDisplayModelColumns : public Gtk::TreeModel::ColumnRecord {
	    ColorDisplayModelColumns() {
		    add (name);
		    add (gdkcolor);
		    add (pVar);
		    add (rgba);
	    }

	    Gtk::TreeModelColumn<std::string>  name;
	    Gtk::TreeModelColumn<Gdk::Color>   gdkcolor;
	    Gtk::TreeModelColumn<ColorVariable<uint32_t> *> pVar;
	    Gtk::TreeModelColumn<uint32_t>     rgba;
	};

	ColorDisplayModelColumns columns;
	Gtk::TreeView color_display;
	Glib::RefPtr<Gtk::TreeStore> color_list;
	Gtk::ColorSelectionDialog color_dialog;
	Gtk::ScrolledWindow scroller;
	Gtk::HBox theme_selection_hbox;
	Gtk::RadioButton dark_button;
	Gtk::RadioButton light_button;
	Gtk::Button reset_button;
	Gtk::CheckButton flat_buttons;
	Gtk::CheckButton blink_rec_button;
	Gtk::CheckButton region_color_button;
	Gtk::CheckButton show_clipping_button;
        Gtk::HScale waveform_gradient_depth;
        Gtk::Label waveform_gradient_depth_label;
        Gtk::HScale timeline_item_gradient_depth;
        Gtk::Label timeline_item_gradient_depth_label;
	Gtk::CheckButton all_dialogs;
	Gtk::CheckButton gradient_waveforms;
	Gtk::Label icon_set_label;
	Gtk::ComboBoxText icon_set_dropdown;

	ColorDisplayModelColumns base_color_columns;
	Gtk::ScrolledWindow base_color_scroller;
	ArdourCanvas::GtkCanvasViewport base_color_viewport;
	ArdourCanvas::Container* base_color_group;
	std::string base_color_edit_name;

	sigc::connection color_dialog_connection;
	void foobar_response (int);
	
	ArdourCanvas::Container* initialize_canvas (ArdourCanvas::Canvas& canvas);
	void build_base_color_canvas (ArdourCanvas::Container&, bool (ThemeManager::*event_handler)(GdkEvent*,std::string), double width, double height);
	void base_color_viewport_allocated (Gtk::Allocation&);
	void base_color_dialog_done (int);
	bool base_color_event (GdkEvent*, std::string);
	void edit_named_color (std::string);
	
	bool button_press_event (GdkEventButton*);


	struct ColorAliasModelColumns : public Gtk::TreeModel::ColumnRecord {
		ColorAliasModelColumns() {
			add (name);
			add (alias);
			add (color);
		}
		
		Gtk::TreeModelColumn<std::string>  name;
		Gtk::TreeModelColumn<std::string>  alias;
		Gtk::TreeModelColumn<Gdk::Color>   color;
	};

	ColorAliasModelColumns       alias_columns;
	Gtk::TreeView                alias_display;
	Glib::RefPtr<Gtk::TreeStore> alias_list;
	Gtk::ScrolledWindow          alias_scroller;

	bool alias_button_press_event (GdkEventButton*);

	Gtk::Window* palette_window;
	std::string palette_edit_name;
	
	void choose_color_from_palette (std::string const &target_name);
	bool palette_chosen (GdkEvent*, std::string);
	void palette_canvas_allocated (Gtk::Allocation& alloc, ArdourCanvas::Container* group, bool (ThemeManager::*event_handler)(GdkEvent*,std::string));
	bool palette_done (GdkEventAny*);

	void setup_aliases ();
};

#endif /* __ardour_gtk_color_manager_h__ */

