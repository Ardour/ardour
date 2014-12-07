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
#include "ardour_ui.h"
#include "global_signals.h"
#include "utils.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace PBD;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;

namespace ARDOUR_UI_UTILS {
	sigc::signal<void> ColorsChanged;
	sigc::signal<void,uint32_t> ColorChanged;
}

ThemeManager::ThemeManager()
	: ArdourWindow (_("Theme Manager"))
	, dark_button (_("Dark Theme"))
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
	, icon_set_label (_("Icon Set"))
	, palette_viewport (*palette_scroller.get_hadjustment(), *palette_scroller.get_vadjustment())
	, palette_group (0)
	, palette_window (0)
{
	set_title (_("Theme Manager"));

	/* Basic color list */
	
	basic_color_list = TreeStore::create (basic_color_columns);
	basic_color_display.set_model (basic_color_list);
	basic_color_display.append_column (_("Object"), basic_color_columns.name);
	
	Gtkmm2ext::CellRendererColorSelector* color_renderer = manage (new Gtkmm2ext::CellRendererColorSelector);
	TreeViewColumn* color_column = manage (new TreeViewColumn (_("Color"), *color_renderer));
	color_column->add_attribute (color_renderer->property_color(), basic_color_columns.gdkcolor);

	basic_color_display.append_column (*color_column);
	
	basic_color_display.get_column (0)->set_data (X_("colnum"), GUINT_TO_POINTER(0));
	basic_color_display.get_column (0)->set_expand (true);
	basic_color_display.get_column (1)->set_data (X_("colnum"), GUINT_TO_POINTER(1));
	basic_color_display.get_column (1)->set_expand (false);
	basic_color_display.set_reorderable (false);
	basic_color_display.get_selection()->set_mode (SELECTION_NONE);
	basic_color_display.set_headers_visible (true);

	basic_color_display.signal_button_press_event().connect (sigc::mem_fun (*this, &ThemeManager::basic_color_button_press_event), false);

	scroller.add (basic_color_display);
	scroller.set_policy (POLICY_NEVER, POLICY_AUTOMATIC);

	/* Now the alias list */
	
	alias_list = TreeStore::create (alias_columns);
	alias_display.set_model (alias_list);
	alias_display.append_column (_("Object"), basic_color_columns.name);

	color_renderer = manage (new Gtkmm2ext::CellRendererColorSelector);
	color_column = manage (new TreeViewColumn (_("Color"), *color_renderer));
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

	Gtk::VBox* vbox = Gtk::manage (new Gtk::VBox ());
	vbox->set_homogeneous (false);
	vbox->pack_start (theme_selection_hbox, PACK_SHRINK);
	vbox->pack_start (reset_button, PACK_SHRINK);
#ifndef __APPLE__
	vbox->pack_start (all_dialogs, PACK_SHRINK);
#endif
	vbox->pack_start (flat_buttons, PACK_SHRINK);
	vbox->pack_start (blink_rec_button, PACK_SHRINK);
	vbox->pack_start (region_color_button, PACK_SHRINK);
	vbox->pack_start (show_clipping_button, PACK_SHRINK);

	Gtk::HBox* hbox;

	vector<string> icon_sets = ::get_icon_sets ();

	if (icon_sets.size() > 1) {
		Gtkmm2ext::set_popdown_strings (icon_set_dropdown, icon_sets);
		icon_set_dropdown.set_active_text (ARDOUR_UI::config()->get_icon_set());

		hbox = Gtk::manage (new Gtk::HBox());
		hbox->set_spacing (6);
		hbox->pack_start (icon_set_label, false, false);
		hbox->pack_start (icon_set_dropdown, true, true);
		vbox->pack_start (*hbox, PACK_SHRINK);
	}

	
	hbox = Gtk::manage (new Gtk::HBox());
	hbox->set_spacing (6);
	hbox->pack_start (waveform_gradient_depth, true, true);
	hbox->pack_start (waveform_gradient_depth_label, false, false);
	vbox->pack_start (*hbox, PACK_SHRINK);

	hbox = Gtk::manage (new Gtk::HBox());
	hbox->set_spacing (6);
	hbox->pack_start (timeline_item_gradient_depth, true, true);
	hbox->pack_start (timeline_item_gradient_depth_label, false, false);
	vbox->pack_start (*hbox, PACK_SHRINK);

	palette_group = initialize_palette_canvas (*palette_viewport.canvas());
	palette_viewport.signal_size_allocate().connect (sigc::bind (sigc::mem_fun (*this, &ThemeManager::palette_canvas_allocated), palette_group, palette_viewport.canvas(),
								     sigc::mem_fun (*this, &ThemeManager::palette_event)));
	palette_scroller.add (palette_viewport);
	
	notebook.append_page (alias_scroller, _("Items"));
	notebook.append_page (palette_scroller, _("Palette"));
	notebook.append_page (scroller, _("Colors"));
	
	vbox->pack_start (notebook);

	vbox->show_all ();

	add (*vbox);

	waveform_gradient_depth.set_update_policy (Gtk::UPDATE_DELAYED);
	timeline_item_gradient_depth.set_update_policy (Gtk::UPDATE_DELAYED);
	
	color_dialog.get_colorsel()->set_has_opacity_control (true);
	color_dialog.get_colorsel()->set_has_palette (true);
	
	flat_buttons.set_active (ARDOUR_UI::config()->get_flat_buttons());
	blink_rec_button.set_active (ARDOUR_UI::config()->get_blink_rec_arm());
	region_color_button.set_active (ARDOUR_UI::config()->get_color_regions_using_track_color());
	show_clipping_button.set_active (ARDOUR_UI::config()->get_show_waveform_clipping());

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
	icon_set_dropdown.signal_changed().connect (sigc::mem_fun (*this, &ThemeManager::on_icon_set_changed));

	Gtkmm2ext::UI::instance()->set_tip (all_dialogs, 
					    string_compose (_("Mark all floating windows to be type \"Dialog\" rather than using \"Utility\" for some.\n"
							      "This may help with some window managers. This requires a restart of %1 to take effect"),
							    PROGRAM_NAME));

	set_size_request (-1, 400);
	setup_basic_color_display ();
	/* no need to call setup_palette() here, it will be done when its size is allocated */
	setup_aliases ();

	/* Trigger setting up the GTK color scheme and loading the RC file */
	ARDOUR_UI::config()->color_theme_changed ();
}

