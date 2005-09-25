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
#include <gtk--/window.h>
#include <gtk--/combo.h>
#include <gtk--/label.h>
#include <gtk--/paned.h>
#include <gtk/gtkpaned.h>

#include <gtkmmext/utils.h>

#include "ardour_ui.h"
#include "utils.h"
#include "i18n.h"
#include "rgb_macros.h"

using namespace std;
using namespace Gtk;

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
fit_to_pixels (string str, int32_t pixel_width, Gdk_Font& font)
{
	gint width;
	gint lbearing;
	gint rbearing;
	gint ascent;
	gint descent;

	int namelen = str.length();
	char cstr[namelen+1];
	strcpy (cstr, str.c_str());
	
	while (namelen) {
		
		gdk_string_extents (font,
				    cstr,
				    &lbearing,
				    &rbearing,
				    &width,
				    &ascent,
				    &descent);

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

void
strip_whitespace_edges (string& str)
{
	string::size_type i;
	string::size_type len;
	string::size_type s;

	len = str.length();

	for (i = 0; i < len; ++i) {
		if (isgraph (str[i])) {
			break;
		}
	}

	s = i;

	for (i = len - 1; i >= 0; --i) {
		if (isgraph (str[i])) {
			break;
		}
	}

	str = str.substr (s, (i - s) + 1);
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
	ARDOUR_UI::instance()->allow_focus (false);
	win->hide_all ();
	return TRUE;
}

void
allow_keyboard_focus (bool yn)
{
	ARDOUR_UI::instance()->allow_focus (yn);
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
		error << compose (_("bad XPM header %1"), xpm[0])
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
		error << compose (_("bad XPM header %1"), xpm[0])
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

GtkCanvasPoints*
get_canvas_points (string who, uint32_t npoints)
{
	// cerr << who << ": wants " << npoints << " canvas points" << endl;
#ifdef TRAP_EXCESSIVE_POINT_REQUESTS
	if (npoints > (uint32_t) gdk_screen_width() + 4) {
		abort ();
	}
#endif
	return gtk_canvas_points_new (npoints);
}

int
channel_combo_get_channel_count (Gtk::Combo& combo)
{
	string str = combo.get_entry()->get_text();
	int chns;

	if (str == _("mono")) {
		return 1;
	} else if (str == _("stereo")) {
		return 2;
	} else if ((chns = atoi (str)) != 0) {
		return chns;
	} else {
		return 0;
	}
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

string
get_font_for_style (string widgetname)
{
	Gtk::Label foobar;

	foobar.set_name (widgetname);
	foobar.ensure_style();

	if (foobar.get_style() == 0 || foobar.get_style()->gtkobj()->rc_style == 0 || foobar.get_style()->gtkobj()->rc_style->font_name == 0) {
		return "fixed";
	}

	string str = foobar.get_style()->gtkobj()->rc_style->font_name;

	if (str.empty()) {
		return "fixed"; // standard X Window fallback font
	} else {
		return str;
	}
}
gint
pane_handler (GdkEventButton* ev, Gtk::Paned* pane)
{
	if (ev->window != Gtkmmext::get_paned_handle (*pane)) {
		return FALSE;
	}

	if (Keyboard::is_delete_event (ev)) {

		gint pos;
		gint cmp;

		pos = Gtkmmext::gtk_paned_get_position (pane->gtkobj());

		if (dynamic_cast<VPaned*>(pane)) {
			cmp = pane->height();
		} else {
			cmp = pane->width();
		}

		/* we have to use approximations here because we can't predict the
		   exact position or sizes of the pane (themes, etc)
		*/

		if (pos < 10 || abs (pos - cmp) < 10) {

			/* already collapsed: restore it (note that this is cast from a pointer value to int, which is tricky on 64bit */
			
			pane->set_position ((gint64) pane->get_data ("rpos"));

		} else {	

			int collapse_direction;

			/* store the current position */

			pane->set_data ("rpos", (gpointer) pos);

			/* collapse to show the relevant child in full */
			
			collapse_direction = (gint64) pane->get_data ("collapse-direction");

			if (collapse_direction) {
				pane->set_position (1);
			} else {
				if (dynamic_cast<VPaned*>(pane)) {
					pane->set_position (pane->height());
				} else {
					pane->set_position (pane->width());
				}
			}
		}

		return TRUE;
	} 

	return FALSE;
}
uint32_t
rgba_from_style (string style, uint32_t r, uint32_t g, uint32_t b, uint32_t a)
{
	Gtk::Label foo;
	
	foo.set_name (style);
	foo.ensure_style ();
	
	GtkRcStyle* waverc = foo.get_style()->gtkobj()->rc_style;

	if (waverc) {
		r = waverc->fg[GTK_STATE_NORMAL].red / 257;
		g = waverc->fg[GTK_STATE_NORMAL].green / 257;
		b = waverc->fg[GTK_STATE_NORMAL].blue / 257;

		/* what a hack ... "a" is for "active" */

		a = waverc->fg[GTK_STATE_ACTIVE].red / 257; 

	} else {
		warning << compose (_("missing RGBA style for \"%1\""), style) << endl;
	}
	
	return (uint32_t) RGBA_TO_UINT(r,g,b,a);
}
