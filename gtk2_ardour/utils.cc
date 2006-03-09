/*
    Copyright (C) 2003 Paul Davis 

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

    $Id$
*/

#include <cstdlib>
#include <cctype>
#include <libart_lgpl/art_misc.h>
#include <gtkmm/window.h>
#include <gtkmm/combo.h>
#include <gtkmm/label.h>
#include <gtkmm/paned.h>
#include <gtk/gtkpaned.h>

#include <gtkmm2ext/utils.h>

#include "ardour_ui.h"
#include "keyboard.h"
#include "utils.h"
#include "i18n.h"
#include "rgb_macros.h"
#include "canvas_impl.h"

using namespace std;
using namespace Gtk;
using namespace sigc;
using namespace Glib;

string
short_version (string orig, string::size_type target_length)
{
	/* this tries to create a recognizable abbreviation
	   of "orig" by removing characters until we meet
	   a certain target length.

	   note that we deliberately leave digits in the result
	   without modification.
	*/


	string::size_type pos;

	/* remove white-space and punctuation, starting at end */

	while (orig.length() > target_length) {
		if ((pos = orig.find_last_of (_("\"\n\t ,<.>/?:;'[{}]~`!@#$%^&*()_-+="))) == string::npos) {
			break;
		}
		orig.replace (pos, 1, "");
	}

	/* remove lower-case vowels, starting at end */

	while (orig.length() > target_length) {
		if ((pos = orig.find_last_of (_("aeiou"))) == string::npos) {
			break;
		}
		orig.replace (pos, 1, "");
	}

	/* remove upper-case vowels, starting at end */

	while (orig.length() > target_length) {
		if ((pos = orig.find_last_of (_("AEIOU"))) == string::npos) {
			break;
		}
		orig.replace (pos, 1, "");
	}

	/* remove lower-case consonants, starting at end */

	while (orig.length() > target_length) {
		if ((pos = orig.find_last_of (_("bcdfghjklmnpqrtvwxyz"))) == string::npos) {
			break;
		}
		orig.replace (pos, 1, "");
	}

	/* remove upper-case consonants, starting at end */

	while (orig.length() > target_length) {
		if ((pos = orig.find_last_of (_("BCDFGHJKLMNPQRTVWXYZ"))) == string::npos) {
			break;
		}
		orig.replace (pos, 1, "");
	}

	/* whatever the length is now, use it */
	
	return orig;
}

string
fit_to_pixels (const string & str, int pixel_width, const string & font)
{
	Label foo;
	int width;
	int height;
	Pango::FontDescription fontdesc (font);
	
	int namelen = str.length();
	char cstr[namelen+1];
	strcpy (cstr, str.c_str());
	
	while (namelen) {
		Glib::RefPtr<Pango::Layout> layout = foo.create_pango_layout (cstr);

		layout->set_font_description (fontdesc);
		layout->get_pixel_size (width, height);

		if (width < (pixel_width)) {
			break;
		}

		--namelen;
		cstr[namelen] = '\0';

	}

	return cstr;
}

int
atoi (const string& s)
{
	return atoi (s.c_str());
}

double
atof (const string& s)
{
	return atof (s.c_str());
}

vector<string>
internationalize (const char **array)
{
	vector<string> v;

	for (uint32_t i = 0; array[i]; ++i) {
		v.push_back (_(array[i]));
	}

	return v;
}

