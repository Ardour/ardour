/*
    Copyright (C) 2000-2005 Paul Davis

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

#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <string>
#include <sstream>

#include "pbd/basename.h"
#include "pbd/enumwriter.h"

#include "ardour/audioregion.h"
#include "ardour/source.h"
#include "ardour/audiofilesource.h"
#include "ardour/silentfilesource.h"
#include "ardour/region_factory.h"
#include "ardour/session.h"
#include "ardour/profile.h"

#include "gtkmm2ext/treeutils.h"
#include "gtkmm2ext/utils.h"

#include "widgets/choice.h"
#include "widgets/tooltips.h"

#include "audio_clock.h"
#include "editor.h"
#include "editing.h"
#include "editing_convert.h"
#include "keyboard.h"
#include "ardour_ui.h"
#include "gui_thread.h"
#include "actions.h"
#include "region_view.h"
#include "utils.h"
#include "editor_drag.h"
#include "main_clock.h"
#include "ui_config.h"

#include "pbd/i18n.h"

#include "editor_sources.h"

using namespace std;
using namespace ARDOUR;
using namespace ArdourWidgets;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Gtk;
using namespace Glib;
using namespace Editing;
using Gtkmm2ext::Keyboard;

struct ColumnInfo {
	int         index;
	const char* label;
	const char* tooltip;
};

EditorSources::EditorSources (Editor* e)
	: EditorComponent (e)
	, old_focus (0)
	, _menu (0)
	, _selection (0)
{
	_display.set_size_request (100, -1);
	_display.set_rules_hint (true);
	_display.set_name ("SourcesList");
	_display.set_fixed_height_mode (true);

	/* Try to prevent single mouse presses from initiating edits.
	   This relies on a hack in gtktreeview.c:gtk_treeview_button_press()
	*/
	_display.set_data ("mouse-edits-require-mod1", (gpointer) 0x1);

	_model = TreeStore::create (_columns);
	_model->set_sort_column (0, SORT_ASCENDING);

	/* column widths */
	int bbt_width, date_width, height;

	Glib::RefPtr<Pango::Layout> layout = _display.create_pango_layout (X_("000|000|000"));
	Gtkmm2ext::get_pixel_size (layout, bbt_width, height);

	Glib::RefPtr<Pango::Layout> layout2 = _display.create_pango_layout (X_("2018-10-14 12:12:30"));
	Gtkmm2ext::get_pixel_size (layout2, date_width, height);

	TreeViewColumn* col_name = manage (new TreeViewColumn ("", _columns.name));
	col_name->set_fixed_width (bbt_width*2);
	col_name->set_sizing (TREE_VIEW_COLUMN_FIXED);
	col_name->set_sort_column(0);

	TreeViewColumn* col_take_id = manage (new TreeViewColumn ("", _columns.take_id));
	col_take_id->set_fixed_width (date_width);
	col_take_id->set_sizing (TREE_VIEW_COLUMN_FIXED);
	col_take_id->set_sort_column(1);

	TreeViewColumn* col_nat_pos = manage (new TreeViewColumn ("", _columns.natural_pos));
	col_nat_pos->set_fixed_width (bbt_width);
	col_nat_pos->set_sizing (TREE_VIEW_COLUMN_FIXED);
	col_nat_pos->set_sort_column(6);

	TreeViewColumn* col_path = manage (new TreeViewColumn ("", _columns.path));
	col_path->set_fixed_width (bbt_width);
	col_path->set_sizing (TREE_VIEW_COLUMN_FIXED);
	col_path->set_sort_column(3);

	_display.append_column (*col_name);
	_display.append_column (*col_take_id);
	_display.append_column (*col_nat_pos);
	_display.append_column (*col_path);

	TreeViewColumn* col;
	Gtk::Label* l;

	ColumnInfo ci[] = {
		{ 0,   _("Source"),    _("Source name, with number of channels in []'s") },
		{ 1,   _("Take ID"),   _("Take ID") },
		{ 2,   _("Orig Pos"),  _("Original Position of the file on timeline, when it was recorded") },
		{ 3,   _("Path"),      _("Path (folder) of the file locationlosition of end of region") },
		{ -1, 0, 0 }
	};

	for (int i = 0; ci[i].index >= 0; ++i) {
		col = _display.get_column (ci[i].index);
		l = manage (new Label (ci[i].label));
		set_tooltip (*l, ci[i].tooltip);
		col->set_widget (*l);
		l->show ();
	}
	_display.set_model (_model);

	_display.set_headers_visible (true);
	_display.set_rules_hint ();

	_display.get_selection()->set_select_function (sigc::mem_fun (*this, &EditorSources::selection_filter));

	//set the color of the name field
	TreeViewColumn* tv_col = _display.get_column(0);
	CellRendererText* renderer = dynamic_cast<CellRendererText*>(_display.get_column_cell_renderer (0));
	tv_col->add_attribute(renderer->property_text(), _columns.name);
	tv_col->add_attribute(renderer->property_foreground_gdk(), _columns.color_);

	//right-align the Natural Pos column
	TreeViewColumn* nat_col = _display.get_column(2);
	nat_col->set_alignment (ALIGN_RIGHT);
	renderer = dynamic_cast<CellRendererText*>(_display.get_column_cell_renderer (2));
	if (renderer) {
		renderer->property_xalign() = ( 1.0 );
	}

	//the PATH field should expand when the pane is opened wider
	tv_col = _display.get_column(3);
	renderer = dynamic_cast<CellRendererText*>(_display.get_column_cell_renderer (3));
	tv_col->add_attribute(renderer->property_text(), _columns.path);
	tv_col->set_expand (true);

	_display.get_selection()->set_mode (SELECTION_MULTIPLE);
	_display.add_object_drag (_columns.source.index(), "sources");
	_display.set_drag_column (_columns.name.index());

	/* setup DnD handling */

	list<TargetEntry> source_list_target_table;

	source_list_target_table.push_back (TargetEntry ("text/plain"));
	source_list_target_table.push_back (TargetEntry ("text/uri-list"));
	source_list_target_table.push_back (TargetEntry ("application/x-rootwin-drop"));

	_display.add_drop_targets (source_list_target_table);
	_display.signal_drag_data_received().connect (sigc::mem_fun(*this, &EditorSources::drag_data_received));

	_scroller.add (_display);
	_scroller.set_policy (POLICY_AUTOMATIC, POLICY_AUTOMATIC);

	_display.signal_button_press_event().connect (sigc::mem_fun(*this, &EditorSources::button_press), false);
	_change_connection = _display.get_selection()->signal_changed().connect (sigc::mem_fun(*this, &EditorSources::selection_changed));

	_scroller.signal_key_press_event().connect (sigc::mem_fun(*this, &EditorSources::key_press), false);
	_scroller.signal_focus_in_event().connect (sigc::mem_fun (*this, &EditorSources::focus_in), false);
	_scroller.signal_focus_out_event().connect (sigc::mem_fun (*this, &EditorSources::focus_out));

	_display.signal_enter_notify_event().connect (sigc::mem_fun (*this, &EditorSources::enter_notify), false);
	_display.signal_leave_notify_event().connect (sigc::mem_fun (*this, &EditorSources::leave_notify), false);

	ARDOUR_UI::instance()->primary_clock->mode_changed.connect (sigc::mem_fun(*this, &EditorSources::clock_format_changed));

	e->EditorFreeze.connect (editor_freeze_connection, MISSING_INVALIDATOR, boost::bind (&EditorSources::freeze_tree_model, this), gui_context());
	e->EditorThaw.connect (editor_thaw_connection, MISSING_INVALIDATOR, boost::bind (&EditorSources::thaw_tree_model, this), gui_context());
}

