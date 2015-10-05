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

#include <cmath>
#include <iostream>
#include <fstream>
#include <errno.h>

#include "fix_carbon.h"

#include <pbd/gstdio_compat.h>

#include <gtkmm/stock.h>
#include <gtkmm/settings.h>

#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/cell_renderer_color_selector.h"
#include "gtkmm2ext/utils.h"

#include "pbd/file_utils.h"
#include "pbd/compose.h"

#include "ardour/filesystem_paths.h"

#include "canvas/container.h"
#include "canvas/rectangle.h"
#include "canvas/scroll_group.h"
#include "canvas/wave_view.h"

#include "ardour_button.h"
#include "ardour_dialog.h"
#include "theme_manager.h"
#include "rgb_macros.h"
#include "ui_config.h"
#include "utils.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace PBD;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;

ThemeManager::ThemeManager()
        : dark_button (_("Dark Theme"))
	, light_button (_("Light Theme"))
	, reset_button (_("Restore Defaults"))
	, flat_buttons (_("Draw \"flat\" buttons"))
	, blink_rec_button (_("Blink Rec-Arm buttons"))
	, region_color_button (_("Color regions using their track's color"))
	, show_clipping_button (_("Show waveform clipping"))
	, waveform_gradient_depth (0, 1.0, 0.05)
	, waveform_gradient_depth_label (_("Waveforms color gradient depth"))
	, timeline_item_gradient_depth (0, 1.0, 0.05)
	, timeline_item_gradient_depth_label (_("Timeline item gradient depth"))
	, all_dialogs (_("All floating windows are dialogs"))
	, transients_follow_front (_("Transient windows follow front window."))
	, icon_set_label (_("Icon Set"))
	, palette_viewport (*palette_scroller.get_hadjustment(), *palette_scroller.get_vadjustment())
	, palette_group (0)
	, palette_window (0)
{
	/* Now the alias list */

	alias_list = TreeStore::create (alias_columns);
	alias_display.set_model (alias_list);
	alias_display.append_column (_("Object"), alias_columns.name);

	Gtkmm2ext::CellRendererColorSelector* color_renderer = manage (new Gtkmm2ext::CellRendererColorSelector);
	TreeViewColumn* color_column = manage (new TreeViewColumn (_("Color"), *color_renderer));
	color_column->add_attribute (color_renderer->property_color(), alias_columns.color);
	alias_display.append_column (*color_column);

	alias_display.get_column (0)->set_data (X_("colnum"), GUINT_TO_POINTER(0));
	alias_display.get_column (0)->set_expand (true);
	alias_display.get_column (1)->set_data (X_("colnum"), GUINT_TO_POINTER(1));
	alias_display.get_column (1)->set_expand (false);
	alias_display.set_reorderable (false);
	alias_display.get_selection()->set_mode (SELECTION_NONE);
	alias_display.set_headers_visible (true);

	alias_display.signal_button_press_event().connect (sigc::mem_fun (*this, &ThemeManager::alias_button_press_event), false);

	alias_scroller.add (alias_display);

	/* various buttons */

	RadioButton::Group group = dark_button.get_group();
	light_button.set_group(group);
	theme_selection_hbox.set_homogeneous(false);
	theme_selection_hbox.pack_start (dark_button);
	theme_selection_hbox.pack_start (light_button);

	set_homogeneous (false);
#if 0 // disable light/dark theme choice. until the 'light theme gets some attention.
	pack_start (theme_selection_hbox, PACK_SHRINK);
#endif
	pack_start (reset_button, PACK_SHRINK);
#ifndef __APPLE__
	pack_start (all_dialogs, PACK_SHRINK);
	pack_start (transients_follow_front, PACK_SHRINK);
#endif
	pack_start (flat_buttons, PACK_SHRINK);
	pack_start (blink_rec_button, PACK_SHRINK);
	pack_start (region_color_button, PACK_SHRINK);
	pack_start (show_clipping_button, PACK_SHRINK);

	Gtk::HBox* hbox;

	vector<string> icon_sets = ::get_icon_sets ();

	if (icon_sets.size() > 1) {
		Gtkmm2ext::set_popdown_strings (icon_set_dropdown, icon_sets);
		icon_set_dropdown.set_active_text (UIConfiguration::instance().get_icon_set());

		hbox = Gtk::manage (new Gtk::HBox());
		hbox->set_spacing (6);
		hbox->pack_start (icon_set_label, false, false);
		hbox->pack_start (icon_set_dropdown, true, true);
		pack_start (*hbox, PACK_SHRINK);
	}


	hbox = Gtk::manage (new Gtk::HBox());
	hbox->set_spacing (6);
	hbox->pack_start (waveform_gradient_depth, true, true);
	hbox->pack_start (waveform_gradient_depth_label, false, false);
	pack_start (*hbox, PACK_SHRINK);

	hbox = Gtk::manage (new Gtk::HBox());
	hbox->set_spacing (6);
	hbox->pack_start (timeline_item_gradient_depth, true, true);
	hbox->pack_start (timeline_item_gradient_depth_label, false, false);
	pack_start (*hbox, PACK_SHRINK);

	palette_group = initialize_palette_canvas (*palette_viewport.canvas());
	palette_viewport.signal_size_allocate().connect (sigc::bind (sigc::mem_fun (*this, &ThemeManager::palette_canvas_allocated), palette_group, palette_viewport.canvas(),
								     sigc::mem_fun (*this, &ThemeManager::palette_event)));
	palette_scroller.add (palette_viewport);

	modifier_scroller.add (modifier_vbox);

	notebook.append_page (alias_scroller, _("Items"));
	notebook.append_page (palette_scroller, _("Palette"));
	notebook.append_page (modifier_scroller, _("Transparency"));

	pack_start (notebook);

	show_all ();

	waveform_gradient_depth.set_update_policy (Gtk::UPDATE_DELAYED);
	timeline_item_gradient_depth.set_update_policy (Gtk::UPDATE_DELAYED);

	color_dialog.get_colorsel()->set_has_opacity_control (true);
	color_dialog.get_colorsel()->set_has_palette (true);

	set_ui_to_state();

	color_dialog.get_ok_button()->signal_clicked().connect (sigc::bind (sigc::mem_fun (color_dialog, &Gtk::Dialog::response), RESPONSE_ACCEPT));
	color_dialog.get_cancel_button()->signal_clicked().connect (sigc::bind (sigc::mem_fun (color_dialog, &Gtk::Dialog::response), RESPONSE_CANCEL));
	dark_button.signal_toggled().connect (sigc::mem_fun (*this, &ThemeManager::on_dark_theme_button_toggled));
	light_button.signal_toggled().connect (sigc::mem_fun (*this, &ThemeManager::on_light_theme_button_toggled));
	reset_button.signal_clicked().connect (sigc::mem_fun (*this, &ThemeManager::reset_canvas_colors));
	flat_buttons.signal_toggled().connect (sigc::mem_fun (*this, &ThemeManager::on_flat_buttons_toggled));
	blink_rec_button.signal_toggled().connect (sigc::mem_fun (*this, &ThemeManager::on_blink_rec_arm_toggled));
	region_color_button.signal_toggled().connect (sigc::mem_fun (*this, &ThemeManager::on_region_color_toggled));
	show_clipping_button.signal_toggled().connect (sigc::mem_fun (*this, &ThemeManager::on_show_clip_toggled));
	waveform_gradient_depth.signal_value_changed().connect (sigc::mem_fun (*this, &ThemeManager::on_waveform_gradient_depth_change));
	timeline_item_gradient_depth.signal_value_changed().connect (sigc::mem_fun (*this, &ThemeManager::on_timeline_item_gradient_depth_change));
	all_dialogs.signal_toggled().connect (sigc::mem_fun (*this, &ThemeManager::on_all_dialogs_toggled));
	transients_follow_front.signal_toggled().connect (sigc::mem_fun (*this, &ThemeManager::on_transients_follow_front_toggled));
	icon_set_dropdown.signal_changed().connect (sigc::mem_fun (*this, &ThemeManager::on_icon_set_changed));

	Gtkmm2ext::UI::instance()->set_tip (all_dialogs,
					    string_compose (_("Mark all floating windows to be type \"Dialog\" rather than using \"Utility\" for some.\n"
							      "This may help with some window managers. This requires a restart of %1 to take effect"),
							    PROGRAM_NAME));
	Gtkmm2ext::UI::instance()->set_tip (transients_follow_front,
					    string_compose (_("Make transient windows follow the front window when toggling between the editor and mixer.\n"
							      "This requires a restart of %1 to take effect"), PROGRAM_NAME));

	set_size_request (-1, 400);
	/* no need to call setup_palette() here, it will be done when its size is allocated */
	setup_aliases ();
	setup_modifiers ();

	UIConfiguration::instance().ColorsChanged.connect (sigc::mem_fun (*this, &ThemeManager::colors_changed));
}

