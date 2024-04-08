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

#ifndef __gtk2_ardour_livetrax_tc_widget_h__
#define __gtk2_ardour_livetrax_tc_widget_h__

#include "gtkmm2ext/cairo_widget.h"

class LiveTraxTCWidget : public CairoWidget
{
  public:
	LiveTraxTCWidget ();

	bool on_button_release_event (GdkEventButton*);
	void render (Cairo::RefPtr<Cairo::Context> const&, cairo_rectangle_t*);
	void parameter_changed (std::string const &);
	void on_size_request (Gtk::Requisition*);

  private:
	double bg_r, bg_g, bg_b, bg_a;
	double fg_r, fg_g, fg_b, fg_a;
	double txt_r, txt_g, txt_b, txt_a;
};

#endif /* __gtk2_ardour_livetrax_tc_widget_h__ */
