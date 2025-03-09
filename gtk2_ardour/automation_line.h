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

#ifndef __gtk2_ardour_automation_line_base_h__
#define __gtk2_ardour_automation_line_base_h__

#include <vector>
#include <list>
#include <string>
#include <sys/types.h>

#include <sigc++/signal.h>

#include "pbd/undo.h"
#include "pbd/statefuldestructible.h"
#include "pbd/memento_command.h"

#include "ardour/automation_list.h"
#include "ardour/parameter_descriptor.h"
#include "ardour/types.h"

#include "canvas/types.h"
#include "canvas/container.h"
#include "canvas/poly_line.h"

#include "selectable.h"

namespace ArdourCanvas {
	class Rectangle;
}

class EditorAutomationLine;
class ControlPoint;
class PointSelection;
class TimeAxisView;
class AutomationTimeAxisView;
class Selection;
class EditingContext;

/** A GUI representation of an ARDOUR::AutomationList */
class AutomationLine : public sigc::trackable, public PBD::StatefulDestructible, public SelectableOwner
{
public:
	enum VisibleAspects {
		Line = 0x1,
		ControlPoints = 0x2,
		SelectedControlPoints = 0x4
	};

	AutomationLine (const std::string&                      name,
	                    EditingContext&                         ec,
	                    ArdourCanvas::Item&                     parent,
	                    ArdourCanvas::Rectangle*                drag_base,
	                    std::shared_ptr<ARDOUR::AutomationList> al,
	                    const ARDOUR::ParameterDescriptor&      desc);

	virtual ~AutomationLine ();

	virtual Temporal::timepos_t get_origin () const;

	ArdourCanvas::Rectangle* drag_base() const { return _drag_base; }

	void set_sensitive (bool);
	bool sensitive() const { return _sensitive; }

	void queue_reset ();
	void reset ();
	void clear ();
	void set_fill (bool f) { _fill = f; } // owner needs to call set_height

	void set_selected_points (PointSelection const &);
	void _get_selectables (Temporal::timepos_t const &, Temporal::timepos_t const &, double, double, std::list<Selectable*>&, bool within);
	void get_inverted_selectables (Selection&, std::list<Selectable*>& results);

	virtual void remove_point (ControlPoint&);
	bool control_points_adjacent (double xval, uint32_t& before, uint32_t& after);

	/* dragging API */
	virtual void start_drag_single (ControlPoint*, double, float);
	virtual void start_drag_line (uint32_t, uint32_t, float);
	virtual void start_drag_multiple (std::list<ControlPoint*>, float, XMLNode *);
	virtual std::pair<float, float> drag_motion (Temporal::timecnt_t const &, float, bool, bool with_push, uint32_t& final_index);
	virtual void end_drag (bool with_push, uint32_t final_index);
	virtual void end_draw_merge () {}

	ControlPoint* nth (uint32_t);
	ControlPoint const * nth (uint32_t) const;
	uint32_t npoints() const { return control_points.size(); }

	std::string  name()    const { return _name; }
	bool    visible() const { return _visible != VisibleAspects(0); }
	guint32 height()  const { return _height; }

	void set_line_color (std::string const & color, std::string color_mode = std::string());
	void set_insensitive_line_color (uint32_t color);
	uint32_t get_line_color() const;
	uint32_t get_line_fill_color() const;
	uint32_t get_line_selected_color() const;
	bool control_points_inherit_color () const;
	void set_control_points_inherit_color (bool);

	void set_visibility (VisibleAspects);
	void add_visibility (VisibleAspects);
	void remove_visibility (VisibleAspects);

	void hide ();
	void hide_all ();
	void show ();
	void set_height (guint32);

	bool get_uses_gain_mapping () const;
	void tempo_map_changed ();

	ArdourCanvas::Container& canvas_group() const { return *group; }
	ArdourCanvas::Item&  parent_group() const { return _parent_group; }
	ArdourCanvas::Item&  grab_item() const { return *line; }

	virtual std::string get_verbose_cursor_string (double) const;
	std::string get_verbose_cursor_relative_string (double, double) const;
	std::string fraction_to_string (double) const;
	std::string delta_to_string (double) const;
	double string_to_fraction (std::string const &) const;

	void   view_to_model_coord_y (double &) const;