ThemeManager::~ThemeManager()
{
}

int
ThemeManager::save (string /*path*/)
{
	return 0;
}

bool
ThemeManager::basic_color_button_press_event (GdkEventButton* ev)
{
	TreeIter iter;
	TreeModel::Path path;
	TreeViewColumn* column;
	int cellx;
	int celly;

	if (!basic_color_display.get_path_at_pos ((int)ev->x, (int)ev->y, path, column, cellx, celly)) {
		return false;
	}

	switch (GPOINTER_TO_UINT (column->get_data (X_("colnum")))) {
	case 0:
		/* allow normal processing to occur */
		return false;

	case 1: /* color */
		if ((iter = basic_color_list->get_iter (path))) {

			ColorVariable<ArdourCanvas::Color>* var = (*iter)[basic_color_columns.pVar];
			if (!var) {
				/* parent row, do nothing */
				return false;
			}

			Gdk::Color color;
			double r, g, b, a;

			ArdourCanvas::color_to_rgba (var->get(), r, g, b, a);
			color.set_rgb_p (r, g, b);
			color_dialog.get_colorsel()->set_previous_color (color);
			color_dialog.get_colorsel()->set_current_color (color);
			color_dialog.get_colorsel()->set_previous_alpha ((guint16) (a * 65535.0));
			color_dialog.get_colorsel()->set_current_alpha ((guint16) (a * 65535.0));

			ColorVariable<ArdourCanvas::Color>* ccvar = (*iter)[basic_color_columns.pVar];
			
			color_dialog_connection.disconnect ();
			color_dialog_connection = color_dialog.signal_response().connect (sigc::bind (sigc::mem_fun (*this, &ThemeManager::basic_color_response), ccvar));
			color_dialog.present ();
		}
	}

	return true;
}

void
ThemeManager::basic_color_response (int result, ColorVariable<ArdourCanvas::Color>* color_variable)
{
	Gdk::Color color;
	double a;
	
	color_dialog_connection.disconnect ();
	
	switch (result) {
	case RESPONSE_CANCEL:
		break;
	case RESPONSE_ACCEPT:
	case RESPONSE_OK:
		color = color_dialog.get_colorsel()->get_current_color();
		a = color_dialog.get_colorsel()->get_current_alpha() / 65535.0;

		color_variable->set (ArdourCanvas::rgba_to_color (color.get_red_p(),
								  color.get_green_p(),
								  color.get_blue_p(),
								  a));
		setup_basic_color_display ();
		setup_palette ();
		setup_aliases ();	
		ColorsChanged(); //EMIT SIGNAL
		break;
		
	default:
		break;
		
	}
	
	color_dialog.hide ();
}

void
ThemeManager::on_flat_buttons_toggled ()
{
	ARDOUR_UI::config()->set_flat_buttons (flat_buttons.get_active());
	ARDOUR_UI::config()->set_dirty ();
	ArdourButton::set_flat_buttons (flat_buttons.get_active());
	/* force a redraw */
	gtk_rc_reset_styles (gtk_settings_get_default());
}

