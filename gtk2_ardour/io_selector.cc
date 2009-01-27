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

#include <ardour/port_insert.h>
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

IOSelector::IOSelector (ARDOUR::Session& session, boost::shared_ptr<ARDOUR::IO> io, bool offer_inputs)
	: PortMatrix (session, io->default_type(), offer_inputs)
	, _session (session)
	, _io (io)
{
	/* Listen for ports changing on the IO */
	_io->PortCountChanged.connect (sigc::hide (mem_fun (*this, &IOSelector::ports_changed)));

	_port_group = new PortGroup ("", true);
	_row_ports.push_back (_port_group);
	
	setup ();
}

IOSelector::~IOSelector ()
{
	delete _port_group;
}

void
IOSelector::setup ()
{
	_port_group->clear ();
	_port_group->add_bundle (boost::shared_ptr<ARDOUR::Bundle> (new ARDOUR::Bundle));
	_port_group->only_bundle()->set_name (_io->name());

	if (offering_input ()) {
		const PortSet& ps (_io->outputs());

		int j = 0;
		for (PortSet::const_iterator i = ps.begin(); i != ps.end(); ++i) {
			char buf[32];
			snprintf (buf, sizeof(buf), _("out %d"), j + 1);
			_port_group->only_bundle()->add_channel (buf);
			_port_group->only_bundle()->add_port_to_channel (j, _session.engine().make_port_name_non_relative (i->name()));
			++j;
		}
		
	} else {
		
		const PortSet& ps (_io->inputs());

		int j = 0;
		for (PortSet::const_iterator i = ps.begin(); i != ps.end(); ++i) {
			char buf[32];
			snprintf (buf, sizeof(buf), _("in %d"), j + 1);
			_port_group->only_bundle()->add_channel (buf);
			_port_group->only_bundle()->add_port_to_channel (j, _session.engine().make_port_name_non_relative (i->name()));
			++j;
		}

	}

	PortMatrix::setup ();
}

void
IOSelector::ports_changed ()
{
	ENSURE_GUI_THREAD (mem_fun (*this, &IOSelector::ports_changed));

	setup ();
}

void
IOSelector::set_state (
	boost::shared_ptr<ARDOUR::Bundle> ab,
	uint32_t ac,
	boost::shared_ptr<ARDOUR::Bundle> bb,
	uint32_t bc,
	bool s,
	uint32_t k
	)
{
	ARDOUR::Bundle::PortList const& our_ports = ab->channel_ports (ac);
	ARDOUR::Bundle::PortList const& other_ports = bb->channel_ports (bc);

	for (ARDOUR::Bundle::PortList::const_iterator i = our_ports.begin(); i != our_ports.end(); ++i) {
		for (ARDOUR::Bundle::PortList::const_iterator j = other_ports.begin(); j != other_ports.end(); ++j) {

			Port* f = _session.engine().get_port_by_name (*i);
			if (!f) {
				return;
			}

			if (s) {
				if (!offering_input()) {
					_io->connect_input (f, *j, 0);
				} else {
					_io->connect_output (f, *j, 0);
				}
			} else {
				if (!offering_input()) {
					_io->disconnect_input (f, *j, 0);
				} else {
					_io->disconnect_output (f, *j, 0);
				}
			}
		}
	}
}

PortMatrix::State
IOSelector::get_state (
	boost::shared_ptr<ARDOUR::Bundle> ab,
	uint32_t ac,
	boost::shared_ptr<ARDOUR::Bundle> bb,
	uint32_t bc
	) const
{
	ARDOUR::Bundle::PortList const& our_ports = ab->channel_ports (ac);
	ARDOUR::Bundle::PortList const& other_ports = bb->channel_ports (bc);

	for (ARDOUR::Bundle::PortList::const_iterator i = our_ports.begin(); i != our_ports.end(); ++i) {
		for (ARDOUR::Bundle::PortList::const_iterator j = other_ports.begin(); j != other_ports.end(); ++j) {

			Port* f = _session.engine().get_port_by_name (*i);

			/* since we are talking about an IO, our ports should all have an associated Port *,
			   so the above call should never fail */
			assert (f);
			
			if (!f->connected_to (*j)) {
				/* if any one thing is not connected, all bets are off */
				return NOT_ASSOCIATED;
			}
		}
	}

	return ASSOCIATED;
}

uint32_t
IOSelector::n_rows () const
{
	if (!offering_input()) {
		return _io->inputs().num_ports (_io->default_type());
	} else {
		return _io->outputs().num_ports (_io->default_type());
	}
}

uint32_t
IOSelector::maximum_rows () const
{
	if (!offering_input()) {
		return _io->input_maximum ().get (_io->default_type());
	} else {
		return _io->output_maximum ().get (_io->default_type());
	}
}


uint32_t
IOSelector::minimum_rows () const
{
	if (!offering_input()) {
		return _io->input_minimum ().get (_io->default_type());
	} else {
		return _io->output_minimum ().get (_io->default_type());
	}
}

void
IOSelector::add_channel (boost::shared_ptr<ARDOUR::Bundle> b)
{
	/* we ignore the bundle parameter, as we know what it is that we're adding to */
	
	// The IO selector only works for single typed IOs
	const ARDOUR::DataType t = _io->default_type ();

	if (!offering_input()) {

		try {
			_io->add_input_port ("", this);
		}

		catch (AudioEngine::PortRegistrationFailure& err) {
			MessageDialog msg (_("There are no more JACK ports available."));
			msg.run ();
		}

	} else {

		try {
			_io->add_output_port ("", this);
		}

		catch (AudioEngine::PortRegistrationFailure& err) {
			MessageDialog msg (_("There are no more JACK ports available."));
			msg.run ();
		}
	}
}