	double              model_to_view_coord_y (double) const;
	Temporal::timecnt_t model_to_view_coord_x (Temporal::timepos_t const &) const;

	double compute_delta (double from, double to) const;
	void   apply_delta (double& val, double delta) const;

	void set_list(std::shared_ptr<ARDOUR::AutomationList> list);
	std::shared_ptr<ARDOUR::AutomationList> the_list() const { return alist; }

	void track_entered();
	void track_exited();

	bool is_last_point (ControlPoint &);
	bool is_first_point (ControlPoint &);

	XMLNode& get_state () const;
	int set_state (const XMLNode&, int version);
	void set_colors();

	void modify_points_y (std::vector<ControlPoint*> const&, double);

	virtual MementoCommandBinder<ARDOUR::AutomationList>* memento_command_binder ();

	std::pair<Temporal::timepos_t, Temporal::timepos_t> get_point_x_range () const;

	void set_maximum_time (Temporal::timepos_t const &);
	Temporal::timepos_t maximum_time () const {
		return _maximum_time;
	}

	void set_offset (Temporal::timepos_t const &);
	Temporal::timepos_t offset () { return _offset; }
	void set_width (Temporal::timecnt_t const &);

	Temporal::timepos_t session_position (Temporal::timepos_t const &) const;
	void dump (std::ostream&) const;

	double dt_to_dx (Temporal::timepos_t const &, Temporal::timecnt_t const &);

	ARDOUR::ParameterDescriptor const & param() const { return _desc; }
	EditingContext& editing_context() const { return _editing_context; }

	void add (std::shared_ptr<ARDOUR::AutomationControl>, GdkEvent*, Temporal::timepos_t const &, double y, bool with_guard_points);

protected:

	std::string    _name;
	guint32        _height;
	std::string    _line_color_name;
	std::string    _line_color_mod;
	uint32_t       _insensitive_line_color;
	uint32_t       _view_index_offset;
	std::shared_ptr<ARDOUR::AutomationList> alist;

	VisibleAspects _visible;

	bool    terminal_points_can_slide;
	bool    update_pending;
	bool    have_reset_timeout;
	bool    no_draw;
	bool    _is_boolean;
	/** true if we did a push at any point during the current drag */
	bool    did_push;

	EditingContext&             _editing_context;
	ArdourCanvas::Item&         _parent_group;
	ArdourCanvas::Rectangle*    _drag_base;
	ArdourCanvas::Container*    group;
	ArdourCanvas::PolyLine*     line; /* line */
	ArdourCanvas::Points        line_points; /* coordinates for canvas line */
	std::vector<ControlPoint*>  control_points; /* visible control points */

	class ContiguousControlPoints : public std::list<ControlPoint*> {
          public:
		ContiguousControlPoints (AutomationLine& al);
		Temporal::timecnt_t clamp_dt (Temporal::timecnt_t const & dx, Temporal::timepos_t const & region_limit);
		void move (Temporal::timecnt_t const &, double dvalue);
		void compute_x_bounds ();
          private:
		AutomationLine& line;
		Temporal::timepos_t before_x;
		Temporal::timepos_t after_x;
	};

	friend class ContiguousControlPoints;

	typedef std::shared_ptr<ContiguousControlPoints> CCP;
	std::vector<CCP> contiguous_points;

	bool sync_model_with_view_point (ControlPoint&);
	bool sync_model_with_view_points (std::list<ControlPoint*>);
	void start_drag_common (double, float);

	void reset_callback (const Evoral::ControlList&);
	void list_changed ();

	virtual bool event_handler (GdkEvent*) = 0;

private:
	std::list<ControlPoint*> _drag_points; ///< points we are dragging
	std::list<ControlPoint*> _push_points; ///< additional points we are dragging if "push" is enabled
	bool _drag_had_movement; ///< true if the drag has seen movement, otherwise false
	double _last_drag_fraction; ///< last y position of the drag, as a fraction
	/** offset from the start of the automation list to the start of the line, so that
	 *  a +ve offset means that the 0 on the line is at _offset in the list
	 */
	Temporal::timepos_t _offset;

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
	bool _control_points_inherit_color;
	bool _sensitive;

	friend class AudioRegionGainLine;
	friend class RegionFxLine;
};

#endif /* __gtk2_ardour_automation_line_base_h__ */

