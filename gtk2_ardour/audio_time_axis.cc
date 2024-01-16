/*
 * Copyright (C) 2005-2006 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2005-2006 Sampo Savolainen <v2@iki.fi>
 * Copyright (C) 2005-2006 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2005-2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2005-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Karsten Wiese <fzuuzf@googlemail.com>
 * Copyright (C) 2006-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2014 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
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

#include <cstdlib>
#include <cmath>
#include <cassert>

#include <algorithm>
#include <string>
#include <vector>

#include <sigc++/bind.h>

#include "pbd/error.h"
#include "pbd/stl_delete.h"
#include "pbd/memento_command.h"

#include "gtkmm2ext/colors.h"
#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/utils.h"

#include "widgets/ardour_button.h"

#include "ardour/event_type_map.h"
#include "ardour/pannable.h"
#include "ardour/panner.h"
#include "ardour/panner_shell.h"

#include "audio_time_axis.h"
#include "automation_line.h"
#include "enums.h"
#include "gui_thread.h"
#include "automation_time_axis.h"
#include "keyboard.h"
#include "playlist_selector.h"
#include "public_editor.h"
#include "audio_region_view.h"
#include "audio_streamview.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace ArdourWidgets;
using namespace PBD;
using namespace Gtk;
using namespace Editing;

AudioTimeAxisView::AudioTimeAxisView (PublicEditor& ed, Session* sess, ArdourCanvas::Canvas& canvas)
	: SessionHandlePtr (sess)
	, RouteTimeAxisView(ed, sess, canvas)
{
	UIConfiguration::instance().ParameterChanged.connect (sigc::mem_fun (*this, &AudioTimeAxisView::parameter_changed));
}

void
AudioTimeAxisView::set_route (std::shared_ptr<Route> rt)
{
	_route = rt;

	/* RouteTimeAxisView::set_route() sets up some things in the View,
	   so it must be created before RouteTimeAxis::set_route() is
	   called.
	*/
	_view = new AudioStreamView (*this);

	RouteTimeAxisView::set_route (rt);

	_view->apply_color (Gtkmm2ext::gdk_color_to_rgba (color()), StreamView::RegionColor);

	// Make sure things are sane...
	assert(!is_track() || is_audio_track());

	if (is_audio_track()) {
		controls_ebox.set_name ("AudioTrackControlsBaseUnselected");
		time_axis_frame.set_name ("AudioTrackControlsBaseUnselected");
	} else { // bus
		controls_ebox.set_name ("AudioBusControlsBaseUnselected");
		time_axis_frame.set_name ("AudioBusControlsBaseUnselected");
	}

	/* if set_state above didn't create a gain automation child, we need to make one */
	if (automation_child (GainAutomation) == 0) {
		create_automation_child (GainAutomation, false);
	}

	if (automation_child (TrimAutomation) == 0) {
		create_automation_child (TrimAutomation, false);
	}

	/* if set_state above didn't create a mute automation child, we need to make one */
	if (automation_child (MuteAutomation) == 0) {
		create_automation_child (MuteAutomation, false);
	}

	if (_route->panner_shell()) {
		_route->panner_shell()->Changed.connect (*this, invalidator (*this),
		                                         boost::bind (&AudioTimeAxisView::ensure_pan_views, this, false), gui_context());
	}

	/* map current state of the route */

	processors_changed (RouteProcessorChange ());
	reset_processor_automation_curves ();
	ensure_pan_views (false);
	update_control_names ();

	if (is_audio_track()) {

		/* ask for notifications of any new RegionViews */
		_view->RegionViewAdded.connect (sigc::mem_fun(*this, &AudioTimeAxisView::region_view_added));

		if (!_editor.have_idled()) {
			/* first idle will do what we need */
		} else {
			first_idle ();
		}

	} else {
		post_construct ();
	}
}

AudioTimeAxisView::~AudioTimeAxisView ()
{
	delete _view;
	_view = nullptr;
}

void
AudioTimeAxisView::first_idle ()
{
	_view->attach ();
	post_construct ();
}

AudioStreamView*
AudioTimeAxisView::audio_view()
{
	return dynamic_cast<AudioStreamView*>(_view);
}