bool
EditorSources::focus_in (GdkEventFocus*)
{
	Window* win = dynamic_cast<Window*> (_scroller.get_toplevel ());

	if (win) {
		old_focus = win->get_focus ();
	} else {
		old_focus = 0;
	}

	/* try to do nothing on focus in (doesn't work, hence selection_count nonsense) */
	return true;
}

bool
EditorSources::focus_out (GdkEventFocus*)
{
	if (old_focus) {
		old_focus->grab_focus ();
		old_focus = 0;
	}

	return false;
}

bool
EditorSources::enter_notify (GdkEventCrossing*)
{
	/* arm counter so that ::selection_filter() will deny selecting anything for the
	   next two attempts to change selection status.
	*/
	_scroller.grab_focus ();
	Keyboard::magic_widget_grab_focus ();
	return false;
}

bool
EditorSources::leave_notify (GdkEventCrossing*)
{
	if (old_focus) {
		old_focus->grab_focus ();
		old_focus = 0;
	}

	Keyboard::magic_widget_drop_focus ();
	return false;
}

void
EditorSources::set_session (ARDOUR::Session* s)
{
	SessionHandlePtr::set_session (s);

	if (s) {
		//get all existing sources
		s->foreach_source (sigc::mem_fun (*this, &EditorSources::add_source));

		//register to get new sources that are recorded/imported
		s->SourceAdded.connect (source_added_connection, MISSING_INVALIDATOR, boost::bind (&EditorSources::add_source, this, _1), gui_context());
		s->SourceRemoved.connect (source_removed_connection, MISSING_INVALIDATOR, boost::bind (&EditorSources::remove_source, this, _1), gui_context());

		//register for source property changes ( some things like take_id aren't immediately available at construction )
		ARDOUR::Source::SourcePropertyChanged.connect (source_property_connection, MISSING_INVALIDATOR, boost::bind (&EditorSources::source_changed, this, _1), gui_context());
	} else {
		clear();
	}
}

