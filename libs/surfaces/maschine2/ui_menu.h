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

#ifndef _ardour_maschine2_menu_h_
#define _ardour_maschine2_menu_h_

#include <vector>
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

class Maschine2;
class M2EncoderInterface;

class Maschine2Menu : public ArdourCanvas::Container
{
	public:
		Maschine2Menu (PBD::EventLoop*, ArdourCanvas::Item*, const std::vector<std::string>&, double width = 64);
		virtual ~Maschine2Menu ();

		void set_control (M2EncoderInterface*);
		void set_active (uint32_t index);
		void set_wrap (bool);

		uint32_t active () const { return _active; }
		uint32_t items() const { return _displays.size(); }

		PBD::Signal0<void> ActiveChanged;

		void render (ArdourCanvas::Rect const &, Cairo::RefPtr<Cairo::Context>) const;

	private:
		void rearrange (uint32_t);
		void encoder_changed (int);

		M2EncoderInterface* _ctrl;
		PBD::EventLoop* _eventloop;
		PBD::ScopedConnection encoder_connection;

		std::vector<ArdourCanvas::Text*> _displays;
		ArdourCanvas::Rectangle* _active_bg;

		double   _baseline;
		double   _height;
		double   _width;
		uint32_t _active;
		bool     _wrap;

		uint32_t _first;
		uint32_t _last;
		double   _rotary;
};

} // namespace

#endif