void
AudioTimeAxisView::create_automation_child (const Evoral::Parameter& param, bool show)
{
	if (param.type() == NullAutomation) {
		return;
	}

	AutomationTracks::iterator existing = _automation_tracks.find (param);

	if (existing != _automation_tracks.end()) {

		/* automation track created because we had existing data for
		 * the processor, but visibility may need to be controlled
		 * since it will have been set visible by default.
		 */

		existing->second->set_marked_for_display (show);

		if (!no_redraw) {
			request_redraw ();
		}

		return;
	}

	if (param.type() == GainAutomation) {

		create_gain_automation_child (param, show);

	} else if (param.type() == TrimAutomation) {

		create_trim_automation_child (param, show);

	} else if (param.type() == PanWidthAutomation ||
	           param.type() == PanElevationAutomation ||
	           param.type() == PanAzimuthAutomation) {

		ensure_pan_views (show);

	} else if (param.type() == PluginAutomation) {

		/* handled elsewhere */

	} else if (param.type() == MuteAutomation) {

		create_mute_automation_child (param, show);


	} else {
		error << "AudioTimeAxisView: unknown automation child " << EventTypeMap::instance().to_symbol(param) << endmsg;
	}
}

void
AudioTimeAxisView::show_all_automation (bool apply_to_selection)
{
	if (apply_to_selection) {
		_editor.get_selection().tracks.foreach_audio_time_axis (boost::bind (&AudioTimeAxisView::show_all_automation, _1, false));
	} else {

		no_redraw = true;

		RouteTimeAxisView::show_all_automation ();

		no_redraw = false;
		request_redraw ();
	}
}

void
AudioTimeAxisView::show_existing_automation (bool apply_to_selection)
{
	if (apply_to_selection) {
		_editor.get_selection().tracks.foreach_audio_time_axis (boost::bind (&AudioTimeAxisView::show_existing_automation, _1, false));
	} else {
		no_redraw = true;

		RouteTimeAxisView::show_existing_automation ();

		no_redraw = false;

		request_redraw ();
	}
}

void
AudioTimeAxisView::hide_all_automation (bool apply_to_selection)
{
	if (apply_to_selection) {
		_editor.get_selection().tracks.foreach_audio_time_axis (boost::bind (&AudioTimeAxisView::hide_all_automation, _1, false));
	} else {
		no_redraw = true;

		RouteTimeAxisView::hide_all_automation();

		no_redraw = false;
		request_redraw ();
	}
}

void
AudioTimeAxisView::route_active_changed ()
{
	RouteTimeAxisView::route_active_changed();
	update_control_names ();

	if (!_route->active()) {
		controls_table.hide();
		inactive_table.show();
		RouteTimeAxisView::hide_all_automation();
	} else {
		inactive_table.hide();
		controls_table.show();
	}
}

void
AudioTimeAxisView::parameter_changed (string const & p)
{
	if (p == "vertical-region-gap") {
		_view->update_contents_height ();
	}
}


/**
 *    Set up the names of the controls so that they are coloured
 *    correctly depending on whether this route is inactive or
 *    selected.
 */

void
AudioTimeAxisView::update_control_names ()
{
	if (is_audio_track()) {
		if (_route->active()) {
			controls_base_selected_name = "AudioTrackControlsBaseSelected";
			controls_base_unselected_name = "AudioTrackControlsBaseUnselected";
		} else {
			controls_base_selected_name = "AudioTrackControlsBaseInactiveSelected";
			controls_base_unselected_name = "AudioTrackControlsBaseInactiveUnselected";
		}
	} else {
		if (_route->active()) {
			controls_base_selected_name = "BusControlsBaseSelected";
			controls_base_unselected_name = "BusControlsBaseUnselected";
		} else {
			controls_base_selected_name = "BusControlsBaseInactiveSelected";
			controls_base_unselected_name = "BusControlsBaseInactiveUnselected";
		}
	}

	if (selected()) {
		controls_ebox.set_name (controls_base_selected_name);
		time_axis_frame.set_name (controls_base_selected_name);
	} else {
		controls_ebox.set_name (controls_base_unselected_name);
		time_axis_frame.set_name (controls_base_unselected_name);
	}
}

void
AudioTimeAxisView::build_automation_action_menu (bool for_selection)
{
	RouteTimeAxisView::build_automation_action_menu (for_selection);
}