ThemeManager::~ThemeManager()
{
}

void
ThemeManager::setup_modifiers ()
{
	UIConfiguration* uic (&UIConfiguration::instance());
	UIConfiguration::Modifiers& modifiers (uic->modifiers);
	Gtk::HBox* mod_hbox;
	Gtk::Label* mod_label;
	Gtk::HScale* mod_scale;

	Gtkmm2ext::container_clear (modifier_vbox);

	for (UIConfiguration::Modifiers::const_iterator m = modifiers.begin(); m != modifiers.end(); ++m) {
		mod_hbox = manage (new HBox);

		mod_scale = manage (new HScale (0.0, 1.0, 0.01));
		mod_scale->set_draw_value (false);
		mod_scale->set_value (m->second.a());
		mod_scale->set_update_policy (Gtk::UPDATE_DISCONTINUOUS);
		mod_scale->signal_value_changed().connect (sigc::bind (sigc::mem_fun (*this, &ThemeManager::modifier_edited), mod_scale, m->first));

		mod_label = manage (new Label (m->first));
		mod_label->set_alignment (1.0, 0.5);
		mod_label->set_size_request (150, -1); /* 150 pixels should be enough for anyone */

		mod_hbox->pack_start (*mod_label, false, true, 12);
		mod_hbox->pack_start (*mod_scale, true, true);

		modifier_vbox.pack_start (*mod_hbox, false, false);
	}

	modifier_vbox.show_all ();

}

