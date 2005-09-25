#ifndef __ardour_gtk_automation_time_axis_h__
#define __ardour_gtk_automation_time_axis_h__

#include <vector>
#include <list>
#include <string>
#include <ardour/types.h>

#include "time_axis_view.h"

using std::vector;
using std::list;
using std::string;

namespace ARDOUR {
	class Session;
	class Route;
}

namespace Gtk {
	class Widget;
}

class PublicEditor;
class TimeSelection;
class AudioRegionSelection;
class PointSelection;
class AutomationLine;
class GhostRegion;
class Selection;
class Selectable;

class AutomationTimeAxisView : public TimeAxisView {
  public:
	AutomationTimeAxisView (ARDOUR::Session&,
				ARDOUR::Route&,
				PublicEditor&,
				TimeAxisView& parent,
				Gtk::Widget* parent,
				std::string name, /* translatable */
				std::string state_name, /* not translatable */
				std::string plug_name = "");

	~AutomationTimeAxisView();
	
	void set_height (TimeAxisView::TrackHeight);
	void set_samples_per_unit (double);
	std::string name() const { return _name; }

	virtual void add_automation_event (GtkCanvasItem *item, GdkEvent *event, jack_nframes_t, double) = 0;

	void clear_lines ();
	void add_line (AutomationLine&);

	vector<AutomationLine*> lines;

	void set_selected_points (PointSelection&);
	void get_selectables (jack_nframes_t start, jack_nframes_t end, double top, double bot, list<Selectable *>&);
	void get_inverted_selectables (Selection&, list<Selectable*>& results);

	void show_timestretch (jack_nframes_t start, jack_nframes_t end) {}
	void hide_timestretch () {}

	/* editing operations */
	
	bool cut_copy_clear (Selection&, Editing::CutCopyOp);
	bool cut_copy_clear_objects (PointSelection&, Editing::CutCopyOp);
	bool paste (jack_nframes_t, float times, Selection&, size_t nth);

	void add_ghost (GhostRegion*);
	void remove_ghost (GhostRegion*);

	void show_all_control_points ();
	void hide_all_but_selected_control_points ();
	void set_state (const XMLNode&);
	XMLNode* get_state_node ();

  protected:
	ARDOUR::Route& route;
	GtkCanvasItem* base_rect;
	string _name;
	string _state_name;
	bool    in_destructor;

	Gtk::Button        hide_button;
	Gtk::Button        height_button;
	Gtk::Button        clear_button;
	Gtk::Button        auto_button; 
	Gtk::Menu*         automation_menu;
	Gtk::Label*        plugname;
	bool               plugname_packed;

	Gtk::CheckMenuItem*     auto_off_item;
	Gtk::CheckMenuItem*     auto_play_item;
	Gtk::CheckMenuItem*     auto_touch_item;
	Gtk::CheckMenuItem*     auto_write_item;

	void clear_clicked ();
	void height_clicked ();
	void hide_clicked ();
	void auto_clicked ();

	virtual void build_display_menu ();

	list<GhostRegion*> ghosts;

	bool cut_copy_clear_one (AutomationLine&, Selection&, Editing::CutCopyOp);
	bool cut_copy_clear_objects_one (AutomationLine&, PointSelection&, Editing::CutCopyOp);
	bool paste_one (AutomationLine&, jack_nframes_t, float times, Selection&, size_t nth);

	virtual void set_automation_state (ARDOUR::AutoState) = 0;
	bool ignore_state_request;

	void automation_state_changed ();
	sigc::connection automation_connection;

	void entered ();
	void exited ();
};

#endif /* __ardour_gtk_automation_time_axis_h__ */