gint
just_hide_it (GdkEventAny *ev, Gtk::Window *win)
{
	win->hide_all ();
	return TRUE;
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

	savergb = rgb = (unsigned char*)art_alloc (h * w * 3);
	
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

	savergb = rgb = (unsigned char*)art_alloc (h * w * 4);
	
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
get_canvas_points (string who, uint32_t npoints)
{
	// cerr << who << ": wants " << npoints << " canvas points" << endl;
#ifdef TRAP_EXCESSIVE_POINT_REQUESTS
	if (npoints > (uint32_t) gdk_screen_width() + 4) {
		abort ();
	}
#endif
	return new ArdourCanvas::Points (npoints);
}

static int32_t 
int_from_hex (char hic, char loc) 
{
	int hi;		/* hi byte */
	int lo;		/* low byte */

	hi = (int) hic;

	if( ('0'<=hi) && (hi<='9') ) {
		hi -= '0';
	} else if( ('a'<= hi) && (hi<= 'f') ) {
		hi -= ('a'-10);
	} else if( ('A'<=hi) && (hi<='F') ) {
		hi -= ('A'-10);
	}
	
	lo = (int) loc;
	
	if( ('0'<=lo) && (lo<='9') ) {
		lo -= '0';
	} else if( ('a'<=lo) && (lo<='f') ) {
		lo -= ('a'-10);
	} else if( ('A'<=lo) && (lo<='F') ) {
		lo -= ('A'-10);
	}

	return lo + (16 * hi);
}

void
url_decode (string& url)
{
	string::iterator last;
	string::iterator next;

	for (string::iterator i = url.begin(); i != url.end(); ++i) {
		if ((*i) == '+') {
			*i = ' ';
		}
	}

	if (url.length() <= 3) {
		return;
	}

	last = url.end();

	--last; /* points at last char */
	--last; /* points at last char - 1 */

	for (string::iterator i = url.begin(); i != last; ) {

		if (*i == '%') {

			next = i;

			url.erase (i);
			
			i = next;
			++next;
			
			if (isxdigit (*i) && isxdigit (*next)) {
				/* replace first digit with char */
				*i = int_from_hex (*i,*next);
				++i; /* points at 2nd of 2 digits */
				url.erase (i);
			}
		} else {
			++i;
		}
	}
}

Pango::FontDescription
get_font_for_style (string widgetname)
{
	Gtk::Window window (WINDOW_TOPLEVEL);
	Gtk::Label foobar;
	Glib::RefPtr<Style> style;

	window.add (foobar);
	foobar.set_name (widgetname);
	foobar.ensure_style();

	style = foobar.get_style ();
	return style->get_font();
}

gint
pane_handler (GdkEventButton* ev, Gtk::Paned* pane)
{
	if (ev->window != Gtkmm2ext::get_paned_handle (*pane)) {
		return FALSE;
	}

	if (Keyboard::is_delete_event (ev)) {

		gint pos;
		gint cmp;
		
		pos = pane->get_position ();

		if (dynamic_cast<VPaned*>(pane)) {
			cmp = pane->get_height();
		} else {
			cmp = pane->get_width();
		}

		/* we have to use approximations here because we can't predict the
		   exact position or sizes of the pane (themes, etc)
		*/

		if (pos < 10 || abs (pos - cmp) < 10) {

			/* already collapsed: restore it (note that this is cast from a pointer value to int, which is tricky on 64bit */
			
			pane->set_position ((intptr_t) pane->get_data ("rpos"));

		} else {	

			int collapse_direction;

			/* store the current position */

			pane->set_data ("rpos", (gpointer) pos);

			/* collapse to show the relevant child in full */
			
			collapse_direction = (intptr_t) pane->get_data ("collapse-direction");

			if (collapse_direction) {
				pane->set_position (1);
			} else {
				if (dynamic_cast<VPaned*>(pane)) {
					pane->set_position (pane->get_height());
				} else {
					pane->set_position (pane->get_width());
				}
			}
		}

		return TRUE;
	} 

	return FALSE;
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
	
	GtkRcStyle* waverc = foo.get_style()->gobj()->rc_style;

	if (waverc) {
		if (attr == "fg") {
			r = waverc->fg[state].red / 257;
			g = waverc->fg[state].green / 257;
			b = waverc->fg[state].blue / 257;
			/* what a hack ... "a" is for "active" */
			if (state == Gtk::STATE_NORMAL && rgba) {
				a = waverc->fg[GTK_STATE_ACTIVE].red / 257;
			}
		} else if (attr == "bg") {
			r = g = b = 0;
			r = waverc->bg[state].red / 257;
			g = waverc->bg[state].green / 257;
			b = waverc->bg[state].blue / 257;
		} else if (attr == "base") {
			r = waverc->base[state].red / 257;
			g = waverc->base[state].green / 257;
			b = waverc->base[state].blue / 257;
		} else if (attr == "text") {
			r = waverc->text[state].red / 257;
			g = waverc->text[state].green / 257;
			b = waverc->text[state].blue / 257;
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
key_press_focus_accelerator_handler (Gtk::Window& window, GdkEventKey* ev)
{
	GtkWindow* win = window.gobj();

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

	if (ev->state & ~Gdk::SHIFT_MASK) {
		/* modifiers in effect, accelerate first */
		if (!gtk_window_activate_key (win, ev)) {
			return gtk_window_propagate_key_event (win, ev);
		} else {
			return true;
		} 
	}
	
	/* no modifiers, propagate first */

	if (!gtk_window_propagate_key_event (win, ev)) {
		return gtk_window_activate_key (win, ev);
	} 


	return true;
}

