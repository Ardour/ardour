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

#include <pango/pangoft2.h> // for fontmap resolution control for GnomeCanvas
#include <pango/pangocairo.h> // for fontmap resolution control for GnomeCanvas

#include <cstdlib>
#include <clocale>
#include <cstring>
#include <cctype>
#include <fstream>
#include <list>
#include <sys/stat.h>
#include <libart_lgpl/art_misc.h>
#include <gtkmm/rc.h>
#include <gtkmm/window.h>
#include <gtkmm/combo.h>
#include <gtkmm/label.h>
#include <gtkmm/paned.h>
#include <gtk/gtkpaned.h>
#include <boost/algorithm/string.hpp>

#include "pbd/file_utils.h"

#include <gtkmm2ext/utils.h>
#include "ardour/rc_configuration.h"

#include "ardour/filesystem_paths.h"

#include "ardour_ui.h"
#include "debug.h"
#include "public_editor.h"
#include "keyboard.h"
#include "utils.h"
#include "i18n.h"
#include "rgb_macros.h"
#include "canvas_impl.h"
#include "gui_thread.h"

using namespace std;
using namespace Gtk;
using namespace Glib;
using namespace PBD;
using Gtkmm2ext::Keyboard;

sigc::signal<void>  DPIReset;


/** Add an element to a menu, settings its sensitivity.
 * @param m Menu to add to.
 * @param e Element to add.
 * @param s true to make sensitive, false to make insensitive
 */
void
add_item_with_sensitivity (Menu_Helpers::MenuList& m, Menu_Helpers::MenuElem e, bool s)
{
	m.push_back (e);
	if (!s) {
		m.back().set_sensitive (false);
	}
}


gint
just_hide_it (GdkEventAny */*ev*/, Gtk::Window *win)
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
xpm2rgb (const char** xpm, uint32_t& w, uint32_t& h)
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
xpm2rgba (const char** xpm, uint32_t& w, uint32_t& h)
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

ArdourCanvas::Points*
get_canvas_points (string /*who*/, uint32_t npoints)
{
	// cerr << who << ": wants " << npoints << " canvas points" << endl;
#ifdef TRAP_EXCESSIVE_POINT_REQUESTS
	if (npoints > (uint32_t) gdk_screen_width() + 4) {
		abort ();
	}
#endif
	return new ArdourCanvas::Points (npoints);
}

Pango::FontDescription
get_font_for_style (string widgetname)
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

uint32_t
rgba_from_style (string style, uint32_t r, uint32_t g, uint32_t b, uint32_t a, string attr, int state, bool rgba)
{
	/* In GTK+2, styles aren't set up correctly if the widget is not
	   attached to a toplevel window that has a screen pointer.
	*/

	static Gtk::Window* window = 0;

	if (window == 0) {
		window = new Window (WINDOW_TOPLEVEL);
	}

	Gtk::Label foo;

	window->add (foo);

	foo.set_name (style);
	foo.ensure_style ();

	GtkRcStyle* rc = foo.get_style()->gobj()->rc_style;

	if (rc) {
		if (attr == "fg") {
			r = rc->fg[state].red / 257;
			g = rc->fg[state].green / 257;
			b = rc->fg[state].blue / 257;

			/* what a hack ... "a" is for "active" */
			if (state == Gtk::STATE_NORMAL && rgba) {
				a = rc->fg[GTK_STATE_ACTIVE].red / 257;
			}
		} else if (attr == "bg") {
			r = g = b = 0;
			r = rc->bg[state].red / 257;
			g = rc->bg[state].green / 257;
			b = rc->bg[state].blue / 257;
		} else if (attr == "base") {
			r = rc->base[state].red / 257;
			g = rc->base[state].green / 257;
			b = rc->base[state].blue / 257;
		} else if (attr == "text") {
			r = rc->text[state].red / 257;
			g = rc->text[state].green / 257;
			b = rc->text[state].blue / 257;
		}
	} else {
		warning << string_compose (_("missing RGBA style for \"%1\""), style) << endl;
	}

	window->remove ();

	if (state == Gtk::STATE_NORMAL && rgba) {
		return (uint32_t) RGBA_TO_UINT(r,g,b,a);
	} else {
		return (uint32_t) RGB_TO_UINT(r,g,b);
	}
}

