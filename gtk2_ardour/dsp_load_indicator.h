/*
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __gtkardour_dsp_load_indicator_h__
#define __gtkardour_dsp_load_indicator_h__

#include <pangomm.h>

#include "gtkmm2ext/cairo_widget.h"

class DspLoadIndicator : public CairoWidget
{
	public:
	DspLoadIndicator ();
	~DspLoadIndicator ();

	void set_xrun_count (const unsigned int xruns);
	void set_dsp_load (const double load);

private:
	void on_size_request (Gtk::Requisition*);
	void render (Cairo::RefPtr<Cairo::Context> const&, cairo_rectangle_t*);
	bool on_button_release_event (GdkEventButton*);

	void update_tooltip ();

	Glib::RefPtr<Pango::Layout> _layout;
	float _dsp_load;
	unsigned int _xrun_count;
};

#endif
