/*
    Copyright (C) 2010 Paul Davis

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

#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/rgb_macros.h"
#include "gtkmm2ext/gui_thread.h"

#include "ardour/rc_configuration.h" // for widget prelight preference

#include "waves_icon_button.h"
#include "ardour_ui.h"
#include "global_signals.h"

#include "i18n.h"

#define REFLECTION_HEIGHT 2

using namespace Gdk;
using namespace Gtk;
using namespace Glib;
using namespace PBD;
using std::max;
using std::min;
using namespace std;

WavesIconButton::WavesIconButton ()
	:  WavesButton()
{
}

WavesIconButton::~WavesIconButton()
{
}


void
WavesIconButton::render (cairo_t* cr)
{
	void (*rounded_function)(cairo_t*, double, double, double, double, double);

	Glib::RefPtr<Gdk::Pixbuf> pixbuf = ((CairoWidget::active_state() == Gtkmm2ext::ImplicitActive) ? _implicit_active_pixbuf : Glib::RefPtr<Gdk::Pixbuf>(0));

	if (pixbuf == 0) {
		pixbuf = (get_state() == Gtk::STATE_INSENSITIVE) ? 
						(_inactive_pixbuf ? _inactive_pixbuf : _normal_pixbuf) : 
						(_hovering ? 
							(_pushed ? (_active_pixbuf ?
											_active_pixbuf : 
											_normal_pixbuf) :
									   (_prelight_pixbuf ? 
											_prelight_pixbuf :
											(get_active() ? 
								(_active_pixbuf ? 
									_active_pixbuf :
									_normal_pixbuf) :
								_normal_pixbuf))) :
							(get_active() ? 
								(_active_pixbuf ? 
									_active_pixbuf :
									_normal_pixbuf) :
								_normal_pixbuf));
	}

	if ((_left_border_width != 0) ||
		(_top_border_width != 0) ||
		(_right_border_width != 0) ||
		(_bottom_border_width != 0)) {
		cairo_set_source_rgba (cr, _border_color.get_red_p(), _border_color.get_blue_p(), _border_color.get_green_p(), 1);
		rounded_function (cr, 0, 0, get_width(), get_height(), _corner_radius);
		cairo_fill (cr);
	}

	//rounded_function (cr, _left_border_width, _top_border_width, get_width()-_left_border_width-_right_border_width, get_height()-_top_border_width-_bottom_border_width, _corner_radius);
	//cairo_set_source_rgba (cr, bgcolor.get_red_p(), bgcolor.get_green_p(), bgcolor.get_blue_p(), 1);
	//cairo_fill (cr);

	// pixbuf, if any
	if (pixbuf) {
		double x = (get_width() - pixbuf->get_width())/2.0;
		double y = (get_height() - pixbuf->get_height())/2.0;

		cairo_rectangle (cr, x, y, pixbuf->get_width(), pixbuf->get_height());
		gdk_cairo_set_source_pixbuf (cr, pixbuf->gobj(), x, y);
		cairo_fill (cr);
	}
}

void
WavesIconButton::set_normal_image (const RefPtr<Gdk::Pixbuf>& img)
{
	_normal_pixbuf = img;
	queue_draw ();
}

void
WavesIconButton::set_active_image (const RefPtr<Gdk::Pixbuf>& img)
{
	_active_pixbuf = img;
	queue_draw ();
}

void
WavesIconButton::set_implicit_active_image (const RefPtr<Gdk::Pixbuf>& img)
{
	_implicit_active_pixbuf = img;
	queue_draw ();
}

void
WavesIconButton::set_inactive_image (const RefPtr<Gdk::Pixbuf>& img)
{
	_inactive_pixbuf = img;
	queue_draw ();
}

void
WavesIconButton::set_prelight_image (const RefPtr<Gdk::Pixbuf>& img)
{
	_prelight_pixbuf = img;
	queue_draw ();
}
