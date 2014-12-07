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

#include "canvas/types.h"
#include "canvas/canvas.h"

#include "ardour_window.h"

#include "ui_config.h"

namespace ArdourCanvas {
	class Container;
	class ScrollGroup;
}

class ArdourDialog;

class ThemeManager : public ArdourWindow
{
  public:
	ThemeManager();
	~ThemeManager();

	int save (std::string path);
	void setup_basic_color_display ();
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
	
	struct BasicColorDisplayModelColumns : public Gtk::TreeModel::ColumnRecord {
		BasicColorDisplayModelColumns() {
			add (name);
			add (gdkcolor);
			add (pVar);
			add (rgba);
		}
		
		Gtk::TreeModelColumn<std::string>  name;
		Gtk::TreeModelColumn<Gdk::Color>   gdkcolor;
		Gtk::TreeModelColumn<ColorVariable<ArdourCanvas::Color> *> pVar;
		Gtk::TreeModelColumn<ArdourCanvas::Color>     rgba;
	};
	
	BasicColorDisplayModelColumns basic_color_columns;
	Gtk::TreeView basic_color_display;
	Glib::RefPtr<Gtk::TreeStore> basic_color_list;

	bool basic_color_button_press_event (GdkEventButton*);

	Gtk::ColorSelectionDialog color_dialog;
	sigc::connection color_dialog_connection;
	
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

	/* handles response from color dialog when it used to 
	   edit a basic color
	*/
	void basic_color_response (int, ColorVariable<ArdourCanvas::Color>*);

	/* handls response from color dialog when it is used to
	   edit a derived color.
	*/
	void palette_color_response (int, std::string);

	Gtk::ScrolledWindow palette_scroller;
	ArdourCanvas::GtkCanvasViewport palette_viewport;
	ArdourCanvas::Container* palette_group;
	
	/* these methods create and manage a canvas for use in either the
	   palette tab or in a separate dialog. Different behaviour is
	   accomplished by changing the event handler passed into the 
	   allocation handler. We do it there because we have to rebuild
	   the canvas on allocation events, and during the rebuild, connect
	   each rectangle to the event handler.

	   the alternative is one event handler for the canvas and a map
	   of where each color rectangle is. nothing wrong with this
	   but the per-rect event setup is simpler and avoids building
	   and looking up the map information.
	*/
	ArdourCanvas::Container* initialize_palette_canvas (ArdourCanvas::Canvas& canvas);
	void build_palette_canvas (ArdourCanvas::Canvas&, ArdourCanvas::Container&, sigc::slot<bool,GdkEvent*,std::string> event_handler);
	void palette_canvas_allocated (Gtk::Allocation& alloc, ArdourCanvas::Container* group, ArdourCanvas::Canvas* canvas, sigc::slot<bool,GdkEvent*,std::string> event_handler);
	void palette_size_request (Gtk::Requisition*);

	/* handles events from a palette canvas inside the palette (derived
	   colors) tab
	*/
	bool palette_event (GdkEvent*, std::string name);
	/* allows user to edit a named color (e.g. "color 3") after clicking
	   on it inside the palette tab.
	*/
	void edit_palette_color (std::string);
	
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

	ArdourDialog* palette_window;
	sigc::connection palette_response_connection;
	
	void choose_color_from_palette (std::string const &target_name);
	
	bool alias_palette_event (GdkEvent*, std::string, std::string);
	void alias_palette_response (int, std::string, std::string);

	void setup_aliases ();
	void setup_palette ();
};

#endif /* __ardour_gtk_color_manager_h__ */