void
EditorSources::remove_source (boost::shared_ptr<ARDOUR::Source> source)
{
	TreeModel::iterator i;
	TreeModel::Children rows = _model->children();
	for (i = rows.begin(); i != rows.end(); ++i) {
		boost::shared_ptr<ARDOUR::Source> ss = (*i)[_columns.source];
		if (source == ss) {
			_model->erase(i);
			break;
		}
	}
}

void
EditorSources::populate_row (TreeModel::Row row, boost::shared_ptr<ARDOUR::Source> source)
{
	ENSURE_GUI_THREAD (*this, &ARDOUR_UI::record_state_changed, row, source);

	if (!source) {
		return;
	}

	string str;
	Gdk::Color c;

	bool missing_source = boost::dynamic_pointer_cast<SilentFileSource>(source) != NULL;
	if (missing_source) {
		set_color_from_rgba (c, UIConfiguration::instance().color ("region list missing source"));
	} else {
		set_color_from_rgba (c, UIConfiguration::instance().color ("region list whole file"));
	}

	row[_columns.color_] = c;

	if (source->name()[0] == '/') { // external file

		str = ".../";

		boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource>(source);
		if (afs) {
			str += source->name();
			str += "[";
			str += afs->n_channels();  //ToDo:   num channels may be its own column?
			str += "]";
		} else {
			str += source->name();
		}

	} else {
		str = source->name();
	}

	row[_columns.name] = str;
	row[_columns.source] = source;

	if (missing_source) {
		row[_columns.path] = _("(MISSING) ") + Gtkmm2ext::markup_escape_text (source->name());

	} else {
		boost::shared_ptr<FileSource> fs = boost::dynamic_pointer_cast<FileSource>(source);
		if (fs) {
			row[_columns.path] = Gtkmm2ext::markup_escape_text (fs->path());
		} else {
			row[_columns.path] = Gtkmm2ext::markup_escape_text (source->name());
		}
	}

	row[_columns.take_id] = source->take_id();


	//Natural Position (samples, an invisible column for sorting)
	row[_columns.natural_s] = source->natural_position();

	//Natural Position (text representation)
	char buf[64];
	snprintf(buf, 16, "--" );
	if (source->natural_position() > 0) {
		format_position (source->natural_position(), buf, sizeof (buf));
	}
	row[_columns.natural_pos] = buf;
}

void
EditorSources::add_source (boost::shared_ptr<ARDOUR::Source> source)
{
	if (!source || !_session ) {
		return;
	}

	boost::shared_ptr<FileSource> fs = boost::dynamic_pointer_cast<FileSource> (source);

	if (!fs || fs->empty()) {
		return;
	}

	TreeModel::Row row = *(_model->append());
	populate_row (row, source);
}

void
EditorSources::source_changed (boost::shared_ptr<ARDOUR::Source> source)
{
	TreeModel::iterator i;
	TreeModel::Children rows = _model->children();
	for (i = rows.begin(); i != rows.end(); ++i) {
		boost::shared_ptr<ARDOUR::Source> ss = (*i)[_columns.source];
		if (source == ss) {
			populate_row(*i, source);
			break;
		}
	}
}

