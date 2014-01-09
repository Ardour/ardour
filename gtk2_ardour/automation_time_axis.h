/*
    Copyright (C) 2000-2007 Paul Davis

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

#ifndef __ardour_gtk_automation_time_axis_h__
#define __ardour_gtk_automation_time_axis_h__

#include <list>
#include <string>
#include <utility>

#include <boost/shared_ptr.hpp>

#include "ardour/types.h"
#include "ardour/automatable.h"
#include "ardour/automation_list.h"

#include "canvas/rectangle.h"

#include "time_axis_view.h"
#include "automation_controller.h"

namespace ARDOUR {
	class Session;
	class Route;
	class AutomationControl;
}

class PublicEditor;
class TimeSelection;
class RegionSelection;
class PointSelection;
class AutomationLine;
class Selection;
class Selectable;
class AutomationStreamView;
class AutomationController;


class AutomationTimeAxisView : public TimeAxisView {
  public:
	AutomationTimeAxisView (ARDOUR::Session*,
				boost::shared_ptr<ARDOUR::Route>,
				boost::shared_ptr<ARDOUR::Automatable>,
				boost::shared_ptr<ARDOUR::AutomationControl>,
				Evoral::Parameter,
				PublicEditor&,
				TimeAxisView& parent,
				bool show_regions,
				ArdourCanvas::Canvas& canvas,
				const std::string & name, /* translatable */
				const std::string & plug_name = "");

	~AutomationTimeAxisView();

	virtual void set_height (uint32_t);
	void set_samples_per_pixel (double);
	std::string name() const { return _name; }

	void add_automation_event (GdkEvent *, framepos_t, double);

	void clear_lines ();

	/** @return Our AutomationLine, if this view has one, or 0 if it uses AutomationRegionViews */
	boost::shared_ptr<AutomationLine> line() { return _line; }

	/** @return All AutomationLines associated with this view */
	std::list<boost::shared_ptr<AutomationLine> > lines () const;

	void set_selected_points (PointSelection&);
	void get_selectables (ARDOUR::framepos_t start, ARDOUR::framepos_t end, double top, double bot, std::list<Selectable *>&);
	void get_inverted_selectables (Selection&, std::list<Selectable*>& results);

	void show_timestretch (framepos_t /*start*/, framepos_t /*end*/, int /*layers*/, int /*layer*/) {}
	void hide_timestretch () {}

	/* editing operations */

	void cut_copy_clear (Selection&, Editing::CutCopyOp);
	bool paste (ARDOUR::framepos_t, float times, Selection&, size_t nth);

	int  set_state (const XMLNode&, int version);

	std::string state_id() const;
	static bool parse_state_id (std::string const &, PBD::ID &, bool &, Evoral::Parameter &);

	boost::shared_ptr<ARDOUR::AutomationControl> control()    { return _control; }
	boost::shared_ptr<AutomationController>      controller() { return _controller; }
	Evoral::Parameter parameter () const {
		return _parameter;
	}

	ArdourCanvas::Item* base_item () const {
		return _base_rect;
	}

	bool has_automation () const;

	boost::shared_ptr<ARDOUR::Route> parent_route () {
		return _route;
	}

	bool show_regions () const {
		return _show_regions;
	}

	static void what_has_visible_automation (const boost::shared_ptr<ARDOUR::Automatable>& automatable, std::set<Evoral::Parameter>& visible);

  protected:
	/** parent route */
	boost::shared_ptr<ARDOUR::Route> _route;
	/** control; 0 if we are editing region-based automation */
	boost::shared_ptr<ARDOUR::AutomationControl> _control;
	/** control owner; may be _route, or 0 if we are editing region-based automation */
	boost::shared_ptr<ARDOUR::Automatable> _automatable;
	/** controller owner; 0 if we are editing region-based automation */
	boost::shared_ptr<AutomationController> _controller;
	Evoral::Parameter _parameter;

	ArdourCanvas::Rectangle* _base_rect;
	boost::shared_ptr<AutomationLine> _line;

	std::string _name;

	/** AutomationStreamView if we are editing region-based automation (for MIDI), otherwise 0 */
	AutomationStreamView* _view;

	bool    ignore_toggle;
	bool    first_call_to_set_height;

	Gtk::Button        hide_button;
	Gtk::Button        auto_button;
	Gtk::Menu*         automation_menu;
	Gtk::Label*        plugname;
	bool               plugname_packed;

	Gtk::CheckMenuItem*     auto_off_item;
	Gtk::CheckMenuItem*     auto_play_item;
	Gtk::CheckMenuItem*     auto_touch_item;
	Gtk::CheckMenuItem*     auto_write_item;

	Gtk::CheckMenuItem* mode_discrete_item;
	Gtk::CheckMenuItem* mode_line_item;

	bool _show_regions;

	void add_line (boost::shared_ptr<AutomationLine>);

	void clear_clicked ();
	void hide_clicked ();
	void auto_clicked ();

	void build_display_menu ();

	void cut_copy_clear_one (AutomationLine&, Selection&, Editing::CutCopyOp);
	bool paste_one (AutomationLine&, ARDOUR::framepos_t, float times, Selection&, size_t nth);
	void route_going_away ();

	void set_automation_state (ARDOUR::AutoState);
	bool ignore_state_request;

	void automation_state_changed ();

	void set_interpolation (ARDOUR::AutomationList::InterpolationStyle);
	void interpolation_changed (ARDOUR::AutomationList::InterpolationStyle);

	PBD::ScopedConnectionList _list_connections;
	PBD::ScopedConnectionList _route_connections;

	void entered ();
	void exited ();

	//void set_colors ();
	void color_handler ();

	static Pango::FontDescription name_font;
	static bool have_name_font;

private:
	int set_state_2X (const XMLNode &, int);
};

#endif /* __ardour_gtk_automation_time_axis_h__ */
