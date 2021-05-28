/*
 * Copyright (C) 2014 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_canvas_tracking_text_h__
#define __ardour_canvas_tracking_text_h__

#include "canvas/text.h"
#include <string>

namespace ArdourCanvas {

class LIBCANVAS_API TrackingText : public Text
{
public:
	TrackingText (Canvas*);
	TrackingText (Item*);

	void show_and_track (bool track_x, bool track_y);
	void set_offset (Duple const&);
	void set_x_offset (double);
	void set_y_offset (double);

private:
	bool  track_x;
	bool  track_y;
	Duple offset;

	void pointer_motion (Duple const&);
	void init ();
};

} // namespace ArdourCanvas

#endif /* __ardour_canvas_tracking_text_h__ */
