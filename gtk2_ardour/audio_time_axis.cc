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
#include <ardour/insert.h>
#include <ardour/location.h>
#include <ardour/panner.h>
#include <ardour/playlist.h>
#include <ardour/session.h>
#include <ardour/session_playlist.h>
#include <ardour/utils.h>

#include "ardour_ui.h"
#include "audio_time_axis.h"
#include "automation_gain_line.h"
#include "automation_pan_line.h"
#include "canvas_impl.h"
#include "crossfade_view.h"
#include "enums.h"
#include "gain_automation_time_axis.h"
#include "keyboard.h"
#include "pan_automation_time_axis.h"
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
	gain_track = 0;
	pan_track = 0;
	waveform_item = 0;
	pan_automation_item = 0;
	gain_automation_item = 0;

	_view = new AudioStreamView (*this);

	add_gain_automation_child ();
	add_pan_automation_child ();

	ignore_toggle = false;

	mute_button->set_active (false);
	solo_button->set_active (false);
	
	if (is_audio_track())
		controls_ebox.set_name ("AudioTimeAxisViewControlsBaseUnselected");
	else // bus
		controls_ebox.set_name ("AudioBusControlsBaseUnselected");

	/* map current state of the route */

	redirects_changed (0);
	reset_redirect_automation_curves ();

	ensure_xml_node ();

	set_state (*xml_node);
	
	_route->panner().Changed.connect (mem_fun(*this, &AudioTimeAxisView::update_pans));

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
AudioTimeAxisView::set_state (const XMLNode& node)
{
	const XMLProperty *prop;
	
	TimeAxisView::set_state (node);
	
	if ((prop = node.property ("shown_editor")) != 0) {
		if (prop->value() == "no") {
			_marked_for_display = false;
		} else {
			_marked_for_display = true;
		}
	} else {
		_marked_for_display = true;
	}
	
	XMLNodeList nlist = node.children();
	XMLNodeConstIterator niter;
	XMLNode *child_node;
	
	
	show_gain_automation = false;
	show_pan_automation  = false;
	
	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		child_node = *niter;

		if (child_node->name() == "gain") {
			XMLProperty *prop=child_node->property ("shown");
			
			if (prop != 0) {
				if (prop->value() == "yes") {
					show_gain_automation = true;
				}
			}
			continue;
		}
		
		if (child_node->name() == "pan") {
			XMLProperty *prop=child_node->property ("shown");
			
			if (prop != 0) {
				if (prop->value() == "yes") {
					show_pan_automation = true;
				}			
			}
			continue;
		}
	}
}

