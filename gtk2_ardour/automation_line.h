/*
 * Copyright (C) 2005-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Karsten Wiese <fzuuzf@googlemail.com>
 * Copyright (C) 2005 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006 Hans Fugal <hans@fugal.net>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2007-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_automation_line_h__
#define __ardour_automation_line_h__

#include <vector>
#include <list>
#include <string>
#include <sys/types.h>

#include <sigc++/signal.h>

#include "pbd/undo.h"
#include "pbd/statefuldestructible.h"
#include "pbd/memento_command.h"

#include "temporal/time_converter.h"

#include "ardour/automation_list.h"
#include "ardour/parameter_descriptor.h"
#include "ardour/types.h"

#include "canvas/types.h"
#include "canvas/container.h"
#include "canvas/poly_line.h"

class AutomationLine;
class ControlPoint;
class PointSelection;
class TimeAxisView;
class AutomationTimeAxisView;
class Selectable;
class Selection;
class PublicEditor;


/** A GUI representation of an ARDOUR::AutomationList */
class AutomationLine : public sigc::trackable, public PBD::StatefulDestructible
{
public:
	enum VisibleAspects {
		Line = 0x1,
		ControlPoints = 0x2,
		SelectedControlPoints = 0x4
	};

	AutomationLine (const std::string&                                 name,
	                TimeAxisView&                                      tv,
	                ArdourCanvas::Item&                                parent,
	                boost::shared_ptr<ARDOUR::AutomationList>          al,
	                const ARDOUR::ParameterDescriptor&                 desc);

	virtual ~AutomationLine ();

	void queue_reset ();
	void reset ();
	void clear ();
	void set_fill (bool f) { _fill = f; } // owner needs to call set_height

	void set_selected_points (PointSelection const &);
	void get_selectables (Temporal::timepos_t const &, Temporal::timepos_t const &, double, double, std::list<Selectable*>&);
	void get_inverted_selectables (Selection&, std::list<Selectable*>& results);

	virtual void remove_point (ControlPoint&);
	bool control_points_adjacent (double xval, uint32_t& before, uint32_t& after);

	/* dragging API */
	virtual void start_drag_single (ControlPoint*, double, float);
	virtual void start_drag_line (uint32_t, uint32_t, float);
	virtual void start_drag_multiple (std::list<ControlPoint*>, float, XMLNode *);
	virtual std::pair<float, float> drag_motion (double, float, bool, bool with_push, uint32_t& final_index);
	virtual void end_drag (bool with_push, uint32_t final_index);

	ControlPoint* nth (uint32_t);
	ControlPoint const * nth (uint32_t) const;
	uint32_t npoints() const { return control_points.size(); }

	std::string  name()    const { return _name; }
	bool    visible() const { return _visible != VisibleAspects(0); }
	guint32 height()  const { return _height; }

	void     set_line_color (uint32_t);
	uint32_t get_line_color() const { return _line_color; }

	void set_visibility (VisibleAspects);
	void add_visibility (VisibleAspects);
	void remove_visibility (VisibleAspects);

	void hide ();
	void set_height (guint32);

	bool get_uses_gain_mapping () const;

	TimeAxisView& trackview;

	ArdourCanvas::Container& canvas_group() const { return *group; }
	ArdourCanvas::Item&  parent_group() const { return _parent_group; }
	ArdourCanvas::Item&  grab_item() const { return *line; }

	virtual std::string get_verbose_cursor_string (double) const;
	std::string get_verbose_cursor_relative_string (double, double) const;
	std::string fraction_to_string (double) const;
	std::string delta_to_string (double) const;
	double string_to_fraction (std::string const &) const;
	Temporal::timepos_t view_to_model_coord (double& x, double& y) const;
	void   view_to_model_coord_y (double &) const;
	Temporal::timepos_t model_to_view_coord (Evoral::ControlEvent const &, double& y) const;
	void   model_to_view_coord_y (double &) const;

	double compute_delta (double from, double to) const;
	void   apply_delta (double& val, double delta) const;

	void set_list(boost::shared_ptr<ARDOUR::AutomationList> list);
	boost::shared_ptr<ARDOUR::AutomationList> the_list() const { return alist; }

