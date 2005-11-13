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

    $Id$
*/

#ifndef __ardour_automation_line_h__
#define __ardour_automation_line_h__

#include <vector>
#include <list>
#include <string>
#include <sys/types.h>

#include <gtkmm.h>
#include <libgnomecanvasmm/libgnomecanvasmm.h>
#include <sigc++/signal.h>

#include <pbd/undo.h>

#include <ardour/automation_event.h>


using std::vector;
using std::string;

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

class ControlPoint 
{
  public:
        ControlPoint (AutomationLine& al, sigc::slot<bool,GdkEvent*,ControlPoint*>);
	ControlPoint (const ControlPoint&, bool dummy_arg_to_force_special_copy_constructor);
	~ControlPoint ();

	enum ShapeType {
		Full,
		Start,
		End
	};
	
	void move_to (double x, double y, ShapeType);
	void reset (double x, double y, ARDOUR::AutomationList::iterator, uint32_t, ShapeType);
	double get_x() const { return _x; }
	double get_y() const { return _y; }

	void hide (); 
	void show ();
	void show_color (bool entered, bool hide_too);

	void set_size (double);
	void set_visible (bool);

	Gnome::Canvas::SimpleRect* item;
	AutomationLine& line;
	uint32_t view_index;
	ARDOUR::AutomationList::iterator model;
	bool can_slide;
	bool selected;
	
  private:
	double _x;
	double _y;
	double _size;
	ShapeType _shape;
};

class AutomationLine : public sigc::trackable
{
  public:
        AutomationLine (string name, TimeAxisView&, Gnome::Canvas::Group&, ARDOUR::AutomationList&,
			sigc::slot<bool,GdkEvent*,ControlPoint*>, sigc::slot<bool,GdkEvent*,AutomationLine*>);

	virtual ~AutomationLine ();

	void queue_reset ();
	void reset ();
	void clear();

	void set_selected_points (PointSelection&);
	void get_selectables (jack_nframes_t& start, jack_nframes_t& end,
			      double botfrac, double topfrac, 
			      list<Selectable*>& results);
	void get_inverted_selectables (Selection&, list<Selectable*>& results);

	virtual void remove_point (ControlPoint&);
	bool control_points_adjacent (double xval, uint32_t& before, uint32_t& after);
	
	/* dragging API */

	virtual void start_drag (ControlPoint*, float fraction);
	virtual void point_drag(ControlPoint&, jack_nframes_t x, float, bool with_push);
	virtual void end_drag (ControlPoint*);
	virtual void line_drag(uint32_t i1, uint32_t i2, float, bool with_push);

	ControlPoint* nth (uint32_t);
	uint32_t npoints() const { return control_points.size(); }

	string  name() const { return _name; }
	bool    visible() const { return _visible; }
	guint32 height() const { return _height; }

	void         set_line_color (uint32_t);
	uint32_t get_line_color() const { return _line_color; }

	void    show ();
	void    hide ();
	void    set_height (guint32);
	void    set_verbose_cursor_uses_gain_mapping (bool yn);

	TimeAxisView& trackview;

	Gnome::Canvas::Group& canvas_group() const { return *group; }
	Gnome::Canvas::Item&  parent_group() const { return _parent_group; }
	Gnome::Canvas::Item&  grab_item() const { return *line; }

	void show_selection();
	void hide_selection ();

	void set_point_size (double size);

	virtual string  get_verbose_cursor_string (float);
	virtual void view_to_model_y (double&) = 0;
	virtual void model_to_view_y (double&) = 0;

	ARDOUR::AutomationList& the_list() const { return alist; }

	void show_all_control_points ();
	void hide_all_but_selected_control_points ();

	bool is_last_point (ControlPoint &);
	bool is_first_point (ControlPoint &);

  protected:
	string _name;
	guint32 _height;
	uint32_t _line_color;
	ARDOUR::AutomationList& alist;

	bool    _visible  : 1;
	bool    _vc_uses_gain_mapping : 1;
	bool    terminal_points_can_slide : 1;
	bool    update_pending : 1;
	bool    no_draw : 1;
	bool    points_visible : 1;
	
	Gnome::Canvas::Group&  _parent_group;
	Gnome::Canvas::Group*   group;
	Gnome::Canvas::Line*   line; /* line */
	Gnome::Canvas::Points  line_points; /* coordinates for canvas line */
	vector<ControlPoint*>  control_points; /* visible control points */

	sigc::slot<bool,GdkEvent*,ControlPoint*> point_slot;

	struct ALPoint {
	    double x;
	    double y;
	    ALPoint (double xx, double yy) : x(xx), y(yy) {}
	};

	typedef std::vector<ALPoint> ALPoints;

	static void invalidate_point (ALPoints&, uint32_t index);
	static bool invalid_point (ALPoints&, uint32_t index);
	
	void determine_visible_control_points (ALPoints&);
	void sync_model_from (ControlPoint&);
	void sync_model_with_view_point (ControlPoint&);
	void sync_model_with_view_line (uint32_t, uint32_t);
	void modify_view (ControlPoint&, double, double, bool with_push);
	
	virtual void change_model (ARDOUR::AutomationList::iterator, double x, double y);
	virtual void change_model_range (ARDOUR::AutomationList::iterator,ARDOUR::AutomationList::iterator, double delta, float ydelta);

	void reset_callback (const ARDOUR::AutomationList&);
	void list_changed (ARDOUR::Change);

	UndoAction get_memento();
	
  private:
	uint32_t drags;
	double   first_drag_fraction;
	double   last_drag_fraction;
	uint32_t line_drag_cp1;
	uint32_t line_drag_cp2;

	void modify_view_point(ControlPoint&, double, double, bool with_push);
	void reset_line_coords (ControlPoint&);
	void update_line ();

	struct ModelRepresentation {
	    ARDOUR::AutomationList::iterator start;
	    ARDOUR::AutomationList::iterator end;
	    jack_nframes_t xpos;
	    double ypos;
	    jack_nframes_t xmin;
	    double ymin;
	    jack_nframes_t xmax;
	    double ymax;
	    jack_nframes_t xval;
	    double yval;
	};

	void model_representation (ControlPoint&, ModelRepresentation&);

	friend class AudioRegionGainLine;
};

#endif /* __ardour_automation_line_h__ */

