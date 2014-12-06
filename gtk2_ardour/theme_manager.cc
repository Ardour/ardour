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
	, base_color_viewport (*base_color_scroller.get_hadjustment(), *base_color_scroller.get_vadjustment())
	, base_color_group (0)
	, palette_window (0)
{
	set_title (_("Theme Manager"));

	/* Basic color list */
	
	color_list = TreeStore::create (columns);
	color_display.set_model (color_list);
	color_display.append_column (_("Object"), columns.name);
	
	Gtkmm2ext::CellRendererColorSelector* color_renderer = manage (new Gtkmm2ext::CellRendererColorSelector);
	TreeViewColumn* color_column = manage (new TreeViewColumn (_("Color"), *color_renderer));
	color_column->add_attribute (color_renderer->property_color(), columns.gdkcolor);

	color_display.append_column (*color_column);
	
	color_display.get_column (0)->set_data (X_("colnum"), GUINT_TO_POINTER(0));
	color_display.get_column (0)->set_expand (true);
	color_display.get_column (1)->set_data (X_("colnum"), GUINT_TO_POINTER(1));
	color_display.get_column (1)->set_expand (false);
	color_display.set_reorderable (false);
	color_display.get_selection()->set_mode (SELECTION_NONE);
	color_display.set_headers_visible (true);

	color_display.signal_button_press_event().connect (sigc::mem_fun (*this, &ThemeManager::button_press_event), false);

	scroller.add (color_display);
	scroller.set_policy (POLICY_NEVER, POLICY_AUTOMATIC);

	/* Now the alias list */
	
	alias_list = TreeStore::create (alias_columns);
	alias_display.set_model (alias_list);
	alias_display.append_column (_("Object"), columns.name);

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

	base_color_viewport.signal_size_allocate().connect (sigc::mem_fun (*this, &ThemeManager::base_color_viewport_allocated));
	base_color_scroller.add (base_color_viewport);
	
	notebook.append_page (scroller, _("Palette"));
	notebook.append_page (base_color_scroller, _("Base Colors"));
	notebook.append_page (alias_scroller, _("Items"));
	
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
	setup_theme ();
	setup_aliases ();
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
ThemeManager::button_press_event (GdkEventButton* ev)
{
	TreeIter iter;
	TreeModel::Path path;
	TreeViewColumn* column;
	int cellx;
	int celly;

	if (!color_display.get_path_at_pos ((int)ev->x, (int)ev->y, path, column, cellx, celly)) {
		return false;
	}

	switch (GPOINTER_TO_UINT (column->get_data (X_("colnum")))) {
	case 0:
		/* allow normal processing to occur */
		return false;

	case 1: /* color */
		if ((iter = color_list->get_iter (path))) {

			ColorVariable<uint32_t>* var = (*iter)[columns.pVar];
			if (!var) {
				/* parent row, do nothing */
				return false;
			}
			
			int r,g, b, a;
			uint32_t rgba = (*iter)[columns.rgba];
			Gdk::Color color;

			UINT_TO_RGBA (rgba, &r, &g, &b, &a);
			color.set_rgb_p (r / 255.0, g / 255.0, b / 255.0);
			color_dialog.get_colorsel()->set_previous_color (color);
			color_dialog.get_colorsel()->set_current_color (color);
			color_dialog.get_colorsel()->set_previous_alpha ((guint16) (a * 256));
			color_dialog.get_colorsel()->set_current_alpha ((guint16) (a * 256));

			color_dialog_connection.disconnect ();
			color_dialog_connection = color_dialog.signal_response().connect (sigc::mem_fun (*this, &ThemeManager::foobar_response));
			color_dialog.present ();
		}
	}

	return true;
}

void
ThemeManager::foobar_response (int result)
{
	// ColorVariable<uint32_t> *ccvar;
	int r,g, b, a;
	uint32_t rgba;
	Gdk::Color color;

	color_dialog_connection.disconnect ();
	
	switch (result) {
	case RESPONSE_CANCEL:
		break;
	case RESPONSE_ACCEPT:
		color = color_dialog.get_colorsel()->get_current_color();
		a = color_dialog.get_colorsel()->get_current_alpha();
		r = (int) floor (color.get_red_p() * 255.0);
		g = (int) floor (color.get_green_p() * 255.0);
		b = (int) floor (color.get_blue_p() * 255.0);
		
		rgba = RGBA_TO_UINT(r,g,b,a>>8);
		// (*iter)[columns.rgba] = rgba;
		// (*iter)[columns.gdkcolor] = color;
		
		// ccvar = (*iter)[columns.pVar];
		// ccvar->set(rgba);
		/* mark dirty ... */
		ARDOUR_UI::config()->set_dirty ();
		/* but save it immediately */
		ARDOUR_UI::config()->save_state ();
		
		ColorsChanged(); //EMIT SIGNAL
		break;
		
	default:
		break;
		
	}
	
	color_dialog.hide ();
}

void
load_rc_file (const string& filename, bool themechange)
{
	std::string rc_file_path;

	if (!find_file (ardour_config_search_path(), filename, rc_file_path)) {
		warning << string_compose (_("Unable to find UI style file %1 in search path %2. %3 will look strange"),
                                           filename, ardour_config_search_path().to_string(), PROGRAM_NAME)
				<< endmsg;
		return;
	}

	info << "Loading ui configuration file " << rc_file_path << endmsg;

	Gtkmm2ext::UI::instance()->load_rcfile (rc_file_path, themechange);
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

        ARDOUR_UI::config()->set_ui_rc_file("ui_dark.rc");
	ARDOUR_UI::config()->set_dirty ();

	load_rc_file (ARDOUR_UI::config()->get_ui_rc_file(), true);
}

void
ThemeManager::on_light_theme_button_toggled()
{
	if (!light_button.get_active()) return;

        ARDOUR_UI::config()->set_ui_rc_file("ui_light.rc");
	load_rc_file (ARDOUR_UI::config()->get_ui_rc_file(), true);
}

void
ThemeManager::setup_theme ()
{
	int r, g, b, a;

	color_list->clear();

	for (std::map<std::string,ColorVariable<uint32_t> *>::iterator i = ARDOUR_UI::config()->configurable_colors.begin(); i != ARDOUR_UI::config()->configurable_colors.end(); i++) {


		ColorVariable<uint32_t>* var = i->second;

		TreeModel::Children rows = color_list->children();
		TreeModel::Row row;
		string::size_type colon;

		if ((colon = var->name().find (':')) != string::npos) {

			/* this is supposed to be a child node, so find the
			 * parent 
			 */

			string parent = var->name().substr (0, colon);
			TreeModel::iterator ri;

			for (ri = rows.begin(); ri != rows.end(); ++ri) {
				string s = (*ri)[columns.name];
				if (s == parent) {
					break;
				}
			}

			if (ri == rows.end()) {
				/* not found, add the parent as new top level row */
				row = *(color_list->append());
				row[columns.name] = parent;
				row[columns.pVar] = 0;
				
				/* now add the child as a child of this one */

				row = *(color_list->insert (row->children().end()));
				row[columns.name] = var->name().substr (colon+1);
			} else {
				row = *(color_list->insert ((*ri)->children().end()));
				row[columns.name] = var->name().substr (colon+1);
			}

		} else {
			/* add as a child */
			row = *(color_list->append());
			row[columns.name] = var->name();
		}

		Gdk::Color col;
		uint32_t rgba = var->get();
		UINT_TO_RGBA (rgba, &r, &g, &b, &a);
		//cerr << (*i)->name() << " == " << hex << rgba << ": " << hex << r << " " << hex << g << " " << hex << b << endl;
		col.set_rgb_p (r / 255.0, g / 255.0, b / 255.0);

		row[columns.pVar] = var;
		row[columns.rgba] = rgba;
		row[columns.gdkcolor] = col;
	}

	ColorsChanged.emit();

	bool env_defined = false;
	string rcfile = Glib::getenv("ARDOUR3_UI_RC", env_defined);

	if(!env_defined) {
		rcfile = ARDOUR_UI::config()->get_ui_rc_file();
	}

	if (rcfile == "ui_dark.rc") {
		dark_button.set_active();
	} else if (rcfile == "ui_light.rc") {
		light_button.set_active();
	}
	
	flat_buttons.set_active (ARDOUR_UI::config()->get_flat_buttons());
	blink_rec_button.set_active (ARDOUR_UI::config()->get_blink_rec_arm());
	waveform_gradient_depth.set_value (ARDOUR_UI::config()->get_waveform_gradient_depth());
	timeline_item_gradient_depth.set_value (ARDOUR_UI::config()->get_timeline_item_gradient_depth());
	all_dialogs.set_active (ARDOUR_UI::config()->get_all_floating_windows_are_dialogs());
	
	load_rc_file(rcfile, false);
}

void
ThemeManager::reset_canvas_colors()
{
	ARDOUR_UI::config()->load_defaults();
	setup_theme ();
	/* mark dirty ... */
	ARDOUR_UI::config()->set_dirty ();
	/* but save it immediately */
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
ThemeManager::initialize_canvas (ArdourCanvas::Canvas& canvas)
{
	using namespace ArdourCanvas;

	/* hide background */
	canvas.set_background_color (rgba_to_color (0.0, 0.0, 1.0, 0.0));

	ScrollGroup* base_color_scroll_group = new ScrollGroup (canvas.root(), ScrollGroup::ScrollSensitivity (ScrollGroup::ScrollsVertically|ScrollGroup::ScrollsHorizontally));
	canvas.add_scroller (*base_color_scroll_group);
	return new ArdourCanvas::Container (base_color_scroll_group);
}

void
ThemeManager::build_base_color_canvas (ArdourCanvas::Container& group, bool (ThemeManager::*event_handler)(GdkEvent*,std::string), double width, double height)
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

	uint32_t color_num = 0;

	/* clear existing rects and delete them */
	
	group.clear (true);
	
	for (uint32_t y = 0; y < height - box_size && color_num < color_limit; y += box_size) {
		for (uint32_t x = 0; x < width - box_size && color_num < color_limit; x += box_size) {
			Rectangle* r = new Rectangle (&group, ArdourCanvas::Rect (x, y, x + box_size, y + box_size));

			string name = nc[color_num++].name;

			UIConfiguration::RelativeColors::iterator c = relatives.find (name);

			if (c != relatives.end()) {
				Color color = c->second.get().color ();
				r->set_fill_color (color);
				r->set_outline_color (rgba_to_color (0.0, 0.0, 0.0, 1.0));
				r->set_tooltip (name);
				r->Event.connect (sigc::bind (sigc::mem_fun (*this, event_handler), name));
			}
		}
	}
}
	
void
ThemeManager::base_color_viewport_allocated (Gtk::Allocation&)
{
	if (!base_color_group) {
		base_color_group = initialize_canvas (*base_color_viewport.canvas());
	}

	build_base_color_canvas (*base_color_group, &ThemeManager::base_color_event,
				 base_color_viewport.canvas()->width(),
				 base_color_viewport.canvas()->height());
				 
}

bool
ThemeManager::base_color_event (GdkEvent*ev, string name)
{
	switch (ev->type) {
	case GDK_BUTTON_RELEASE:
		edit_named_color (name);
		break;
	default:
		break;
	}

	return true;
}

void
ThemeManager::edit_named_color (std::string name)
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

	base_color_edit_name = name;
	
	color_dialog_connection.disconnect ();
	color_dialog_connection = color_dialog.signal_response().connect (sigc::mem_fun (*this, &ThemeManager::base_color_dialog_done));
	color_dialog.present();
}

