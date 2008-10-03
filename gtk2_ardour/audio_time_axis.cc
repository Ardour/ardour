/*
    Copyright (C) 2000-2006 Paul Davis 

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
#include <cassert>

#include <algorithm>
#include <string>
#include <vector>

#include <sigc++/bind.h>

#include <pbd/error.h>
#include <pbd/stl_delete.h>
#include <pbd/memento_command.h>

#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/selector.h>
#include <gtkmm2ext/stop_signal.h>
#include <gtkmm2ext/bindable_button.h>
#include <gtkmm2ext/utils.h>

#include <ardour/audioplaylist.h>
#include <ardour/audio_diskstream.h>
#include <ardour/processor.h>
#include <ardour/location.h>
#include <ardour/panner.h>
#include <ardour/playlist.h>
#include <ardour/profile.h>
#include <ardour/session.h>
#include <ardour/session_playlist.h>
#include <ardour/utils.h>

#include "ardour_ui.h"
#include "audio_time_axis.h"
#include "automation_line.h"
#include "canvas_impl.h"
#include "crossfade_view.h"
#include "enums.h"
#include "automation_time_axis.h"
#include "keyboard.h"
#include "playlist_selector.h"
#include "prompter.h"
#include "public_editor.h"
#include "audio_region_view.h"
#include "simplerect.h"
#include "audio_streamview.h"
#include "utils.h"

#include <ardour/audio_track.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Editing;

AudioTimeAxisView::AudioTimeAxisView (PublicEditor& ed, Session& sess, boost::shared_ptr<Route> rt, Canvas& canvas)
	: AxisView(sess)
	, RouteTimeAxisView(ed, sess, rt, canvas)
{
	// Make sure things are sane...
	assert(!is_track() || is_audio_track());

	subplugin_menu.set_name ("ArdourContextMenu");
	waveform_item = 0;

	_view = new AudioStreamView (*this);

	create_automation_child (GainAutomation, false);

	ignore_toggle = false;

	mute_button->set_active (false);
	solo_button->set_active (false);
	
	if (is_audio_track()) {
		controls_ebox.set_name ("AudioTrackControlsBaseUnselected");
	} else { // bus
		controls_ebox.set_name ("AudioBusControlsBaseUnselected");
	}

	ensure_xml_node ();

	set_state (*xml_node);
	
	_route->panner().Changed.connect (bind (mem_fun(*this, &AudioTimeAxisView::update_pans), false));

	/* map current state of the route */

	processors_changed ();
	reset_processor_automation_curves ();
	update_pans (false);
	update_control_names ();

	if (is_audio_track()) {

		/* ask for notifications of any new RegionViews */
		_view->RegionViewAdded.connect (mem_fun(*this, &AudioTimeAxisView::region_view_added));

		if (!editor.have_idled()) {
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

guint32
AudioTimeAxisView::show_at (double y, int& nth, Gtk::VBox *parent)
{
	ensure_xml_node ();
	xml_node->add_property ("shown_editor", "yes");
		
	return TimeAxisView::show_at (y, nth, parent);
}

void
AudioTimeAxisView::hide ()
{
	ensure_xml_node ();
	xml_node->add_property ("shown_editor", "no");

	TimeAxisView::hide ();
}


void
AudioTimeAxisView::append_extra_display_menu_items ()
{
	using namespace Menu_Helpers;

	MenuList& items = display_menu->items();

	// crossfade stuff
	if (!Profile->get_sae()) {
		items.push_back (MenuElem (_("Hide all crossfades"), mem_fun(*this, &AudioTimeAxisView::hide_all_xfades)));
		items.push_back (MenuElem (_("Show all crossfades"), mem_fun(*this, &AudioTimeAxisView::show_all_xfades)));
	}

	// waveform menu
	Menu *waveform_menu = manage(new Menu);
	MenuList& waveform_items = waveform_menu->items();
	waveform_menu->set_name ("ArdourContextMenu");
	
	waveform_items.push_back (CheckMenuElem (_("Show waveforms"), mem_fun(*this, &AudioTimeAxisView::toggle_waveforms)));
	waveform_item = static_cast<CheckMenuItem *> (&waveform_items.back());
	ignore_toggle = true;
	waveform_item->set_active (editor.show_waveforms());
	ignore_toggle = false;

	waveform_items.push_back (SeparatorElem());
	
	RadioMenuItem::Group group;
	
	waveform_items.push_back (RadioMenuElem (group, _("Traditional"), bind (mem_fun(*this, &AudioTimeAxisView::set_waveform_shape), Traditional)));
	traditional_item = static_cast<RadioMenuItem *> (&waveform_items.back());

	if (!Profile->get_sae()) {
		waveform_items.push_back (RadioMenuElem (group, _("Rectified"), bind (mem_fun(*this, &AudioTimeAxisView::set_waveform_shape), Rectified)));
		rectified_item = static_cast<RadioMenuItem *> (&waveform_items.back());
	} else {
		rectified_item = 0;
	}

	waveform_items.push_back (SeparatorElem());
	
	RadioMenuItem::Group group2;

	waveform_items.push_back (RadioMenuElem (group2, _("Linear"), bind (mem_fun(*this, &AudioTimeAxisView::set_waveform_scale), LinearWaveform)));
	linearscale_item = static_cast<RadioMenuItem *> (&waveform_items.back());

	waveform_items.push_back (RadioMenuElem (group2, _("Logarithmic"), bind (mem_fun(*this, &AudioTimeAxisView::set_waveform_scale), LogWaveform)));
	logscale_item = static_cast<RadioMenuItem *> (&waveform_items.back());

	// setting initial item state
	AudioStreamView* asv = audio_view();
	if (asv) {
		ignore_toggle = true;
		if (asv->get_waveform_shape() == Rectified && rectified_item) {
			rectified_item->set_active(true);
		} else {
			traditional_item->set_active(true);
		}

		if (asv->get_waveform_scale() == LogWaveform) 
			logscale_item->set_active(true);
		else linearscale_item->set_active(true);
		ignore_toggle = false;
	}

	items.push_back (MenuElem (_("Waveform"), *waveform_menu));

}
	
Gtk::Menu*
AudioTimeAxisView::build_mode_menu()
{
	using namespace Menu_Helpers;

	Menu* mode_menu = manage (new Menu);
	MenuList& items = mode_menu->items();
	mode_menu->set_name ("ArdourContextMenu");

	RadioMenuItem::Group mode_group;
	items.push_back (RadioMenuElem (mode_group, _("Normal"),
				bind (mem_fun (*this, &AudioTimeAxisView::set_track_mode), ARDOUR::Normal)));
	normal_track_mode_item = dynamic_cast<RadioMenuItem*>(&items.back());
	items.push_back (RadioMenuElem (mode_group, _("Tape"),
				bind (mem_fun (*this, &AudioTimeAxisView::set_track_mode), ARDOUR::Destructive)));
	destructive_track_mode_item = dynamic_cast<RadioMenuItem*>(&items.back());

	switch (track()->mode()) {
		case ARDOUR::Destructive:
			destructive_track_mode_item->set_active ();
			break;
		case ARDOUR::Normal:
			normal_track_mode_item->set_active ();
			break;
	}

	return mode_menu;
}

void
AudioTimeAxisView::toggle_waveforms ()
{
	AudioStreamView* asv = audio_view();
	assert(asv);

	if (asv && waveform_item && !ignore_toggle) {
		asv->set_show_waveforms (waveform_item->get_active());
	}
}

void
AudioTimeAxisView::set_show_waveforms (bool yn)
{
	AudioStreamView* asv = audio_view();
	assert(asv);

	if (waveform_item) {
		waveform_item->set_active (yn);
	} else {
		asv->set_show_waveforms (yn);
	}
}

void
AudioTimeAxisView::set_show_waveforms_recording (bool yn)
{
	AudioStreamView* asv = audio_view();

	if (asv) {
		asv->set_show_waveforms_recording (yn);
	}
}

void
AudioTimeAxisView::set_waveform_shape (WaveformShape shape)
{
	AudioStreamView* asv = audio_view();

	if (asv && !ignore_toggle) {
		asv->set_waveform_shape (shape);
	}

	map_frozen ();
}	

void
AudioTimeAxisView::set_waveform_scale (WaveformScale scale)
{
	AudioStreamView* asv = audio_view();

	if (asv && !ignore_toggle) {
		asv->set_waveform_scale (scale);
	}

	map_frozen ();
}	

void
AudioTimeAxisView::create_automation_child (const Evoral::Parameter& param, bool show)
{
	if (param.type() == GainAutomation) {

		boost::shared_ptr<AutomationControl> c = _route->gain_control();
		if (!c) {
			error << "Route has no gain automation, unable to add automation track view." << endmsg;
			return;
		}

		boost::shared_ptr<AutomationTimeAxisView> gain_track(new AutomationTimeAxisView (_session,
				_route, _route, c,
				editor,
				*this,
				false,
				parent_canvas,
				_route->describe_parameter(param)));

		add_automation_child(Evoral::Parameter(GainAutomation), gain_track, show);

	} else if (param.type() == PanAutomation) {

		ensure_xml_node ();
		update_pans (show);

	} else {
		error << "AudioTimeAxisView: unknown automation child " << EventTypeMap::instance().to_symbol(param) << endmsg;
	}
}

void
AudioTimeAxisView::update_pans (bool show)
{
	const set<Evoral::Parameter>& params = _route->panner().what_can_be_automated();
	set<Evoral::Parameter>::iterator p;

	uint32_t i = 0;
	for (p = params.begin(); p != params.end(); ++p) {
		boost::shared_ptr<ARDOUR::AutomationControl> pan_control
			= boost::dynamic_pointer_cast<ARDOUR::AutomationControl>(
				_route->panner().data().control(*p));
		
		if (pan_control->parameter().type() == NullAutomation) {
			error << "Pan control has NULL automation type!" << endmsg;
			continue;
		}

		boost::shared_ptr<AutomationTimeAxisView> pan_track(new AutomationTimeAxisView (_session,
					_route, _route, pan_control, 
					editor,
					*this,
					false,
					parent_canvas,
					_route->describe_parameter(pan_control->parameter())));
		add_automation_child(*p, pan_track, show);
		++i;
	}
}
		
void
AudioTimeAxisView::show_all_automation ()
{
	no_redraw = true;

	RouteTimeAxisView::show_all_automation ();

	no_redraw = false;

	 _route->gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
}

void
AudioTimeAxisView::show_existing_automation ()
{
	no_redraw = true;

	RouteTimeAxisView::show_existing_automation ();

	no_redraw = false;

	 _route->gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
}

void
AudioTimeAxisView::hide_all_automation ()
{
	no_redraw = true;

	RouteTimeAxisView::hide_all_automation();

	no_redraw = false;
	 _route->gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
}

void
AudioTimeAxisView::show_all_xfades ()
{
	AudioStreamView* asv = audio_view();

	if (asv) {
		asv->show_all_xfades ();
	}
}

void
AudioTimeAxisView::hide_all_xfades ()
{
	AudioStreamView* asv = audio_view();
	
	if (asv) {
		asv->hide_all_xfades ();
	}
}

void
AudioTimeAxisView::hide_dependent_views (TimeAxisViewItem& tavi)
{
	AudioStreamView* asv = audio_view();
	AudioRegionView* rv;

	if (asv && (rv = dynamic_cast<AudioRegionView*>(&tavi)) != 0) {
		asv->hide_xfades_involving (*rv);
	}
}

void
AudioTimeAxisView::reveal_dependent_views (TimeAxisViewItem& tavi)
{
	AudioStreamView* asv = audio_view();
	AudioRegionView* rv;

	if (asv && (rv = dynamic_cast<AudioRegionView*>(&tavi)) != 0) {
		asv->reveal_xfades_involving (*rv);
	}
}

void
AudioTimeAxisView::route_active_changed ()
{
	RouteTimeAxisView::route_active_changed ();
	update_control_names ();
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

	if (get_selected()) {
		controls_ebox.set_name (controls_base_selected_name);
	} else {
		controls_ebox.set_name (controls_base_unselected_name);
	}
}

