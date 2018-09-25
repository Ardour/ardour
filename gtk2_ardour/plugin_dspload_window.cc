/*
 * Copyright (C) 2018 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <gtkmm/frame.h>
#include <gtkmm/label.h>
#include <gtkmm/viewport.h>

#include "ardour/session.h"
#include "gtkmm2ext/gui_thread.h"

#include "plugin_dspload_ui.h"
#include "plugin_dspload_window.h"

#include "pbd/i18n.h"

using namespace ARDOUR;

PluginDSPLoadWindow::PluginDSPLoadWindow ()
	: ArdourWindow (_("Plugin DSP Load"))
	, _reset_button (_("Reset All Stats"))
{
	_scroller.set_border_width (0);
	_scroller.set_shadow_type (Gtk::SHADOW_NONE);
	_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
	_scroller.add (_box);

	_reset_button.set_name ("generic button");
	_reset_button.signal_clicked.connect (sigc::mem_fun (*this, &PluginDSPLoadWindow::clear_all_stats));

	add (_scroller);
	_box.show ();
	_scroller.show ();

	Gtk::Viewport* viewport = (Gtk::Viewport*) _scroller.get_child();
	viewport->set_shadow_type(Gtk::SHADOW_NONE);
	viewport->set_border_width(0);
}

PluginDSPLoadWindow::~PluginDSPLoadWindow ()
{
	drop_references ();
}

void
PluginDSPLoadWindow::set_session (Session* s)
{
	ArdourWindow::set_session (s);
	if (!s) {
		drop_references ();
	} else if (is_visible ()) {
		refill_processors ();
	}
}

void
PluginDSPLoadWindow::session_going_away ()
{
	ENSURE_GUI_THREAD (*this, &PluginDSPLoadWindow::session_going_away);
	ArdourWindow::session_going_away ();
	drop_references ();
}

void
PluginDSPLoadWindow::on_show ()
{
	ArdourWindow::on_show ();
	refill_processors ();
}

void
PluginDSPLoadWindow::on_hide ()
{
	ArdourWindow::on_hide ();
	drop_references ();
}

void
PluginDSPLoadWindow::clear_all_stats ()
{
	RouteList routes = _session->get_routelist ();
	for (RouteList::const_iterator i = routes.begin(); i != routes.end(); ++i) {
		(*i)->foreach_processor (sigc::mem_fun (*this, &PluginDSPLoadWindow::clear_processor_stats));
	}
}

void
PluginDSPLoadWindow::drop_references ()
{
	std::list<Gtk::Widget*> children = _box.get_children ();
	for (std::list<Gtk::Widget*>::iterator child = children.begin(); child != children.end(); ++child) {
		(*child)->hide ();
		_box.remove (**child);
		if (*child != &_reset_button) {
			delete *child;
		}
	}
	_route_connections.drop_connections ();
	_processor_connections.drop_connections ();
}

void
PluginDSPLoadWindow::refill_processors ()
{
	drop_references ();
	if (!_session || _session->deletion_in_progress()) {
		/* may be called from session d'tor, removing monitor-section w/plugin */
		return;
	}
	RouteList routes = _session->get_routelist ();
	for (RouteList::const_iterator i = routes.begin(); i != routes.end(); ++i) {

		(*i)->foreach_processor (sigc::bind (sigc::mem_fun (*this, &PluginDSPLoadWindow::add_processor_to_display), (*i)->name()));

		(*i)->processors_changed.connect (
				_route_connections, invalidator (*this), boost::bind (&PluginDSPLoadWindow::refill_processors, this), gui_context()
				);

		(*i)->DropReferences.connect (
				_route_connections, invalidator (*this), boost::bind (&PluginDSPLoadWindow::drop_references, this), gui_context()
				);
	}

	if (_box.get_children().size() == 0) {
		_box.add (*Gtk::manage (new Gtk::Label (_("No Plugins"))));
		_box.show_all ();
	} else if (_box.get_children().size() > 1) {
		_box.pack_start (_reset_button, Gtk::PACK_SHRINK, 2);
		_reset_button.show ();
	}
}

void
PluginDSPLoadWindow::add_processor_to_display (boost::weak_ptr<Processor> w, std::string const& route_name)
{
	boost::shared_ptr<Processor> p = w.lock ();
	boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert> (p);
	if (!pi) {
		return;
	}
	p->DropReferences.connect (_processor_connections, MISSING_INVALIDATOR, boost::bind (&PluginDSPLoadWindow::drop_references, this), gui_context());
	PluginLoadStatsGui* plsg = new PluginLoadStatsGui (pi);
	
	std::string name = route_name + " - " + pi->name();
	Gtk::Frame* frame = new Gtk::Frame (name.c_str());
	frame->add (*Gtk::manage (plsg));
	_box.pack_start (*frame, Gtk::PACK_SHRINK, 2);

	plsg->start_updating ();
	frame->show_all ();
}

void
PluginDSPLoadWindow::clear_processor_stats (boost::weak_ptr<Processor> w)
{
	boost::shared_ptr<Processor> p = w.lock ();
	boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert> (p);
	if (pi) {
		pi->clear_stats ();
	}
}