void
ThemeManager::modifier_edited (Gtk::Range* range, string name)
{
	using namespace ArdourCanvas;

	double alpha = range->get_value();
	SVAModifier svam (SVAModifier::Assign, -1.0, -1.0, alpha);
	UIConfiguration::instance().set_modifier (name, svam);
}

void
ThemeManager::colors_changed ()
{
	setup_palette ();
	setup_aliases ();
	setup_modifiers ();
}

int
ThemeManager::save (string /*path*/)
{
	return 0;
}

void
ThemeManager::on_flat_buttons_toggled ()
{
	UIConfiguration::instance().set_flat_buttons (flat_buttons.get_active());
	ArdourButton::set_flat_buttons (flat_buttons.get_active());
	/* force a redraw */
	gtk_rc_reset_styles (gtk_settings_get_default());
}

void
ThemeManager::on_blink_rec_arm_toggled ()
{
	UIConfiguration::instance().set_blink_rec_arm (blink_rec_button.get_active());
	UIConfiguration::instance().ParameterChanged("blink-rec-arm");
}

void
ThemeManager::on_region_color_toggled ()
{
	UIConfiguration::instance().set_color_regions_using_track_color (region_color_button.get_active());
}

void
ThemeManager::on_show_clip_toggled ()
{
	UIConfiguration::instance().set_show_waveform_clipping (show_clipping_button.get_active());
	// "show-waveform-clipping" was a session config key
	ArdourCanvas::WaveView::set_global_show_waveform_clipping (UIConfiguration::instance().get_show_waveform_clipping());
}

void
ThemeManager::on_all_dialogs_toggled ()
{
	UIConfiguration::instance().set_all_floating_windows_are_dialogs (all_dialogs.get_active());
}

void
ThemeManager::on_transients_follow_front_toggled ()
{
	UIConfiguration::instance().set_transients_follow_front (transients_follow_front.get_active());
}

void
ThemeManager::on_waveform_gradient_depth_change ()
{
	double v = waveform_gradient_depth.get_value();

	UIConfiguration::instance().set_waveform_gradient_depth (v);
	ArdourCanvas::WaveView::set_global_gradient_depth (v);
}

void
ThemeManager::on_timeline_item_gradient_depth_change ()
{
	double v = timeline_item_gradient_depth.get_value();

	UIConfiguration::instance().set_timeline_item_gradient_depth (v);
}

void
ThemeManager::on_icon_set_changed ()
{
	string new_set = icon_set_dropdown.get_active_text();
	UIConfiguration::instance().set_icon_set (new_set);
}

