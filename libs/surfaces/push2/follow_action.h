/*
 * Copyright (C) 2022 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2022 Ben Loftis <ben@harrisonconsoles.com>
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

#ifndef __ardour_push2_follow_action_h__
#define __ardour_push2_follow_action_h__

/* This might be moved into libcanvas one day, but for now it is only used by
 * the push code. It has a dependency on ARDOUR::Trigger so it tricky to place
 * correctly.
 */

#include "canvas/rectangle.h"

namespace ArdourCanvas  {

class FollowActionIcon : public ArdourCanvas::Rectangle
{
  public:
	FollowActionIcon (Canvas*);
	FollowActionIcon (Item*);

	void render (Rect const &, Cairo::RefPtr<Cairo::Context>) const;
	void compute_bounding_box () const;

	void set_font_description (Pango::FontDescription const &);
	void set_size (double size);
	void set_scale (double scale);
	void set_trigger (boost::shared_ptr<ARDOUR::Trigger> t);
	void reset_trigger ();

  private:
	boost::shared_ptr<ARDOUR::Trigger> trigger;;
	Pango::FontDescription font_description;
	double size;
	double scale;
};

}

#endif /* __ardour_push2_follow_action_h__ */
