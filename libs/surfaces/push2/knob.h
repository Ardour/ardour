/*
 * Copyright (C) 2016 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_push2_knob_h__
#define __ardour_push2_knob_h__

#include <boost/shared_ptr.hpp>
#include <sigc++/trackable.h>

#include <cairomm/refptr.h>

#include "pbd/signals.h"

#include "canvas/container.h"

namespace ArdourCanvas {
	class Text;
}

namespace ARDOUR {
	class AutomationControl;
}

namespace Cairo {
	class Context;
}

namespace ArdourSurface {

class Push2;

class Push2Knob : public sigc::trackable, public ArdourCanvas::Container
{
public:
	enum Element {
		Arc = 0x1,
		Bevel = 0x2,
		unused2 = 0x4,
		unused3 = 0x8,
		unused4 = 0x10,
		unused5 = 0x20,
	};

	enum Flags {
		NoFlags = 0,
		Detent = 0x1,
		ArcToZero = 0x2,
	};

	Push2Knob (Push2& p, ArdourCanvas::Item*, Element e = default_elements, Flags flags = NoFlags);
	virtual ~Push2Knob ();

	static Element default_elements;

	void add_flag (Flags);
	void remove_flag (Flags);

	void set_controllable (boost::shared_ptr<ARDOUR::AutomationControl> c);
	boost::shared_ptr<ARDOUR::AutomationControl> controllable() const { return _controllable; }

	void set_text_color (Gtkmm2ext::Color);
	void set_arc_start_color (Gtkmm2ext::Color);
	void set_arc_end_color (Gtkmm2ext::Color);
	void set_radius (double r);

	void render (ArdourCanvas::Rect const &, Cairo::RefPtr<Cairo::Context>) const;
	void compute_bounding_box() const;

  protected:
	void controllable_changed ();
	PBD::ScopedConnection watch_connection;
	boost::shared_ptr<ARDOUR::AutomationControl> _controllable;

  private:
	Push2& p2;
	Element _elements;
	Flags   _flags;
	double  _r;
	float   _val; // current value [0..1]
	float   _normal; // default value, arc

	Gtkmm2ext::Color text_color;
	Gtkmm2ext::Color arc_start_color;
	Gtkmm2ext::Color arc_end_color;
	ArdourCanvas::Text* text;

	void set_pan_azimuth_text (double);
	void set_pan_width_text (double);
	void set_gain_text (double);
};

} // namespace

#endif /* __ardour_push2_knob_h__ */
