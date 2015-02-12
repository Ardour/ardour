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

WavesIconButton::WavesIconButton (const std::string& title)
	: WavesButton (title)
{
}

WavesIconButton::~WavesIconButton ()
{
}


void
WavesIconButton::render (cairo_t* cr, cairo_rectangle_t*)
{
	Glib::RefPtr<Gdk::Pixbuf> pixbuf = ((CairoWidget::active_state() == Gtkmm2ext::ImplicitActive) ? _implicit_active_pixbuf : Glib::RefPtr<Gdk::Pixbuf>(0));

	if (pixbuf == 0) {
		pixbuf = (get_state() == Gtk::STATE_INSENSITIVE) ? 
						_inactive_pixbuf : 
						(_pushed ? (get_active () ? _normal_pixbuf : _active_pixbuf) :
								   (get_active () ? _active_pixbuf : _normal_pixbuf));
	}

	// pixbuf, if any
	if (pixbuf) {
		cairo_rectangle (cr, 0, 0, pixbuf->get_width(), pixbuf->get_height());
		gdk_cairo_set_source_pixbuf (cr, pixbuf->gobj(), 0, 0);
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
