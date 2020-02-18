/*
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016-2019 Paul Davis <paul@linuxaudiosystems.com>
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

#include <cmath>
#include <errno.h>

#include <gtkmm/stock.h>

#include "fix_carbon.h"

#include "pbd/gstdio_compat.h"

#include "pbd/compose.h"
#include "pbd/file_utils.h"
#include "pbd/replace_all.h"

#include "ardour/filesystem_paths.h"
#include "ardour/profile.h"

#include "gtkmm2ext/cell_renderer_color_selector.h"
#include "gtkmm2ext/utils.h"

#include "canvas/container.h"
#include "canvas/rectangle.h"
#include "canvas/scroll_group.h"

#include "waveview/wave_view.h"

#include "ardour_dialog.h"
#include "color_theme_manager.h"
#include "rgb_macros.h"
#include "ui_config.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace std;
using namespace Gtk;
using namespace PBD;
using namespace ARDOUR;
using namespace Gtkmm2ext;
using namespace ARDOUR_UI_UTILS;

ColorThemeManager::ColorThemeManager ()
	: reset_button (_("Restore Defaults"))
	, palette_viewport (*palette_scroller.get_hadjustment(), *palette_scroller.get_vadjustment())
	, palette_group (0)
	, palette_window (0)
	, color_theme_label (_("Color Theme"))
{
	std::map<string,string> color_themes;

	get_color_themes (color_themes);
	int n = 0;

	if (color_themes.size() > 1) {
		theme_list = TreeStore::create (color_theme_columns);

		TreeModel::iterator selected_iter = theme_list->children().end();

		for (std::map<string,string>::iterator c = color_themes.begin(); c != color_themes.end(); ++c) {
			TreeModel::Row row;

			row = *(theme_list->append());
			row[color_theme_columns.name] = c->first;

			string color_file_name = c->second;

			row[color_theme_columns.path] = color_file_name;

			/* match second (path; really basename) since that is
			   what we store/restore.
			*/

			if (UIConfiguration::instance().get_color_file() == color_file_name) {
				selected_iter = row;
			}
		}

		color_theme_dropdown.set_model (theme_list);
		color_theme_dropdown.pack_start (color_theme_columns.name);

		if (selected_iter != theme_list->children().end()) {
			color_theme_dropdown.set_active (selected_iter);
		}

		Gtk::HBox* hbox = Gtk::manage (new Gtk::HBox());
		Gtk::Alignment* align = Gtk::manage (new Gtk::Alignment (0, 0.5, 0, 1.0));
		align->add (color_theme_dropdown);
		hbox->set_spacing (6);
		hbox->pack_start (color_theme_label, false, false);
		hbox->pack_start (*align, true, true);
		hbox->show_all ();
		table.attach (*hbox, 0, 3, n, n + 1);
		++n;
	}

	reset_button.signal_clicked().connect (sigc::mem_fun (*this, &ColorThemeManager::reset_canvas_colors));

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
	alias_display.set_headers_visible (true);

	alias_display.signal_button_press_event().connect (sigc::mem_fun (*this, &ColorThemeManager::alias_button_press_event), false);

	alias_scroller.add (alias_display);

	palette_group = initialize_palette_canvas (*palette_viewport.canvas());
	palette_viewport.signal_size_allocate().connect (sigc::bind (sigc::mem_fun (*this, &ColorThemeManager::palette_canvas_allocated), palette_group, palette_viewport.canvas(),
	                                                             sigc::mem_fun (*this, &ColorThemeManager::palette_event)));
	palette_scroller.add (palette_viewport);

	modifier_scroller.add (modifier_vbox);

	notebook.append_page (alias_scroller, _("Items"));
	notebook.append_page (palette_scroller, _("Palette"));
	notebook.append_page (modifier_scroller, _("Transparency"));

	notebook.set_size_request (400, 400);

	table.attach (notebook, 0, 3, n, n + 1);
	++n;

	Alignment* a = manage (new Alignment (0, 0.5, 0, 1.0));
	a->add (reset_button);
	table.attach (*a, 0, 1, n, n + 1);

	color_dialog.get_colorsel()->set_has_opacity_control (true);
	color_dialog.get_colorsel()->set_has_palette (true);
	color_dialog.get_ok_button()->signal_clicked().connect (sigc::bind (sigc::mem_fun (color_dialog, &Gtk::Dialog::response), RESPONSE_ACCEPT));
	color_dialog.get_cancel_button()->signal_clicked().connect (sigc::bind (sigc::mem_fun (color_dialog, &Gtk::Dialog::response), RESPONSE_CANCEL));

	color_theme_dropdown.signal_changed().connect (sigc::mem_fun (*this, &ColorThemeManager::on_color_theme_changed));

	/* no need to call setup_palette() here, it will be done when its size is allocated */
	setup_aliases ();
	setup_modifiers ();

	UIConfiguration::instance().ColorsChanged.connect (sigc::mem_fun (*this, &ColorThemeManager::colors_changed));
}


ColorThemeManager::~ColorThemeManager ()
{
	if (palette_group) {
		palette_group->clear (true);
		delete palette_group;
	}
}


void
ColorThemeManager::setup_modifiers ()
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
		mod_scale->signal_value_changed().connect (sigc::bind (sigc::mem_fun (*this, &ColorThemeManager::modifier_edited), mod_scale, m->first));

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
ColorThemeManager::modifier_edited (Gtk::Range* range, string name)
{
	using namespace ArdourCanvas;

	double alpha = range->get_value();
	SVAModifier svam (SVAModifier::Assign, -1.0, -1.0, alpha);
	UIConfiguration::instance().set_modifier (name, svam);
}

