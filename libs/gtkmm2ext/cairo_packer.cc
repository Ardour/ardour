/*
 * Copyright (C) 2011-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2016 Robin Gareus <robin@gareus.org>
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

#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/cairo_widget.h"
#include "gtkmm2ext/cairo_packer.h"

void
CairoPacker::draw_background (Gtk::Widget& w, GdkEventExpose*)
{
	int x, y;
	Gtk::Widget* window_parent;
	Glib::RefPtr<Gdk::Window> win = Gtkmm2ext::window_to_draw_on (w, &window_parent);

	if (win) {

		Cairo::RefPtr<Cairo::Context> context = win->create_cairo_context();
		w.translate_coordinates (*window_parent, 0, 0, x, y);

		Gdk::Color bg = get_bg ();

		context->set_source_rgba (bg.get_red_p(), bg.get_green_p(), bg.get_blue_p(), 1.0);
		Gtkmm2ext::rounded_rectangle (context, x, y, w.get_allocation().get_width(), w.get_allocation().get_height(), 4);
		context->fill ();
	}
}

CairoHPacker::CairoHPacker ()
{
}

void
CairoHPacker::on_realize ()
{
	HBox::on_realize ();
	CairoWidget::provide_background_for_cairo_widget (*this, get_bg ());
}

Gdk::Color
CairoHPacker::get_bg () const
{
	return get_style()->get_bg (Gtk::STATE_NORMAL);
}

bool
CairoHPacker::on_expose_event (GdkEventExpose* ev)
{
	draw_background (*this, ev);
	return HBox::on_expose_event (ev);
}

void
CairoHPacker::on_size_allocate (Gtk::Allocation& alloc)
{
	get_parent()->queue_draw();
	HBox::on_size_allocate (alloc);
}

CairoVPacker::CairoVPacker ()
{
}

bool
CairoVPacker::on_expose_event (GdkEventExpose* ev)
{
	draw_background (*this, ev);
	return VBox::on_expose_event (ev);
}

void
CairoVPacker::on_realize ()
{
	VBox::on_realize ();
	CairoWidget::provide_background_for_cairo_widget (*this, get_bg());
}

void
CairoVPacker::on_size_allocate (Gtk::Allocation& alloc)
{
	get_parent()->queue_draw();
	VBox::on_size_allocate (alloc);
}

Gdk::Color
CairoVPacker::get_bg () const
{
	return get_style()->get_bg (Gtk::STATE_NORMAL);
}
