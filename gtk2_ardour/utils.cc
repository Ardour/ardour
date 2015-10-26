/*
    Copyright (C) 2003 Paul Davis

    This program is free software; you an redistribute it and/or modify
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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <cstdlib>
#include <clocale>
#include <cstring>
#include <cctype>
#include <cmath>
#include <list>
#include <sys/stat.h>
#include <gtkmm/rc.h>
#include <gtkmm/window.h>
#include <gtkmm/combo.h>
#include <gtkmm/label.h>
#include <gtkmm/paned.h>
#include <gtk/gtkpaned.h>
#include <boost/algorithm/string.hpp>

#include "pbd/file_utils.h"

#include <gtkmm2ext/utils.h>

#include "ardour/filesystem_paths.h"

#include "canvas/item.h"
#include "canvas/utils.h"

#include "debug.h"
#include "public_editor.h"
#include "keyboard.h"
#include "utils.h"
#include "i18n.h"
#include "rgb_macros.h"
#include "gui_thread.h"
#include "ui_config.h"
#include "ardour_dialog.h"
#include "ardour_ui.h"

using namespace std;
using namespace Gtk;
using namespace Glib;
using namespace PBD;
using Gtkmm2ext::Keyboard;

namespace ARDOUR_UI_UTILS {
	sigc::signal<void>  DPIReset;
}

#ifdef PLATFORM_WINDOWS
#define random() rand()
#endif


/** Add an element to a menu, settings its sensitivity.
 * @param m Menu to add to.
 * @param e Element to add.
 * @param s true to make sensitive, false to make insensitive
 */
void
ARDOUR_UI_UTILS::add_item_with_sensitivity (Menu_Helpers::MenuList& m, Menu_Helpers::MenuElem e, bool s)
{
	m.push_back (e);
	if (!s) {
		m.back().set_sensitive (false);
	}
}


gint
ARDOUR_UI_UTILS::just_hide_it (GdkEventAny */*ev*/, Gtk::Window *win)
{
	win->hide ();
	return 0;
}

/* xpm2rgb copied from nixieclock, which bore the legend:

    nixieclock - a nixie desktop timepiece
    Copyright (C) 2000 Greg Ercolano, erco@3dsite.com

    and was released under the GPL.
*/

unsigned char*
ARDOUR_UI_UTILS::xpm2rgb (const char** xpm, uint32_t& w, uint32_t& h)
{
	static long vals[256], val;
	uint32_t t, x, y, colors, cpp;
	unsigned char c;
	unsigned char *savergb, *rgb;

	// PARSE HEADER

	if ( sscanf(xpm[0], "%u%u%u%u", &w, &h, &colors, &cpp) != 4 ) {
		error << string_compose (_("bad XPM header %1"), xpm[0])
		      << endmsg;
		return 0;
	}

	savergb = rgb = (unsigned char*) malloc (h * w * 3);

	// LOAD XPM COLORMAP LONG ENOUGH TO DO CONVERSION
	for (t = 0; t < colors; ++t) {
		sscanf (xpm[t+1], "%c c #%lx", &c, &val);
		vals[c] = val;
	}

	// COLORMAP -> RGB CONVERSION
	//    Get low 3 bytes from vals[]
	//

	const char *p;
	for (y = h-1; y > 0; --y) {

		for (p = xpm[1+colors+(h-y-1)], x = 0; x < w; x++, rgb += 3) {
			val = vals[(int)*p++];
			*(rgb+2) = val & 0xff; val >>= 8;  // 2:B
			*(rgb+1) = val & 0xff; val >>= 8;  // 1:G
			*(rgb+0) = val & 0xff;             // 0:R
		}
	}

	return (savergb);
}

