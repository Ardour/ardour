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
	
	if (is_audio_track())
		controls_ebox.set_name ("AudioTimeAxisViewControlsBaseUnselected");
	else // bus
		controls_ebox.set_name ("AudioBusControlsBaseUnselected");

	/* map current state of the route */

	processors_changed ();
	reset_processor_automation_curves ();

	ensure_xml_node ();

	set_state (*xml_node);
	
	_route->panner().Changed.connect (bind (mem_fun(*this, &AudioTimeAxisView::update_pans), false));

	update_control_names ();

	if (is_audio_track()) {

		/* ask for notifications of any new RegionViews */
		_view->RegionViewAdded.connect (mem_fun(*this, &AudioTimeAxisView::region_view_added));
		_view->attach ();
	}

	post_construct ();
}

AudioTimeAxisView::~AudioTimeAxisView ()
{
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
	items.push_back (MenuElem (_("Hide all crossfades"), mem_fun(*this, &AudioTimeAxisView::hide_all_xfades)));
	items.push_back (MenuElem (_("Show all crossfades"), mem_fun(*this, &AudioTimeAxisView::show_all_xfades)));

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

	waveform_items.push_back (RadioMenuElem (group, _("Rectified"), bind (mem_fun(*this, &AudioTimeAxisView::set_waveform_shape), Rectified)));
	rectified_item = static_cast<RadioMenuItem *> (&waveform_items.back());

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
		if (asv->get_waveform_shape() == Rectified) 
			rectified_item->set_active(true);
		else traditional_item->set_active(true);

		if (asv->get_waveform_scale() == LogWaveform) 
			logscale_item->set_active(true);
		else linearscale_item->set_active(true);
		ignore_toggle = false;
	}

	items.push_back (MenuElem (_("Waveform"), *waveform_menu));


	Menu *layers_menu = manage(new Menu);
	MenuList &layers_items = layers_menu->items();
	layers_menu->set_name("ArdourContextMenu");

	RadioMenuItem::Group layers_group;
	
	layers_items.push_back(RadioMenuElem (layers_group, _("Overlaid"), bind (mem_fun (*this, &AudioTimeAxisView::set_layer_display), Overlaid)));
	layers_items.push_back(RadioMenuElem (layers_group, _("Stacked"), bind (mem_fun (*this, &AudioTimeAxisView::set_layer_display), Stacked)));

	items.push_back (MenuElem (_("Layers"), *layers_menu));
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
AudioTimeAxisView::create_automation_child (Parameter param, bool show)
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
				parent_canvas,
				_route->describe_parameter(param)));

		add_automation_child(Parameter(GainAutomation), gain_track, show);

	} else if (param.type() == PanAutomation) {

		ensure_xml_node ();
		update_pans (show);

	} else {
		error << "AudioTimeAxisView: unknown automation child " << param.to_string() << endmsg;
	}
}

void
AudioTimeAxisView::update_pans (bool show)
{
	Panner::iterator p;

	uint32_t i = 0;
	for (p = _route->panner().begin(); p != _route->panner().end(); ++p) {
		boost::shared_ptr<AutomationControl> pan_control = (*p)->pan_control();
		
		if (pan_control->parameter().type() == NullAutomation) {
			error << "Pan control has NULL automation type!" << endmsg;
			continue;
		}

		boost::shared_ptr<AutomationTimeAxisView> pan_track(new AutomationTimeAxisView (_session,
					_route, _route/*FIXME*/, pan_control, 
					editor,
					*this,
					parent_canvas,
					_route->describe_parameter(pan_control->parameter())));
		add_automation_child(Parameter(PanAutomation, i), pan_track, show);
		++i;
	}
}
		
void
AudioTimeAxisView::show_all_automation ()
{
	no_redraw = true;

	pan_automation_item->set_active (true);
	gain_automation_item->set_active (true);
	
	RouteTimeAxisView::show_all_automation ();

	no_redraw = false;

	 _route->gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
}

void
AudioTimeAxisView::show_existing_automation ()
{
	no_redraw = true;

	pan_automation_item->set_active (true);
	gain_automation_item->set_active (true);

	RouteTimeAxisView::show_existing_automation ();

	no_redraw = false;

	 _route->gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
}

void
AudioTimeAxisView::hide_all_automation ()
{
	no_redraw = true;

	pan_automation_item->set_active (false);
	gain_automation_item->set_active (false);

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
			controls_ebox.set_name ("AudioTrackControlsBaseUnselected");
			controls_base_selected_name = "AudioTrackControlsBaseSelected";
			controls_base_unselected_name = "AudioTrackControlsBaseUnselected";
		} else {
			controls_ebox.set_name ("AudioTrackControlsBaseInactiveUnselected");
			controls_base_selected_name = "AudioTrackControlsBaseInactiveSelected";
			controls_base_unselected_name = "AudioTrackControlsBaseInactiveUnselected";
		}
	} else {
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
AudioTimeAxisView::set_layer_display (LayerDisplay d)
{
	AudioStreamView* asv = audio_view ();
	if (asv) {
		asv->set_layer_display (d);
	}
}
