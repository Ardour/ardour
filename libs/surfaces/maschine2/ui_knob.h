/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
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

#ifndef _ardour_maschine2_knob_h_
#define _ardour_maschine2_knob_h_

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
	class Region;
}

namespace ArdourSurface {

class Maschine2;
class M2EncoderInterface;

class Maschine2Knob : public sigc::trackable, public ArdourCanvas::Container
{
	public:
		Maschine2Knob (PBD::EventLoop*, ArdourCanvas::Item*);
		virtual ~Maschine2Knob ();

		void set_controllable (boost::shared_ptr<ARDOUR::AutomationControl>);
		void set_control (M2EncoderInterface*);
		boost::shared_ptr<ARDOUR::AutomationControl> controllable() const { return _controllable; }

		void render (ArdourCanvas::Rect const &, Cairo::RefPtr<Cairo::Context>) const;
		void compute_bounding_box() const;

	protected:
		void controllable_changed ();
		void encoder_changed (int);

		PBD::ScopedConnection watch_connection;
		PBD::ScopedConnection encoder_connection;

		boost::shared_ptr<ARDOUR::AutomationControl> _controllable;
		M2EncoderInterface* _ctrl;

	private:
		PBD::EventLoop* _eventloop;

		float  _radius;
		float  _val; // current value [0..1]
		float  _normal; // default value, arc

		ArdourCanvas::Text* text;
};

} // namespace

#endif