unsigned char*
ARDOUR_UI_UTILS::xpm2rgba (const char** xpm, uint32_t& w, uint32_t& h)
{
	static long vals[256], val;
	uint32_t t, x, y, colors, cpp;
	unsigned char c;
	unsigned char *savergb, *rgb;
	char transparent;

	// PARSE HEADER

	if ( sscanf(xpm[0], "%u%u%u%u", &w, &h, &colors, &cpp) != 4 ) {
		error << string_compose (_("bad XPM header %1"), xpm[0])
		      << endmsg;
		return 0;
	}

	savergb = rgb = (unsigned char*) malloc (h * w * 4);

	// LOAD XPM COLORMAP LONG ENOUGH TO DO CONVERSION

	if (strstr (xpm[1], "None")) {
		sscanf (xpm[1], "%c", &transparent);
		t = 1;
	} else {
		transparent = 0;
		t = 0;
	}

	for (; t < colors; ++t) {
		sscanf (xpm[t+1], "%c c #%lx", &c, &val);
		vals[c] = val;
	}

	// COLORMAP -> RGB CONVERSION
	//    Get low 3 bytes from vals[]
	//

	const char *p;
	for (y = h-1; y > 0; --y) {

		char alpha;

		for (p = xpm[1+colors+(h-y-1)], x = 0; x < w; x++, rgb += 4) {

			if (transparent && (*p++ == transparent)) {
				alpha = 0;
				val = 0;
			} else {
				alpha = 255;
				val = vals[(int)*p];
			}

			*(rgb+3) = alpha;                  // 3: alpha
			*(rgb+2) = val & 0xff; val >>= 8;  // 2:B
			*(rgb+1) = val & 0xff; val >>= 8;  // 1:G
			*(rgb+0) = val & 0xff;             // 0:R
		}
	}

	return (savergb);
}

/** Returns a Pango::FontDescription given a string describing the font.
 *
 * If the returned FontDescription does not specify a family, then
 * the family is set to "Sans". This mirrors GTK's behaviour in
 * gtkstyle.c.
 *
 * Some environments will force Pango to specify the family
 * even if it was not specified in the string describing the font.
 * Such environments should be left unaffected by this function,
 * since the font family will be left alone.
 *
 * There may be other similar font specification enforcement
 * that we might add here later.
 */
Pango::FontDescription
ARDOUR_UI_UTILS::sanitized_font (std::string const& name)
{
	Pango::FontDescription fd (name);

	if (fd.get_family().empty()) {
		fd.set_family ("Sans");
	}

	return fd;
}

Pango::FontDescription
ARDOUR_UI_UTILS::get_font_for_style (string widgetname)
{
	Gtk::Window window (WINDOW_TOPLEVEL);
	Gtk::Label foobar;
	Glib::RefPtr<Gtk::Style> style;

	window.add (foobar);
	foobar.set_name (widgetname);
	foobar.ensure_style();

	style = foobar.get_style ();

	Glib::RefPtr<const Pango::Layout> layout = foobar.get_layout();

	PangoFontDescription *pfd = const_cast<PangoFontDescription *> (pango_layout_get_font_description(const_cast<PangoLayout *>(layout->gobj())));

	if (!pfd) {

		/* layout inherited its font description from a PangoContext */

		PangoContext* ctxt = (PangoContext*) pango_layout_get_context (const_cast<PangoLayout*>(layout->gobj()));
		pfd =  pango_context_get_font_description (ctxt);
		return Pango::FontDescription (pfd); /* make a copy */
	}

	return Pango::FontDescription (pfd); /* make a copy */
}

void
ARDOUR_UI_UTILS::set_color_from_rgb (Gdk::Color& c, uint32_t rgb)
{
	/* Gdk::Color color ranges are 16 bit, so scale from 8 bit by
	   multiplying by 256.
	*/
	c.set_rgb ((rgb >> 16)*256, ((rgb & 0xff00) >> 8)*256, (rgb & 0xff)*256);
}

void
ARDOUR_UI_UTILS::set_color_from_rgba (Gdk::Color& c, uint32_t rgba)
{
	/* Gdk::Color color ranges are 16 bit, so scale from 8 bit by
	   multiplying by 256.
	*/
	c.set_rgb ((rgba >> 24)*256, ((rgba & 0xff0000) >> 16)*256, ((rgba & 0xff00) >> 8)*256);
}

uint32_t
ARDOUR_UI_UTILS::gdk_color_to_rgba (Gdk::Color const& c)
{
	/* since alpha value is not available from a Gdk::Color, it is
	   hardcoded as 0xff (aka 255 or 1.0)
	*/

	const uint32_t r = c.get_red_p () * 255.0;
	const uint32_t g = c.get_green_p () * 255.0;
	const uint32_t b = c.get_blue_p () * 255.0;
	const uint32_t a = 0xff;

	return RGBA_TO_UINT (r,g,b,a);
}


bool
ARDOUR_UI_UTILS::relay_key_press (GdkEventKey* ev, Gtk::Window* win)
{
	return ARDOUR_UI::instance()->key_event_handler (ev, win);
}

