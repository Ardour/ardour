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

#include "ardour/plugin_insert.h"
#include "ardour/port_insert.h"
#include "ardour/route.h"
#include "ardour/session.h"

#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/utils.h"

#include "widgets/frame.h"

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
	: _idle_refill_processors_id (-1)
{
	_scroller.set_policy (Gtk::POLICY_AUTOMATIC, Gtk::POLICY_NEVER);
	_scroller.add (_box);

	_box.set_spacing (4);

	pack_start (_scroller, true, true);
	show_all();
}

RoutePropertiesBox::~RoutePropertiesBox ()
{
}

void
RoutePropertiesBox::session_going_away ()
{
	ENSURE_GUI_THREAD (*this, &RoutePropertiesBox::session_going_away);
	SessionHandlePtr::session_going_away ();

	drop_plugin_uis ();
	drop_route ();
}

void
RoutePropertiesBox::set_route (std::shared_ptr<Route> r)
{
	if (r == _route) {
		return;
	}
	assert (r);
	_route = r;
	_route_connections.drop_connections ();

	_route->processors_changed.connect (_route_connections, invalidator (*this), std::bind (&RoutePropertiesBox::idle_refill_processors, this), gui_context());
	_route->PropertyChanged.connect (_route_connections, invalidator (*this), std::bind (&RoutePropertiesBox::property_changed, this, _1), gui_context ());
	_route->DropReferences.connect (_route_connections, invalidator (*this), std::bind (&RoutePropertiesBox::drop_route, this), gui_context());
	refill_processors ();
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
			h = std::max<int> (h, ui->get_preferred_height () + /* frame label */ 30 * ui_scale);
		}
		h = std::min<int> (h, 300 * ui_scale);
		_box.set_size_request (-1, h);
		_scroller.show_all ();
	}
	_idle_refill_processors_id = -1;
}
