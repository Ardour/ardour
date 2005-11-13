#include <ardour/route.h>

#include "ardour_ui.h"
#include "automation_time_axis.h"
#include "automation_line.h"
#include "public_editor.h"
#include "canvas-simplerect.h"
#include "canvas-waveview.h"
#include "selection.h"
#include "ghostregion.h"
#include "rgb_macros.h"
#include "automation_selectable.h"
#include "point_selection.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace Gtk;
using namespace Editing;

static const gchar * small_x_xpm[] = {
"11 11 2 1",
" 	c None",
".	c #000000",
"           ",
"           ",
"  .     .  ",
"   .   .   ",
"    . .    ",
"     .     ",
"    . .    ",
"   .   .   ",
"  .     .  ",
"           ",
"           "};

AutomationTimeAxisView::AutomationTimeAxisView (Session& s, Route& r, PublicEditor& e, TimeAxisView& rent, Widget* p, std::string nom, std::string state_name, std::string nomparent)

	: AxisView (s), 
	  TimeAxisView (s, e, &rent, p),
	  route (r),
	  _name (nom),
	  _state_name (state_name),
	  height_button (_("h")),
	  clear_button (_("clear")),
	  auto_button (X_("")) /* force addition of a label */
{
	automation_menu = 0;
	in_destructor = false;
	auto_off_item = 0;
	auto_touch_item = 0;
	auto_write_item = 0;
	auto_play_item = 0;
	ignore_state_request = false;

	//	base_rect = gnome_canvas_item_new (GNOME_CANVAS_GROUP(canvas_display),
	//			 gnome_canvas_simplerect_get_type(),
	//			 "x1", 0.0,
	//			 "y1", 0.0,
	//			 "x2", 1000000.0,
	//			 "outline_color_rgba", color_map[cAutomationTrackOutline],
	//			 /* outline ends and bottom */
	//			 "outline_what", (guint32) (0x1|0x2|0x8),
	//			 "fill_color_rgba", color_map[cAutomationTrackFill],
	//			 NULL);
	base_rect = new Gnome::Canvas::SimpleRect(*canvas_display);
	base_rect->set_property ("x1", 0.0);
	base_rect->set_property ("y1", 0.0);
	base_rect->set_property ("x2", 1000000.0);
	base_rect->set_property ("outline_color_rgba", color_map[cAutomationTrackOutline]);
	/* outline ends and bottom */
	base_rect->set_property ("outline_what", (guint32) (0x1|0x2|0x8));
	base_rect->set_property ("fill_color_rgba", color_map[cAutomationTrackFill]);
	
	base_rect->set_data ("trackview", this);

	gtk_signal_connect (GTK_OBJECT(base_rect), "event",
			    (GtkSignalFunc) PublicEditor::canvas_automation_track_event,
			    this);

	hide_button.add (*(manage (new Gtk::Image (Gdk::Pixbuf::create_from_xpm_data(small_x_xpm)))));

	height_button.set_name ("TrackSizeButton");
	auto_button.set_name ("TrackVisualButton");
	clear_button.set_name ("TrackVisualButton");
	hide_button.set_name ("TrackRemoveButton");

	ARDOUR_UI::instance()->tooltips().set_tip(height_button, _("track height"));
	ARDOUR_UI::instance()->tooltips().set_tip(auto_button, _("automation state"));
	ARDOUR_UI::instance()->tooltips().set_tip(clear_button, _("clear track"));
	ARDOUR_UI::instance()->tooltips().set_tip(hide_button, _("hide track"));

	/* rearrange the name display */

	/* we never show these for automation tracks, so make
	   life easier and remove them.
	*/

	name_hbox.remove (name_entry);

	/* move the name label over a bit */

	string shortpname = _name;
	bool shortened = false;
	
	if (_name.length()) {
		if (shortpname.length() > 18) {
			shortpname = shortpname.substr (0, 16);
			shortpname += "...";
			shortened = true;
		}
	}
	name_label.set_text (shortpname);
	name_label.set_alignment (1.0, 0.5);

	if (nomparent.length()) {

		/* limit the plug name string */

		string pname = nomparent;

		if (pname.length() > 14) {
			pname = pname.substr (0, 11);
			pname += "...";
			shortened = true;
		}

 		plugname = new Label (pname);
		plugname->set_name (X_("TrackPlugName"));
		plugname->set_alignment (1.0, 0.5);
		name_label.set_name (X_("TrackParameterName"));
		controls_table.remove (name_hbox);
		controls_table.attach (*plugname, 1, 6, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
		plugname_packed = true;
		controls_table.attach (name_hbox, 1, 6, 1, 2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	} else {
		plugname = 0;
		plugname_packed = false;
	}

	if (shortened) {
		string tipname = nomparent;
		if (!tipname.empty()) {
			tipname += ": ";
		}
		tipname += _name;
		ARDOUR_UI::instance()->tooltips().set_tip(controls_ebox, tipname);
	}
	
	/* add the buttons */
	controls_table.attach (hide_button, 0, 1, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	controls_table.attach (height_button, 0, 1, 1, 2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);

	controls_table.attach (auto_button, 7, 9, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	controls_table.attach (clear_button, 7, 9, 1, 2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
	
	controls_table.show_all ();

	height_button.signal_clicked().connect (mem_fun(*this, &AutomationTimeAxisView::height_clicked));
	clear_button.signal_clicked().connect (mem_fun(*this, &AutomationTimeAxisView::clear_clicked));
	hide_button.signal_clicked().connect (mem_fun(*this, &AutomationTimeAxisView::hide_clicked));
	auto_button.signal_clicked().connect (mem_fun(*this, &AutomationTimeAxisView::auto_clicked));

	controls_base_selected_name = X_("AutomationTrackControlsBaseSelected");
	controls_base_unselected_name = X_("AutomationTrackControlsBase");
	controls_ebox.set_name (controls_base_unselected_name);

	controls_frame.set_shadow_type (Gtk::SHADOW_ETCHED_OUT);

	XMLNode* xml_node = get_parent_with_state()->get_child_xml_node (_state_name);
	set_state (*xml_node);

	/* make sure labels etc. are correct */

	automation_state_changed ();
}

AutomationTimeAxisView::~AutomationTimeAxisView ()
{
	in_destructor = true;

	for (list<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); ++i) {
		delete *i;
	}
}

void
AutomationTimeAxisView::auto_clicked ()
{
	using namespace Menu_Helpers;

	if (automation_menu == 0) {
		automation_menu = manage (new Menu);
		automation_menu->set_name ("ArdourContextMenu");
		MenuList& items (automation_menu->items());

		items.push_back (MenuElem (_("off"), 
					   bind (mem_fun(*this, &AutomationTimeAxisView::set_automation_state), (AutoState) Off)));
		items.push_back (MenuElem (_("play"),
					   bind (mem_fun(*this, &AutomationTimeAxisView::set_automation_state), (AutoState) Play)));
		items.push_back (MenuElem (_("write"),
					   bind (mem_fun(*this, &AutomationTimeAxisView::set_automation_state), (AutoState) Write)));
		items.push_back (MenuElem (_("touch"),
					   bind (mem_fun(*this, &AutomationTimeAxisView::set_automation_state), (AutoState) Touch)));
	}

	automation_menu->popup (1, 0);
}


void
AutomationTimeAxisView::automation_state_changed ()
{
	AutoState state;

	/* update button label */

	if (lines.empty()) {
		state = Off;
	} else {
		state = lines.front()->the_list().automation_state ();
	}

	switch (state & (Off|Play|Touch|Write)) {
	case Off:
		static_cast<Gtk::Label*>(auto_button.get_child())->set_text (_("off"));
		if (auto_off_item) {
			ignore_state_request = true;
			auto_off_item->set_active (true);
			auto_play_item->set_active (false);
			auto_touch_item->set_active (false);
			auto_write_item->set_active (false);
			ignore_state_request = false;
		}
		break;
	case Play:
		static_cast<Gtk::Label*>(auto_button.get_child())->set_text (_("play"));
		if (auto_play_item) {
			ignore_state_request = true;
			auto_play_item->set_active (true);
			auto_off_item->set_active (false);
			auto_touch_item->set_active (false);
			auto_write_item->set_active (false);
			ignore_state_request = false;
		}
		break;
	case Write:
		static_cast<Gtk::Label*>(auto_button.get_child())->set_text (_("write"));
		if (auto_write_item) {
			ignore_state_request = true;
			auto_write_item->set_active (true);
			auto_off_item->set_active (false);
			auto_play_item->set_active (false);
			auto_touch_item->set_active (false);
			ignore_state_request = false;
		}
		break;
	case Touch:
		static_cast<Gtk::Label*>(auto_button.get_child())->set_text (_("touch"));
		if (auto_touch_item) {
			ignore_state_request = true;
			auto_touch_item->set_active (true);
			auto_off_item->set_active (false);
			auto_play_item->set_active (false);
			auto_write_item->set_active (false);
			ignore_state_request = false;
		}
		break;
	default:
		static_cast<Gtk::Label*>(auto_button.get_child())->set_text (_("???"));
		break;
	}
}

void
AutomationTimeAxisView::height_clicked ()
{
	popup_size_menu (0);
}

void
AutomationTimeAxisView::clear_clicked ()
{
	_session.begin_reversible_command (_("clear automation"));
	for (vector<AutomationLine*>::iterator i = lines.begin(); i != lines.end(); ++i) {
		(*i)->clear ();
	}
	_session.commit_reversible_command ();
}

void
AutomationTimeAxisView::set_height (TrackHeight h)
{
	bool changed = (height != (uint32_t) h);

	TimeAxisView* state_parent = get_parent_with_state ();
	XMLNode* xml_node = state_parent->get_child_xml_node (_state_name);

	controls_table.show_all ();

	TimeAxisView::set_height (h);
	gtk_object_set (GTK_OBJECT(base_rect), "y2", (double) h, NULL);

	for (vector<AutomationLine*>::iterator i = lines.begin(); i != lines.end(); ++i) {
		(*i)->set_height (h);
	}

	for (list<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); ++i) {
		(*i)->set_height ();
	}

	switch (height) {
	case Largest:
		xml_node->add_property ("track_height", "largest");
		controls_table.remove (name_hbox);
		if (plugname) {
			if (plugname_packed) {
				controls_table.remove (*plugname);
				plugname_packed = false;
			}
			controls_table.attach (*plugname, 1, 6, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
			plugname_packed = true;
			controls_table.attach (name_hbox, 1, 6, 1, 2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
		} else {
			controls_table.attach (name_hbox, 1, 6, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
		}
		controls_table.show_all ();
		name_label.show ();
		break;

	case Large:
		xml_node->add_property ("track_height", "large");
		controls_table.remove (name_hbox);
		if (plugname) {
			if (plugname_packed) {
				controls_table.remove (*plugname);
				plugname_packed = false;
			}
			controls_table.attach (*plugname, 1, 6, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
			plugname_packed = true;
		} else {
			controls_table.attach (name_hbox, 1, 6, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
		}
		controls_table.show_all ();
		name_label.show ();
		break;

	case Larger:
		xml_node->add_property ("track_height", "larger");
		controls_table.remove (name_hbox);
		if (plugname) {
			if (plugname_packed) {
				controls_table.remove (*plugname);
				plugname_packed = false;
			}
			controls_table.attach (*plugname, 1, 6, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
			plugname_packed = true;
		} else {
			controls_table.attach (name_hbox, 1, 6, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
		}
		controls_table.show_all ();
		name_label.show ();
		break;

	case Normal:
		xml_node->add_property ("track_height", "normal");
		controls_table.remove (name_hbox);
		if (plugname) {
			if (plugname_packed) {
				controls_table.remove (*plugname);
				plugname_packed = false;
			}
			controls_table.attach (*plugname, 1, 6, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
			plugname_packed = true;
			controls_table.attach (name_hbox, 1, 6, 1, 2, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
		} else {
			controls_table.attach (name_hbox, 1, 6, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
		}
		controls_table.show_all ();
		name_label.show ();
		break;

	case Smaller:
		xml_node->add_property ("track_height", "smaller");
		controls_table.remove (name_hbox);
		if (plugname) {
			if (plugname_packed) {
				controls_table.remove (*plugname);
				plugname_packed = false;
			}
		}
		controls_table.attach (name_hbox, 1, 6, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
		controls_table.hide_all ();
		name_hbox.show_all ();
		controls_table.show ();
		break;

	case Small:
		xml_node->add_property ("track_height", "small");
		controls_table.remove (name_hbox);
		if (plugname) {
			if (plugname_packed) {
				controls_table.remove (*plugname);
				plugname_packed = false;
			}
		} 
		controls_table.attach (name_hbox, 1, 6, 0, 1, Gtk::FILL|Gtk::EXPAND, Gtk::FILL|Gtk::EXPAND);
		controls_table.hide_all ();
		name_hbox.show_all ();
		controls_table.show ();
		break;
	}

	if (changed) {
		/* only emit the signal if the height really changed */
		 route.gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
	}
}

void
AutomationTimeAxisView::set_samples_per_unit (double spu)
{
	TimeAxisView::set_samples_per_unit (editor.get_current_zoom());

	for (vector<AutomationLine*>::iterator i = lines.begin(); i != lines.end(); ++i) {
		(*i)->reset ();
	}
}
 
void
AutomationTimeAxisView::hide_clicked ()
{
	set_marked_for_display (false);
	hide ();
}


void
AutomationTimeAxisView::build_display_menu ()
{
	using namespace Menu_Helpers;

	/* get the size menu ready */

	build_size_menu ();

	/* prepare it */

	TimeAxisView::build_display_menu ();

	/* now fill it with our stuff */

	MenuList& items = display_menu->items();

	items.push_back (MenuElem (_("Height"), *size_menu));
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Hide"), mem_fun(*this, &AutomationTimeAxisView::hide_clicked)));
	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Clear"), mem_fun(*this, &AutomationTimeAxisView::clear_clicked)));
	items.push_back (SeparatorElem());

	Menu* auto_state_menu = manage (new Menu);
	auto_state_menu->set_name ("ArdourContextMenu");
	MenuList& as_items = auto_state_menu->items();
	
	as_items.push_back (CheckMenuElem (_("off"), 
					   bind (mem_fun(*this, &AutomationTimeAxisView::set_automation_state), (AutoState) Off)));
	auto_off_item = dynamic_cast<CheckMenuItem*>(&as_items.back());

	as_items.push_back (CheckMenuElem (_("play"),
					   bind (mem_fun(*this, &AutomationTimeAxisView::set_automation_state), (AutoState) Play)));
	auto_play_item = dynamic_cast<CheckMenuItem*>(&as_items.back());

	as_items.push_back (CheckMenuElem (_("write"),
					   bind (mem_fun(*this, &AutomationTimeAxisView::set_automation_state), (AutoState) Write)));
	auto_write_item = dynamic_cast<CheckMenuItem*>(&as_items.back());

	as_items.push_back (CheckMenuElem (_("touch"),
					   bind (mem_fun(*this, &AutomationTimeAxisView::set_automation_state), (AutoState) Touch)));
	auto_touch_item = dynamic_cast<CheckMenuItem*>(&as_items.back());

	items.push_back (MenuElem (_("State"), *auto_state_menu));

	/* make sure the automation menu state is correct */

	automation_state_changed ();
}

bool
AutomationTimeAxisView::cut_copy_clear (Selection& selection, CutCopyOp op)
{
	bool ret = false;

	for (vector<AutomationLine*>::iterator i = lines.begin(); i != lines.end(); ++i) {
		ret = cut_copy_clear_one ((**i), selection, op);
	}

	return ret;
}

bool
AutomationTimeAxisView::cut_copy_clear_one (AutomationLine& line, Selection& selection, CutCopyOp op)
{
	AutomationList* what_we_got = 0;
	AutomationList& alist (line.the_list());
	bool ret = false;

	_session.add_undo (alist.get_memento());

	switch (op) {
	case Cut:
		if ((what_we_got = alist.cut (selection.time.front().start, selection.time.front().end)) != 0) {
			editor.get_cut_buffer().add (what_we_got);
			_session.add_redo_no_execute (alist.get_memento());
			ret = true;
		}
		break;
	case Copy:
		if ((what_we_got = alist.copy (selection.time.front().start, selection.time.front().end)) != 0) {
			editor.get_cut_buffer().add (what_we_got);
		}
		break;

	case Clear:
		if ((what_we_got = alist.cut (selection.time.front().start, selection.time.front().end)) != 0) {
			_session.add_redo_no_execute (alist.get_memento());
			delete what_we_got;
			what_we_got = 0;
			ret = true;
		}
		break;
	}

	if (what_we_got) {
		for (AutomationList::iterator x = what_we_got->begin(); x != what_we_got->end(); ++x) {
			double foo = (*x)->value;
			line.model_to_view_y (foo);
			(*x)->value = foo;
		}
	}

	return ret;
}

bool
AutomationTimeAxisView::cut_copy_clear_objects (PointSelection& selection, CutCopyOp op)
{
	bool ret = false;

	for (vector<AutomationLine*>::iterator i = lines.begin(); i != lines.end(); ++i) {
		ret = cut_copy_clear_objects_one ((**i), selection, op);
	}

	return ret;
}

bool
AutomationTimeAxisView::cut_copy_clear_objects_one (AutomationLine& line, PointSelection& selection, CutCopyOp op)
{
	AutomationList* what_we_got = 0;
	AutomationList& alist (line.the_list());
	bool ret = false;

	_session.add_undo (alist.get_memento());

	for (PointSelection::iterator i = selection.begin(); i != selection.end(); ++i) {

		if (&(*i).track != this) {
			continue;
		}

		switch (op) {
		case Cut:
			if ((what_we_got = alist.cut ((*i).start, (*i).end)) != 0) {
				editor.get_cut_buffer().add (what_we_got);
				_session.add_redo_no_execute (alist.get_memento());
				ret = true;
			}
			break;
		case Copy:
			if ((what_we_got = alist.copy ((*i).start, (*i).end)) != 0) {
				editor.get_cut_buffer().add (what_we_got);
			}
			break;
			
		case Clear:
			if ((what_we_got = alist.cut ((*i).start, (*i).end)) != 0) {
				_session.add_redo_no_execute (alist.get_memento());
				delete what_we_got;
				what_we_got = 0;
				ret = true;
			}
			break;
		}
	}
		
	if (what_we_got) {
		for (AutomationList::iterator x = what_we_got->begin(); x != what_we_got->end(); ++x) {
			double foo = (*x)->value;
			line.model_to_view_y (foo);
			(*x)->value = foo;
		}
	}

	return ret;
}

bool
AutomationTimeAxisView::paste (jack_nframes_t pos, float times, Selection& selection, size_t nth)
{
	bool ret = true;

	for (vector<AutomationLine*>::iterator i = lines.begin(); i != lines.end(); ++i) {
		ret = paste_one (**i, pos, times, selection, nth);
	}

	return ret;
}

bool
AutomationTimeAxisView::paste_one (AutomationLine& line, jack_nframes_t pos, float times, Selection& selection, size_t nth)
{
	AutomationSelection::iterator p;
	AutomationList& alist (line.the_list());
	
	for (p = selection.lines.begin(); p != selection.lines.end() && nth; ++p, --nth);

	if (p == selection.lines.end()) {
		return false;
	}

	/* Make a copy of the list because we have to scale the
	   values from view coordinates to model coordinates, and we're
	   not supposed to modify the points in the selection.
	*/
	   
	AutomationList copy (**p);

	for (AutomationList::iterator x = copy.begin(); x != copy.end(); ++x) {
		double foo = (*x)->value;
		line.view_to_model_y (foo);
		(*x)->value = foo;
	}

	_session.add_undo (alist.get_memento());
	alist.paste (copy, pos, times);
	_session.add_redo_no_execute (alist.get_memento());

	return true;
}

void
AutomationTimeAxisView::add_ghost (GhostRegion* gr)
{
	ghosts.push_back (gr);
	gr->GoingAway.connect (mem_fun(*this, &AutomationTimeAxisView::remove_ghost));
}

void
AutomationTimeAxisView::remove_ghost (GhostRegion* gr)
{
	if (in_destructor) {
		return;
	}

	list<GhostRegion*>::iterator i;

	for (i = ghosts.begin(); i != ghosts.end(); ++i) {
		if ((*i) == gr) {
			ghosts.erase (i);
			break;
		}
	}
}

void
AutomationTimeAxisView::get_selectables (jack_nframes_t start, jack_nframes_t end, double top, double bot, list<Selectable*>& results)
{
	if (!lines.empty() && touched (top, bot)) {
		double topfrac;
		double botfrac;

		/* remember: this is X Window - coordinate space starts in upper left and moves down.
		   y_position is the "origin" or "top" of the track.
		*/

		double mybot = y_position + height; // XXX need to include Editor::track_spacing; 

		if (y_position >= top && mybot <= bot) {

			/* y_position is below top, mybot is above bot, so we're fully
			   covered vertically.
			*/

			topfrac = 1.0;
			botfrac = 0.0;

		} else {

			/* top and bot are within y_position .. mybot */

			topfrac = 1.0 - ((top - y_position) / height);
			botfrac = 1.0 - ((bot - y_position) / height);
		}

		for (vector<AutomationLine*>::iterator i = lines.begin(); i != lines.end(); ++i) {
			(*i)->get_selectables (start, end, botfrac, topfrac, results);
		}
	}
}

void
AutomationTimeAxisView::get_inverted_selectables (Selection& sel, list<Selectable*>& result)
{
	for (vector<AutomationLine*>::iterator i = lines.begin(); i != lines.end(); ++i) {
		(*i)->get_inverted_selectables (sel, result);
	}
}

void
AutomationTimeAxisView::set_selected_points (PointSelection& points)
{
	for (vector<AutomationLine*>::iterator i = lines.begin(); i != lines.end(); ++i) {
		(*i)->set_selected_points (points);
	}
}

void
AutomationTimeAxisView::clear_lines ()
{
	for (vector<AutomationLine*>::iterator i = lines.begin(); i != lines.end(); ++i) {
		delete *i;
	}

	lines.clear ();
	automation_connection.disconnect ();
}

void
AutomationTimeAxisView::add_line (AutomationLine& line)
{
	bool get = false;

	if (lines.empty()) {
		/* first line is the Model for automation state */
		automation_connection = line.the_list().automation_state_changed.connect
			(mem_fun(*this, &AutomationTimeAxisView::automation_state_changed));
		get = true;
	}

	lines.push_back (&line);
	line.set_height (height);

	if (get) {
		/* pick up the current state */
		automation_state_changed ();
	}
}

void
AutomationTimeAxisView::show_all_control_points ()
{
	for (vector<AutomationLine*>::iterator i = lines.begin(); i != lines.end(); ++i) {
		(*i)->show_all_control_points ();
	}
}

void
AutomationTimeAxisView::hide_all_but_selected_control_points ()
{
	for (vector<AutomationLine*>::iterator i = lines.begin(); i != lines.end(); ++i) {
		(*i)->hide_all_but_selected_control_points ();
	}
}

void
AutomationTimeAxisView::entered()
{
	show_all_control_points ();
}

void
AutomationTimeAxisView::exited ()
{
	hide_all_but_selected_control_points ();
}

void
AutomationTimeAxisView::set_state (const XMLNode& node)
{
	TimeAxisView::set_state (node);
}

XMLNode*
AutomationTimeAxisView::get_state_node ()
{
	TimeAxisView* state_parent = get_parent_with_state ();

	if (state_parent) {
		return state_parent->get_child_xml_node (_state_name);
	} else {
		return 0;
	}
}
