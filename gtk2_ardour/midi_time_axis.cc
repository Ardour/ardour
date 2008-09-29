/*
    Copyright (C) 2000 Paul Davis 

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
#include <vector>

#include <sigc++/bind.h>

#include <pbd/error.h>
#include <pbd/stl_delete.h>
#include <pbd/whitespace.h>

#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/selector.h>
#include <gtkmm2ext/stop_signal.h>
#include <gtkmm2ext/bindable_button.h>
#include <gtkmm2ext/utils.h>

#include <ardour/midi_playlist.h>
#include <ardour/midi_diskstream.h>
#include <ardour/processor.h>
#include <ardour/ladspa_plugin.h>
#include <ardour/location.h>
#include <ardour/playlist.h>
#include <ardour/session.h>
#include <ardour/session_playlist.h>
#include <ardour/utils.h>

#include "ardour_ui.h"
#include "midi_time_axis.h"
#include "automation_time_axis.h"
#include "automation_line.h"
#include "add_midi_cc_track_dialog.h"
#include "canvas_impl.h"
#include "crossfade_view.h"
#include "enums.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "playlist_selector.h"
#include "plugin_selector.h"
#include "plugin_ui.h"
#include "point_selection.h"
#include "prompter.h"
#include "public_editor.h"
#include "region_view.h"
#include "rgb_macros.h"
#include "selection.h"
#include "simplerect.h"
#include "midi_streamview.h"
#include "utils.h"
#include "midi_scroomer.h"
#include "piano_roll_header.h"
#include "ghostregion.h"

#include <ardour/midi_track.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace sigc;
using namespace Editing;


MidiTimeAxisView::MidiTimeAxisView (PublicEditor& ed, Session& sess, boost::shared_ptr<Route> rt, Canvas& canvas)
	: AxisView(sess) // virtually inherited
	, RouteTimeAxisView(ed, sess, rt, canvas)
	, _ignore_signals(false)  
	, _range_scroomer(0)
	, _piano_roll_header(0)
	, _note_mode(Sustained)
	, _note_mode_item(NULL)
	, _percussion_mode_item(NULL)
	, _midi_expander("MIDI")
{
	subplugin_menu.set_name ("ArdourContextMenu");

	_view = new MidiStreamView (*this);

	ignore_toggle = false;

	mute_button->set_active (false);
	solo_button->set_active (false);
	
	if (is_midi_track()) {
		controls_ebox.set_name ("MidiTimeAxisViewControlsBaseUnselected");
		_note_mode = midi_track()->note_mode();
	} else { // MIDI bus (which doesn't exist yet..)
		controls_ebox.set_name ("MidiBusControlsBaseUnselected");
	}

	/* map current state of the route */

	processors_changed ();

	ensure_xml_node ();

	set_state (*xml_node);
	
	_route->processors_changed.connect (mem_fun(*this, &MidiTimeAxisView::processors_changed));

	if (is_track()) {
		_piano_roll_header = new PianoRollHeader(*midi_view());
		_range_scroomer = new MidiScroomer(midi_view()->note_range_adjustment);

		controls_hbox.pack_start(*_range_scroomer);
		controls_hbox.pack_start(*_piano_roll_header);

		controls_ebox.set_name ("MidiTrackControlsBaseUnselected");
		controls_base_selected_name = "MidiTrackControlsBaseSelected";
		controls_base_unselected_name = "MidiTrackControlsBaseUnselected";

		midi_view()->NoteRangeChanged.connect (mem_fun(*this, &MidiTimeAxisView::update_range));

		/* ask for notifications of any new RegionViews */
		_view->RegionViewAdded.connect (mem_fun(*this, &MidiTimeAxisView::region_view_added));
		_view->attach ();
	}
		
	// add channel selector expander
	HBox *channel_selector_box = manage(new HBox());
	channel_selector_box->pack_start(_channel_selector, SHRINK, 0);
	_midi_expander.add(*channel_selector_box);
	_midi_expander.property_expanded().signal_changed().connect(
			mem_fun(this, &MidiTimeAxisView::channel_selector_toggled));
	controls_vbox.pack_end(_midi_expander, SHRINK, 0);
	boost::shared_ptr<MidiDiskstream> diskstream = midi_track()->midi_diskstream();

	// restore channel selector settings
	_channel_selector.set_channel_mode(diskstream->get_channel_mode(), diskstream->get_channel_mask());
	_channel_selector.mode_changed.connect(
		mem_fun(*midi_track()->midi_diskstream(), &MidiDiskstream::set_channel_mode));

}

MidiTimeAxisView::~MidiTimeAxisView ()
{
	delete _piano_roll_header;
	_piano_roll_header = 0;

	delete _range_scroomer;
	_range_scroomer = 0;
}

