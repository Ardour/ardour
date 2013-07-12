/*
    Copyright (C) 2000-2004 Paul Davis

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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <boost/foreach.hpp>

#include "pbd/file_utils.h"
#include "pbd/error.h"

#include "ardour/filesystem_paths.h"

#include "actions.h"
#include "mixer_actor.h"
#include "mixer_strip.h"
#include "route_ui.h"

#include "i18n.h"

#ifdef SearchPath
#undef SearchPath
#endif

using namespace ARDOUR;
using namespace Gtk;
using namespace PBD;

MixerActor::MixerActor ()
{
	register_actions ();
	load_bindings ();
}

MixerActor::~MixerActor ()
{
}

void
MixerActor::register_actions ()
{
	myactions.register_action ("Mixer", "solo", _("Toggle Solo on Mixer-Selected Tracks/Busses"), sigc::mem_fun (*this, &MixerActor::solo_action));
	myactions.register_action ("Mixer", "mute", _("Toggle Mute on Mixer-Selected Tracks/Busses"), sigc::mem_fun (*this, &MixerActor::mute_action));
	myactions.register_action ("Mixer", "recenable", _("Toggle Rec-enable on Mixer-Selected Tracks/Busses"), sigc::mem_fun (*this, &MixerActor::rec_enable_action));
	myactions.register_action ("Mixer", "increment-gain", _("Decrease Gain on Mixer-Selected Tracks/Busses"), sigc::mem_fun (*this, &MixerActor::step_gain_up_action));
	myactions.register_action ("Mixer", "decrement-gain", _("Increase Gain on Mixer-Selected Tracks/Busses"), sigc::mem_fun (*this, &MixerActor::step_gain_down_action));
	myactions.register_action ("Mixer", "unity-gain", _("Set Gain to 0dB on Mixer-Selected Tracks/Busses"), sigc::mem_fun (*this, &MixerActor::unity_gain_action));


	myactions.register_action ("Mixer", "copy-processors", _("Copy Selected Processors"), sigc::mem_fun (*this, &MixerActor::copy_processors));
	myactions.register_action ("Mixer", "cut-processors", _("Cut Selected Processors"), sigc::mem_fun (*this, &MixerActor::cut_processors));
	myactions.register_action ("Mixer", "paste-processors", _("Paste Selected Processors"), sigc::mem_fun (*this, &MixerActor::paste_processors));
	myactions.register_action ("Mixer", "delete-processors", _("Delete Selected Processors"), sigc::mem_fun (*this, &MixerActor::delete_processors));
	myactions.register_action ("Mixer", "select-all-processors", _("Select All (visible) Processors"), sigc::mem_fun (*this, &MixerActor::select_all_processors));
	myactions.register_action ("Mixer", "toggle-processors", _("Toggle Selected Processors"), sigc::mem_fun (*this, &MixerActor::toggle_processors));
	myactions.register_action ("Mixer", "ab-plugins", _("Toggle Selected Plugins"), sigc::mem_fun (*this, &MixerActor::ab_plugins));


	myactions.register_action ("Mixer", "scroll-left", _("Scroll Mixer Window to the left"), sigc::mem_fun (*this, &MixerActor::scroll_left));
	myactions.register_action ("Mixer", "scroll-right", _("Scroll Mixer Window to the left"), sigc::mem_fun (*this, &MixerActor::scroll_right));

	myactions.register_action ("Mixer", "toggle-midi-input-active", _("Toggle MIDI Input Active for Mixer-Selected Tracks/Busses"), 
				   sigc::bind (sigc::mem_fun (*this, &MixerActor::toggle_midi_input_active), false));
}

void
MixerActor::load_bindings ()
{
        /* XXX move this to a better place */
	
        bindings.set_action_map (myactions);

	std::string binding_file;

	if (find_file_in_search_path (ardour_config_search_path(), "mixer.bindings", binding_file)) {
                bindings.load (binding_file);
		info << string_compose (_("Loaded mixer bindings from %1"), binding_file) << endmsg;
        } else {
		error << string_compose (_("Could not find mixer.bindings in search path %1"), ardour_config_search_path().to_string()) << endmsg;
	}
}