void
ThemeManager::on_dark_theme_button_toggled()
{
	if (!dark_button.get_active()) return;

	UIConfiguration* uic (&UIConfiguration::instance());

        uic->set_color_file("dark");
}

void
ThemeManager::on_light_theme_button_toggled()
{
	if (!light_button.get_active()) return;

	UIConfiguration* uic (&UIConfiguration::instance());

        uic->set_color_file("light");
}

void
ThemeManager::set_ui_to_state()
{
	/* there is no way these values can change individually
	 * by themselves (w/o user-interaction)
	 * hence a common combined update function suffices
	 */

	if (UIConfiguration::instance().get_color_file() == "light") {
		light_button.set_active(true);
	} else {
		dark_button.set_active(true);
	}

	/* there is no need to block signal handlers, here,
	 * all elements check if the value has changed and ignore NOOPs
	 */
	all_dialogs.set_active (UIConfiguration::instance().get_all_floating_windows_are_dialogs());
	transients_follow_front.set_active (UIConfiguration::instance().get_transients_follow_front());
	flat_buttons.set_active (UIConfiguration::instance().get_flat_buttons());
	blink_rec_button.set_active (UIConfiguration::instance().get_blink_rec_arm());
	region_color_button.set_active (UIConfiguration::instance().get_color_regions_using_track_color());
	show_clipping_button.set_active (UIConfiguration::instance().get_show_waveform_clipping());
	waveform_gradient_depth.set_value(UIConfiguration::instance().get_waveform_gradient_depth());
	timeline_item_gradient_depth.set_value(UIConfiguration::instance().get_timeline_item_gradient_depth());
}

void
ThemeManager::reset_canvas_colors()
{
	string cfile;
	string basename;

	basename = "my-";
	basename += UIConfiguration::instance().get_color_file();
	basename += ".colors";

	if (find_file (ardour_config_search_path(), basename, cfile)) {
		string backup = cfile + string (X_(".old"));
		g_rename (cfile.c_str(), backup.c_str());
		/* don't really care if it fails */
	}

	UIConfiguration::instance().load_defaults();
	UIConfiguration::instance().save_state ();
	set_ui_to_state();
}

ArdourCanvas::Container*
ThemeManager::initialize_palette_canvas (ArdourCanvas::Canvas& canvas)
{
	using namespace ArdourCanvas;

	/* hide background */
	canvas.set_background_color (rgba_to_color (0.0, 0.0, 1.0, 0.0));

	/* bi-directional scroll group */

	ScrollGroup* scroll_group = new ScrollGroup (canvas.root(), ScrollGroup::ScrollSensitivity (ScrollGroup::ScrollsVertically|ScrollGroup::ScrollsHorizontally));
	canvas.add_scroller (*scroll_group);

	/* new container to hold everything */

	return new ArdourCanvas::Container (scroll_group);
}

void
ThemeManager::palette_canvas_allocated (Gtk::Allocation& alloc, ArdourCanvas::Container* group, ArdourCanvas::Canvas* canvas, sigc::slot<bool,GdkEvent*,std::string> event_handler)
{
	build_palette_canvas (*canvas, *group, event_handler);
}

struct NamedColor {
	string name;
	ArdourCanvas::HSV    color;
	NamedColor (string s, ArdourCanvas::HSV c) : name (s), color (c) {}
};

struct SortByHue {
	bool operator() (NamedColor const & a, NamedColor const & b) {
		using namespace ArdourCanvas;
		const HSV black (0, 0, 0);
		if (a.color.is_gray() || b.color.is_gray()) {
			return black.distance (a.color) < black.distance (b.color);
		} else {
			return a.color.h < b.color.h;
			// const HSV red (rgba_to_color (1.0, 0.0, 0.0, 1.0));
			// return red.distance (a.color) < red.distance (b.color);
		}
	}
};


