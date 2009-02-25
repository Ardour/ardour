/*
    Copyright (C) 2002 Paul Davis 

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

#ifndef __ardour_automation_line_h__
#define __ardour_automation_line_h__

#include <vector>
#include <list>
#include <string>
#include <sys/types.h>

#include <libgnomecanvasmm/line.h>
#include <sigc++/signal.h>
#include "canvas.h"
#include "simplerect.h"

#include "evoral/TimeConverter.hpp"

#include "pbd/undo.h"
#include "pbd/statefuldestructible.h" 

#include "ardour/automation_list.h"
#include "ardour/types.h"

class AutomationLine;
class ControlPoint;
class PointSelection;
class TimeAxisView;
class AutomationTimeAxisView;
class Selectable;
class Selection;

namespace Gnome {
	namespace Canvas {
		class SimpleRect;
	}
}

class AutomationLine : public sigc::trackable, public PBD::StatefulThingWithGoingAway
{
  public:
	AutomationLine (const string& name, TimeAxisView&, ArdourCanvas::Group&,
			boost::shared_ptr<ARDOUR::AutomationList>,
			const Evoral::TimeConverter<double, ARDOUR::sframes_t>* converter = 0);
	virtual ~AutomationLine ();

	void queue_reset ();
	void reset ();
	void clear();

	void set_selected_points (PointSelection&);
	void get_selectables (nframes_t& start, nframes_t& end,
			      double botfrac, double topfrac, 
			      std::list<Selectable*>& results);
	void get_inverted_selectables (Selection&, std::list<Selectable*>& results);

	virtual void remove_point (ControlPoint&);
	bool control_points_adjacent (double xval, uint32_t& before, uint32_t& after);
	
	/* dragging API */
	virtual void start_drag (ControlPoint*, nframes_t x, float fraction);
	virtual void point_drag(ControlPoint&, nframes_t x, float, bool with_push);
	virtual void end_drag (ControlPoint*);
	virtual void line_drag(uint32_t i1, uint32_t i2, float, bool with_push);

	ControlPoint* nth (uint32_t);
	uint32_t npoints() const { return control_points.size(); }

	string  name()    const { return _name; }
	bool    visible() const { return _visible; }
	guint32 height()  const { return _height; }

	void     set_line_color (uint32_t);
	uint32_t get_line_color() const { return _line_color; }

	void set_interpolation(ARDOUR::AutomationList::InterpolationStyle style);

	void    show ();
	void    hide ();
	void    set_height (guint32);
	void    set_uses_gain_mapping (bool yn);
	bool    get_uses_gain_mapping () const { return _uses_gain_mapping; }

	TimeAxisView& trackview;

	ArdourCanvas::Group& canvas_group() const { return *group; }
	ArdourCanvas::Item&  parent_group() const { return _parent_group; }
	ArdourCanvas::Item&  grab_item() const { return *line; }

	void show_selection();
	void hide_selection ();

	string get_verbose_cursor_string (double) const;
	string fraction_to_string (double) const;
	double string_to_fraction (string const &) const;
	void   view_to_model_coord (double& x, double& y) const;
	void   model_to_view_coord (double& x, double& y) const;

	void set_list(boost::shared_ptr<ARDOUR::AutomationList> list);
	boost::shared_ptr<ARDOUR::AutomationList> the_list() const { return alist; }

	void show_all_control_points ();
	void hide_all_but_selected_control_points ();

	void track_entered();
	void track_exited();

	bool is_last_point (ControlPoint &);
	bool is_first_point (ControlPoint &);

	XMLNode& get_state (void);
	int set_state (const XMLNode&);
	void set_colors();

	void modify_point_y (ControlPoint&, double);
	
  protected:

	string    _name;
	guint32   _height;
	uint32_t  _line_color;
	
	boost::shared_ptr<ARDOUR::AutomationList> alist;

	bool    _visible                  : 1;
	bool    _uses_gain_mapping        : 1;
	bool    terminal_points_can_slide : 1;
	bool    update_pending            : 1;
	bool    no_draw                   : 1;
	bool    points_visible            : 1;
	bool    did_push;

	ArdourCanvas::Group&        _parent_group;
	ArdourCanvas::Group*        group;
	ArdourCanvas::Line*         line; /* line */
	ArdourCanvas::Points        line_points; /* coordinates for canvas line */
	std::vector<ControlPoint*>  control_points; /* visible control points */

	struct ALPoint {
	    double x;
	    double y;
	    ALPoint (double xx, double yy) : x(xx), y(yy) {}
	};

	typedef std::vector<ALPoint> ALPoints;

	static void invalidate_point (ALPoints&, uint32_t index);
	static bool invalid_point (ALPoints&, uint32_t index);
	
	void determine_visible_control_points (ALPoints&);
	void sync_model_with_view_point (ControlPoint&, bool did_push, int64_t distance);
	void sync_model_with_view_line (uint32_t, uint32_t);
	
	virtual void change_model (ARDOUR::AutomationList::iterator, double x, double y);

	void reset_callback (const Evoral::ControlList&);
	void list_changed ();

	virtual bool event_handler (GdkEvent*);
	virtual void add_model_point (ALPoints& tmp_points, double frame, double yfract);
	
  private:
	uint32_t drags;
	double   first_drag_fraction;
	double   last_drag_fraction;
	uint32_t line_drag_cp1;
	uint32_t line_drag_cp2;
	int64_t  drag_x;
	int64_t  drag_distance;

	const Evoral::TimeConverter<double, ARDOUR::sframes_t>& _time_converter;
	ARDOUR::AutomationList::InterpolationStyle              _interpolation;

	void modify_view_point (ControlPoint&, double, double, bool with_push);
	void reset_line_coords (ControlPoint&);

	double control_point_box_size ();

	struct ModelRepresentation {
	    ARDOUR::AutomationList::iterator start;
	    ARDOUR::AutomationList::iterator end;
	    double xpos;
	    double ypos;
	    double xmin;
	    double ymin;
	    double xmax;
	    double ymax;
	    double xval;
	    double yval;
	};

	void model_representation (ControlPoint&, ModelRepresentation&);

	friend class AudioRegionGainLine;
};

#endif /* __ardour_automation_line_h__ */