void
MixerActor::solo_action ()
{
	GdkEventButton ev;

	ev.type = GDK_BUTTON_PRESS;
	ev.button = 1;
	ev.state = 0;

	set_route_targets_for_operation ();

	BOOST_FOREACH(RouteUI* r, _route_targets) {
		r->solo_press (&ev);
	}
}

void
MixerActor::mute_action ()
{
	GdkEventButton ev;

	ev.type = GDK_BUTTON_PRESS;
	ev.button = 1;
	ev.state = 0;

	set_route_targets_for_operation ();

	BOOST_FOREACH(RouteUI* r, _route_targets) {
		r->mute_press (&ev);
	}
}

void
MixerActor::rec_enable_action ()
{
	GdkEventButton ev;

	ev.type = GDK_BUTTON_PRESS;
	ev.button = 1;
	ev.state = 0;

	set_route_targets_for_operation ();

	BOOST_FOREACH(RouteUI* r, _route_targets) {
		r->rec_enable_press (&ev);
	}
}

void
MixerActor::step_gain_up_action ()
{
	set_route_targets_for_operation ();

	BOOST_FOREACH(RouteUI* r, _route_targets) {
		MixerStrip* ms = dynamic_cast<MixerStrip*> (r);
		if (ms) {
			ms->step_gain_up ();
		}
	}
}

void
MixerActor::step_gain_down_action ()
{
	set_route_targets_for_operation ();

	BOOST_FOREACH(RouteUI* r, _route_targets) {
		MixerStrip* ms = dynamic_cast<MixerStrip*> (r);
		if (ms) {
			ms->step_gain_down ();
		}
	}
}

void
MixerActor::unity_gain_action ()
{
	set_route_targets_for_operation ();

	BOOST_FOREACH(RouteUI* r, _route_targets) {
		boost::shared_ptr<Route> rp = r->route();
		if (rp) {
			rp->set_gain (1.0, this);
		}
	}
}

void
MixerActor::copy_processors ()
{
	set_route_targets_for_operation ();

	BOOST_FOREACH(RouteUI* r, _route_targets) {
		MixerStrip* ms = dynamic_cast<MixerStrip*> (r);
		if (ms) {
			ms->copy_processors ();
		}
	}
}
void
MixerActor::cut_processors ()
{
	set_route_targets_for_operation ();

	BOOST_FOREACH(RouteUI* r, _route_targets) {
		MixerStrip* ms = dynamic_cast<MixerStrip*> (r);
		if (ms) {
			ms->cut_processors ();
		}
	}
}
void
MixerActor::paste_processors ()
{
	set_route_targets_for_operation ();

	BOOST_FOREACH(RouteUI* r, _route_targets) {
		MixerStrip* ms = dynamic_cast<MixerStrip*> (r);
		if (ms) {
			ms->paste_processors ();
		}
	}
}
void
MixerActor::select_all_processors ()
{
	set_route_targets_for_operation ();

	BOOST_FOREACH(RouteUI* r, _route_targets) {
		MixerStrip* ms = dynamic_cast<MixerStrip*> (r);
		if (ms) {
			ms->select_all_processors ();
		}
	}
}
void
MixerActor::delete_processors ()
{
	set_route_targets_for_operation ();

	BOOST_FOREACH(RouteUI* r, _route_targets) {
		MixerStrip* ms = dynamic_cast<MixerStrip*> (r);
		if (ms) {
			ms->delete_processors ();
		}
	}
}
void
MixerActor::toggle_processors ()
{
	set_route_targets_for_operation ();

	BOOST_FOREACH(RouteUI* r, _route_targets) {
		MixerStrip* ms = dynamic_cast<MixerStrip*> (r);
		if (ms) {
			ms->toggle_processors ();
		}
	}
}
void
MixerActor::ab_plugins ()
{
	set_route_targets_for_operation ();

	BOOST_FOREACH(RouteUI* r, _route_targets) {
		MixerStrip* ms = dynamic_cast<MixerStrip*> (r);
		if (ms) {
			ms->ab_plugins ();
		}
	}
}

