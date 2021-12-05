/*
 * Copyright (C) 2021 Robin Gareus <robin@gareus.org>
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

#include <list>

#include <sigc++/bind.h>

#include "pbd/unwind.h"

#include "ardour/audio_track.h"
#include "ardour/pannable.h"
#include "ardour/panner.h"
#include "ardour/panner_manager.h"
#include "ardour/panner_shell.h"
#include "ardour/profile.h"

#include "gtkmm2ext/utils.h"

#include "widgets/tooltips.h"

#include "gui_thread.h"
#include "meter_patterns.h"
#include "mixer_ui.h"
#include "plugin_selector.h"
#include "plugin_ui.h"
#include "trigger_stopper.h"
#include "trigger_strip.h"
#include "ui_config.h"

#include "pbd/i18n.h"

#define PX_SCALE(px) std::max ((float)px, rintf ((float)px* UIConfiguration::instance ().get_ui_scale ()))

using namespace ARDOUR;
using namespace ArdourWidgets;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace std;

PBD::Signal1<void, TriggerStrip*> TriggerStrip::CatchDeletion;

TriggerStrip::TriggerStrip (Session* s, boost::shared_ptr<ARDOUR::Route> rt)
	: SessionHandlePtr (s)
	, RouteUI (s)
	, _clear_meters (true)
	, _pb_selection ()
	, _stopper_widget ( -1, 16 )
	, _processor_box (s, boost::bind (&TriggerStrip::plugin_selector, this), _pb_selection, 0)
	, _trigger_display (*rt->triggerbox (), -1., 8*16.)
	, _panners (s)
	, _level_meter (s)
{
	init ();
	set_route (rt);

	io_changed ();
	name_changed ();
	map_frozen ();
	update_sensitivity ();
	show ();
}

TriggerStrip::~TriggerStrip ()
{
	CatchDeletion (this);
}

void
TriggerStrip::self_delete ()
{
	delete this;
}

string
TriggerStrip::state_id () const
{
	return string_compose ("trigger %1", _route->id ().to_s ());
}

void
TriggerStrip::set_session (Session* s)
{
	RouteUI::set_session (s);
	if (!s) {
		return;
	}
	s->config.ParameterChanged.connect (*this, invalidator (*this), ui_bind (&TriggerStrip::parameter_changed, this, _1), gui_context ());
}

string
TriggerStrip::name () const
{
	return _route->name ();
}

Gdk::Color
TriggerStrip::color () const
{
	return RouteUI::route_color ();
}

void
TriggerStrip::init ()
{
	_name_button.set_name ("mixer strip button");
	_name_button.set_text_ellipsize (Pango::ELLIPSIZE_END);
	_name_button.signal_size_allocate ().connect (sigc::mem_fun (*this, &TriggerStrip::name_button_resized));

	/* strip layout */
	global_vpacker.set_spacing (2);
	global_vpacker.pack_start (_trigger_display, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (_stopper_widget, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (_processor_box, true, true);
	global_vpacker.pack_start (_name_button, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (_panners, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (mute_solo_table, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (volume_table, Gtk::PACK_SHRINK);

	/* Mute & Solo */
	mute_solo_table.set_homogeneous (true);
	mute_solo_table.set_spacings (2);
	mute_solo_table.attach (*mute_button, 0, 1, 0, 1);
	mute_solo_table.attach (*solo_button, 1, 2, 0, 1);

	volume_table.attach (_level_meter, 0, 1, 0, 1);
	/*Note: _gain_control is added in set_route */

	/* top-level */
	global_frame.add (global_vpacker);
	global_frame.set_shadow_type (Gtk::SHADOW_IN);
	global_frame.set_name ("BaseFrame");

	add (global_frame);

	/* Signals */
	_name_button.signal_button_press_event ().connect (sigc::mem_fun (*this, &TriggerStrip::name_button_press), false);

	ArdourMeter::ResetAllPeakDisplays.connect (sigc::mem_fun (*this, &TriggerStrip::reset_peak_display));
	ArdourMeter::ResetRoutePeakDisplays.connect (sigc::mem_fun (*this, &TriggerStrip::reset_route_peak_display));
	ArdourMeter::ResetGroupPeakDisplays.connect (sigc::mem_fun (*this, &TriggerStrip::reset_group_peak_display));

	/* Visibility */
	_stopper_widget.show ();
	_name_button.show ();
	_trigger_display.show ();
	_processor_box.show ();
	_level_meter.show ();

	mute_button->show ();
	solo_button->show ();

	mute_solo_table.show ();
	volume_table.show ();
	global_frame.show ();
	global_vpacker.show ();
	show ();

	/* Width -- wide channel strip
	 * Note that panners require an ven number of horiz. pixels 
	 */
	const float scale = std::max (1.f, UIConfiguration::instance ().get_ui_scale ());
	int         width = rintf (110.f * scale) + 1;
	width &= ~1;
	set_size_request (width, -1);
}

void
TriggerStrip::set_route (boost::shared_ptr<Route> rt)
{
	RouteUI::set_route (rt);

	_stopper = new TriggerStopper(_stopper_widget.root(), _route->triggerbox());
	
	_processor_box.set_route (rt);

	/* Fader/Gain */
	boost::shared_ptr<AutomationControl> ac = _route->gain_control ();
	_gain_control                           = AutomationController::create (ac->parameter (), ParameterDescriptor (ac->parameter ()), ac, false);
	_gain_control->set_name (X_("ProcessorControlSlider"));
	_gain_control->set_size_request (PX_SCALE (19), -1);
	_gain_control->disable_vertical_scroll ();
	volume_table.attach (*_gain_control, 0, 1, 1, 2);

	_level_meter.set_meter (_route->shared_peak_meter ().get ());
	_level_meter.clear_meters ();
	_level_meter.setup_meters (PX_SCALE (100), PX_SCALE (10), 6);

	_route->input ()->changed.connect (*this, invalidator (*this), boost::bind (&TriggerStrip::io_changed, this), gui_context ());
	_route->output ()->changed.connect (*this, invalidator (*this), boost::bind (&TriggerStrip::io_changed, this), gui_context ());
	_route->io_changed.connect (route_connections, invalidator (*this), boost::bind (&TriggerStrip::io_changed, this), gui_context ());

	if (_route->panner_shell ()) {
		update_panner_choices ();
		_route->panner_shell ()->Changed.connect (route_connections, invalidator (*this), boost::bind (&TriggerStrip::connect_to_pan, this), gui_context ());
	}

	_panners.set_panner (_route->main_outs ()->panner_shell (), _route->main_outs ()->panner ());
	_panners.setup_pan ();
	connect_to_pan ();

#if 0
	if (_route->panner()) {
		((Gtk::Label*)_panners.pan_automation_state_button.get_child())->set_text (GainMeterBase::short_astate_string (_route->pannable()->automation_state()));
	}

#endif
}

void
TriggerStrip::set_button_names ()
{
	mute_button->set_text (_("Mute"));
	monitor_input_button->set_text (_("In"));
	monitor_disk_button->set_text (_("Disk"));

	if (!Config->get_solo_control_is_listen_control ()) {
		solo_button->set_text (_("Solo"));
	} else {
		switch (Config->get_listen_position ()) {
			case AfterFaderListen:
				solo_button->set_text (_("AFL"));
				break;
			case PreFaderListen:
				solo_button->set_text (_("PFL"));
				break;
		}
	}
}

void
TriggerStrip::connect_to_pan ()
{
	ENSURE_GUI_THREAD (*this, &TriggerStrip::connect_to_pan)

	_panstate_connection.disconnect ();

	if (!_route->panner ()) {
		return;
	}

	boost::shared_ptr<Pannable> p = _route->pannable ();

	p->automation_state_changed.connect (_panstate_connection, invalidator (*this), boost::bind (&PannerUI::pan_automation_state_changed, &_panners), gui_context ());

	if (_panners._panner == 0) {
		_panners.panshell_changed ();
	}
	update_panner_choices ();
}

void
TriggerStrip::update_panner_choices ()
{
	/* code-dup TriggerStrip::update_panner_choices */
	ENSURE_GUI_THREAD (*this, &TriggerStrip::update_panner_choices);
	if (!_route->panner_shell ()) {
		return;
	}

	uint32_t in  = _route->output ()->n_ports ().n_audio ();
	uint32_t out = in;
	if (_route->panner ()) {
		in = _route->panner ()->in ().n_audio ();
	}

	_panners.set_available_panners (PannerManager::instance ().PannerManager::get_available_panners (in, out));
}

void
TriggerStrip::route_property_changed (const PropertyChange& what_changed)
{
	if (what_changed.contains (ARDOUR::Properties::name)) {
		name_changed ();
	}
}

void
TriggerStrip::route_color_changed ()
{
	_name_button.modify_bg (STATE_NORMAL, color ());
}

void
TriggerStrip::route_active_changed ()
{
	RouteUI::route_active_changed ();
	update_sensitivity ();
}

void
TriggerStrip::update_sensitivity ()
{
	bool en = _route->active ();
	monitor_input_button->set_sensitive (en);
	monitor_disk_button->set_sensitive (en);

#if 0
	if (!en) {
		end_rename (true);
	}

	if (!is_track() || track()->mode() != ARDOUR::Normal) {
		_playlist_button.set_sensitive (false);
	}
#endif
}

PluginSelector*
TriggerStrip::plugin_selector ()
{
	return Mixer_UI::instance ()->plugin_selector ();
}

void
TriggerStrip::hide_processor_editor (boost::weak_ptr<Processor> p)
{
	/* TODO consolidate w/ TriggerStrip::hide_processor_editor
	 * -> RouteUI ?
	 */
	boost::shared_ptr<Processor> processor (p.lock ());
	if (!processor) {
		return;
	}

	Gtk::Window* w = _processor_box.get_processor_ui (processor);

	if (w) {
		w->hide ();
	}
}

void
TriggerStrip::map_frozen ()
{
	ENSURE_GUI_THREAD (*this, &TriggerStrip::map_frozen)

	boost::shared_ptr<AudioTrack> at = audio_track ();

	bool en = _route->active () || ARDOUR::Profile->get_mixbus ();

	if (at) {
		switch (at->freeze_state ()) {
			case AudioTrack::Frozen:
				_processor_box.set_sensitive (false);
				_route->foreach_processor (sigc::mem_fun (*this, &TriggerStrip::hide_processor_editor));
				break;
			default:
				_processor_box.set_sensitive (en);
				break;
		}
	} else {
		_processor_box.set_sensitive (en);
	}
	RouteUI::map_frozen ();
}

void
TriggerStrip::fast_update ()
{
	if (is_mapped ()) {
		if (_clear_meters) {
			_level_meter.clear_meters ();
			_clear_meters = false;
		}
		_level_meter.update_meters ();
	}
}

void
TriggerStrip::reset_route_peak_display (Route* route)
{
	if (_route && _route.get () == route) {
		reset_peak_display ();
	}
}

void
TriggerStrip::reset_group_peak_display (RouteGroup* group)
{
	if (_route && group == _route->route_group ()) {
		reset_peak_display ();
	}
}

void
TriggerStrip::reset_peak_display ()
{
	//_route->shared_peak_meter ()->reset_max ();
	_clear_meters = true;
}

void
TriggerStrip::parameter_changed (string p)
{
}

void
TriggerStrip::io_changed ()
{
	if (has_audio_outputs ()) {
		_panners.show_all ();
	} else {
		_panners.hide_all ();
	}
}

void
TriggerStrip::name_changed ()
{
	_name_button.set_text (_route->name ());
	set_tooltip (_name_button, Gtkmm2ext::markup_escape_text (_route->name ()));
}

void
TriggerStrip::name_button_resized (Gtk::Allocation& alloc)
{
	_name_button.set_layout_ellipsize_width (alloc.get_width () * PANGO_SCALE);
}

bool
TriggerStrip::name_button_press (GdkEventButton*)
{
	// TODO
	return false;
}