bool
ARDOUR_UI_UTILS::emulate_key_event (unsigned int keyval)
{
	GdkDisplay  *display = gtk_widget_get_display (GTK_WIDGET(ARDOUR_UI::instance()->main_window().gobj()));
	GdkKeymap   *keymap  = gdk_keymap_get_for_display (display);
	GdkKeymapKey *keymapkey = NULL;
	gint n_keys;

	if (!gdk_keymap_get_entries_for_keyval(keymap, keyval, &keymapkey, &n_keys)) return false;
	if (n_keys !=1) { g_free(keymapkey); return false;}

	Gtk::Window& main_window (ARDOUR_UI::instance()->main_window());

	GdkEventKey ev;
	ev.type = GDK_KEY_PRESS;
	ev.window = main_window.get_window()->gobj();
	ev.send_event = FALSE;
	ev.time = 0;
	ev.state = 0;
	ev.keyval = keyval;
	ev.length = 0;
	ev.string = const_cast<gchar*> ("");
	ev.hardware_keycode = keymapkey[0].keycode;
	ev.group = keymapkey[0].group;
	g_free(keymapkey);

	relay_key_press (&ev, &main_window);
	ev.type = GDK_KEY_RELEASE;
	return relay_key_press(&ev, &main_window);
}

string
ARDOUR_UI_UTILS::show_gdk_event_state (int state)
{
	string s;
	if (state & GDK_SHIFT_MASK) {
		s += "+SHIFT";
	}
	if (state & GDK_LOCK_MASK) {
		s += "+LOCK";
	}
	if (state & GDK_CONTROL_MASK) {
		s += "+CONTROL";
	}
	if (state & GDK_MOD1_MASK) {
		s += "+MOD1";
	}
	if (state & GDK_MOD2_MASK) {
		s += "+MOD2";
	}
	if (state & GDK_MOD3_MASK) {
		s += "+MOD3";
	}
	if (state & GDK_MOD4_MASK) {
		s += "+MOD4";
	}
	if (state & GDK_MOD5_MASK) {
		s += "+MOD5";
	}
	if (state & GDK_BUTTON1_MASK) {
		s += "+BUTTON1";
	}
	if (state & GDK_BUTTON2_MASK) {
		s += "+BUTTON2";
	}
	if (state & GDK_BUTTON3_MASK) {
		s += "+BUTTON3";
	}
	if (state & GDK_BUTTON4_MASK) {
		s += "+BUTTON4";
	}
	if (state & GDK_BUTTON5_MASK) {
		s += "+BUTTON5";
	}
	if (state & GDK_SUPER_MASK) {
		s += "+SUPER";
	}
	if (state & GDK_HYPER_MASK) {
		s += "+HYPER";
	}
	if (state & GDK_META_MASK) {
		s += "+META";
	}
	if (state & GDK_RELEASE_MASK) {
		s += "+RELEASE";
	}

	return s;
}

Glib::RefPtr<Gdk::Pixbuf>
ARDOUR_UI_UTILS::get_xpm (std::string name)
{
	if (!xpm_map[name]) {

		Searchpath spath(ARDOUR::ardour_data_search_path());

		spath.add_subdirectory_to_paths("pixmaps");

		std::string data_file_path;

		if(!find_file (spath, name, data_file_path)) {
			fatal << string_compose (_("cannot find XPM file for %1"), name) << endmsg;
		}

		try {
			xpm_map[name] =  Gdk::Pixbuf::create_from_file (data_file_path);
		} catch(const Glib::Error& e)	{
			warning << "Caught Glib::Error: " << e.what() << endmsg;
		}
	}

	return xpm_map[name];
}

vector<string>
ARDOUR_UI_UTILS::get_icon_sets ()
{
	Searchpath spath(ARDOUR::ardour_data_search_path());
	spath.add_subdirectory_to_paths ("icons");
	vector<string> r;

	r.push_back (_("default"));

	for (vector<string>::iterator s = spath.begin(); s != spath.end(); ++s) {

		vector<string> entries;

		get_paths (entries, *s, false, false);

		for (vector<string>::iterator e = entries.begin(); e != entries.end(); ++e) {
			if (Glib::file_test (*e, Glib::FILE_TEST_IS_DIR)) {
				r.push_back (Glib::filename_to_utf8 (Glib::path_get_basename(*e)));
			}
		}
	}

	return r;
}

