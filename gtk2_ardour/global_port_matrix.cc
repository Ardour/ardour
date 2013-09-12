/*
    Copyright (C) 2009 Paul Davis

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

#include <gtkmm/image.h>
#include <gtkmm/stock.h>

#include "global_port_matrix.h"
#include "utils.h"

#include "ardour/bundle.h"
#include "ardour/session.h"
#include "ardour/audioengine.h"
#include "ardour/port.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;

GlobalPortMatrix::GlobalPortMatrix (Gtk::Window* p, Session* s, DataType t)
	: PortMatrix (p, s, t)
{
	setup_all_ports ();
	init ();
}

void
GlobalPortMatrix::setup_ports (int dim)
{
	if (!_session) {
		return;
	}

	_ports[dim].suspend_signals ();
	_ports[dim].gather (_session, type(), dim == FLOW_IN, false, show_only_bundles ());
	_ports[dim].resume_signals ();
}

void
GlobalPortMatrix::set_state (BundleChannel c[2], bool s)
{
	if (!_session) {
		return;
	}

	Bundle::PortList const & in_ports = c[FLOW_IN].bundle->channel_ports (c[FLOW_IN].channel);
	Bundle::PortList const & out_ports = c[FLOW_OUT].bundle->channel_ports (c[FLOW_OUT].channel);

	for (Bundle::PortList::const_iterator i = in_ports.begin(); i != in_ports.end(); ++i) {
		for (Bundle::PortList::const_iterator j = out_ports.begin(); j != out_ports.end(); ++j) {

			boost::shared_ptr<Port> p = _session->engine().get_port_by_name (*i);
			boost::shared_ptr<Port> q = _session->engine().get_port_by_name (*j);

			if (p) {
				if (s) {
					p->connect (*j);
				} else {
					p->disconnect (*j);
				}
			} else if (q) {
				if (s) {
					q->connect (*i);
				} else {
					q->disconnect (*i);
				}
			} else {
				/* two non-Ardour ports */
				if (s) {
					AudioEngine::instance()->connect (*j, *i);
				} else {
					AudioEngine::instance()->disconnect (*j, *i);
				}
			}
		}
	}
}

PortMatrixNode::State
GlobalPortMatrix::get_state (BundleChannel c[2]) const
{
	if (_session == 0) {
		return PortMatrixNode::NOT_ASSOCIATED;
	}

	if (c[0].bundle->nchannels() == ChanCount::ZERO || c[1].bundle->nchannels() == ChanCount::ZERO) {
		return PortMatrixNode::NOT_ASSOCIATED;
	}

	Bundle::PortList const & in_ports = c[FLOW_IN].bundle->channel_ports (c[FLOW_IN].channel);
	Bundle::PortList const & out_ports = c[FLOW_OUT].bundle->channel_ports (c[FLOW_OUT].channel);
	if (in_ports.empty() || out_ports.empty()) {
		/* we're looking at a bundle with no parts associated with this channel,
		   so nothing to connect */
		return PortMatrixNode::NOT_ASSOCIATED;
	}

	for (Bundle::PortList::const_iterator i = in_ports.begin(); i != in_ports.end(); ++i) {
		for (Bundle::PortList::const_iterator j = out_ports.begin(); j != out_ports.end(); ++j) {

			boost::shared_ptr<Port> p = AudioEngine::instance()->get_port_by_name (*i);
			boost::shared_ptr<Port> q = AudioEngine::instance()->get_port_by_name (*j);

			if (!p && !q) {
				/* two non-Ardour ports; things are slightly more involved */

				/* get a port handle for one of them .. */

				PortEngine::PortHandle ph = AudioEngine::instance()->port_engine().get_port_by_name (*i);
				if (!ph) {
					return PortMatrixNode::NOT_ASSOCIATED;
				}

				/* see if it is connected to the other one ... */

				if (AudioEngine::instance()->port_engine().connected_to (ph, *j, false)) {
					return PortMatrixNode::ASSOCIATED;
				}

				return PortMatrixNode::NOT_ASSOCIATED;
			}

			if (p && p->connected_to (*j) == false) {
				return PortMatrixNode::NOT_ASSOCIATED;
			} else if (q && q->connected_to (*i) == false) {
				return PortMatrixNode::NOT_ASSOCIATED;
			}

		}
	}

	return PortMatrixNode::ASSOCIATED;
}

GlobalPortMatrixWindow::GlobalPortMatrixWindow (Session* s, DataType t)
	: ArdourWindow (X_("reset me soon"))
	, _port_matrix (this, s, t)
{
	switch (t) {
	case DataType::AUDIO:
		set_title (_("Audio Connection Manager"));
		break;
	case DataType::MIDI:
		set_title (_("MIDI Connection Manager"));
		break;
	}

	signal_key_press_event().connect (sigc::mem_fun (_port_matrix, &PortMatrix::key_press));

	add (_port_matrix);
	_port_matrix.show ();
}

void
GlobalPortMatrixWindow::on_show ()
{
	Gtk::Window::on_show ();
	pair<uint32_t, uint32_t> const pm_max = _port_matrix.max_size ();
	resize_window_to_proportion_of_monitor (this, pm_max.first, pm_max.second);
}

void
GlobalPortMatrixWindow::set_session (Session* s)
{
	_port_matrix.set_session (s);

	if (!s) {
		hide ();
	}
}

void
GlobalPortMatrix::set_session (Session *s)
{
	SessionHandlePtr::set_session (s);
	if (!s) return;
	setup_all_ports ();
	init();
}

string
GlobalPortMatrix::disassociation_verb () const
{
	return _("Disconnect");
}

string
GlobalPortMatrix::channel_noun () const
{
	return _("port");
}

