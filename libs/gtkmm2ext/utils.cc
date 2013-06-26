/*
    Copyright (C) 1999 Paul Barton-Davis 

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

#include <map>

#include <gtk/gtkpaned.h>
#include <gtk/gtk.h>

#include <gtkmm/widget.h>
#include <gtkmm/button.h>
#include <gtkmm/window.h>
#include <gtkmm/paned.h>
#include <gtkmm/label.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/tooltip.h>

#include "gtkmm2ext/utils.h"

#include "i18n.h"

using namespace std;

void
Gtkmm2ext::init (const char* localedir)
{
#ifdef ENABLE_NLS
	(void) bindtextdomain(PACKAGE, localedir);
#endif
}

void
Gtkmm2ext::get_ink_pixel_size (Glib::RefPtr<Pango::Layout> layout,
			       int& width,
			       int& height)
{
	Pango::Rectangle ink_rect = layout->get_ink_extents ();
	
	width = (ink_rect.get_width() + PANGO_SCALE / 2) / PANGO_SCALE;
	height = (ink_rect.get_height() + PANGO_SCALE / 2) / PANGO_SCALE;
}

void
get_pixel_size (Glib::RefPtr<Pango::Layout> layout,
			       int& width,
			       int& height)
{
	layout->get_pixel_size (width, height);
}

void
Gtkmm2ext::set_size_request_to_display_given_text (Gtk::Widget &w, const gchar *text,
						   gint hpadding, gint vpadding)
{
	int width, height;
	w.ensure_style ();
	
	get_pixel_size (w.create_pango_layout (text), width, height);
	w.set_size_request(width + hpadding, height + vpadding);
}

void
Gtkmm2ext::set_size_request_to_display_given_text (Gtk::Widget &w, 
						   const std::vector<std::string>& strings,
						   gint hpadding, gint vpadding)
{
	int width, height;
	int width_max = 0;
	int height_max = 0;
	w.ensure_style ();
        vector<string> copy;
        const vector<string>* to_use;
        vector<string>::const_iterator i;

        for (i = strings.begin(); i != strings.end(); ++i) {
                if ((*i).find_first_of ("gy") != string::npos) {
                        /* contains a descender */
                        break;
                }
        }
	
        if (i == strings.end()) {
                /* make a copy of the strings then add one that has a descender */
                copy = strings;
                copy.push_back ("g");
                to_use = &copy;
        } else {
                to_use = &strings;
        }
	
	for (vector<string>::const_iterator i = to_use->begin(); i != to_use->end(); ++i) {
		get_pixel_size (w.create_pango_layout (*i), width, height);
		width_max = max(width_max,width);
		height_max = max(height_max, height);
	}

	w.set_size_request(width_max + hpadding, height_max + vpadding);
}

static inline guint8
demultiply_alpha (guint8 src,
                  guint8 alpha)
{
        /* cairo pixel buffer data contains RGB values with the alpha
           values premultiplied.

           GdkPixbuf pixel buffer data contains RGB values without the
           alpha value applied.

           this removes the alpha component from the cairo version and
           returns the GdkPixbuf version.
        */
	return alpha ? ((guint (src) << 8) - src) / alpha : 0;
}

