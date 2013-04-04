/*
    Copyright (C) 2003 Paul Davis

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

#ifndef __gtk_ardour_crossfade_view_h__
#define __gtk_ardour_crossfade_view_h__

#include <vector>
#include "pbd/signals.h"
#include "ardour/crossfade.h"

#include "time_axis_view_item.h"

class RouteTimeAxisView;
class AudioRegionView;

namespace ArdourCanvas {
	class PolyLine;
}

class CrossfadeView : public TimeAxisViewItem
{
public:
	CrossfadeView (ArdourCanvas::Group*,
		       RouteTimeAxisView&,
		       boost::shared_ptr<ARDOUR::Crossfade>,
		       double initial_frames_per_pixel,
		       Gdk::Color& basic_color,
		       AudioRegionView& leftview,
		       AudioRegionView& rightview);

	~CrossfadeView ();

	boost::shared_ptr<ARDOUR::Crossfade> crossfade;  // ok, let 'em have it

	AudioRegionView& left_view;    // and these too
	AudioRegionView& right_view;

	void set_heights (double, double);

	bool valid() const { return _valid; }
	bool visible() const { return _visible; }
	void set_valid (bool yn);

	static PBD::Signal1<void,CrossfadeView*> CatchDeletion;

	void fake_hide ();
	void hide ();
	void show ();
	void horizontal_position_changed ();

protected:
	void reset_width_dependent_items (double pixel_width);

private:
	bool _valid;
	bool _visible;
	bool _all_in_view;
	double _child_height;

	ArdourCanvas::PolyLine *fade_in;
	ArdourCanvas::PolyLine *fade_out;

	void crossfade_changed (const PBD::PropertyChange&);
	void crossfade_fades_changed ();
	void active_changed ();
	void redraw_curves ();
	void color_handler ();
};

#endif /* __gtk_ardour_crossfade_view_h__ */
