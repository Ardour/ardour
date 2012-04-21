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
#include "pbd/memento_command.h"

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

/** A GUI representation of an ARDOUR::AutomationList */
class AutomationLine : public sigc::trackable, public PBD::StatefulDestructible
{
  public:
	AutomationLine (const std::string& name, TimeAxisView&, ArdourCanvas::Group&,
			boost::shared_ptr<ARDOUR::AutomationList>,
			Evoral::TimeConverter<double, ARDOUR::framepos_t>* converter = 0);
	virtual ~AutomationLine ();

	void queue_reset ();
	void reset ();
	void clear ();

	std::list<ControlPoint*> point_selection_to_control_points (PointSelection const &);
	void set_selected_points (PointSelection const &);
	void get_selectables (ARDOUR::framepos_t, ARDOUR::framepos_t, double, double, std::list<Selectable*>&);
	void get_inverted_selectables (Selection&, std::list<Selectable*>& results);

	virtual void remove_point (ControlPoint&);
	bool control_points_adjacent (double xval, uint32_t& before, uint32_t& after);

	/* dragging API */
	virtual void start_drag_single (ControlPoint*, double, float);
	virtual void start_drag_line (uint32_t, uint32_t, float);
	virtual void start_drag_multiple (std::list<ControlPoint*>, float, XMLNode *);
	virtual std::pair<double, float> drag_motion (double, float, bool, bool);
	virtual void end_drag ();

	ControlPoint* nth (uint32_t);
	ControlPoint const * nth (uint32_t) const;
	uint32_t npoints() const { return control_points.size(); }

	std::string  name()    const { return _name; }
	bool    visible() const { return _visible; }
	guint32 height()  const { return _height; }

	void     set_line_color (uint32_t);
	uint32_t get_line_color() const { return _line_color; }

	void    show ();
	void    hide ();
	void    set_height (guint32);
	void    set_uses_gain_mapping (bool yn);
	bool    get_uses_gain_mapping () const { return _uses_gain_mapping; }

	TimeAxisView& trackview;

	ArdourCanvas::Group& canvas_group() const { return *group; }
	ArdourCanvas::Item&  parent_group() const { return _parent_group; }
	ArdourCanvas::Item&  grab_item() const { return *line; }

	std::string get_verbose_cursor_string (double) const;
	std::string fraction_to_string (double) const;
	double string_to_fraction (std::string const &) const;
	void   view_to_model_coord (double& x, double& y) const;
	void   view_to_model_coord_y (double &) const;
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
	int set_state (const XMLNode&, int version);
	void set_colors();

	void modify_point_y (ControlPoint&, double);

	virtual MementoCommandBinder<ARDOUR::AutomationList>* memento_command_binder ();

	const Evoral::TimeConverter<double, ARDOUR::framepos_t>& time_converter () const {
		return *_time_converter;
	}

	std::pair<ARDOUR::framepos_t, ARDOUR::framepos_t> get_point_x_range () const;

	void set_maximum_time (ARDOUR::framecnt_t);
 	ARDOUR::framecnt_t maximum_time () const {
		return _maximum_time;
	}

	void set_offset (ARDOUR::framecnt_t);
	void set_width (ARDOUR::framecnt_t);

  protected:

	std::string    _name;
	guint32   _height;
	uint32_t  _line_color;

	boost::shared_ptr<ARDOUR::AutomationList> alist;
	Evoral::TimeConverter<double, ARDOUR::framepos_t>* _time_converter;
	/** true if _time_converter belongs to us (ie we should delete it) */
	bool _our_time_converter;

	bool    _visible                  : 1;
	bool    _uses_gain_mapping        : 1;
	bool    terminal_points_can_slide : 1;
	bool    update_pending            : 1;
	bool    no_draw                   : 1;
	bool    _is_boolean               : 1;
	bool    points_visible            : 1;
	bool    did_push;

	ArdourCanvas::Group&        _parent_group;
	ArdourCanvas::Group*        group;
	ArdourCanvas::Line*         line; /* line */
	ArdourCanvas::Points        line_points; /* coordinates for canvas line */
	std::vector<ControlPoint*>  control_points; /* visible control points */

	void sync_model_with_view_point (ControlPoint&, bool, int64_t);
	void sync_model_with_view_points (std::list<ControlPoint*>, bool, int64_t);
	void start_drag_common (double, float);

	virtual void change_model (ARDOUR::AutomationList::iterator, double x, double y);

	void reset_callback (const Evoral::ControlList&);
	void list_changed ();

	virtual bool event_handler (GdkEvent*);

  private:
	std::list<ControlPoint*> _drag_points; ///< points we are dragging
	std::list<ControlPoint*> _push_points; ///< additional points we are dragging if "push" is enabled
	bool _drag_had_movement; ///< true if the drag has seen movement, otherwise false
	double _drag_x; ///< last x position of the drag, in units
	double _drag_distance; ///< total x movement of the drag, in units
	double _last_drag_fraction; ///< last y position of the drag, as a fraction
	/** offset from the start of the automation list to the start of the line, so that
	 *  a +ve offset means that the 0 on the line is at _offset in the list
	 */
	ARDOUR::framecnt_t _offset;

	void reset_line_coords (ControlPoint&);
	void add_visible_control_point (uint32_t, uint32_t, double, double, ARDOUR::AutomationList::iterator, uint32_t);
	double control_point_box_size ();
	void connect_to_list ();
	void interpolation_changed (ARDOUR::AutomationList::InterpolationStyle);

	PBD::ScopedConnectionList _list_connections;

	/** maximum time that a point on this line can be at, relative to the position of its region or start of its track */
	ARDOUR::framecnt_t _maximum_time;

	friend class AudioRegionGainLine;
};

#endif /* __ardour_automation_line_h__ */