MidiStreamView*
MidiTimeAxisView::midi_view()
{
	return dynamic_cast<MidiStreamView*>(_view);
}

guint32
MidiTimeAxisView::show_at (double y, int& nth, Gtk::VBox *parent)
{
	ensure_xml_node ();
	xml_node->add_property ("shown_editor", "yes");
		
	guint32 ret = TimeAxisView::show_at (y, nth, parent);
	_piano_roll_header->show();
	_range_scroomer->show();
	return ret;
}

void
MidiTimeAxisView::hide ()
{
	ensure_xml_node ();
	xml_node->add_property ("shown_editor", "no");

	TimeAxisView::hide ();
}

void
MidiTimeAxisView::set_height (uint32_t h)
{
	RouteTimeAxisView::set_height (h);

	if (height >= hLarger) {
		_midi_expander.show();
		if (is_track() && _range_scroomer)
			_range_scroomer->show();
		if (is_track() && _piano_roll_header)
			_piano_roll_header->show();

	} else if (height >= hLarge) {
		_midi_expander.show();
		_midi_expander.set_expanded(FALSE);
		if (is_track() && _range_scroomer)
			_range_scroomer->show();
		if (is_track() && _piano_roll_header)
			_piano_roll_header->show();

	} else if (height >= hNormal) {
		_midi_expander.show();
		_midi_expander.set_expanded(FALSE);
		if (is_track() && _range_scroomer)
			_range_scroomer->hide();
		if (is_track() && _piano_roll_header)
			_piano_roll_header->hide();
	} else {
		_midi_expander.hide();
		_midi_expander.set_expanded(FALSE);
		if (is_track() && _range_scroomer)
			_range_scroomer->hide();
		if (is_track() && _piano_roll_header)
			_piano_roll_header->hide();
	}
}

void
MidiTimeAxisView::append_extra_display_menu_items ()
{
	using namespace Menu_Helpers;

	MenuList& items = display_menu->items();

	// Note range
	Menu *range_menu = manage(new Menu);
	MenuList& range_items = range_menu->items();
	range_menu->set_name ("ArdourContextMenu");
	
	range_items.push_back (MenuElem (_("Show Full Range"), bind (
			mem_fun(*this, &MidiTimeAxisView::set_note_range),
			MidiStreamView::FullRange)));
	
	range_items.push_back (MenuElem (_("Fit Contents"), bind (
			mem_fun(*this, &MidiTimeAxisView::set_note_range),
			MidiStreamView::ContentsRange)));

	items.push_back (MenuElem (_("Note range"), *range_menu));
}

void
MidiTimeAxisView::build_automation_action_menu ()
{
	using namespace Menu_Helpers;

	RouteTimeAxisView::build_automation_action_menu ();

	MenuList& automation_items = automation_action_menu->items();
	
	automation_items.push_back (SeparatorElem());
	automation_items.push_back (MenuElem (_("Controller..."), 
			mem_fun(*this, &MidiTimeAxisView::add_cc_track)));
	automation_items.push_back (MenuElem (_("Bender"), 
			sigc::bind(mem_fun(*this, &MidiTimeAxisView::add_parameter_track),
				Evoral::Parameter(MidiPitchBenderAutomation))));
	automation_items.push_back (MenuElem (_("Pressure"), 
			sigc::bind(mem_fun(*this, &MidiTimeAxisView::add_parameter_track),
				Evoral::Parameter(MidiChannelPressureAutomation))));

}

Gtk::Menu*
MidiTimeAxisView::build_mode_menu()
{
	using namespace Menu_Helpers;

	Menu* mode_menu = manage (new Menu);
	MenuList& items = mode_menu->items();
	mode_menu->set_name ("ArdourContextMenu");

	RadioMenuItem::Group mode_group;
	items.push_back (RadioMenuElem (mode_group, _("Sustained"),
				bind (mem_fun (*this, &MidiTimeAxisView::set_note_mode), Sustained)));
	_note_mode_item = dynamic_cast<RadioMenuItem*>(&items.back());
	_note_mode_item->set_active(_note_mode == Sustained);

	items.push_back (RadioMenuElem (mode_group, _("Percussive"),
				bind (mem_fun (*this, &MidiTimeAxisView::set_note_mode), Percussive)));
	_percussion_mode_item = dynamic_cast<RadioMenuItem*>(&items.back());
	_percussion_mode_item->set_active(_note_mode == Percussive);

	return mode_menu;
}
	
void
MidiTimeAxisView::set_note_mode(NoteMode mode)
{
	if (_note_mode != mode || midi_track()->note_mode() != mode) {
		_note_mode = mode;
		midi_track()->set_note_mode(mode);
		_view->redisplay_diskstream();
	}
}


void
MidiTimeAxisView::set_note_range(MidiStreamView::VisibleNoteRange range)
{
	if (!_ignore_signals)
		midi_view()->set_note_range(range);
}


