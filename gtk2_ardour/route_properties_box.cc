/*
 * Copyright (C) 2011-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2024 Ben Loftis <ben@harrisonconsoles.com>
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

#include <cassert>
#include <ytkmm/widget.h>

#include "pbd/compose.h"

#include "ardour/audio_track.h"
#include "ardour/plugin_insert.h"
#include "ardour/port_insert.h"
#include "ardour/route.h"
#include "ardour/session.h"

#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/utils.h"

#include "ardour_ui.h"
#include "mixer_ui.h"
#include "plugin_selector.h"
#include "plugin_ui.h"
#include "port_insert_ui.h"
#include "route_properties_box.h"
#include "timers.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace ARDOUR;
using namespace ArdourWidgets;

RoutePropertiesBox::RoutePropertiesBox ()
	: _insert_box (nullptr)
	, _show_insert (false)
	, _idle_refill_processors_id (-1)
{
	_scroller.set_policy (Gtk::POLICY_AUTOMATIC, Gtk::POLICY_NEVER);
	_scroller.add (_box);

	_box.set_spacing (4);
	_insert_frame.set_no_show_all ();

	pack_start (_insert_frame, false, false, 4);
	pack_start (_scroller, true, true);
	show_all();

	ARDOUR_UI::instance()->ActionsReady.connect_same_thread (_forever_connections, std::bind (&RoutePropertiesBox::ui_actions_ready, this));
}

RoutePropertiesBox::~RoutePropertiesBox ()
{
}

void
RoutePropertiesBox::ui_actions_ready ()
{
	Glib::RefPtr<ToggleAction> tact = ActionManager::get_toggle_action (X_("Editor"), X_("show-editor-mixer"));
	tact->signal_toggled().connect (sigc::mem_fun (*this, &RoutePropertiesBox::update_processor_box_visibility));
}

void
RoutePropertiesBox::session_going_away ()
{
	ENSURE_GUI_THREAD (*this, &RoutePropertiesBox::session_going_away);
	SessionHandlePtr::session_going_away ();

	_insert_frame.remove ();
	drop_plugin_uis ();
	drop_route ();
	delete _insert_box;
	_insert_box = nullptr;
}

void
RoutePropertiesBox::set_session (ARDOUR::Session* s) {
	SessionHandlePtr::set_session (s);
	if (!s) {
		return;
	}
	delete _insert_box;
	_insert_box = new ProcessorBox (_session, std::bind (&Mixer_UI::plugin_selector, Mixer_UI::instance()), Mixer_UI::instance()->selection(), 0);
	_insert_box->show_all ();

	float ui_scale = std::max<float> (1.f, UIConfiguration::instance().get_ui_scale());
	_insert_frame.remove ();
	_insert_frame.add (*_insert_box);
	_insert_frame.set_padding (4);
	_insert_frame.set_size_request (144 * ui_scale, 236 * ui_scale);

	_session->SurroundMasterAddedOrRemoved.connect (_session_connections, invalidator (*this), std::bind (&RoutePropertiesBox::surround_master_added_or_removed, this), gui_context());
}

void
RoutePropertiesBox::surround_master_added_or_removed ()
{
	set_route (_route, true);
}

void
RoutePropertiesBox::set_route (std::shared_ptr<Route> r, bool force_update)
{
	if (r == _route && !force_update) {
		return;
	}

	if (!r) {
		drop_route ();
		return;
	}

	_route = r;
	_route_connections.drop_connections ();

	_route->processors_changed.connect (_route_connections, invalidator (*this), std::bind (&RoutePropertiesBox::idle_refill_processors, this), gui_context());
	_route->PropertyChanged.connect (_route_connections, invalidator (*this), std::bind (&RoutePropertiesBox::property_changed, this, _1), gui_context ());
	_route->DropReferences.connect (_route_connections, invalidator (*this), std::bind (&RoutePropertiesBox::drop_route, this), gui_context());

	std::shared_ptr<AudioTrack> at = std::dynamic_pointer_cast<AudioTrack>(_route);
	if (at) {
		at->FreezeChange.connect (_route_connections, invalidator (*this), std::bind (&RoutePropertiesBox::map_frozen, this), gui_context());
	}

	_insert_box->set_route (r);
	refill_processors ();
}

void
RoutePropertiesBox::map_frozen ()
{
	ENSURE_GUI_THREAD (*this, &RoutePropertiesBox::map_frozen)
		std::shared_ptr<AudioTrack> at = std::dynamic_pointer_cast<AudioTrack>(_route);
	if (at && _insert_box) {
		switch (at->freeze_state()) {
			case AudioTrack::Frozen:
				_insert_box->set_sensitive (false);
				break;
			default:
				_insert_box->set_sensitive (true);
				break;
		}
	}
}

void
RoutePropertiesBox::update_processor_box_visibility ()
{
	_show_insert = !ActionManager::get_toggle_action (X_("Editor"), X_("show-editor-mixer"))->get_active ();
	if (!_show_insert || _proc_uis.empty ()) {
		_insert_frame.hide ();
	} else {
		_insert_frame.show ();
	}

#ifndef MIXBUS
	if (_show_insert || !_proc_uis.empty ()) {
		float ui_scale = std::max<float> (1.f, UIConfiguration::instance().get_ui_scale());
		set_size_request (-1, 365 * ui_scale); // match with SelectionPropertiesBox
	} else {
		set_size_request (-1, -1);
	}
#endif
}

void
RoutePropertiesBox::property_changed (const PBD::PropertyChange& what_changed)
{
}

void
RoutePropertiesBox::drop_route ()
{
	drop_plugin_uis ();
	_route.reset ();
	_route_connections.drop_connections ();
	if (_idle_refill_processors_id >= 0) {
		g_source_destroy (g_main_context_find_source_by_id (NULL, _idle_refill_processors_id));
		_idle_refill_processors_id = -1;
	}
}

void
RoutePropertiesBox::drop_plugin_uis ()
{
	std::list<Gtk::Widget*> children = _box.get_children ();
	for (auto const& child : children) {
		child->hide ();
		_box.remove (*child);
		delete child;
	}

	for (auto const& ui : _proc_uis) {
		ui->stop_updating (0);
		delete ui;
	}

	_processor_connections.drop_connections ();
	_proc_uis.clear ();
	_insert_frame.hide ();
}

void
RoutePropertiesBox::add_processor_to_display (std::weak_ptr<Processor> w)
{
	std::shared_ptr<Processor> p = w.lock ();
	std::shared_ptr<PlugInsertBase> pib = std::dynamic_pointer_cast<PlugInsertBase> (p);
	if (!pib) {
		return;
	}
#ifdef MIXBUS
	if (std::dynamic_pointer_cast<PluginInsert> (pib)->channelstrip () != Processor::None) {
		return;
	}
#endif
	GenericPluginUI* plugin_ui = new GenericPluginUI (pib, true, true);
	if (plugin_ui->empty ()) {
		delete plugin_ui;
		return;
	}
	//pib->DropReferences.connect (_processor_connections, invalidator (*this), std::bind (&RoutePropertiesBox::refill_processors, this), gui_context());
	_proc_uis.push_back (plugin_ui);

	ArdourWidgets::Frame* frame = new ArdourWidgets::Frame ();
	frame->set_label (p->display_name ());
	frame->add (*plugin_ui);
	frame->set_padding (0);
	_box.pack_start (*frame, false, false);
	plugin_ui->show ();
}

int
RoutePropertiesBox::_idle_refill_processors (gpointer arg)
{
	static_cast<RoutePropertiesBox*>(arg)->refill_processors ();
	return 0;
}

void
RoutePropertiesBox::idle_refill_processors ()
{
	if (_idle_refill_processors_id < 0) {
		_idle_refill_processors_id = g_idle_add_full (G_PRIORITY_HIGH_IDLE + 10, _idle_refill_processors, this, NULL);
	}
}

void
RoutePropertiesBox::refill_processors ()
{
	if (!_session || _session->deletion_in_progress()) {
		return;
	}
	drop_plugin_uis ();

	assert (_route);

	if (!_route) {
		_idle_refill_processors_id = -1;
		return;
	}

	_route->foreach_processor (sigc::mem_fun (*this, &RoutePropertiesBox::add_processor_to_display));
	if (_proc_uis.empty ()) {
		_scroller.hide ();
	} else {
		float ui_scale = std::max<float> (1.f, UIConfiguration::instance().get_ui_scale());
		int h = 100 * ui_scale;
		for (auto const& ui : _proc_uis) {
			h = std::max<int> (h, ui->get_preferred_height () + /* frame label */ 34 * ui_scale);
		}
		h = std::min<int> (h, 300 * ui_scale);
		_box.set_size_request (-1, h);
		_scroller.show_all ();
	}
	update_processor_box_visibility ();
	_idle_refill_processors_id = -1;
}
