/*
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __ardour_gtk_color_manager_h__
#define __ardour_gtk_color_manager_h__

#include <gtkmm/treeview.h>
#include <gtkmm/treestore.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/colorselection.h>
#include <gtkmm/button.h>

#include "canvas/types.h"
#include "canvas/canvas.h"

#include "option_editor.h"
#include "ui_config.h"

namespace ArdourCanvas {
	class Container;
	class ScrollGroup;
}

class ArdourDialog;

class ColorThemeManager : public OptionEditorMiniPage
{
public:
	ColorThemeManager();
	~ColorThemeManager();

	void reset_canvas_colors();
	void on_color_theme_changed ();

	/** Called when a configuration parameter's value has changed.
	 *  @param p parameter name
	 */
	void parameter_changed (std::string const & p);

	/** Called to instruct the object to set its UI state from the configuration */
	void set_state_from_config ();

	void set_note (std::string const &);

	void add_to_page (OptionEditorPage*);

	Gtk::Widget& tip_widget();

private:
	Gtk::Button reset_button;
	Gtk::Notebook notebook;

	/* handls response from color dialog when it is used to
	 * edit a derived color.
	 */
	void palette_color_response (int, std::string);

	Gtk::ScrolledWindow palette_scroller;
	ArdourCanvas::GtkCanvasViewport palette_viewport;
	ArdourCanvas::Container* palette_group;

	/* these methods create and manage a canvas for use in either the
	 * palette tab or in a separate dialog. Different behaviour is
	 * accomplished by changing the event handler passed into the
	 * allocation handler. We do it there because we have to rebuild
	 * the canvas on allocation events, and during the rebuild, connect
	 * each rectangle to the event handler.
	 *
	 * the alternative is one event handler for the canvas and a map
	 * of where each color rectangle is. nothing wrong with this
	 * but the per-rect event setup is simpler and avoids building
	 * and looking up the map information.
	 */
	ArdourCanvas::Container* initialize_palette_canvas (ArdourCanvas::Canvas& canvas);
	void build_palette_canvas (ArdourCanvas::Canvas&, ArdourCanvas::Container&, sigc::slot<bool,GdkEvent*,std::string> event_handler);
	void palette_canvas_allocated (Gtk::Allocation& alloc, ArdourCanvas::Container* group, ArdourCanvas::Canvas* canvas, sigc::slot<bool,GdkEvent*,std::string> event_handler);
	void palette_size_request (Gtk::Requisition*);

	/* handles events from a palette canvas inside the palette (derived
	 * colors) tab
	 */
	bool palette_event (GdkEvent*, std::string name);
	/* allows user to edit a named color (e.g. "color 3") after clicking
	 * on it inside the palette tab.
	 */
	void edit_palette_color (std::string);

	struct ColorAliasModelColumns : public Gtk::TreeModel::ColumnRecord {
		ColorAliasModelColumns() {
			add (name);
			add (alias);
			add (color);
			add (key);
		}

		Gtk::TreeModelColumn<std::string>  name;
		Gtk::TreeModelColumn<std::string>  alias;
		Gtk::TreeModelColumn<Gdk::Color>   color;
		Gtk::TreeModelColumn<std::string>  key;
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

	Gtk::ScrolledWindow modifier_scroller;
	Gtk::VBox modifier_vbox;

	void setup_modifiers ();
	void modifier_edited (Gtk::Range*, std::string);

	Gtk::ColorSelectionDialog color_dialog;
	sigc::connection color_dialog_connection;

	void colors_changed ();
	void set_ui_to_state ();


	struct ColorThemeModelColumns : public Gtk::TreeModel::ColumnRecord {
		ColorThemeModelColumns() {
			add (name);
			add (path);
		}

		Gtk::TreeModelColumn<std::string>  name;
		Gtk::TreeModelColumn<std::string>  path;
	};

	ColorThemeModelColumns color_theme_columns;
	Glib::RefPtr<Gtk::TreeStore> theme_list;

	Gtk::Label color_theme_label;
	Gtk::ComboBox color_theme_dropdown;

};

#endif /* __ardour_gtk_color_manager_h__ */