void
MidiTimeAxisView::update_range()
{
	MidiGhostRegion* mgr;

	for(list<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); ++i) {
		if ((mgr = dynamic_cast<MidiGhostRegion*>(*i)) != 0) {
			mgr->update_range();
		}
	}
}

void
MidiTimeAxisView::show_all_automation ()
{
	if (midi_track()) {
		const set<Evoral::Parameter> params = midi_track()->midi_diskstream()->
				midi_playlist()->contained_automation();

		for (set<Evoral::Parameter>::const_iterator i = params.begin(); i != params.end(); ++i) {
			create_automation_child(*i, true);
		}
	}

	RouteTimeAxisView::show_all_automation ();
}

void
MidiTimeAxisView::show_existing_automation ()
{
	if (midi_track()) {
		const set<Evoral::Parameter> params = midi_track()->midi_diskstream()->
				midi_playlist()->contained_automation();

		for (set<Evoral::Parameter>::const_iterator i = params.begin(); i != params.end(); ++i) {
			create_automation_child(*i, true);
		}
	}

	RouteTimeAxisView::show_existing_automation ();
}

/** Prompt for a controller with a dialog and add an automation track for it
 */
void
MidiTimeAxisView::add_cc_track()
{
	int response;
	Evoral::Parameter param(0, 0, 0);

	{
		AddMidiCCTrackDialog dialog;
		dialog.set_transient_for(editor);
		response = dialog.run();
		
		if (response == Gtk::RESPONSE_ACCEPT)
			param = dialog.parameter();
	}

	if (param.type() != 0 && response == Gtk::RESPONSE_ACCEPT)
		create_automation_child(param, true);
}


/** Add an automation track for the given parameter (pitch bend, channel pressure).
 */
void
MidiTimeAxisView::add_parameter_track(const Evoral::Parameter& param)
{
	create_automation_child(param, true);
}

void
MidiTimeAxisView::create_automation_child (const Evoral::Parameter& param, bool show)
{
	if (	param.type() == MidiCCAutomation ||
			param.type() == MidiPgmChangeAutomation ||
			param.type() == MidiPitchBenderAutomation ||
			param.type() == MidiChannelPressureAutomation
	   ) {
	
		/* These controllers are region "automation", so we do not create
		 * an AutomationList/Line for the track */

		AutomationTracks::iterator existing = _automation_tracks.find(param);
		if (existing != _automation_tracks.end())
			return;

		boost::shared_ptr<AutomationControl> c
			= boost::dynamic_pointer_cast<AutomationControl>(_route->data().control(param));

		if (!c) {
			c = boost::dynamic_pointer_cast<AutomationControl>(_route->control_factory(param));
			_route->add_control(c);
		}

		boost::shared_ptr<AutomationTimeAxisView> track(new AutomationTimeAxisView (_session,
				_route, boost::shared_ptr<ARDOUR::Automatable>(), c,
				editor,
				*this,
				true,
				parent_canvas,
				_route->describe_parameter(param)));
		
		add_automation_child(param, track, show);

	} else {
		error << "MidiTimeAxisView: unknown automation child "
			<< ARDOUR::EventTypeMap::instance().to_symbol(param) << endmsg;
	}
}

void
MidiTimeAxisView::route_active_changed ()
{
	RouteUI::route_active_changed ();

	if (is_track()) {
		if (_route->active()) {
			controls_ebox.set_name ("MidiTrackControlsBaseUnselected");
			controls_base_selected_name = "MidiTrackControlsBaseSelected";
			controls_base_unselected_name = "MidiTrackControlsBaseUnselected";
		} else {
			controls_ebox.set_name ("MidiTrackControlsBaseInactiveUnselected");
			controls_base_selected_name = "MidiTrackControlsBaseInactiveSelected";
			controls_base_unselected_name = "MidiTrackControlsBaseInactiveUnselected";
		}
	} else {

		throw; // wha?
		
		if (_route->active()) {
			controls_ebox.set_name ("BusControlsBaseUnselected");
			controls_base_selected_name = "BusControlsBaseSelected";
			controls_base_unselected_name = "BusControlsBaseUnselected";
		} else {
			controls_ebox.set_name ("BusControlsBaseInactiveUnselected");
			controls_base_selected_name = "BusControlsBaseInactiveSelected";
			controls_base_unselected_name = "BusControlsBaseInactiveUnselected";
		}
	}
}

void 
MidiTimeAxisView::channel_selector_toggled()
{
	static uint32_t previous_height;
	
	if(_midi_expander.property_expanded()) {

		previous_height = current_height();

		if (previous_height < TimeAxisView::hLargest) {
			set_height (TimeAxisView::hLarge);
		}

	} else {

		set_height (previous_height);
	}
}