	void track_entered();
	void track_exited();

	bool is_last_point (ControlPoint &);
	bool is_first_point (ControlPoint &);

	XMLNode& get_state (void);
	int set_state (const XMLNode&, int version);
	void set_colors();

	void modify_point_y (ControlPoint&, double);

	virtual MementoCommandBinder<ARDOUR::AutomationList>* memento_command_binder ();

	std::pair<ARDOUR::samplepos_t, ARDOUR::samplepos_t> get_point_x_range () const;

	void set_maximum_time (Temporal::timepos_t const &);
	Temporal::timepos_t maximum_time () const {
		return _maximum_time;
	}

	void set_offset (Temporal::timecnt_t const &);
	Temporal::timecnt_t offset () { return _offset; }
	void set_width (Temporal::timecnt_t const &);

	samplepos_t session_sample_position (ARDOUR::AutomationList::const_iterator) const;
	Temporal::timepos_t session_position (ARDOUR::AutomationList::const_iterator) const;

	Temporal::DistanceMeasure const & distance_measure () const { return _distance_measure; }
	void set_distance_measure_origin (Temporal::timepos_t const &);

protected:

	std::string    _name;
	guint32        _height;
	uint32_t       _line_color;

	boost::shared_ptr<ARDOUR::AutomationList> alist;

	VisibleAspects _visible;

	bool    terminal_points_can_slide;
	bool    update_pending;
	bool    have_timeout;
	bool    no_draw;
	bool    _is_boolean;
	/** true if we did a push at any point during the current drag */
	bool    did_push;

	ArdourCanvas::Item&         _parent_group;
	ArdourCanvas::Container*    group;
	ArdourCanvas::PolyLine*     line; /* line */
	ArdourCanvas::Points        line_points; /* coordinates for canvas line */
	std::vector<ControlPoint*>  control_points; /* visible control points */

	class ContiguousControlPoints : public std::list<ControlPoint*> {
public:
		ContiguousControlPoints (AutomationLine& al);
		double clamp_dx (double dx);
		void move (double dx, double dvalue);
		void compute_x_bounds (PublicEditor& e);
private:
		AutomationLine& line;
		double before_x;
		double after_x;
	};

	friend class ContiguousControlPoints;

	typedef boost::shared_ptr<ContiguousControlPoints> CCP;
	std::vector<CCP> contiguous_points;

	bool sync_model_with_view_point (ControlPoint&);
	bool sync_model_with_view_points (std::list<ControlPoint*>);
	void start_drag_common (double, float);

	void reset_callback (const Evoral::ControlList&);
	void list_changed ();

	virtual bool event_handler (GdkEvent*);

private:
	std::list<ControlPoint*> _drag_points; ///< points we are dragging
	std::list<ControlPoint*> _push_points; ///< additional points we are dragging if "push" is enabled
	bool _drag_had_movement; ///< true if the drag has seen movement, otherwise false
	double _drag_x; ///< last x position of the drag, in units
	double _drag_distance; ///< total x movement of the drag, in canvas units
	double _last_drag_fraction; ///< last y position of the drag, as a fraction
	/** offset from the start of the automation list to the start of the line, so that
	 *  a +ve offset means that the 0 on the line is at _offset in the list
	 */
	Temporal::timecnt_t _offset;

	bool is_stepped() const;
	void update_visibility ();
	void reset_line_coords (ControlPoint&);
	void add_visible_control_point (uint32_t, uint32_t, double, double, ARDOUR::AutomationList::iterator, uint32_t);
	double control_point_box_size ();
	void connect_to_list ();
	void interpolation_changed (ARDOUR::AutomationList::InterpolationStyle);

	PBD::ScopedConnectionList _list_connections;

	/** maximum time that a point on this line can be at, relative to the position of its region or start of its track */
	Temporal::timepos_t _maximum_time;

	bool _fill;

	const ARDOUR::ParameterDescriptor _desc;
	Temporal::DistanceMeasure _distance_measure;

	friend class AudioRegionGainLine;
};

#endif /* __ardour_automation_line_h__ */