static void
convert_bgra_to_rgba (guint8 const* src,
		      guint8*       dst,
		      int           width,
		      int           height)
{
	guint8 const* src_pixel = src;
	guint8*       dst_pixel = dst;
	
        /* cairo pixel data is endian-dependent ARGB with A in the most significant 8 bits,
           with premultipled alpha values (see preceding function)

           GdkPixbuf pixel data is non-endian-dependent RGBA with R in the lowest addressable
           8 bits, and non-premultiplied alpha values.

           convert from the cairo values to the GdkPixbuf ones.
        */

	for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
                        /* Cairo [ B G R A ] is actually  [ B G R A ] in memory SOURCE
                                                            0 1 2 3
                           Pixbuf [ R G B A ] is actually [ R G B A ] in memory DEST
                        */
                        dst_pixel[0] = demultiply_alpha (src_pixel[2],
                                                         src_pixel[3]); // R [0] <= [ 2 ]
                        dst_pixel[1] = demultiply_alpha (src_pixel[1],
                                                         src_pixel[3]); // G [1] <= [ 1 ]
                        dst_pixel[2] = demultiply_alpha (src_pixel[0],  
                                                         src_pixel[3]); // B [2] <= [ 0 ]
                        dst_pixel[3] = src_pixel[3]; // alpha
                        
#elif G_BYTE_ORDER == G_BIG_ENDIAN
                        /* Cairo [ B G R A ] is actually  [ A R G B ] in memory SOURCE
                                                            0 1 2 3
                           Pixbuf [ R G B A ] is actually [ R G B A ] in memory DEST
                        */
                        dst_pixel[0] = demultiply_alpha (src_pixel[1],
                                                         src_pixel[0]); // R [0] <= [ 1 ]
                        dst_pixel[1] = demultiply_alpha (src_pixel[2],
                                                         src_pixel[0]); // G [1] <= [ 2 ]
                        dst_pixel[2] = demultiply_alpha (src_pixel[3],
                                                         src_pixel[0]); // B [2] <= [ 3 ]
                        dst_pixel[3] = src_pixel[0]; // alpha
                        
#else
#error ardour does not currently support PDP-endianess
#endif			
                        
                        dst_pixel += 4;
                        src_pixel += 4;
                }
	}
}

Glib::RefPtr<Gdk::Pixbuf>
Gtkmm2ext::pixbuf_from_string(const string& name, const Pango::FontDescription& font, int clip_width, int clip_height, Gdk::Color fg)
{
	static Glib::RefPtr<Gdk::Pixbuf>* empty_pixbuf = 0;

	if (name.empty()) {
		if (empty_pixbuf == 0) {
			empty_pixbuf = new Glib::RefPtr<Gdk::Pixbuf>;
			*empty_pixbuf = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, true, 8, clip_width, clip_height);
		}
		return *empty_pixbuf;
	}

	Glib::RefPtr<Gdk::Pixbuf> buf = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, true, 8, clip_width, clip_height);

	cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, clip_width, clip_height);
	cairo_t* cr = cairo_create (surface);
	cairo_text_extents_t te;
	
	cairo_set_source_rgba (cr, fg.get_red_p(), fg.get_green_p(), fg.get_blue_p(), 1.0);
	cairo_select_font_face (cr, font.get_family().c_str(),
				CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size (cr,  font.get_size() / Pango::SCALE);
	cairo_text_extents (cr, name.c_str(), &te);
	
	cairo_move_to (cr, 0.5, int (0.5 - te.height / 2 - te.y_bearing + clip_height / 2));
	cairo_show_text (cr, name.c_str());
	
	convert_bgra_to_rgba(cairo_image_surface_get_data (surface), buf->get_pixels(), clip_width, clip_height);

	cairo_destroy(cr);
	cairo_surface_destroy(surface);

	return buf;
}

void
Gtkmm2ext::set_popdown_strings (Gtk::ComboBoxText& cr, const vector<string>& strings)
{
	vector<string>::const_iterator i;

	cr.clear ();

	for (i = strings.begin(); i != strings.end(); ++i) {
		cr.append_text (*i);
	}
}

GdkWindow*
Gtkmm2ext::get_paned_handle (Gtk::Paned& paned)
{
	return GTK_PANED(paned.gobj())->handle;
}

void
Gtkmm2ext::set_decoration (Gtk::Window* win, Gdk::WMDecoration decor)
{
	win->get_window()->set_decorations (decor);
}

void Gtkmm2ext::set_treeview_header_as_default_label(Gtk::TreeViewColumn* c)
{
	gtk_tree_view_column_set_widget( c->gobj(), GTK_WIDGET(0) );
}

void
Gtkmm2ext::detach_menu (Gtk::Menu& menu)
{
	/* its possible for a Gtk::Menu to have no gobj() because it has
	   not yet been instantiated. Catch this and provide a safe
	   detach method.
	*/
	if (menu.gobj()) {
		if (menu.get_attach_widget()) {
			menu.detach ();
		}
	}
}