void
AudioTimeAxisView::build_automation_action_menu ()
{
	using namespace Menu_Helpers;

	RouteTimeAxisView::build_automation_action_menu ();

	MenuList& automation_items = automation_action_menu->items();
	
	automation_items.push_back (SeparatorElem());

	automation_items.push_back (CheckMenuElem (_("Fader"), 
						   mem_fun(*this, &AudioTimeAxisView::toggle_gain_track)));
	gain_automation_item = static_cast<CheckMenuItem*> (&automation_items.back());
	gain_automation_item->set_active(show_gain_automation);

	automation_items.push_back (CheckMenuElem (_("Pan"),
						   mem_fun(*this, &AudioTimeAxisView::toggle_pan_track)));
	pan_automation_item = static_cast<CheckMenuItem*> (&automation_items.back());
	pan_automation_item->set_active(show_pan_automation);
	
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
AudioTimeAxisView::add_gain_automation_child ()
{
	XMLProperty* prop;
	AutomationLine* line;

	gain_track = new GainAutomationTimeAxisView (_session,
						     _route,
						     editor,
						     *this,
						     parent_canvas,
						     _("gain"),
						     _route->gain_automation_curve());
	
	line = new AutomationGainLine ("automation gain",
				       _session,
				       *gain_track,
				       *gain_track->canvas_display,
				       _route->gain_automation_curve());

	line->set_line_color (Config->canvasvar_AutomationLine.get());
	

	gain_track->add_line (*line);

	add_child (gain_track);

	gain_track->Hiding.connect (mem_fun(*this, &AudioTimeAxisView::gain_hidden));

	bool hideit = true;
	
	XMLNode* node;

	if ((node = gain_track->get_state_node()) != 0) {
		if  ((prop = node->property ("shown")) != 0) {
			if (prop->value() == "yes") {
				hideit = false;
			}
		} 
	}

	if (hideit) {
		gain_track->hide ();
	}
}

void
AudioTimeAxisView::add_pan_automation_child ()
{
	XMLProperty* prop;

	pan_track = new PanAutomationTimeAxisView (_session, _route, editor, *this, parent_canvas, _("pan"));

	update_pans ();
	
	add_child (pan_track);

	pan_track->Hiding.connect (mem_fun(*this, &AudioTimeAxisView::pan_hidden));

	ensure_xml_node ();
	bool hideit = true;
	
	XMLNode* node;

	if ((node = pan_track->get_state_node()) != 0) {
		if ((prop = node->property ("shown")) != 0) {
			if (prop->value() == "yes") {
				hideit = false;
			}
		} 
	}

	if (hideit) {
		pan_track->hide ();
	}
}

void
AudioTimeAxisView::update_pans ()
{
	Panner::iterator p;
	
	pan_track->clear_lines ();
	
	/* we don't draw lines for "greater than stereo" panning.
	 */

	if (_route->n_outputs().n_audio() > 2) {
		return;
	}

	for (p = _route->panner().begin(); p != _route->panner().end(); ++p) {

		AutomationLine* line;

		line = new AutomationPanLine ("automation pan", _session, *pan_track,
					      *pan_track->canvas_display, 
					      (*p)->automation());

		if (p == _route->panner().begin()) {
			/* first line is a nice orange */
			line->set_line_color (Config->canvasvar_AutomationLine.get());
		} else {
			/* second line is a nice blue */
			line->set_line_color (Config->canvasvar_AutomationLine.get());
		}

		pan_track->add_line (*line);
	}
}
		
void
AudioTimeAxisView::toggle_gain_track ()
{

	bool showit = gain_automation_item->get_active();

	if (showit != gain_track->marked_for_display()) {
		if (showit) {
			gain_track->set_marked_for_display (true);
			gain_track->canvas_display->show();
			gain_track->get_state_node()->add_property ("shown", X_("yes"));
		} else {
			gain_track->set_marked_for_display (false);
			gain_track->hide ();
			gain_track->get_state_node()->add_property ("shown", X_("no"));
		}

		/* now trigger a redisplay */
		
		if (!no_redraw) {
			 _route->gui_changed (X_("track_height"), (void *) 0); /* EMIT_SIGNAL */
		}
	}
}

void
AudioTimeAxisView::gain_hidden ()
{
	gain_track->get_state_node()->add_property (X_("shown"), X_("no"));

	if (gain_automation_item && !_hidden) {
		gain_automation_item->set_active (false);
	}

	 _route->gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
}

void
AudioTimeAxisView::toggle_pan_track ()
{
	bool showit = pan_automation_item->get_active();

	if (showit != pan_track->marked_for_display()) {
		if (showit) {
			pan_track->set_marked_for_display (true);
			pan_track->canvas_display->show();
			pan_track->get_state_node()->add_property ("shown", X_("yes"));
		} else {
			pan_track->set_marked_for_display (false);
			pan_track->hide ();
			pan_track->get_state_node()->add_property ("shown", X_("no"));
		}

		/* now trigger a redisplay */
		
		if (!no_redraw) {
			 _route->gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
		}
	}
}

void
AudioTimeAxisView::pan_hidden ()
{
	pan_track->get_state_node()->add_property ("shown", "no");

	if (pan_automation_item && !_hidden) {
		pan_automation_item->set_active (false);
	}

	 _route->gui_changed ("track_height", (void *) 0); /* EMIT_SIGNAL */
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

XMLNode* 
AudioTimeAxisView::get_child_xml_node (const string & childname)
{
	return RouteUI::get_child_xml_node (childname);
}

void
AudioTimeAxisView::set_layer_display (LayerDisplay d)
{
	AudioStreamView* asv = audio_view ();
	if (asv) {
		asv->set_layer_display (d);
	}
}