void
ThemeManager::base_color_dialog_done (int result)
{
	using namespace ArdourCanvas;

	cerr << "Done, using [" << base_color_edit_name << "] res = " << result << endl;
	
	if (base_color_edit_name.empty()) {
		color_dialog.hide ();
		return;
	}

	UIConfiguration* uic (ARDOUR_UI::instance()->config());
	UIConfiguration::RelativeHSV rhsv ("", HSV());
	Gdk::Color gdkcolor;
	double r,g, b, a;

	switch (result) {
	case RESPONSE_ACCEPT:
	case RESPONSE_OK:
		cerr << "Accepting\n";
		gdkcolor = color_dialog.get_colorsel()->get_current_color();
		a = color_dialog.get_colorsel()->get_current_alpha() / 65535.0;
		r = gdkcolor.get_red_p();
		g = gdkcolor.get_green_p();
		b = gdkcolor.get_blue_p();

		rhsv = uic->color_as_relative_hsv (rgba_to_color (r, g, b, a));
		uic->reset_relative (base_color_edit_name, rhsv);

		/* rebuild */
		
		build_base_color_canvas (*base_color_group, &ThemeManager::base_color_event,
					 base_color_viewport.canvas()->width(),
					 base_color_viewport.canvas()->height());
		
		ColorsChanged(); //EMIT SIGNAL
		break;
		
	default:
		break;
	}

	color_dialog.hide ();
	base_color_edit_name = "";
}

