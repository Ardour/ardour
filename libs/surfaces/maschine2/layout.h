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

#ifndef _ardour_maschine2_layout_h_
#define _ardour_maschine2_layout_h_

#include <sigc++/trackable.h>
#include <cairomm/refptr.h>
#include "canvas/container.h"

namespace ARDOUR {
	class Session;
}

namespace ArdourSurface {

class Maschine2;

class Maschine2Layout : public sigc::trackable, public ArdourCanvas::Container
{
  public:
	Maschine2Layout (Maschine2& m2, ARDOUR::Session& s, std::string const & name);
	virtual ~Maschine2Layout ();

	std::string name() const { return _name; }
	int display_width () const;
	int display_height () const;

	void compute_bounding_box () const;

  protected:
	Maschine2& _m2;
	ARDOUR::Session& _session;
	std::string _name;
};

} /* namespace */

#endif /* _ardour_maschine2_layout_h_ */