void
EditorSources::selection_changed ()
{

	if (_display.get_selection()->count_selected_rows() > 0) {

		TreeIter iter;
		TreeView::Selection::ListHandle_Path rows = _display.get_selection()->get_selected_rows ();

		_editor->get_selection().clear_regions ();

		for (TreeView::Selection::ListHandle_Path::iterator i = rows.begin(); i != rows.end(); ++i) {

			if ((iter = _model->get_iter (*i))) {

				boost::shared_ptr<ARDOUR::Source> source = (*iter)[_columns.source];
				if (source) {

					set<boost::shared_ptr<Region> > regions;
					RegionFactory::get_regions_using_source ( source, regions );

					for (set<boost::shared_ptr<Region> >::iterator region = regions.begin(); region != regions.end(); region++ ) {
						_change_connection.block (true);
						_editor->set_selected_regionview_from_region_list (*region, Selection::Add);
						_change_connection.block (false);

					}
				}
			}

		}
	} else {
		_editor->get_selection().clear_regions ();
	}

}

void
EditorSources::clock_format_changed ()
{
	TreeModel::iterator i;
	TreeModel::Children rows = _model->children();
	for (i = rows.begin(); i != rows.end(); ++i) {
		boost::shared_ptr<ARDOUR::Source> ss = (*i)[_columns.source];
		populate_row(*i, ss);
	}
}

void
EditorSources::format_position (samplepos_t pos, char* buf, size_t bufsize, bool onoff)
{
	Timecode::BBT_Time bbt;
	Timecode::Time timecode;

	if (pos < 0) {
		error << string_compose (_("EditorSources::format_position: negative timecode position: %1"), pos) << endmsg;
		snprintf (buf, bufsize, "invalid");
		return;
	}

	switch (ARDOUR_UI::instance()->primary_clock->mode ()) {
	case AudioClock::BBT:
		bbt = _session->tempo_map().bbt_at_sample (pos);
		if (onoff) {
			snprintf (buf, bufsize, "%03d|%02d|%04d" , bbt.bars, bbt.beats, bbt.ticks);
		} else {
			snprintf (buf, bufsize, "(%03d|%02d|%04d)" , bbt.bars, bbt.beats, bbt.ticks);
		}
		break;

	case AudioClock::MinSec:
		samplepos_t left;
		int hrs;
		int mins;
		float secs;

		left = pos;
		hrs = (int) floor (left / (_session->sample_rate() * 60.0f * 60.0f));
		left -= (samplecnt_t) floor (hrs * _session->sample_rate() * 60.0f * 60.0f);
		mins = (int) floor (left / (_session->sample_rate() * 60.0f));
		left -= (samplecnt_t) floor (mins * _session->sample_rate() * 60.0f);
		secs = left / (float) _session->sample_rate();
		if (onoff) {
			snprintf (buf, bufsize, "%02d:%02d:%06.3f", hrs, mins, secs);
		} else {
			snprintf (buf, bufsize, "(%02d:%02d:%06.3f)", hrs, mins, secs);
		}
		break;

	case AudioClock::Seconds:
		if (onoff) {
			snprintf (buf, bufsize, "%.1f", pos / (float)_session->sample_rate());
		} else {
			snprintf (buf, bufsize, "(%.1f)", pos / (float)_session->sample_rate());
		}
		break;

	case AudioClock::Samples:
		if (onoff) {
			snprintf (buf, bufsize, "%" PRId64, pos);
		} else {
			snprintf (buf, bufsize, "(%" PRId64 ")", pos);
		}
		break;

	case AudioClock::Timecode:
	default:
		_session->timecode_time (pos, timecode);
		if (onoff) {
			snprintf (buf, bufsize, "%02d:%02d:%02d:%02d", timecode.hours, timecode.minutes, timecode.seconds, timecode.frames);
		} else {
			snprintf (buf, bufsize, "(%02d:%02d:%02d:%02d)", timecode.hours, timecode.minutes, timecode.seconds, timecode.frames);
		}
		break;
	}
}

void
EditorSources::show_context_menu (int button, int time)
{

}

