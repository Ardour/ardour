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

#include "global_port_matrix.h"
#include "i18n.h"
#include "ardour/bundle.h"
#include "ardour/session.h"
#include "ardour/audioengine.h"
#include "ardour/port.h"

GlobalPortMatrix::GlobalPortMatrix (ARDOUR::Session& s, ARDOUR::DataType t)
	: PortMatrix (s, t)
{
	setup ();
}

void
GlobalPortMatrix::setup ()
{
	_ports[IN].gather (_session, true);
	_ports[OUT].gather (_session, false);
	
	PortMatrix::setup ();
}

void
GlobalPortMatrix::set_state (ARDOUR::BundleChannel c[2], bool s)
{
	ARDOUR::Bundle::PortList const & in_ports = c[IN].bundle->channel_ports (c[IN].channel);
	ARDOUR::Bundle::PortList const & out_ports = c[OUT].bundle->channel_ports (c[OUT].channel);

	for (ARDOUR::Bundle::PortList::const_iterator i = in_ports.begin(); i != in_ports.end(); ++i) {
		for (ARDOUR::Bundle::PortList::const_iterator j = out_ports.begin(); j != out_ports.end(); ++j) {

			ARDOUR::Port* p = _session.engine().get_port_by_name (*i);
			ARDOUR::Port* q = _session.engine().get_port_by_name (*j);

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
			}

			/* we don't handle connections between two non-Ardour ports */
		}
	}
}


PortMatrix::State
GlobalPortMatrix::get_state (ARDOUR::BundleChannel c[2]) const
{
	ARDOUR::Bundle::PortList const & in_ports = c[IN].bundle->channel_ports (c[IN].channel);
	ARDOUR::Bundle::PortList const & out_ports = c[OUT].bundle->channel_ports (c[OUT].channel);

	for (ARDOUR::Bundle::PortList::const_iterator i = in_ports.begin(); i != in_ports.end(); ++i) {
		for (ARDOUR::Bundle::PortList::const_iterator j = out_ports.begin(); j != out_ports.end(); ++j) {

			ARDOUR::Port* p = _session.engine().get_port_by_name (*i);
			ARDOUR::Port* q = _session.engine().get_port_by_name (*j);

			/* we don't know the state of connections between two non-Ardour ports */
			if (!p && !q) {
				return UNKNOWN;
			}

			if (p && p->connected_to (*j) == false) {
				return NOT_ASSOCIATED;
			} else if (q && q->connected_to (*i) == false) {
				return NOT_ASSOCIATED;
			}

		}
	}

	return ASSOCIATED;
}


GlobalPortMatrixWindow::GlobalPortMatrixWindow (ARDOUR::Session& s, ARDOUR::DataType t)
	: _port_matrix (s, t)
{
	switch (t) {
	case ARDOUR::DataType::AUDIO:
		set_title (_("Audio Connections Manager"));
		break;
	case ARDOUR::DataType::MIDI:
		set_title (_("MIDI Connections Manager"));
		break;
	}
	
	Gtk::VBox* vbox = Gtk::manage (new Gtk::VBox);
	vbox->pack_start (_port_matrix);
	add (*vbox);
	show_all ();
}