bool
Gtkmm2ext::possibly_translate_keyval_to_make_legal_accelerator (uint32_t& keyval)
{
	int fakekey = GDK_VoidSymbol;

	switch (keyval) {
	case GDK_Tab:
	case GDK_ISO_Left_Tab:
		fakekey = GDK_nabla;
		break;

	case GDK_Up:
		fakekey = GDK_uparrow;
		break;

	case GDK_Down:
		fakekey = GDK_downarrow;
		break;

	case GDK_Right:
		fakekey = GDK_rightarrow;
		break;

	case GDK_Left:
		fakekey = GDK_leftarrow;
		break;

	case GDK_Return:
		fakekey = GDK_3270_Enter;
		break;

	case GDK_KP_Enter:
		fakekey = GDK_F35;
		break;

	default:
		break;
	}

	if (fakekey != GDK_VoidSymbol) {
		keyval = fakekey;
		return true;
	}

	return false;
}

uint32_t
Gtkmm2ext::possibly_translate_legal_accelerator_to_real_key (uint32_t keyval)
{
	switch (keyval) {
	case GDK_nabla:
		return GDK_Tab;
		break;

	case GDK_uparrow:
		return GDK_Up;
		break;

	case GDK_downarrow:
		return GDK_Down;
		break;

	case GDK_rightarrow:
		return GDK_Right;
		break;

	case GDK_leftarrow:
		return GDK_Left;
		break;

	case GDK_3270_Enter:
		return GDK_Return;

	case GDK_F35:
		return GDK_KP_Enter;
		break;
	}

	return keyval;
}

int
Gtkmm2ext::physical_screen_height (Glib::RefPtr<Gdk::Window> win)
{
        GdkScreen* scr = gdk_screen_get_default();

        if (win) {
                GdkRectangle r;
                gint monitor = gdk_screen_get_monitor_at_window (scr, win->gobj());
                gdk_screen_get_monitor_geometry (scr, monitor, &r);
                return r.height;
        } else {
                return gdk_screen_get_height (scr);
        }
}

int
Gtkmm2ext::physical_screen_width (Glib::RefPtr<Gdk::Window> win)
{
        GdkScreen* scr = gdk_screen_get_default();
        
        if (win) {
                GdkRectangle r;
                gint monitor = gdk_screen_get_monitor_at_window (scr, win->gobj());
                gdk_screen_get_monitor_geometry (scr, monitor, &r);
                return r.width;
        } else {
                return gdk_screen_get_width (scr);
        }
}

void
Gtkmm2ext::container_clear (Gtk::Container& c)
{
        list<Gtk::Widget*> children = c.get_children();
        for (list<Gtk::Widget*>::iterator child = children.begin(); child != children.end(); ++child) {
                c.remove (**child);
        }
}

void
Gtkmm2ext::rounded_rectangle (Cairo::RefPtr<Cairo::Context> context, double x, double y, double w, double h, double r)
{
	rounded_rectangle (context->cobj(), x, y, w, h, r);
}
void
Gtkmm2ext::rounded_top_rectangle (Cairo::RefPtr<Cairo::Context> context, double x, double y, double w, double h, double r)
{
	rounded_top_rectangle (context->cobj(), x, y, w, h, r);
}
void
Gtkmm2ext::rounded_top_left_rectangle (Cairo::RefPtr<Cairo::Context> context, double x, double y, double w, double h, double r)
{
	rounded_top_left_rectangle (context->cobj(), x, y, w, h, r);
}
void
Gtkmm2ext::rounded_top_right_rectangle (Cairo::RefPtr<Cairo::Context> context, double x, double y, double w, double h, double r)
{
	rounded_top_right_rectangle (context->cobj(), x, y, w, h, r);
}
void
Gtkmm2ext::rounded_top_half_rectangle (Cairo::RefPtr<Cairo::Context> context, double x, double y, double w, double h, double r)
{
	rounded_top_half_rectangle (context->cobj(), x, y, w, h, r);
}
void
Gtkmm2ext::rounded_bottom_half_rectangle (Cairo::RefPtr<Cairo::Context> context, double x, double y, double w, double h, double r)
{
	rounded_bottom_half_rectangle (context->cobj(), x, y, w, h, r);
}
void
Gtkmm2ext::rounded_right_half_rectangle (Cairo::RefPtr<Cairo::Context> context, double x, double y, double w, double h, double r)
{
	rounded_right_half_rectangle (context->cobj(), x, y, w, h, r);
}