void
ThemeManager::palette_canvas_allocated (Gtk::Allocation& alloc, ArdourCanvas::Container* group, bool (ThemeManager::*event_handler)(GdkEvent*,std::string))
{
	build_base_color_canvas (*group, event_handler, alloc.get_width(), alloc.get_height());
}

bool
ThemeManager::palette_chosen (GdkEvent* ev, string name)
{
	switch (ev->type) {
	case GDK_BUTTON_RELEASE:
		break;
	default:
		return false;
		break;
	}

	UIConfiguration* uic (ARDOUR_UI::instance()->config());
	uic->set_alias (palette_edit_name, name);

	(void) palette_done ((GdkEventAny*) 0);

	/* rebuild alias list with new color: inefficient but simple */
	
	setup_aliases ();
	
	return true;
}

bool
ThemeManager::palette_done (GdkEventAny*)
{
	palette_edit_name = "";
	palette_window->hide ();
	return true;
}

void
ThemeManager::choose_color_from_palette (string const & name)
{
	if (!palette_window) {
		palette_window = new Gtk::Window (WINDOW_TOPLEVEL);
		ArdourCanvas::GtkCanvas* canvas = new ArdourCanvas::GtkCanvas ();
		ArdourCanvas::Container* group = initialize_canvas (*canvas);
		
		canvas->signal_size_allocate().connect (sigc::bind (sigc::mem_fun (*this, &ThemeManager::palette_canvas_allocated), group, &ThemeManager::palette_chosen));
		palette_window->signal_delete_event().connect (sigc::mem_fun (*this, &ThemeManager::palette_done));
		
		palette_window->add (*canvas);
		canvas->show ();
	}

	palette_edit_name = name;

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
			choose_color_from_palette ((*iter)[alias_columns.name]);
		}
		break;
	}

	return true;
}