bool
rgba_p_from_style (string style, float *r, float *g, float *b, string attr, int state)
{
	static Gtk::Window* window = 0;
	assert (r && g && b);

	if (window == 0) {
		window = new Window (WINDOW_TOPLEVEL);
	}

	Gtk::EventBox foo;

	window->add (foo);

	foo.set_name (style);
	foo.ensure_style ();

	GtkRcStyle* rc = foo.get_style()->gobj()->rc_style;

	if (!rc) {
		warning << string_compose (_("missing RGBA style for \"%1\""), style) << endl;
		return false;
	}
	if (attr == "fg") {
		*r = rc->fg[state].red / 65535.0;
		*g = rc->fg[state].green / 65535.0;
		*b = rc->fg[state].blue / 65535.0;
	} else if (attr == "bg") {
		*r = rc->bg[state].red / 65535.0;
		*g = rc->bg[state].green / 65535.0;
		*b = rc->bg[state].blue / 65535.0;
	} else if (attr == "base") {
		*r = rc->base[state].red / 65535.0;
		*g = rc->base[state].green / 65535.0;
		*b = rc->base[state].blue / 65535.0;
	} else if (attr == "text") {
		*r = rc->text[state].red / 65535.0;
		*g = rc->text[state].green / 65535.0;
		*b = rc->text[state].blue / 65535.0;
	} else {
		return false;
	}

	window->remove ();
	return true;
}

bool
canvas_item_visible (ArdourCanvas::Item* item)
{
	return (item->gobj()->object.flags & GNOME_CANVAS_ITEM_VISIBLE) ? true : false;
}

void
set_color (Gdk::Color& c, int rgb)
{
	c.set_rgb((rgb >> 16)*256, ((rgb & 0xff00) >> 8)*256, (rgb & 0xff)*256);
}

bool
relay_key_press (GdkEventKey* ev, Gtk::Window* win)
{
	PublicEditor& ed (PublicEditor::instance());

	if (!key_press_focus_accelerator_handler (*win, ev)) {
		if (&ed == 0) {
			/* early key press in pre-main-window-dialogs, no editor yet */
			return false;
		}
		return ed.on_key_press_event(ev);
	} else {
		return true;
	}
}

bool
forward_key_press (GdkEventKey* ev)
{
        return PublicEditor::instance().on_key_press_event(ev);
}

bool
emulate_key_event (Gtk::Widget* w, unsigned int keyval)
{
	GdkDisplay  *display = gtk_widget_get_display (GTK_WIDGET(w->gobj()));
	GdkKeymap   *keymap  = gdk_keymap_get_for_display (display);
	GdkKeymapKey *keymapkey = NULL;
	gint n_keys;

	if (!gdk_keymap_get_entries_for_keyval(keymap, keyval, &keymapkey, &n_keys)) return false;
	if (n_keys !=1) { g_free(keymapkey); return false;}

	GdkEventKey ev;
	ev.type = GDK_KEY_PRESS;
	ev.window = gtk_widget_get_window(GTK_WIDGET(w->gobj()));
	ev.send_event = FALSE;
	ev.time = 0;
	ev.state = 0;
	ev.keyval = keyval;
	ev.length = 0;
	ev.string = (const gchar*) "";
	ev.hardware_keycode = keymapkey[0].keycode;
	ev.group = keymapkey[0].group;
	g_free(keymapkey);

	forward_key_press(&ev);
	ev.type = GDK_KEY_RELEASE;
	return forward_key_press(&ev);
}

