/*
 * Copyright (C) 2024 Paul Davis <paul@linuxaudiosystems.com>
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

#include <iostream>

#include "livetrax_tc_widget.h"
#include "ui_config.h"

LiveTraxTCWidget::LiveTraxTCWidget()
{
	Gtkmm2ext::Color c;
	c = UIConfiguration::instance().color ("widget:blue");
	Gtkmm2ext::color_to_rgba (c, bg_r, bg_g, bg_b, bg_a);
	c = UIConfiguration::instance().color ("neutral:foreground");
	Gtkmm2ext::color_to_rgba (c, txt_r, txt_g, txt_b, txt_a);
	c = UIConfiguration::instance().color ("theme:bg1");
	Gtkmm2ext::color_to_rgba (c, fg_r, fg_g, fg_b, fg_a);
}

bool
LiveTraxTCWidget::on_button_release_event (GdkEventButton* ev)
{
	std::cerr << "here\n";
	return true;
}

void
LiveTraxTCWidget::render (Cairo::RefPtr<Cairo::Context> const & context, cairo_rectangle_t*)
{
	/* draw the background */
	context->set_source_rgba (bg_r, bg_g, bg_b, 1.);
	context->rectangle (0, 0, get_width(), get_height());
	context->fill ();

	Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create (context);
	layout->set_text ("TC\nSource");
	layout->set_font_description (UIConfiguration::instance().get_NormalBoldFont());
	context->move_to (get_width() / 2.0 + 10., 10.);
	context->set_source_rgba (txt_r, txt_g, txt_b, 1.);
	layout->show_in_cairo_context (context);

	double rect_width = (get_width() / 2.);
	double rect_height = (get_height() - 3.) / 2.;


	context->set_source_rgba (fg_r, fg_g, fg_b, 1.);
	context->rectangle (1., 1., rect_width, rect_height);
	context->fill ();

	context->set_source_rgba (fg_r, fg_g, fg_b, 1.);
	context->rectangle (1., 2 + rect_height, rect_width, rect_height);
	context->fill ();

	layout->set_text ("LTC");
	layout->set_font_description (UIConfiguration::instance().get_NormalFont());
	context->move_to (4., 3.); // XXXX need to adjust by + text height/2
	context->set_source_rgba (txt_r, txt_g, txt_b, 1.);
	layout->show_in_cairo_context (context);

	layout->set_text ("25 FPS");
	layout->set_font_description (UIConfiguration::instance().get_NormalFont());
	context->move_to (4., 3. + rect_height); // XXXX need to adjust by + text height/2
	context->set_source_rgba (txt_r, txt_g, txt_b, 1.);
	layout->show_in_cairo_context (context);
}

void
LiveTraxTCWidget::parameter_changed (std::string const & param)
{
}

void
LiveTraxTCWidget::on_size_request (Gtk::Requisition* r)
{
	r->width = 150;
	r->height = -1;
}