void
Gtkmm2ext::rounded_rectangle (cairo_t* cr, double x, double y, double w, double h, double r)
{
	double degrees = M_PI / 180.0;

	cairo_new_sub_path (cr);
	cairo_arc (cr, x + w - r, y + r, r, -90 * degrees, 0 * degrees);  //tr
	cairo_arc (cr, x + w - r, y + h - r, r, 0 * degrees, 90 * degrees);  //br
	cairo_arc (cr, x + r, y + h - r, r, 90 * degrees, 180 * degrees);  //bl
	cairo_arc (cr, x + r, y + r, r, 180 * degrees, 270 * degrees);  //tl
	cairo_close_path (cr);
}

void
Gtkmm2ext::rounded_right_half_rectangle (cairo_t* cr, double x, double y, double w, double h, double r)
{
	double degrees = M_PI / 180.0;

	cairo_new_sub_path (cr);
	cairo_arc (cr, x + w - r, y + r, r, -90 * degrees, 0 * degrees);  //tr
	cairo_arc (cr, x + w - r, y + h - r, r, 0 * degrees, 90 * degrees);  //br
	cairo_line_to (cr, x, y + h); // bl
	cairo_line_to (cr, x, y); // tl
	cairo_close_path (cr);
}

void
Gtkmm2ext::rounded_top_half_rectangle (cairo_t* cr, double x, double y, double w, double h, double r)
{
	double degrees = M_PI / 180.0;

	cairo_new_sub_path (cr);
	cairo_move_to (cr, x+w, y+h);
	cairo_line_to (cr, x, y+h);
	cairo_arc (cr, x + r, y + r, r, 180 * degrees, 270 * degrees);  //tl
	cairo_arc (cr, x + w - r, y + r, r, -90 * degrees, 0 * degrees);  //tr
	cairo_close_path (cr);
}

void
Gtkmm2ext::rounded_bottom_half_rectangle (cairo_t* cr, double x, double y, double w, double h, double r)
{
	double degrees = M_PI / 180.0;

	cairo_new_sub_path (cr);
	cairo_move_to (cr, x, y);
	cairo_line_to (cr, x+w, y);
	cairo_arc (cr, x + w - r, y + h - r, r, 0 * degrees, 90 * degrees);  //br
	cairo_arc (cr, x + r, y + h - r, r, 90 * degrees, 180 * degrees);  //bl
	cairo_close_path (cr);
}


void
Gtkmm2ext::rounded_top_rectangle (cairo_t* cr, double x, double y, double w, double h, double r)
{
	double degrees = M_PI / 180.0;

	cairo_new_sub_path (cr);
	cairo_move_to (cr, x+w, y+h);
	cairo_line_to (cr, x, y+h);
	cairo_arc (cr, x + r, y + r, r, 180 * degrees, 270 * degrees);  //tl
	cairo_arc (cr, x + w - r, y + r, r, -90 * degrees, 0 * degrees);  //tr
	cairo_close_path (cr);
}

void
Gtkmm2ext::rounded_top_left_rectangle (cairo_t* cr, double x, double y, double w, double h, double r)
{
/*    A****B
      H    *
      *    *
      *    *
      F****E
*/
	cairo_move_to (cr, x+r,y); // Move to A
	cairo_line_to (cr, x+w,y); // Straight line to B
	cairo_line_to (cr, x+w,y+h); // Move to E
	cairo_line_to (cr, x,y+h); // Line to F
	cairo_line_to (cr, x,y+r); // Line to H
	cairo_curve_to (cr, x,y,x,y,x+r,y); // Curve to A
}

void
Gtkmm2ext::rounded_top_right_rectangle (cairo_t* cr, double x, double y, double w, double h, double r)
{
/*    A****BQ
      *    C
      *    *
      *    *
      F****E
*/
	cairo_move_to (cr, x,y); // Move to A
	cairo_line_to (cr, x+w-r,y); // Straight line to B
	cairo_curve_to (cr, x+w,y,x+w,y,x+w,y+r); // Curve to C, Control points are both at Q
	cairo_line_to (cr, x+w,y+h); // Move to E
	cairo_line_to (cr, x,y+h); // Line to F
	cairo_line_to (cr, x,y); // Line to A
}

