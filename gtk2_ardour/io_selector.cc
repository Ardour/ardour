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

#include <gtkmm/messagedialog.h>
#include <glibmm/objectbase.h>

#include <gtkmm2ext/doi.h>

#include "ardour/port_insert.h"
#include "ardour/session.h"
#include "ardour/io.h"
#include "ardour/audioengine.h"
#include "ardour/track.h"
#include "ardour/audio_track.h"
#include "ardour/midi_track.h"
#include "ardour/data_type.h"
#include "ardour/port.h"
#include "ardour/bundle.h"

#include "io_selector.h"
#include "utils.h"
#include "gui_thread.h"
#include "i18n.h"

using namespace ARDOUR;
using namespace Gtk;

IOSelector::IOSelector (ARDOUR::Session& session, boost::shared_ptr<ARDOUR::IO> io)
	: PortMatrix (session, io->default_type())
	, _io (io)
{
	/* signal flow from 0 to 1 */

	_find_inputs_for_io_outputs = (_io->direction() == IO::Output);

	if (_find_inputs_for_io_outputs) {
		_other = 1;
		_ours = 0;
	} else {
		_other = 0;
		_ours = 1;
	}

	_port_group.reset (new PortGroup (""));
	_ports[_ours].add_group (_port_group);
	
	setup_all_ports ();
}

void
IOSelector::setup_ports (int dim)
{
	_ports[dim].suspend_signals ();

	if (dim == _other) {

		_ports[_other].gather (_session, _find_inputs_for_io_outputs);
		
	} else {

		_port_group->clear ();
		_port_group->add_bundle (_io->bundle (), _io);
	}

	_ports[dim].resume_signals ();
}

void
IOSelector::set_state (ARDOUR::BundleChannel c[2], bool s)
{
	ARDOUR::Bundle::PortList const & our_ports = c[_ours].bundle->channel_ports (c[_ours].channel);
	ARDOUR::Bundle::PortList const & other_ports = c[_other].bundle->channel_ports (c[_other].channel);

	for (ARDOUR::Bundle::PortList::const_iterator i = our_ports.begin(); i != our_ports.end(); ++i) {
		for (ARDOUR::Bundle::PortList::const_iterator j = other_ports.begin(); j != other_ports.end(); ++j) {

			Port* f = _session.engine().get_port_by_name (*i);
			if (!f) {
				return;
			}

			if (s) {
				_io->connect (f, *j, 0);
			} else {
				_io->disconnect (f, *j, 0);
			}
		}
	}
}

PortMatrixNode::State
IOSelector::get_state (ARDOUR::BundleChannel c[2]) const
{
	ARDOUR::Bundle::PortList const & our_ports = c[_ours].bundle->channel_ports (c[_ours].channel);
	ARDOUR::Bundle::PortList const & other_ports = c[_other].bundle->channel_ports (c[_other].channel);

	if (our_ports.empty() || other_ports.empty()) {
		/* we're looking at a bundle with no parts associated with this channel,
		   so nothing to connect */
		return PortMatrixNode::UNKNOWN;
	}

	for (ARDOUR::Bundle::PortList::const_iterator i = our_ports.begin(); i != our_ports.end(); ++i) {
		for (ARDOUR::Bundle::PortList::const_iterator j = other_ports.begin(); j != other_ports.end(); ++j) {

			Port* f = _session.engine().get_port_by_name (*i);

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
IOSelector::n_io_ports () const
{
	if (!_find_inputs_for_io_outputs) {
		return _io->n_ports().get (_io->default_type());
	} else {
		return _io->n_ports().get (_io->default_type());
	}
}

bool
IOSelector::list_is_global (int dim) const
{
	return (dim == _other);
}

IOSelectorWindow::IOSelectorWindow (ARDOUR::Session& session, boost::shared_ptr<ARDOUR::IO> io, bool /*can_cancel*/)
	: _selector (session, io)
{
	set_name ("IOSelectorWindow2");
	set_title (_("I/O selector"));

	add (_selector);

	set_position (Gtk::WIN_POS_MOUSE);

	io_name_changed (this);

	show_all ();

	signal_delete_event().connect (bind (sigc::ptr_fun (just_hide_it), this));

	resize (32768, 32768);

	_selector.MaxSizeChanged.connect (mem_fun (*this, &IOSelectorWindow::max_size_changed));
}

void
IOSelectorWindow::on_map ()
{
	_selector.setup_all_ports ();
	Window::on_map ();
}

void
IOSelectorWindow::io_name_changed (void* src)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &IOSelectorWindow::io_name_changed), src));
	
	string title;

	if (!_selector.find_inputs_for_io_outputs()) {
		title = string_compose(_("%1 input"), _selector.io()->name());
	} else {
		title = string_compose(_("%1 output"), _selector.io()->name());
	}

	set_title (title);
}