std::string
ARDOUR_UI_UTILS::get_icon_path (const char* cname, string icon_set, bool is_image)
{
	std::string data_file_path;
	string name = cname;

	if (is_image) {
		name += X_(".png");
	}

	Searchpath spath(ARDOUR::ardour_data_search_path());

	if (!icon_set.empty() && icon_set != _("default")) {

		/* add "icons/icon_set" but .. not allowed to add both of these at once */
		spath.add_subdirectory_to_paths ("icons");
		spath.add_subdirectory_to_paths (icon_set);

		find_file (spath, name, data_file_path);
	} else {
		spath.add_subdirectory_to_paths ("icons");
		find_file (spath, name, data_file_path);
	}

	if (is_image && data_file_path.empty()) {

		if (!icon_set.empty() && icon_set != _("default")) {
			warning << string_compose (_("icon \"%1\" not found for icon set \"%2\", fallback to default"), cname, icon_set) << endmsg;
		}

		Searchpath def (ARDOUR::ardour_data_search_path());
		def.add_subdirectory_to_paths ("icons");

		if (!find_file (def, name, data_file_path)) {
			fatal << string_compose (_("cannot find icon image for %1 using %2"), name, spath.to_string()) << endmsg;
			abort(); /*NOTREACHED*/
		}
	}

	return data_file_path;
}

Glib::RefPtr<Gdk::Pixbuf>
ARDOUR_UI_UTILS::get_icon (const char* cname, string icon_set)
{
	Glib::RefPtr<Gdk::Pixbuf> img;
	try {
		img = Gdk::Pixbuf::create_from_file (get_icon_path (cname, icon_set));
	} catch (const Gdk::PixbufError &e) {
		cerr << "Caught PixbufError: " << e.what() << endl;
	} catch (...) {
		error << string_compose (_("Caught exception while loading icon named %1"), cname) << endmsg;
	}

	return img;
}

namespace ARDOUR_UI_UTILS {
Glib::RefPtr<Gdk::Pixbuf>
get_icon (const char* cname)
{
	Glib::RefPtr<Gdk::Pixbuf> img;
	try {
		img = Gdk::Pixbuf::create_from_file (get_icon_path (cname));
	} catch (const Gdk::PixbufError &e) {
		cerr << "Caught PixbufError: " << e.what() << endl;
	} catch (...) {
		error << string_compose (_("Caught exception while loading icon named %1"), cname) << endmsg;
	}

	return img;
}
}

string
ARDOUR_UI_UTILS::longest (vector<string>& strings)
{
	if (strings.empty()) {
		return string ("");
	}

	vector<string>::iterator longest = strings.begin();
	string::size_type longest_length = (*longest).length();

	vector<string>::iterator i = longest;
	++i;

	while (i != strings.end()) {

		string::size_type len = (*i).length();

		if (len > longest_length) {
			longest = i;
			longest_length = len;
		}

		++i;
	}

	return *longest;
}

bool
ARDOUR_UI_UTILS::key_is_legal_for_numeric_entry (guint keyval)
{
	/* we assume that this does not change over the life of the process
	 */

	static int comma_decimal = -1;

	switch (keyval) {
	case GDK_period:
	case GDK_comma:
		if (comma_decimal < 0) {
			std::lconv* lc = std::localeconv();
			if (strchr (lc->decimal_point, ',') != 0) {
				comma_decimal = 1;
			} else {
				comma_decimal = 0;
			}
		}
		break;
	default:
		break;
	}

	switch (keyval) {
	case GDK_decimalpoint:
	case GDK_KP_Separator:
		return true;

	case GDK_period:
		if (comma_decimal) {
			return false;
		} else {
			return true;
		}
		break;
	case GDK_comma:
		if (comma_decimal) {
			return true;
		} else {
			return false;
		}
		break;
	case GDK_minus:
	case GDK_plus:
	case GDK_0:
	case GDK_1:
	case GDK_2:
	case GDK_3:
	case GDK_4:
	case GDK_5:
	case GDK_6:
	case GDK_7:
	case GDK_8:
	case GDK_9:
	case GDK_KP_Add:
	case GDK_KP_Subtract:
	case GDK_KP_Decimal:
	case GDK_KP_0:
	case GDK_KP_1:
	case GDK_KP_2:
	case GDK_KP_3:
	case GDK_KP_4:
	case GDK_KP_5:
	case GDK_KP_6:
	case GDK_KP_7:
	case GDK_KP_8:
	case GDK_KP_9:
	case GDK_Return:
	case GDK_BackSpace:
	case GDK_Delete:
	case GDK_KP_Enter:
	case GDK_Home:
	case GDK_End:
	case GDK_Left:
	case GDK_Right:
		return true;

	default:
		break;
	}

	return false;
}