bool
key_press_focus_accelerator_handler (Gtk::Window& window, GdkEventKey* ev)
{
	GtkWindow* win = window.gobj();
	GtkWidget* focus = gtk_window_get_focus (win);
	bool special_handling_of_unmodified_accelerators = false;
	bool allow_activating = true;
	/* consider all relevant modifiers but not LOCK or SHIFT */
	const guint mask = (Keyboard::RelevantModifierKeyMask & ~(Gdk::SHIFT_MASK|Gdk::LOCK_MASK));

	if (focus) {
		if (GTK_IS_ENTRY(focus) || Keyboard::some_magic_widget_has_focus()) {
			special_handling_of_unmodified_accelerators = true;
		}
	}

#ifdef GTKOSX
        /* at one time this appeared to be necessary. As of July 2012, it does not
           appear to be. if it ever is necessar, figure out if it should apply
           to all platforms.
        */
#if 0 
	if (Keyboard::some_magic_widget_has_focus ()) {
                allow_activating = false;
	}
#endif
#endif


        DEBUG_TRACE (DEBUG::Accelerators, string_compose ("Win = %1 focus = %7 Key event: code = %2  state = %3 special handling ? %4 magic widget focus ? %5 allow_activation ? %6\n",
                                                          win,
                                                          ev->keyval,
                                                          ev->state,
                                                          special_handling_of_unmodified_accelerators,
                                                          Keyboard::some_magic_widget_has_focus(),
                                                          allow_activating,
							  focus));

	/* This exists to allow us to override the way GTK handles
	   key events. The normal sequence is:

	   a) event is delivered to a GtkWindow
	   b) accelerators/mnemonics are activated
	   c) if (b) didn't handle the event, propagate to
	       the focus widget and/or focus chain

	   The problem with this is that if the accelerators include
	   keys without modifiers, such as the space bar or the
	   letter "e", then pressing the key while typing into
	   a text entry widget results in the accelerator being
	   activated, instead of the desired letter appearing
	   in the text entry.

	   There is no good way of fixing this, but this
	   represents a compromise. The idea is that
	   key events involving modifiers (not Shift)
	   get routed into the activation pathway first, then
	   get propagated to the focus widget if necessary.

	   If the key event doesn't involve modifiers,
	   we deliver to the focus widget first, thus allowing
	   it to get "normal text" without interference
	   from acceleration.

	   Of course, this can also be problematic: if there
	   is a widget with focus, then it will swallow
	   all "normal text" accelerators.
	*/

	if (!special_handling_of_unmodified_accelerators) {

		/* XXX note that for a brief moment, the conditional above
		 * included "|| (ev->state & mask)" so as to enforce the
		 * implication of special_handling_of_UNMODIFIED_accelerators.
		 * however, this forces any key that GTK doesn't allow and that
		 * we have an alternative (see next comment) for to be
		 * automatically sent through the accel groups activation
		 * pathway, which prevents individual widgets & canvas items
		 * from ever seeing it if is used by a key binding.
		 * 
		 * specifically, this hid Ctrl-down-arrow from MIDI region
		 * views because it is also bound to an action.
		 *
		 * until we have a robust, clean binding system, this
		 * quirk will have to remain in place.
		 */

		/* pretend that certain key events that GTK does not allow
		   to be used as accelerators are actually something that
		   it does allow. but only where there are no modifiers.
		*/

		uint32_t fakekey = ev->keyval;

		if (Gtkmm2ext::possibly_translate_keyval_to_make_legal_accelerator (fakekey)) {
			DEBUG_TRACE (DEBUG::Accelerators, string_compose ("\tactivate (was %1 now %2) without special hanlding of unmodified accels\n",
									  ev->keyval, fakekey));

			GdkModifierType mod = GdkModifierType (ev->state);

			mod = GdkModifierType (mod & gtk_accelerator_get_default_mod_mask());
#ifdef GTKOSX
			/* GTK on OS X is currently (February 2012) setting both
			   the Meta and Mod2 bits in the event modifier state if 
			   the Command key is down.

			   gtk_accel_groups_activate() does not invoke any of the logic
			   that gtk_window_activate_key() will that sorts out that stupid
			   state of affairs, and as a result it fails to find a match
			   for the key event and the current set of accelerators.

			   to fix this, if the meta bit is set, remove the mod2 bit
			   from the modifier. this assumes that our bindings use Primary
			   which will have set the meta bit in the accelerator entry.
			*/
			if (mod & GDK_META_MASK) {
				mod = GdkModifierType (mod & ~GDK_MOD2_MASK);
			}
#endif

			if (allow_activating && gtk_accel_groups_activate(G_OBJECT(win), fakekey, mod)) {
				DEBUG_TRACE (DEBUG::Accelerators, "\taccel group activated by fakekey\n");
				return true;
			}
		}
	}

	if (!special_handling_of_unmodified_accelerators || (ev->state & mask)) {

		/* no special handling or there are modifiers in effect: accelerate first */

                DEBUG_TRACE (DEBUG::Accelerators, "\tactivate, then propagate\n");
		DEBUG_TRACE (DEBUG::Accelerators, string_compose ("\tevent send-event:%1 time:%2 length:%3 string:%4 hardware_keycode:%5 group:%6\n",
					ev->send_event, ev->time, ev->length, ev->string, ev->hardware_keycode, ev->group));

		if (allow_activating) {
			DEBUG_TRACE (DEBUG::Accelerators, "\tsending to window\n");
			if (gtk_window_activate_key (win, ev)) {
				DEBUG_TRACE (DEBUG::Accelerators, "\t\thandled\n");
				return true;
			}
		} else {
			DEBUG_TRACE (DEBUG::Accelerators, "\tactivation skipped\n");
		}

                DEBUG_TRACE (DEBUG::Accelerators, "\tnot accelerated, now propagate\n");

		return gtk_window_propagate_key_event (win, ev);
	}

	/* no modifiers, propagate first */

        DEBUG_TRACE (DEBUG::Accelerators, "\tpropagate, then activate\n");

	if (!gtk_window_propagate_key_event (win, ev)) {
                DEBUG_TRACE (DEBUG::Accelerators, "\tpropagation didn't handle, so activate\n");
		if (allow_activating) {
			return gtk_window_activate_key (win, ev);
		} else {
			DEBUG_TRACE (DEBUG::Accelerators, "\tactivation skipped\n");
		}

	} else {
                DEBUG_TRACE (DEBUG::Accelerators, "\thandled by propagate\n");
		return true;
	}

        DEBUG_TRACE (DEBUG::Accelerators, "\tnot handled\n");
	return true;
}

