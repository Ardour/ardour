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

#include "canvas.h"
#include "time_axis_view.h"
#include "simplerect.h"
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
	AutomationTimeAxisView (ARDOUR::Session&,
				boost::shared_ptr<ARDOUR::Route>,
				boost::shared_ptr<ARDOUR::Automatable>,
				boost::shared_ptr<ARDOUR::AutomationControl>,
				PublicEditor&,
				TimeAxisView& parent,
				bool show_regions,
				ArdourCanvas::Canvas& canvas,
				const std::string & name, /* translatable */
				const std::string & plug_name = "");

	~AutomationTimeAxisView();
	
	virtual void set_height (uint32_t);
	void set_samples_per_unit (double);
	std::string name() const { return _name; }

	void add_automation_event (ArdourCanvas::Item *item, GdkEvent *event, nframes_t, double);

	void clear_lines ();
	boost::shared_ptr<AutomationLine> line() { return _line; }

	void set_selected_points (PointSelection&);
	void get_selectables (nframes_t start, nframes_t end, double top, double bot, std::list<Selectable *>&);
	void get_inverted_selectables (Selection&, std::list<Selectable*>& results);

	void show_timestretch (nframes_t start, nframes_t end) {}
	void hide_timestretch () {}

	/* editing operations */
	
	bool cut_copy_clear (Selection&, Editing::CutCopyOp);
	bool cut_copy_clear_objects (PointSelection&, Editing::CutCopyOp);
	bool paste (nframes_t, float times, Selection&, size_t nth);
	void reset_objects (PointSelection&);

	int  set_state (const XMLNode&);
	
	guint32 show_at (double y, int& nth, Gtk::VBox *parent);
	void hide ();
	
	static const std::string state_node_name;
	XMLNode* get_state_node();
	
	boost::shared_ptr<ARDOUR::AutomationControl> control()    { return _control; }
	boost::shared_ptr<AutomationController>      controller() { return _controller; }

  protected:
	boost::shared_ptr<ARDOUR::Route> _route; ///< Parent route
	boost::shared_ptr<ARDOUR::AutomationControl> _control; ///< Control
	boost::shared_ptr<ARDOUR::Automatable> _automatable; ///< Control owner, maybe = _route
	
	boost::shared_ptr<AutomationController> _controller;
	
	ArdourCanvas::SimpleRect* _base_rect;
	boost::shared_ptr<AutomationLine> _line;
	AutomationStreamView*             _view;
	
	std::string _name;
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

	void add_line (boost::shared_ptr<AutomationLine>);
	
	void clear_clicked ();
	void hide_clicked ();
	void auto_clicked ();

	void build_display_menu ();

	bool cut_copy_clear_one (AutomationLine&, Selection&, Editing::CutCopyOp);
	bool cut_copy_clear_objects_one (AutomationLine&, PointSelection&, Editing::CutCopyOp);
	bool paste_one (AutomationLine&, nframes_t, float times, Selection&, size_t nth);
	void reset_objects_one (AutomationLine&, PointSelection&);

	void set_automation_state (ARDOUR::AutoState);
	bool ignore_state_request;

	void automation_state_changed ();

	void set_interpolation (ARDOUR::AutomationList::InterpolationStyle);
	void interpolation_changed ();

	sigc::connection automation_connection;

	void update_extra_xml_shown (bool editor_shown);

	void entered ();
	void exited ();

	//void set_colors ();
	void color_handler ();

	static Pango::FontDescription* name_font;
	static bool have_name_font;
};

#endif /* __ardour_gtk_automation_time_axis_h__ */