void
ARDOUR_UI_UTILS::resize_window_to_proportion_of_monitor (Gtk::Window* window, int max_width, int max_height)
{
	Glib::RefPtr<Gdk::Screen> screen = window->get_screen ();
	Gdk::Rectangle monitor_rect;
	screen->get_monitor_geometry (0, monitor_rect);

	int const w = std::min (int (monitor_rect.get_width() * 0.8), max_width);
	int const h = std::min (int (monitor_rect.get_height() * 0.8), max_height);

	window->resize (w, h);
}


/** Replace _ with __ in a string; for use with menu item text to make underscores displayed correctly */
string
ARDOUR_UI_UTILS::escape_underscores (string const & s)
{
	string o;
	string::size_type const N = s.length ();

	for (string::size_type i = 0; i < N; ++i) {
		if (s[i] == '_') {
			o += "__";
		} else {
			o += s[i];
		}
	}

	return o;
}

/** Replace < and > with &lt; and &gt; respectively to make < > display correctly in markup strings */
string
ARDOUR_UI_UTILS::escape_angled_brackets (string const & s)
{
	string o = s;
	boost::replace_all (o, "<", "&lt;");
	boost::replace_all (o, ">", "&gt;");
	return o;
}

Gdk::Color
ARDOUR_UI_UTILS::unique_random_color (list<Gdk::Color>& used_colors)
{
  	Gdk::Color newcolor;

	while (1) {

		double h, s, v;

		h = fmod (random(), 360.0);
		s = (random() % 65535) / 65535.0;
		v = (random() % 65535) / 65535.0;

		s = min (0.5, s); /* not too saturated */
		v = max (0.9, v);  /* not too bright */
		newcolor.set_hsv (h, s, v);

		if (used_colors.size() == 0) {
			used_colors.push_back (newcolor);
			return newcolor;
		}

		for (list<Gdk::Color>::iterator i = used_colors.begin(); i != used_colors.end(); ++i) {
		  Gdk::Color c = *i;
			float rdelta, bdelta, gdelta;

			rdelta = newcolor.get_red() - c.get_red();
			bdelta = newcolor.get_blue() - c.get_blue();
			gdelta = newcolor.get_green() - c.get_green();

			if (sqrt (rdelta*rdelta + bdelta*bdelta + gdelta*gdelta) > 25.0) {
				/* different enough */
				used_colors.push_back (newcolor);
				return newcolor;
			}
		}

		/* XXX need throttle here to make sure we don't spin for ever */
	}
}

string
ARDOUR_UI_UTILS::rate_as_string (float r)
{
	char buf[32];
	if (fmod (r, 1000.0f)) {
		snprintf (buf, sizeof (buf), "%.1f kHz", r/1000.0);
	} else {
		snprintf (buf, sizeof (buf), "%.0f kHz", r/1000.0);
	}
	return buf;
}

bool
ARDOUR_UI_UTILS::windows_overlap (Gtk::Window *a, Gtk::Window *b)
{

	if (!a || !b) {
		return false;
	}
	if (a->get_screen() == b->get_screen()) {
		gint ex, ey, ew, eh;
		gint mx, my, mw, mh;

		a->get_position (ex, ey);
		a->get_size (ew, eh);
		b->get_position (mx, my);
		b->get_size (mw, mh);

		GdkRectangle e;
		GdkRectangle m;
		GdkRectangle r;

		e.x = ex;
		e.y = ey;
		e.width = ew;
		e.height = eh;

		m.x = mx;
		m.y = my;
		m.width = mw;
		m.height = mh;

		if (gdk_rectangle_intersect (&e, &m, &r)) {
			return true;
		}
	}
	return false;
}

bool
ARDOUR_UI_UTILS::overwrite_file_dialog (Gtk::Window& parent, string title, string text)
{
	ArdourDialog dialog (parent, title, true);
	Label label (text);

	dialog.get_vbox()->pack_start (label, true, true);
	dialog.add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	dialog.add_button (_("Overwrite"), Gtk::RESPONSE_ACCEPT);
	dialog.show_all ();

	switch (dialog.run()) {
	case RESPONSE_ACCEPT:
		return true;
	case RESPONSE_CANCEL:
	default:
		return false;
	}
}