void
ThemeManager::build_palette_canvas (ArdourCanvas::Canvas& canvas, ArdourCanvas::Container& group, sigc::slot<bool,GdkEvent*,std::string> event_handler)
{
	using namespace ArdourCanvas;

	/* we want the colors sorted by hue, with their name */

	UIConfiguration::Colors& colors (UIConfiguration::instance().colors);
	vector<NamedColor> nc;
	for (UIConfiguration::Colors::const_iterator x = colors.begin(); x != colors.end(); ++x) {
		nc.push_back (NamedColor (x->first, HSV (x->second)));
	}
	SortByHue sorter;
	sort (nc.begin(), nc.end(), sorter);

	const uint32_t color_limit = nc.size();
	const double box_size = 20.0;
	const double width = canvas.width();
	const double height = canvas.height();

	uint32_t color_num = 0;

	/* clear existing rects and delete them */

	group.clear (true);

	for (uint32_t y = 0; y < height - box_size && color_num < color_limit; y += box_size) {
		for (uint32_t x = 0; x < width - box_size && color_num < color_limit; x += box_size) {
			ArdourCanvas::Rectangle* r = new ArdourCanvas::Rectangle (&group, ArdourCanvas::Rect (x, y, x + box_size, y + box_size));

			string name = nc[color_num++].name;

			UIConfiguration::Colors::iterator c = colors.find (name);

			if (c != colors.end()) {
				Color color = c->second;
				r->set_fill_color (color);
				r->set_outline_color (rgba_to_color (0.0, 0.0, 0.0, 1.0));
				r->set_tooltip (name);
				r->Event.connect (sigc::bind (event_handler, name));
			}
		}
	}
}

void
ThemeManager::palette_size_request (Gtk::Requisition* req)
{
	uint32_t ncolors = UIConfiguration::instance().colors.size();
	const int box_size = 20;

	double c = sqrt ((double)ncolors);
	req->width = (int) floor (c * box_size);
	req->height = (int) floor (c * box_size);

	/* add overflow row if necessary */

	if (fmod (ncolors, c) != 0.0) {
		req->height += box_size;
	}
}

void
ThemeManager::setup_palette ()
{
	build_palette_canvas (*palette_viewport.canvas(), *palette_group, sigc::mem_fun (*this, &ThemeManager::palette_event));
}

bool
ThemeManager::palette_event (GdkEvent* ev, string name)
{
	switch (ev->type) {
	case GDK_BUTTON_RELEASE:
		edit_palette_color (name);
		return true;
	default:
		break;
	}
	return true;
}

void
ThemeManager::edit_palette_color (std::string name)
{
	using namespace ArdourCanvas;
	double r,g, b, a;
	UIConfiguration* uic (&UIConfiguration::instance());
	ArdourCanvas::Color c = uic->color (name);
	Gdk::Color gdkcolor;

	color_to_rgba (c, r, g, b, a);

	gdkcolor.set_rgb_p (r, g, b);
	color_dialog.get_colorsel()->set_previous_color (gdkcolor);
	color_dialog.get_colorsel()->set_current_color (gdkcolor);
	color_dialog.get_colorsel()->set_previous_alpha ((guint16) (a * 65535));
	color_dialog.get_colorsel()->set_current_alpha ((guint16) (a * 65535));

	color_dialog_connection.disconnect ();
	color_dialog_connection = color_dialog.signal_response().connect (sigc::bind (sigc::mem_fun (*this, &ThemeManager::palette_color_response), name));
	color_dialog.present();
}

void
ThemeManager::palette_color_response (int result, std::string name)
{
	using namespace ArdourCanvas;

	color_dialog_connection.disconnect ();

	UIConfiguration* uic (&UIConfiguration::instance());
	Gdk::Color gdkcolor;
	double r,g, b, a;

	switch (result) {
	case RESPONSE_ACCEPT:
	case RESPONSE_OK:
		gdkcolor = color_dialog.get_colorsel()->get_current_color();
		a = color_dialog.get_colorsel()->get_current_alpha() / 65535.0;
		r = gdkcolor.get_red_p();
		g = gdkcolor.get_green_p();
		b = gdkcolor.get_blue_p();

		uic->set_color (name, rgba_to_color (r, g, b, a));
		break;

	default:
		break;
	}

	color_dialog.hide ();
}

bool
ThemeManager::alias_palette_event (GdkEvent* ev, string new_alias, string target_name)
{
	switch (ev->type) {
	case GDK_BUTTON_RELEASE:
		UIConfiguration::instance().set_alias (target_name, new_alias);
		return true;
		break;
	default:
		break;
	}
	return false;
}

void
ThemeManager::alias_palette_response (int response, std::string target_name, std::string old_alias)
{
	switch (response) {
	case GTK_RESPONSE_OK:
	case GTK_RESPONSE_ACCEPT:
		/* rebuild alias list with new color: inefficient but simple */
		setup_aliases ();
		break;

	case GTK_RESPONSE_REJECT:
		/* revert choice */
		UIConfiguration::instance().set_alias (target_name, old_alias);
		break;

	default:
		/* do nothing */
		break;
	}

	palette_window->hide ();
}