Glib::RefPtr<Gdk::Pixbuf>
get_xpm (std::string name)
{
	if (!xpm_map[name]) {

		SearchPath spath(ARDOUR::ardour_data_search_path());

		spath.add_subdirectory_to_paths("pixmaps");

		std::string data_file_path;

		if(!find_file_in_search_path (spath, name, data_file_path)) {
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

std::string
get_icon_path (const char* cname)
{
	string name = cname;
	name += X_(".png");

	SearchPath spath(ARDOUR::ardour_data_search_path());

	spath.add_subdirectory_to_paths("icons");

	std::string data_file_path;

	if (!find_file_in_search_path (spath, name, data_file_path)) {
		fatal << string_compose (_("cannot find icon image for %1 using %2"), name, spath.to_string()) << endmsg;
	}

	return data_file_path;
}

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

string
longest (vector<string>& strings)
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
key_is_legal_for_numeric_entry (guint keyval)
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
set_pango_fontsize ()
{
	long val = ARDOUR::Config->get_font_scale();

	/* FT2 rendering - used by GnomeCanvas, sigh */

	pango_ft2_font_map_set_resolution ((PangoFT2FontMap*) pango_ft2_font_map_new(), val/1024, val/1024);

	/* Cairo rendering, in case there is any */

	pango_cairo_font_map_set_resolution ((PangoCairoFontMap*) pango_cairo_font_map_get_default(), val/1024);
}

void
reset_dpi ()
{
	long val = ARDOUR::Config->get_font_scale();
	set_pango_fontsize ();
	/* Xft rendering */

	gtk_settings_set_long_property (gtk_settings_get_default(),
					"gtk-xft-dpi", val, "ardour");
	DPIReset();//Emit Signal
}

void
resize_window_to_proportion_of_monitor (Gtk::Window* window, int max_width, int max_height)
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
escape_underscores (string const & s)
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
escape_angled_brackets (string const & s)
{
	string o = s;
	boost::replace_all (o, "<", "&lt;");
	boost::replace_all (o, ">", "&gt;");
	return o;
}

Gdk::Color
unique_random_color (list<Gdk::Color>& used_colors)
{
  	Gdk::Color newcolor;

	while (1) {

		/* avoid neon/glowing tones by limiting them to the
		   "inner section" (paler) of a color wheel/circle.
		*/

		const int32_t max_saturation = 48000; // 65535 would open up the whole color wheel

		newcolor.set_red (random() % max_saturation);
		newcolor.set_blue (random() % max_saturation);
		newcolor.set_green (random() % max_saturation);

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
				used_colors.push_back (newcolor);
				return newcolor;
			}
		}

		/* XXX need throttle here to make sure we don't spin for ever */
	}
}