void
ThemeManager::on_blink_rec_arm_toggled ()
{
	ARDOUR_UI::config()->set_blink_rec_arm (blink_rec_button.get_active());
	ARDOUR_UI::config()->set_dirty ();
	ARDOUR::Config->ParameterChanged("blink-rec-arm");
}

void
ThemeManager::on_region_color_toggled ()
{
	ARDOUR_UI::config()->set_color_regions_using_track_color (region_color_button.get_active());
	ARDOUR_UI::config()->set_dirty ();
}

void
ThemeManager::on_show_clip_toggled ()
{
	ARDOUR_UI::config()->set_show_waveform_clipping (show_clipping_button.get_active());
	ARDOUR_UI::config()->set_dirty ();
}

void
ThemeManager::on_all_dialogs_toggled ()
{
	ARDOUR_UI::config()->set_all_floating_windows_are_dialogs (all_dialogs.get_active());
	ARDOUR_UI::config()->set_dirty ();
}

void
ThemeManager::on_waveform_gradient_depth_change ()
{
	double v = waveform_gradient_depth.get_value();

	ARDOUR_UI::config()->set_waveform_gradient_depth (v);
	ARDOUR_UI::config()->set_dirty ();
	ArdourCanvas::WaveView::set_global_gradient_depth (v);
}

void
ThemeManager::on_timeline_item_gradient_depth_change ()
{
	double v = timeline_item_gradient_depth.get_value();

	ARDOUR_UI::config()->set_timeline_item_gradient_depth (v);
	ARDOUR_UI::config()->set_dirty ();
}

void
ThemeManager::on_icon_set_changed ()
{
	string new_set = icon_set_dropdown.get_active_text();
	ARDOUR_UI::config()->set_icon_set (new_set);
}

void
ThemeManager::on_dark_theme_button_toggled()
{
	if (!dark_button.get_active()) return;

	UIConfiguration* uic (ARDOUR_UI::config());
	
        uic->set_ui_rc_file("ui_dark.rc");
}

void
ThemeManager::on_light_theme_button_toggled()
{
	if (!light_button.get_active()) return;

	UIConfiguration* uic (ARDOUR_UI::config());
	
        uic->set_ui_rc_file("ui_light.rc");
}

void
ThemeManager::setup_basic_color_display ()
{
	int r, g, b, a;

	basic_color_list->clear();

	for (std::map<std::string,ColorVariable<uint32_t> *>::iterator i = ARDOUR_UI::config()->configurable_colors.begin(); i != ARDOUR_UI::config()->configurable_colors.end(); i++) {


		ColorVariable<uint32_t>* var = i->second;

		TreeModel::Children rows = basic_color_list->children();
		TreeModel::Row row;
		string::size_type colon;

		if ((colon = var->name().find (':')) != string::npos) {

			/* this is supposed to be a child node, so find the
			 * parent 
			 */

			string parent = var->name().substr (0, colon);
			TreeModel::iterator ri;

			for (ri = rows.begin(); ri != rows.end(); ++ri) {
				string s = (*ri)[basic_color_columns.name];
				if (s == parent) {
					break;
				}
			}

			if (ri == rows.end()) {
				/* not found, add the parent as new top level row */
				row = *(basic_color_list->append());
				row[basic_color_columns.name] = parent;
				row[basic_color_columns.pVar] = 0;
				
				/* now add the child as a child of this one */

				row = *(basic_color_list->insert (row->children().end()));
				row[basic_color_columns.name] = var->name().substr (colon+1);
			} else {
				row = *(basic_color_list->insert ((*ri)->children().end()));
				row[basic_color_columns.name] = var->name().substr (colon+1);
			}

		} else {
			/* add as a child */
			row = *(basic_color_list->append());
			row[basic_color_columns.name] = var->name();
		}

		Gdk::Color col;
		uint32_t rgba = var->get();
		UINT_TO_RGBA (rgba, &r, &g, &b, &a);
		col.set_rgb_p (r / 255.0, g / 255.0, b / 255.0);

		row[basic_color_columns.pVar] = var;
		row[basic_color_columns.rgba] = rgba;
		row[basic_color_columns.gdkcolor] = col;
	}

	UIConfiguration* uic (ARDOUR_UI::config());
	
	flat_buttons.set_active (uic->get_flat_buttons());
	blink_rec_button.set_active (uic->get_blink_rec_arm());
	waveform_gradient_depth.set_value (uic->get_waveform_gradient_depth());
	timeline_item_gradient_depth.set_value (uic->get_timeline_item_gradient_depth());
	all_dialogs.set_active (uic->get_all_floating_windows_are_dialogs());
}