void
ThemeManager::choose_color_from_palette (string const & name)
{
	UIConfiguration* uic (&UIConfiguration::instance());
	UIConfiguration::ColorAliases::iterator i = uic->color_aliases.find (name);

	if (i == uic->color_aliases.end()) {
		return;
	}

	delete palette_window;

	palette_window = new ArdourDialog (_("Color Palette"));
	palette_window->add_button (Stock::CANCEL, RESPONSE_REJECT); /* using CANCEL causes confusion if dialog is closed via CloseAllDialogs */
	palette_window->add_button (Stock::OK, RESPONSE_OK);

	ArdourCanvas::GtkCanvas* canvas = new ArdourCanvas::GtkCanvas ();
	ArdourCanvas::Container* group = initialize_palette_canvas (*canvas);

	canvas->signal_size_request().connect (sigc::mem_fun (*this, &ThemeManager::palette_size_request));
	canvas->signal_size_allocate().connect (sigc::bind (sigc::mem_fun (*this, &ThemeManager::palette_canvas_allocated), group, canvas,
							    sigc::bind (sigc::mem_fun (*this, &ThemeManager::alias_palette_event), name)));

	palette_window->get_vbox()->pack_start (*canvas);
	palette_window->show_all ();

	palette_response_connection = palette_window->signal_response().connect (sigc::bind (sigc::mem_fun (*this, &ThemeManager::alias_palette_response), name, i->second));

	palette_window->set_position (WIN_POS_MOUSE);
	palette_window->present ();
}

void
ThemeManager::setup_aliases ()
{
	using namespace ArdourCanvas;

	UIConfiguration* uic (&UIConfiguration::instance());
	UIConfiguration::ColorAliases& aliases (uic->color_aliases);

	alias_list->clear ();

	for (UIConfiguration::ColorAliases::iterator i = aliases.begin(); i != aliases.end(); ++i) {
		TreeModel::Children rows = alias_list->children();
		TreeModel::Row row;
		string::size_type colon;

		if ((colon = i->first.find (':')) != string::npos) {

			/* this is supposed to be a child node, so find the
			 * parent
			 */

			string parent = i->first.substr (0, colon);
			TreeModel::iterator ri;

			for (ri = rows.begin(); ri != rows.end(); ++ri) {
				string s = (*ri)[alias_columns.name];
				if (s == parent) {
					break;
				}
			}

			if (ri == rows.end()) {
				/* not found, add the parent as new top level row */
				row = *(alias_list->append());
				row[alias_columns.name] = parent;
				row[alias_columns.alias] = "";

				/* now add the child as a child of this one */

				row = *(alias_list->insert (row->children().end()));
				row[alias_columns.name] = i->first.substr (colon+1);
			} else {
				row = *(alias_list->insert ((*ri)->children().end()));
				row[alias_columns.name] = i->first.substr (colon+1);
			}

		} else {
			/* add as a child */
			row = *(alias_list->append());
			row[alias_columns.name] = i->first;
			row[alias_columns.key] = i->first;
		}

		row[alias_columns.key] = i->first;
		row[alias_columns.alias] = i->second;

		Gdk::Color col;
		double r, g, b, a;
		Color c (uic->color (i->second));
		color_to_rgba (c, r, g, b, a);
		col.set_rgb_p (r, g, b);

		row[alias_columns.color] = col;
	}
}

bool
ThemeManager::alias_button_press_event (GdkEventButton* ev)
{
	TreeIter iter;
	TreeModel::Path path;
	TreeViewColumn* column;
	int cellx;
	int celly;

	if (!alias_display.get_path_at_pos ((int)ev->x, (int)ev->y, path, column, cellx, celly)) {
		return false;
	}

	guint32 colnum = GPOINTER_TO_UINT (column->get_data (X_("colnum")));

	switch (colnum) {
	case 0:
		/* allow normal processing to occur */
		return false;

	case 1: /* color */
		if ((iter = alias_list->get_iter (path))) {
			string target_color_alias = (*iter)[alias_columns.key];
			if (!target_color_alias.empty()) {
				choose_color_from_palette (target_color_alias);
			}
		}
		break;
	}

	return true;
}
