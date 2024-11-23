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
#include <gtkmm/widget.h>

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

#include "pbd/i18n.h"

using namespace Gtk;
using namespace ARDOUR;
using namespace ArdourWidgets;

RoutePropertiesBox::RoutePropertiesBox ()
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

	_route->processors_changed.connect (_route_connections, invalidator (*this), std::bind (&RoutePropertiesBox::refill_processors, this), gui_context());
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
	GenericPluginUI* plugin_ui = new GenericPluginUI (pib, true, true);
	pib->DropReferences.connect (_processor_connections, invalidator (*this), std::bind (&RoutePropertiesBox::refill_processors, this), gui_context());
	_proc_uis.push_back (plugin_ui);

	ArdourWidgets::Frame* frame = new ArdourWidgets::Frame ();
	frame->set_label (p->display_name ());
	frame->add (*plugin_ui);
	_box.pack_start (*frame, false, false);
	plugin_ui->show ();
}

void
RoutePropertiesBox::refill_processors ()
{
	if (!_session || _session->deletion_in_progress()) {
		return;
	}
	drop_plugin_uis ();

	assert (_route);
	_route->foreach_processor (sigc::mem_fun (*this, &RoutePropertiesBox::add_processor_to_display));
	if (_proc_uis.empty ()) {
		_scroller.hide ();
	} else {
		int h = 60;
		for (auto const& ui : _proc_uis) {
			h = std::max<int> (h, ui->get_preferred_height () + /* frame label */ 22);
		}
		h = std::min<int> (h, 300);
		_scroller.set_size_request (-1, h);
		_scroller.show_all ();
	}
}