void
ThemeManager::reset_canvas_colors()
{
	ARDOUR_UI::config()->load_defaults();
	setup_basic_color_display ();
	ARDOUR_UI::config()->set_dirty ();
	ARDOUR_UI::config()->save_state ();
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

void
ThemeManager::build_palette_canvas (ArdourCanvas::Canvas& canvas, ArdourCanvas::Container& group, sigc::slot<bool,GdkEvent*,std::string> event_handler)
{
	using namespace ArdourCanvas;

	/* we want the colors sorted by hue, with their name */

	UIConfiguration::RelativeColors& relatives (ARDOUR_UI::instance()->config()->relative_colors);
	vector<NamedColor> nc;
	for (UIConfiguration::RelativeColors::const_iterator x = relatives.begin(); x != relatives.end(); ++x) {
		nc.push_back (NamedColor (x->first, x->second.get()));
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

			UIConfiguration::RelativeColors::iterator c = relatives.find (name);

			if (c != relatives.end()) {
				Color color = c->second.get().color ();
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
	uint32_t ncolors = ARDOUR_UI::instance()->config()->relative_colors.size();
	const int box_size = 20;

	double c = sqrt (ncolors);
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
	UIConfiguration* uic (ARDOUR_UI::instance()->config());
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
	
	UIConfiguration* uic (ARDOUR_UI::instance()->config());
	UIConfiguration::RelativeHSV rhsv ("", HSV());
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

		rhsv = uic->color_as_relative_hsv (rgba_to_color (r, g, b, a));
		uic->reset_relative (name, rhsv);

		/* rebuild */
		
		setup_palette ();
		ColorsChanged(); //EMIT SIGNAL
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
		ARDOUR_UI::instance()->config()->set_alias (target_name, new_alias);
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
	palette_response_connection.disconnect ();

	switch (response) {
	case GTK_RESPONSE_OK:
	case GTK_RESPONSE_ACCEPT:
		/* rebuild alias list with new color: inefficient but simple */
		
		setup_aliases ();
		break;
	default:
		/* revert choice */
		ARDOUR_UI::instance()->config()->set_alias (target_name, old_alias);
		break;
	}

	palette_window->hide ();
}

void
ThemeManager::choose_color_from_palette (string const & name)
{
	UIConfiguration* uic (ARDOUR_UI::config());
	UIConfiguration::ColorAliases::iterator i = uic->color_aliases.find (name);

	if (i == uic->color_aliases.end()) {
		return;
	}

	if (!palette_window) {
		palette_window = new ArdourDialog (_("Color Palette"));
		palette_window->add_button (Stock::CANCEL, RESPONSE_CANCEL);
		palette_window->add_button (Stock::OK, RESPONSE_OK);

		ArdourCanvas::GtkCanvas* canvas = new ArdourCanvas::GtkCanvas ();
		ArdourCanvas::Container* group = initialize_palette_canvas (*canvas);
		
		canvas->signal_size_request().connect (sigc::mem_fun (*this, &ThemeManager::palette_size_request));
		canvas->signal_size_allocate().connect (sigc::bind (sigc::mem_fun (*this, &ThemeManager::palette_canvas_allocated), group, canvas,
								    sigc::bind (sigc::mem_fun (*this, &ThemeManager::alias_palette_event), name)));

		palette_window->get_vbox()->pack_start (*canvas);
		palette_window->show_all ();
	}

	palette_response_connection.disconnect ();
	palette_response_connection = palette_window->signal_response().connect (sigc::bind (sigc::mem_fun (*this, &ThemeManager::alias_palette_response), name, i->second));

	palette_window->set_position (WIN_POS_MOUSE);
	palette_window->present ();
}

void
ThemeManager::setup_aliases ()
{
	using namespace ArdourCanvas;
	
	UIConfiguration* uic (ARDOUR_UI::instance()->config());
	UIConfiguration::ColorAliases& aliases (uic->color_aliases);

	alias_list->clear ();

	for (UIConfiguration::ColorAliases::iterator i = aliases.begin(); i != aliases.end(); ++i) {
		TreeModel::Row row;

		row = *(alias_list->append());
		row[alias_columns.name] = i->first;
		row[alias_columns.alias] = i->second;

		Color c = uic->color (i->second);

		/* Gdk colors don't support alpha */

		double r, g, b, a;
		color_to_rgba (c, r, g, b, a);
		Gdk::Color gcolor;
		gcolor.set_rgb_p (r, g, b);

		row[alias_columns.color] = gcolor;
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
			string target_color_name = (*iter)[alias_columns.name];
			choose_color_from_palette (target_color_name);
		}
		break;
	}

	return true;
}
