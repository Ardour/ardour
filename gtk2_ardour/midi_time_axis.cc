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
#include "automation_midi_cc_line.h"
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
#include "processor_automation_line.h"
#include "processor_automation_time_axis.h"
#include "midi_controller_time_axis.h"
#include "region_view.h"
#include "rgb_macros.h"
#include "selection.h"
#include "simplerect.h"
#include "midi_streamview.h"
#include "utils.h"

#include <ardour/midi_track.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Editing;


MidiTimeAxisView::MidiTimeAxisView (PublicEditor& ed, Session& sess, boost::shared_ptr<Route> rt, Canvas& canvas)
	: AxisView(sess), // FIXME: won't compile without this, why??
	RouteTimeAxisView(ed, sess, rt, canvas)
{
	subplugin_menu.set_name ("ArdourContextMenu");

	_view = new MidiStreamView (*this);

	ignore_toggle = false;

	mute_button->set_active (false);
	solo_button->set_active (false);
	
	if (is_midi_track())
		controls_ebox.set_name ("MidiTimeAxisViewControlsBaseUnselected");
	else // bus
		controls_ebox.set_name ("MidiBusControlsBaseUnselected");

	/* map current state of the route */

	processors_changed ();

	ensure_xml_node ();

	set_state (*xml_node);
	
	_route->processors_changed.connect (mem_fun(*this, &MidiTimeAxisView::processors_changed));

	if (is_track()) {

		controls_ebox.set_name ("MidiTrackControlsBaseUnselected");
		controls_base_selected_name = "MidiTrackControlsBaseSelected";
		controls_base_unselected_name = "MidiTrackControlsBaseUnselected";

		/* ask for notifications of any new RegionViews */
		_view->RegionViewAdded.connect (mem_fun(*this, &MidiTimeAxisView::region_view_added));
		_view->attach ();

	} /*else { // no MIDI busses yet

		controls_ebox.set_name ("MidiBusControlsBaseUnselected");
		controls_base_selected_name = "MidiBusControlsBaseSelected";
		controls_base_unselected_name = "MidiBusControlsBaseUnselected";
	}*/
}

MidiTimeAxisView::~MidiTimeAxisView ()
{
}

guint32
MidiTimeAxisView::show_at (double y, int& nth, Gtk::VBox *parent)
{
	ensure_xml_node ();
	xml_node->add_property ("shown_editor", "yes");
		
	return TimeAxisView::show_at (y, nth, parent);
}

void
MidiTimeAxisView::hide ()
{
	ensure_xml_node ();
	xml_node->add_property ("shown_editor", "no");

	TimeAxisView::hide ();
}

void
MidiTimeAxisView::build_automation_action_menu ()
{
	using namespace Menu_Helpers;

	RouteTimeAxisView::build_automation_action_menu ();

	MenuList& automation_items = automation_action_menu->items();
	
	automation_items.push_back (SeparatorElem());

	automation_items.push_back (MenuElem (_("Controller..."), 
						   mem_fun(*this, &MidiTimeAxisView::add_controller_track)));
}

/** Prompt for a controller with a dialog and add an automation track for it
 */
void
MidiTimeAxisView::add_controller_track()
{
	AddMidiCCTrackDialog dialog;
	dialog.set_transient_for(editor);
	int response = dialog.run();
	if (response == Gtk::RESPONSE_ACCEPT) {
		ParamID param = dialog.parameter();
		create_automation_child(param);
	}
}

void
MidiTimeAxisView::create_automation_child (ParamID param)
{
	if (param.type() == MidiCCAutomation) {
	
		/* FIXME: this all probably leaks */

		boost::shared_ptr<AutomationControl> c =_route->control(param);

		if (!c) {
			boost::shared_ptr<AutomationList> al(new ARDOUR::AutomationList(param, 0, 127, 64));
			c = boost::shared_ptr<AutomationControl>(new AutomationControl(_session, al));
			_route->add_control(c);
		}

		MidiControllerTimeAxisView* track = new MidiControllerTimeAxisView (_session,
				_route,
				editor,
				*this,
				parent_canvas,
				_route->describe_parameter(param),
				c);

		AutomationMidiCCLine* line = new AutomationMidiCCLine (param.to_string(),
				*track,
				*track->canvas_display,
				c->list());

		line->set_line_color (ARDOUR_UI::config()->canvasvar_AutomationLine.get());

		track->add_line(*line);

		add_automation_child(param, track);

	} else {
		error << "MidiTimeAxisView: unknown automation child " << param.to_string() << endmsg;
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