void
ColorThemeManager::colors_changed ()
{
	setup_palette ();
	setup_aliases ();
	setup_modifiers ();
}

void
ColorThemeManager::reset_canvas_colors()
{
	string cfile;
	string basename;

	/* look for a versioned user-owned color file, and try to rename it */

	basename = UIConfiguration::instance().color_file_name (true, true);

	if (find_file (ardour_config_search_path(), basename, cfile)) {
		string backup = cfile + string (X_(".old"));
		g_rename (cfile.c_str(), backup.c_str());
		/* don't really care if it fails */
	}

	UIConfiguration::instance().load_color_theme (false);
	UIConfiguration::instance().save_state ();
}

ArdourCanvas::Container*
ColorThemeManager::initialize_palette_canvas (ArdourCanvas::Canvas& canvas)
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
ColorThemeManager::palette_canvas_allocated (Gtk::Allocation& alloc, ArdourCanvas::Container* group, ArdourCanvas::Canvas* canvas, sigc::slot<bool,GdkEvent*,std::string> event_handler)
{
	build_palette_canvas (*canvas, *group, event_handler);
}

struct NamedColor {
	string name;
	Gtkmm2ext::HSV    color;
	NamedColor (string s, Gtkmm2ext::HSV c) : name (s), color (c) {}
};

struct SortNamedColor {
	bool operator() (NamedColor const & a, NamedColor const & b) {
		return a.name < b.name;
	}
};


void
ColorThemeManager::build_palette_canvas (ArdourCanvas::Canvas& canvas, ArdourCanvas::Container& group, sigc::slot<bool,GdkEvent*,std::string> event_handler)
{
	using namespace ArdourCanvas;

	/* we want the colors sorted by hue, with their name */

	UIConfiguration::Colors& colors (UIConfiguration::instance().colors);
	vector<NamedColor> nc;
	for (UIConfiguration::Colors::const_iterator x = colors.begin(); x != colors.end(); ++x) {
		nc.push_back (NamedColor (x->first, HSV (x->second)));
	}
	SortNamedColor sorter;
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
ColorThemeManager::palette_size_request (Gtk::Requisition* req)
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
ColorThemeManager::setup_palette ()
{
	build_palette_canvas (*palette_viewport.canvas(), *palette_group, sigc::mem_fun (*this, &ColorThemeManager::palette_event));
}

bool
ColorThemeManager::palette_event (GdkEvent* ev, string name)
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
ColorThemeManager::edit_palette_color (std::string name)
{
	using namespace ArdourCanvas;
	double r,g, b, a;
	UIConfiguration* uic (&UIConfiguration::instance());
	Gtkmm2ext::Color c = uic->color (name);
	Gdk::Color gdkcolor;

	color_to_rgba (c, r, g, b, a);

	gdkcolor.set_rgb_p (r, g, b);
	color_dialog.get_colorsel()->set_previous_color (gdkcolor);
	color_dialog.get_colorsel()->set_current_color (gdkcolor);
	color_dialog.get_colorsel()->set_previous_alpha ((guint16) (a * 65535));
	color_dialog.get_colorsel()->set_current_alpha ((guint16) (a * 65535));

	color_dialog_connection.disconnect ();
	color_dialog_connection = color_dialog.signal_response().connect (sigc::bind (sigc::mem_fun (*this, &ColorThemeManager::palette_color_response), name));
	color_dialog.present();
}

void
ColorThemeManager::palette_color_response (int result, std::string name)
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
ColorThemeManager::alias_palette_event (GdkEvent* ev, string new_alias, string target_name)
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
ColorThemeManager::alias_palette_response (int response, std::string target_name, std::string old_alias)
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
ColorThemeManager::choose_color_from_palette (string const & name)
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

	canvas->signal_size_request().connect (sigc::mem_fun (*this, &ColorThemeManager::palette_size_request));
	canvas->signal_size_allocate().connect (sigc::bind (sigc::mem_fun (*this, &ColorThemeManager::palette_canvas_allocated), group, canvas,
							    sigc::bind (sigc::mem_fun (*this, &ColorThemeManager::alias_palette_event), name)));

	palette_window->get_vbox()->pack_start (*canvas);
	palette_window->show_all ();

	palette_response_connection = palette_window->signal_response().connect (sigc::bind (sigc::mem_fun (*this, &ColorThemeManager::alias_palette_response), name, i->second));

	palette_window->set_position (WIN_POS_MOUSE);
	palette_window->present ();
}

void
ColorThemeManager::setup_aliases ()
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
ColorThemeManager::alias_button_press_event (GdkEventButton* ev)
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

void
ColorThemeManager::parameter_changed (string const&)
{
}

void
ColorThemeManager::set_state_from_config ()
{

}

void
ColorThemeManager::add_to_page (OptionEditorPage* p)
{
	int const n = p->table.property_n_rows();
	int m = n + 1;
	if (!_note.empty ()) {
		++m;
	}
	p->table.resize (m, 3);
	p->table.attach (box, 1, 3, n, n + 1, FILL | EXPAND, SHRINK, 0, 0);
	maybe_add_note (p, n + 1);
}

Gtk::Widget&
ColorThemeManager::tip_widget()
{
	return reset_button; /* XXX need a better widget for this purpose */
}

void
ColorThemeManager::on_color_theme_changed ()
{
	Gtk::TreeModel::iterator iter = color_theme_dropdown.get_active();

	if (iter) {
		Gtk::TreeModel::Row row = *iter;

		if (row) {
			string new_theme = row[color_theme_columns.path];
			UIConfiguration::instance().set_color_file (new_theme);
		}
	}
}
