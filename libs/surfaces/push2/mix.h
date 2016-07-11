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

#ifndef __ardour_push2_mix_layout_h__
#define __ardour_push2_mix_layout_h__

#include "layout.h"

namespace ARDOUR {
	class Stripable;
}

namespace ArdourSurface {

class Push2Knob;

class MixLayout : public Push2Layout
{
   public:
	MixLayout (Push2& p, ARDOUR::Session&, Cairo::RefPtr<Cairo::Context>);
	~MixLayout ();

	bool redraw (Cairo::RefPtr<Cairo::Context>) const;

	void button_upper (uint32_t n);
	void button_lower (uint32_t n);
	void button_left ();
	void button_right ();
	void button_select_press ();
	void button_select_release ();

	void strip_vpot (int, int);
	void strip_vpot_touch (int, bool);

  private:
	Glib::RefPtr<Pango::Layout> tc_clock_layout;
	Glib::RefPtr<Pango::Layout> bbt_clock_layout;
	Glib::RefPtr<Pango::Layout> upper_layout[8];
	Glib::RefPtr<Pango::Layout> lower_layout[8];
	Push2Knob* knobs[8];

	/* stripables */

	int32_t bank_start;
	PBD::ScopedConnectionList stripable_connections;
	boost::shared_ptr<ARDOUR::Stripable> stripable[8];

	void solo_change (int);
	void mute_change (int);

	void stripable_property_change (PBD::PropertyChange const& what_changed, int which);

	void switch_bank (uint32_t base);
};

} /* namespace */

#endif /* __ardour_push2_layout_h__ */
