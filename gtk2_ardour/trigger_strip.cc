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
#include "ardour/profile.h"

#include "gtkmm2ext/utils.h"

#include "widgets/tooltips.h"

#include "gui_thread.h"
#include "mixer_ui.h"
#include "plugin_selector.h"
#include "plugin_ui.h"
#include "trigger_strip.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ArdourWidgets;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace std;

PBD::Signal1<void, TriggerStrip*> TriggerStrip::CatchDeletion;

#define PX_SCALE(pxmin, dflt) rint (std::max ((double)pxmin, (double)dflt* UIConfiguration::instance ().get_ui_scale ()))

TriggerStrip::TriggerStrip (Session* s, boost::shared_ptr<ARDOUR::Route> rt)
	: SessionHandlePtr (s)
	, RouteUI (s)
	, _pb_selection ()
	, _processor_box (s, boost::bind (&TriggerStrip::plugin_selector, this), _pb_selection, 0)
	, _trigger_display (*rt->triggerbox ())
{
	init ();
	RouteUI::set_route (rt);

	/* set route */
	_processor_box.set_route (rt);

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

	/* main layout */
	global_vpacker.set_spacing (2);
	global_vpacker.pack_start (_name_button, Gtk::PACK_SHRINK);
	global_vpacker.pack_start (_trigger_display, true, true); // XXX
	global_vpacker.pack_start (_processor_box, true, true);

	/* top-level layout */
	global_frame.add (global_vpacker);
	global_frame.set_shadow_type (Gtk::SHADOW_IN);
	global_frame.set_name ("BaseFrame");

	add (global_frame);

	/* Signals */
	_name_button.signal_button_press_event ().connect (sigc::mem_fun (*this, &TriggerStrip::name_button_press), false);

	/* Visibility */
	_name_button.show ();
	_trigger_display.show ();
	_processor_box.show ();

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
TriggerStrip::set_button_names ()
{
	mute_button->set_text (S_("Mute|M"));
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
	/* TODO consolidate w/ MixerStrip::hide_processor_editor
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
}

void
TriggerStrip::parameter_changed (string p)
{
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
