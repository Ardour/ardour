/*
    Copyright (C) 2016 Paul Davis

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

#ifndef __ardour_push2_track_mix_layout_h__
#define __ardour_push2_track_mix_layout_h__

#include "layout.h"

namespace ARDOUR {
	class Stripable;
}

namespace ArdourSurface {

class TrackMixLayout : public Push2Layout
{
   public:
	TrackMixLayout (Push2& p, ARDOUR::Session&, Cairo::RefPtr<Cairo::Context>);
	~TrackMixLayout ();

	void set_stripable (boost::shared_ptr<ARDOUR::Stripable>);

	bool redraw (Cairo::RefPtr<Cairo::Context>) const;

	void button_upper (uint32_t n);
	void button_lower (uint32_t n);

	void strip_vpot (int, int);
	void strip_vpot_touch (int, bool);

   private:
	boost::shared_ptr<ARDOUR::Stripable> stripable;
	PBD::ScopedConnectionList stripable_connections;
	bool _dirty;

	Glib::RefPtr<Pango::Layout> name_layout;
	Glib::RefPtr<Pango::Layout> upper_layout[8];
	Glib::RefPtr<Pango::Layout> lower_layout[8];

	void stripable_property_change (PBD::PropertyChange const& what_changed);

	void drop_stripable ();
	void name_changed ();
	void color_changed ();
};

} /* namespace */

#endif /* __ardour_push2_track_mix_layout_h__ */