void
IOSelector::remove_channel (boost::shared_ptr<ARDOUR::Bundle> b, uint32_t c)
{
	Port* f = _session.engine().get_port_by_name (b->channel_ports(c)[0]);
	if (!f) {
		return;
	}
	
	if (offering_input()) {
		_io->remove_output_port (f, this);
	} else {
		_io->remove_input_port (f, this);
	}
}

IOSelectorWindow::IOSelectorWindow (ARDOUR::Session& session, boost::shared_ptr<ARDOUR::IO> io, bool for_input, bool can_cancel)
	: ArdourDialog ("I/O selector")
	, _selector (session, io, !for_input)
	, add_button (_("Add Port"))
	, disconnect_button (_("Disconnect All"))
	, ok_button (can_cancel ? _("OK"): _("Close"))
	, cancel_button (_("Cancel"))
	, rescan_button (_("Rescan"))

{
	/* XXX: what's this for? */
	add_events (Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK);
	
	set_name ("IOSelectorWindow2");

	/* Disconnect All button */
	disconnect_button.set_name ("IOSelectorButton");
	disconnect_button.set_image (*Gtk::manage (new Gtk::Image (Gtk::Stock::DISCONNECT, Gtk::ICON_SIZE_BUTTON)));
	disconnect_button.signal_clicked().connect (sigc::mem_fun (_selector, &IOSelector::disassociate_all));
	get_action_area()->pack_start (disconnect_button, false, false);

	/* Add Port button */
	if (_selector.maximum_rows() > _selector.n_rows()) {
		add_button.set_name ("IOSelectorButton");
		add_button.set_image (*Gtk::manage (new Gtk::Image (Gtk::Stock::ADD, Gtk::ICON_SIZE_BUTTON)));
		get_action_area()->pack_start (add_button, false, false);
		add_button.signal_clicked().connect (sigc::bind (sigc::mem_fun (_selector, &IOSelector::add_channel), boost::shared_ptr<Bundle> ()));
	} 

	/* Rescan button */
 	rescan_button.set_name ("IOSelectorButton");
	rescan_button.set_image (*Gtk::manage (new Gtk::Image (Gtk::Stock::REFRESH, Gtk::ICON_SIZE_BUTTON)));
	rescan_button.signal_clicked().connect (sigc::mem_fun (_selector, &IOSelector::setup));
	get_action_area()->pack_start (rescan_button, false, false);

	io->PortCountChanged.connect (sigc::hide (mem_fun (*this, &IOSelectorWindow::ports_changed)));

	/* Cancel button */
	if (can_cancel) {
		cancel_button.set_name ("IOSelectorButton");
		cancel_button.set_image (*Gtk::manage (new Gtk::Image (Gtk::Stock::CANCEL, Gtk::ICON_SIZE_BUTTON)));
		get_action_area()->pack_start (cancel_button, false, false);
	} else {
		cancel_button.hide();
	}
	cancel_button.signal_clicked().connect (mem_fun(*this, &IOSelectorWindow::cancel));

	/* OK button */
	ok_button.set_name ("IOSelectorButton");
	if (!can_cancel) {
		ok_button.set_image (*Gtk::manage (new Gtk::Image (Gtk::Stock::CLOSE, Gtk::ICON_SIZE_BUTTON)));
	}
	ok_button.signal_clicked().connect (mem_fun(*this, &IOSelectorWindow::accept));
	get_action_area()->pack_start (ok_button, false, false);

	get_vbox()->set_spacing (8);

	get_vbox()->pack_start (_selector, true, true);

	set_position (Gtk::WIN_POS_MOUSE);

	io_name_changed (this);
	ports_changed ();

	show_all ();

	signal_delete_event().connect (bind (sigc::ptr_fun (just_hide_it), this));
}

void
IOSelectorWindow::ports_changed ()
{
	if (_selector.maximum_rows() > _selector.n_rows()) {
		add_button.set_sensitive (true);
	} else {
		add_button.set_sensitive (false);
	}
}

void
IOSelectorWindow::cancel ()
{
	_selector.Finished (IOSelector::Cancelled);
	hide ();
}

void
IOSelectorWindow::accept ()
{
	_selector.Finished (IOSelector::Accepted);
	hide ();
}

void
IOSelectorWindow::on_map ()
{
	_selector.setup ();
	Window::on_map ();
}

void
IOSelectorWindow::io_name_changed (void* src)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &IOSelectorWindow::io_name_changed), src));
	
	string title;

	if (!_selector.offering_input()) {
		title = string_compose(_("%1 input"), _selector.io()->name());
	} else {
		title = string_compose(_("%1 output"), _selector.io()->name());
	}

	set_title (title);
}

PortInsertUI::PortInsertUI (ARDOUR::Session& sess, boost::shared_ptr<ARDOUR::PortInsert> pi)
	: input_selector (sess, pi->io(), true),
	  output_selector (sess, pi->io(), false)
{
	pack_start (output_selector, true, true);
	pack_start (input_selector, true, true);
}

void
PortInsertUI::redisplay ()
{
	input_selector.setup ();
	output_selector.setup ();
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
	  cancel_button (_("Cancel")),
	  rescan_button (_("Rescan"))
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
	rescan_button.set_name ("IOSelectorButton");
	rescan_button.set_image (*Gtk::manage (new Gtk::Image (Gtk::Stock::REFRESH, Gtk::ICON_SIZE_BUTTON)));

	get_action_area()->pack_start (rescan_button, false, false);
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