void
EditorSources::remove_selected_sources ()
{
	vector<string> choices;
	string prompt;

	prompt  = _("Do you want to remove the selected Sources?"
				"\nThis operation cannot be undone."
				"\nThe source files will not actually be deleted until you execute Session->Cleanup.");

	choices.push_back (_("No, do nothing."));
	choices.push_back (_("Only remove the Regions that use these Sources."));
	choices.push_back (_("Yes, remove the Regions and Sources (cannot be undone!"));

	Choice prompter (_("Remove selected Sources"), prompt, choices);

	int opt = prompter.run ();

	if ( opt >= 1) {
		
		std::list<boost::weak_ptr<ARDOUR::Source> > to_be_removed;
		
		if (_display.get_selection()->count_selected_rows() > 0) {

			TreeIter iter;
			TreeView::Selection::ListHandle_Path rows = _display.get_selection()->get_selected_rows ();

			_editor->get_selection().clear_regions ();

			for (TreeView::Selection::ListHandle_Path::iterator i = rows.begin(); i != rows.end(); ++i) {

				if ((iter = _model->get_iter (*i))) {

					boost::shared_ptr<ARDOUR::Source> source = (*iter)[_columns.source];
					if (source) {

						set<boost::shared_ptr<Region> > regions;
						RegionFactory::get_regions_using_source ( source, regions );

						for (set<boost::shared_ptr<Region> >::iterator region = regions.begin(); region != regions.end(); region++ ) {
							_change_connection.block (true);
							_editor->set_selected_regionview_from_region_list (*region, Selection::Add);
							_change_connection.block (false);
						}
						
						to_be_removed.push_back(source);
					}
				}

			}

			_editor->remove_selected_regions();  //this operation is undo-able

			if (opt==2) {	
				for (std::list<boost::weak_ptr<ARDOUR::Source> >::iterator i = to_be_removed.begin(); i != to_be_removed.end(); ++i) {
						_session->remove_source(*i);  //this operation is (currently) not undo-able
				}
			}
		}
	}

}


bool
EditorSources::key_press (GdkEventKey* ev)
{
	switch (ev->keyval) {
	case GDK_Delete:
	case GDK_BackSpace:
		remove_selected_sources();
		return true; 
	}

	return false;
}

bool
EditorSources::button_press (GdkEventButton *ev)
{
	boost::shared_ptr<ARDOUR::Source> source;
	TreeIter iter;
	TreeModel::Path path;
	TreeViewColumn* column;
	int cellx;
	int celly;

	if (_display.get_path_at_pos ((int)ev->x, (int)ev->y, path, column, cellx, celly)) {
		if ((iter = _model->get_iter (path))) {
			source = (*iter)[_columns.source];
		}
	}

	if (Keyboard::is_context_menu_event (ev)) {
		show_context_menu (ev->button, ev->time);
		return false;
	}

	return false;
}

void
EditorSources::selection_mapover (sigc::slot<void,boost::shared_ptr<Region> > sl)
{

}


void
EditorSources::drag_data_received (const RefPtr<Gdk::DragContext>& context,
                                   int x, int y,
                                   const SelectionData& data,
                                   guint info, guint time)
{

}

bool
EditorSources::selection_filter (const RefPtr<TreeModel>& model, const TreeModel::Path& path, bool already_selected)
{
	return true;
}

/** @return Region that has been dragged out of the list, or 0 */
boost::shared_ptr<ARDOUR::Source>
EditorSources::get_dragged_source ()
{
	list<boost::shared_ptr<ARDOUR::Source> > sources;
	TreeView* source;
	_display.get_object_drag_data (sources, &source);

	if (sources.empty()) {
		return boost::shared_ptr<ARDOUR::Source> ();
	}

	assert (sources.size() == 1);
	return sources.front ();
}

void
EditorSources::clear ()
{
	_display.set_model (Glib::RefPtr<Gtk::TreeStore> (0));
	_model->clear ();
	_display.set_model (_model);
}

boost::shared_ptr<ARDOUR::Source>
EditorSources::get_single_selection ()
{
	Glib::RefPtr<TreeSelection> selected = _display.get_selection();

	if (selected->count_selected_rows() != 1) {
		return boost::shared_ptr<ARDOUR::Source> ();
	}

	TreeView::Selection::ListHandle_Path rows = selected->get_selected_rows ();

	/* only one row selected, so rows.begin() is it */

	TreeIter iter = _model->get_iter (*rows.begin());

	if (!iter) {
		return boost::shared_ptr<ARDOUR::Source> ();
	}

	return (*iter)[_columns.source];
}

void
EditorSources::freeze_tree_model ()
{
	_display.set_model (Glib::RefPtr<Gtk::TreeStore>(0));
	_model->set_sort_column (-2, SORT_ASCENDING); //Disable sorting to gain performance
}

void
EditorSources::thaw_tree_model (){

	_model->set_sort_column (0, SORT_ASCENDING); // renabale sorting
	_display.set_model (_model);
}

XMLNode &
EditorSources::get_state () const
{
	XMLNode* node = new XMLNode (X_("SourcesList"));

	//TODO:  save sort state?

	return *node;
}

void
EditorSources::set_state (const XMLNode & node)
{
	bool changed = false;

}
