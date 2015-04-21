/*
    Copyright (C) 2002-2007 Paul Davis

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

#include <stdint.h>

#include <glibmm/objectbase.h>

#include <gtkmm2ext/doi.h>

#include "ardour/audioengine.h"
#include "ardour/bundle.h"
#include "ardour/data_type.h"
#include "ardour/io.h"
#include "ardour/port.h"
#include "ardour/session.h"

#include "monitor_selector.h"
#include "utils.h"
#include "gui_thread.h"
#include "i18n.h"

using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace Gtk;

MonitorSelector::MonitorSelector (Gtk::Window* p, ARDOUR::Session* session, boost::shared_ptr<ARDOUR::IO> io)
	: PortMatrix (p, session, DataType::AUDIO)
	, _io (io)
{
	set_type (DataType::AUDIO);

	/* signal flow from 0 to 1 */

	_find_inputs_for_io_outputs = (_io->direction() == IO::Output);

	if (_find_inputs_for_io_outputs) {
		_other = 1;
		_ours = 0;
	} else {
		_other = 0;
		_ours = 1;
	}

	_port_group.reset (new PortGroup (io->name()));
	_ports[_ours].add_group (_port_group);

	io->changed.connect (_io_connection, invalidator (*this), boost::bind (&MonitorSelector::io_changed_proxy, this), gui_context ());

	setup_all_ports ();
	init ();
}

void
MonitorSelector::io_changed_proxy ()
{
	/* The IO's changed signal is emitted from code that holds its route's processor lock,
	   so we can't call setup_all_ports (which results in a call to Route::foreach_processor)
	   without a deadlock unless we break things up with this idle handler.
	*/

	Glib::signal_idle().connect_once (sigc::mem_fun (*this, &MonitorSelector::io_changed));
}

void
MonitorSelector::io_changed ()
{
	setup_all_ports ();
}

void
MonitorSelector::setup_ports (int dim)
{
	if (!_session) {
		return;
	}

	_ports[dim].suspend_signals ();

	if (dim == _other) {

		_ports[_other].gather (_session, type(), _find_inputs_for_io_outputs, false, show_only_bundles ());

	} else {

		_port_group->clear ();
		_port_group->add_bundle (_io->bundle (), _io);
	}

	_ports[dim].resume_signals ();
}

void
MonitorSelector::set_state (ARDOUR::BundleChannel c[2], bool s)
{
	ARDOUR::Bundle::PortList const & our_ports = c[_ours].bundle->channel_ports (c[_ours].channel);
	ARDOUR::Bundle::PortList const & other_ports = c[_other].bundle->channel_ports (c[_other].channel);

	for (ARDOUR::Bundle::PortList::const_iterator i = our_ports.begin(); i != our_ports.end(); ++i) {
		for (ARDOUR::Bundle::PortList::const_iterator j = other_ports.begin(); j != other_ports.end(); ++j) {

			boost::shared_ptr<Port> f = _session->engine().get_port_by_name (*i);
			if (!f) {
				return;
			}

                        if (s) {
				if (!f->connected_to (*j)) {
					_io->connect (f, *j, 0);
				}
                        } else {
				if (f->connected_to (*j)) {
					_io->disconnect (f, *j, 0);
				}
                        }
		}
	}
}

PortMatrixNode::State
MonitorSelector::get_state (ARDOUR::BundleChannel c[2]) const
{
	if (c[0].bundle->nchannels() == ChanCount::ZERO || c[1].bundle->nchannels() == ChanCount::ZERO) {
		return PortMatrixNode::NOT_ASSOCIATED;
	}

	ARDOUR::Bundle::PortList const & our_ports = c[_ours].bundle->channel_ports (c[_ours].channel);
	ARDOUR::Bundle::PortList const & other_ports = c[_other].bundle->channel_ports (c[_other].channel);

	if (!_session || our_ports.empty() || other_ports.empty()) {
		/* we're looking at a bundle with no parts associated with this channel,
		   so nothing to connect */
		return PortMatrixNode::NOT_ASSOCIATED;
	}

	for (ARDOUR::Bundle::PortList::const_iterator i = our_ports.begin(); i != our_ports.end(); ++i) {
		for (ARDOUR::Bundle::PortList::const_iterator j = other_ports.begin(); j != other_ports.end(); ++j) {

			boost::shared_ptr<Port> f = _session->engine().get_port_by_name (*i);

			/* since we are talking about an IO, our ports should all have an associated Port *,
			   so the above call should never fail */
			assert (f);

			if (!f->connected_to (*j)) {
				/* if any one thing is not connected, all bets are off */
				return PortMatrixNode::NOT_ASSOCIATED;
			}
		}
	}

	return PortMatrixNode::ASSOCIATED;
}

uint32_t
MonitorSelector::n_io_ports () const
{
	if (!_find_inputs_for_io_outputs) {
		return _io->n_ports().get (_io->default_type());
	} else {
		return _io->n_ports().get (_io->default_type());
	}
}

bool
MonitorSelector::list_is_global (int dim) const
{
	return (dim == _other);
}

std::string
MonitorSelector::disassociation_verb () const
{
	return _("Disconnect");
}

std::string
MonitorSelector::channel_noun () const
{
	return _("port");
}

MonitorSelectorWindow::MonitorSelectorWindow (ARDOUR::Session* session, boost::shared_ptr<ARDOUR::IO> io, bool /*can_cancel*/)
	: ArdourWindow (_("Monitor output selector"))
	, _selector (this, session, io)
{
	set_name ("IOSelectorWindow2");

	add (_selector);

	io_name_changed (this);

	show_all ();

	signal_delete_event().connect (sigc::mem_fun (*this, &MonitorSelectorWindow::wm_delete));
}

bool
MonitorSelectorWindow::wm_delete (GdkEventAny* /*event*/)
{
	_selector.Finished (MonitorSelector::Accepted);
	return false;
}


void
MonitorSelectorWindow::on_map ()
{
	_selector.setup_all_ports ();
	Window::on_map ();
}

void
MonitorSelectorWindow::on_show ()
{
	Gtk::Window::on_show ();
	std::pair<uint32_t, uint32_t> const pm_max = _selector.max_size ();
	resize_window_to_proportion_of_monitor (this, pm_max.first, pm_max.second);
}

void
MonitorSelectorWindow::io_name_changed (void*)
{
	ENSURE_GUI_THREAD (*this, &MonitorSelectorWindow::io_name_changed, src)
		
	std::string title;

	if (!_selector.find_inputs_for_io_outputs()) {
		title = string_compose(_("%1 input"), _selector.io()->name());
	} else {
		title = string_compose(_("%1 output"), _selector.io()->name());
	}

	set_title (title);
}