void
IOSelectorWindow::on_realize ()
{
	Window::on_realize ();
	set_max_size ();
}

void
IOSelectorWindow::set_max_size ()
{
	pair<uint32_t, uint32_t> const m = _selector.max_size ();

	GdkGeometry g;
	g.max_width = m.first;
	g.max_height = m.second;

	set_geometry_hints (*this, g, Gdk::HINT_MAX_SIZE);
}

void
IOSelectorWindow::max_size_changed ()
{
	set_max_size ();
	resize (32768, 32768);
}



PortInsertUI::PortInsertUI (ARDOUR::Session& sess, boost::shared_ptr<ARDOUR::PortInsert> pi)
	: input_selector (sess, pi->input())
	, output_selector (sess, pi->output())
{
	output_selector.set_min_height_divisor (2);
	input_selector.set_min_height_divisor (2);
	
	pack_start (output_selector, true, true);
	pack_start (input_selector, true, true);
}

void
PortInsertUI::redisplay ()
{
	input_selector.setup_ports (input_selector.other());
	output_selector.setup_ports (output_selector.other());
}

void
PortInsertUI::finished (IOSelector::Result r)
{
	input_selector.Finished (r);
	output_selector.Finished (r);
}


PortInsertWindow::PortInsertWindow (ARDOUR::Session& sess, boost::shared_ptr<ARDOUR::PortInsert> pi, bool can_cancel)
	: ArdourDialog ("port insert dialog"),
	  _portinsertui (sess, pi),
	  ok_button (can_cancel ? _("OK"): _("Close")),
	  cancel_button (_("Cancel"))
{

	set_name ("IOSelectorWindow");
	string title = _("ardour: ");
	title += pi->name();
	set_title (title);
	
	ok_button.set_name ("IOSelectorButton");
	if (!can_cancel) {
		ok_button.set_image (*Gtk::manage (new Gtk::Image (Gtk::Stock::CLOSE, Gtk::ICON_SIZE_BUTTON)));
	}
	cancel_button.set_name ("IOSelectorButton");

	if (can_cancel) {
		cancel_button.set_image (*Gtk::manage (new Gtk::Image (Gtk::Stock::CANCEL, Gtk::ICON_SIZE_BUTTON)));
		get_action_area()->pack_start (cancel_button, false, false);
	} else {
		cancel_button.hide();
	}
	get_action_area()->pack_start (ok_button, false, false);

	get_vbox()->pack_start (_portinsertui);

	ok_button.signal_clicked().connect (mem_fun (*this, &PortInsertWindow::accept));
	cancel_button.signal_clicked().connect (mem_fun (*this, &PortInsertWindow::cancel));

	signal_delete_event().connect (bind (sigc::ptr_fun (just_hide_it), reinterpret_cast<Window *> (this)));	

	going_away_connection = pi->GoingAway.connect (mem_fun (*this, &PortInsertWindow::plugin_going_away));
}

void
PortInsertWindow::plugin_going_away ()
{
	ENSURE_GUI_THREAD (mem_fun (*this, &PortInsertWindow::plugin_going_away));
	
	going_away_connection.disconnect ();
	delete_when_idle (this);
}

void
PortInsertWindow::on_map ()
{
	_portinsertui.redisplay ();
	Window::on_map ();
}


void
PortInsertWindow::cancel ()
{
	_portinsertui.finished (IOSelector::Cancelled);
	hide ();
}

void
PortInsertWindow::accept ()
{
	_portinsertui.finished (IOSelector::Accepted);
	hide ();
}