Glib::RefPtr<Gdk::Window>
Gtkmm2ext::window_to_draw_on (Gtk::Widget& w, Gtk::Widget** parent)
{
	if (w.get_has_window()) {
		return w.get_window();
	}

	(*parent) = w.get_parent();

	while (*parent) {
		if ((*parent)->get_has_window()) {
			return (*parent)->get_window ();
		}
		(*parent) = (*parent)->get_parent ();
	}

	return Glib::RefPtr<Gdk::Window> ();
}

int
Gtkmm2ext::pixel_width (const string& str, Pango::FontDescription& font)
{
	Gtk::Label foo;
	Glib::RefPtr<Pango::Layout> layout = foo.create_pango_layout ("");

	layout->set_font_description (font);
	layout->set_text (str);

	int width, height;
	Gtkmm2ext::get_ink_pixel_size (layout, width, height);
	return width;
}

#if 0
string
Gtkmm2ext::fit_to_pixels (const string& str, int pixel_width, Pango::FontDescription& font, int& actual_width, bool with_ellipses)
{
	/* DECEMBER 2011: THIS PROTOTYPE OF fit_to_pixels() IS NOT USED
	   ANYWHERE AND HAS NOT BEEN TESTED.
	*/
	Gtk::Label foo;
	Glib::RefPtr<Pango::Layout> layout = foo.create_pango_layout (str);
	Glib::RefPtr<const Pango::LayoutLine> line;

	layout->set_font_description (font);
	layout->set_width (pixel_width * PANGO_SCALE);

	if (with_ellipses) {
        	layout->set_ellipsize (Pango::ELLIPSIZE_END);
	} else {
        	layout->set_wrap (Pango::WRAP_CHAR);
	}

	line = layout->get_line (0);

	/* XXX: might need special care to get the ellipsis character, not sure
           how that works 
	*/	

	string s = string (layout->get_text ().substr(line->get_start_index(), line->get_length()));
	
	cerr << "fit to pixels of " << str << " returns " << s << endl;

	return s;
}
#endif

/** Try to fit a string into a given horizontal space by ellipsizing it.
 *  @param cr Cairo context in which the text will be plotted.
 *  @param name Text.
 *  @param avail Available horizontal space.
 *  @return (Text, possibly ellipsized) and (horizontal size of text)
 */

std::pair<std::string, double>
Gtkmm2ext::fit_to_pixels (cairo_t* cr, std::string name, double avail)
{
	/* XXX hopefully there exists a more efficient way of doing this */

	bool abbreviated = false;
	uint32_t width = 0;

	while (1) {
		cairo_text_extents_t ext;
		cairo_text_extents (cr, name.c_str(), &ext);

		if (ext.width < avail || name.length() <= 4) {
			width = ext.width;
			break;
		}

		if (abbreviated) {
			name = name.substr (0, name.length() - 4) + "...";
		} else {
			name = name.substr (0, name.length() - 3) + "...";
			abbreviated = true;
		}
	}

	return std::make_pair (name, width);
}

Gtk::Label *
Gtkmm2ext::left_aligned_label (string const & t)
{
	Gtk::Label* l = new Gtk::Label (t);
	l->set_alignment (0, 0.5);
	return l;
}

static bool
make_null_tooltip (int, int, bool, const Glib::RefPtr<Gtk::Tooltip>& t)
{
	t->set_tip_area (Gdk::Rectangle (0, 0, 0, 0));
	return true;
}

/** Hackily arrange for the provided widget to have no tooltip,
 *  and also to stop any other widget from providing one while
 * the mouse is over w.
 */
void
Gtkmm2ext::set_no_tooltip_whatsoever (Gtk::Widget& w)
{
	w.property_has_tooltip() = true;
	w.signal_query_tooltip().connect (sigc::ptr_fun (make_null_tooltip));
}

void
Gtkmm2ext::enable_tooltips ()
{
	gtk_rc_parse_string ("gtk-enable-tooltips = 1");
}

void
Gtkmm2ext::disable_tooltips ()
{
	gtk_rc_parse_string ("gtk-enable-tooltips = 0");
}

