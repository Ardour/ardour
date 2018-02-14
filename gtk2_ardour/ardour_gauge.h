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

#ifndef __gtkardour_gauge_h__
#define __gtkardour_gauge_h__

#include <pangomm.h>

#include "gtkmm2ext/cairo_widget.h"

class ArdourGauge : public CairoWidget
{
public:
	ArdourGauge (std::string const& max_text = "00.0%");
	virtual ~ArdourGauge ();

	void blink (bool onoff);

protected:

	enum Status {
		Level_OK,   /* green */
		Level_WARN, /* yellow */
		Level_CRIT  /* red */
	};

	/* gauge's background, indicate alarming conditions eg. xrun */
	virtual bool alert () const { return false; }
	/* guage inidicator color */
	virtual Status indicator () const = 0;
	/* gauge level 0 <= level <= 1 */
	virtual float level () const = 0;

	virtual std::string tooltip_text () = 0;

	void update ();
	void update (std::string const &);

private:
	void on_size_request (Gtk::Requisition*);
	void render (Cairo::RefPtr<Cairo::Context> const&, cairo_rectangle_t*);

	Glib::RefPtr<Pango::Layout> _layout;

	bool _blink;
};

#endif
