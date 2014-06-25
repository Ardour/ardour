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
#include <cmath>
#include <fstream>
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
#include "ardour/rc_configuration.h"
#include "ardour/filesystem_paths.h"

#include "canvas/item.h"
#include "canvas/utils.h"

#include "ardour_ui.h"
#include "debug.h"
#include "public_editor.h"
#include "keyboard.h"
#include "utils.h"
#include "i18n.h"
#include "rgb_macros.h"
#include "gui_thread.h"

using namespace std;
using namespace Gtk;
using namespace Glib;
using namespace PBD;
using Gtkmm2ext::Keyboard;

sigc::signal<void>  DPIReset;

#ifdef PLATFORM_WINDOWS
#define random() rand()
#endif


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
sanitized_font (std::string const& name)
{
	Pango::FontDescription fd (name);

	if (fd.get_family().empty()) {
		fd.set_family ("Sans");
	}

	return fd;
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
		return (uint32_t) RGBA_TO_UINT(r,g,b,255);
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

void
set_color_from_rgb (Gdk::Color& c, uint32_t rgb)
{
	/* Gdk::Color color ranges are 16 bit, so scale from 8 bit by
	   multiplying by 256.
	*/
	c.set_rgb ((rgb >> 16)*256, ((rgb & 0xff00) >> 8)*256, (rgb & 0xff)*256);
}

void
set_color_from_rgba (Gdk::Color& c, uint32_t rgba)
{
	/* Gdk::Color color ranges are 16 bit, so scale from 8 bit by
	   multiplying by 256.
	*/
	c.set_rgb ((rgba >> 24)*256, ((rgba & 0xff0000) >> 16)*256, ((rgba & 0xff00) >> 8)*256);
}

uint32_t
gdk_color_to_rgba (Gdk::Color const& c)
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

uint32_t
contrasting_text_color (uint32_t c)
{
	double r, g, b, a;
	ArdourCanvas::color_to_rgba (c, r, g, b, a);

	const double black_r = 0.0;
	const double black_g = 0.0;
	const double black_b = 0.0;

	const double white_r = 1.0;
	const double white_g = 1.0;
	const double white_b = 1.0;

	/* Use W3C contrast guideline calculation */

	double white_contrast = (max (r, white_r) - min (r, white_r)) +
		(max (g, white_g) - min (g, white_g)) + 
		(max (b, white_b) - min (b, white_b));

	double black_contrast = (max (r, black_r) - min (r, black_r)) +
		(max (g, black_g) - min (g, black_g)) + 
		(max (b, black_b) - min (b, black_b));

	if (white_contrast > black_contrast) {		
		/* use white */
		return ArdourCanvas::rgba_to_color (1.0, 1.0, 1.0, 1.0);
	} else {
		/* use black */
		return ArdourCanvas::rgba_to_color (0.0, 0.0, 0.0, 1.0);
	}
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
	ev.string = const_cast<gchar*> ("");
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
get_icon_sets ()
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
get_icon_path (const char* cname, string icon_set)
{
	std::string data_file_path;
	string name = cname;
	name += X_(".png");

	Searchpath spath(ARDOUR::ardour_data_search_path());

	if (!icon_set.empty() && icon_set != _("default")) {

		/* add "icons/icon_set" but .. not allowed to add both of these at once */
		spath.add_subdirectory_to_paths ("icons");
		spath.add_subdirectory_to_paths (icon_set);
		
		find_file (spath, name, data_file_path);
	}
	
	if (data_file_path.empty()) {
		
		if (!icon_set.empty() && icon_set != _("default")) {
			warning << string_compose (_("icon \"%1\" not found for icon set \"%2\", fallback to default"), cname, icon_set) << endmsg;
		}
		
		Searchpath def (ARDOUR::ardour_data_search_path());
		def.add_subdirectory_to_paths ("icons");
	
		if (!find_file (def, name, data_file_path)) {
			fatal << string_compose (_("cannot find icon image for %1 using %2"), name, spath.to_string()) << endmsg;
			/*NOTREACHED*/
		}
	}

	return data_file_path;
}

Glib::RefPtr<Gdk::Pixbuf>
get_icon (const char* cname, string icon_set)
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

#ifndef PLATFORM_WINDOWS
	pango_ft2_font_map_set_resolution ((PangoFT2FontMap*) pango_ft2_font_map_new(), val/1024, val/1024);
#endif

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
rate_as_string (float r)
{
	char buf[32];
	if (fmod (r, 1000.0f)) {
		snprintf (buf, sizeof (buf), "%.1f kHz", r/1000.0);
	} else {
		snprintf (buf, sizeof (buf), "%.0f kHz", r/1000.0);
	}
	return buf;
}
