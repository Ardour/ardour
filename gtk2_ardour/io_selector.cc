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

#include "io_selector.h"
#include "utils.h"
#include "gui_thread.h"
#include "i18n.h"

using namespace ARDOUR;
using namespace Gtk;

IOSelector::IOSelector (ARDOUR::Session& session, boost::shared_ptr<ARDOUR::IO> io, bool offer_inputs)
	: PortMatrix (session, io->default_type(), offer_inputs,
		      PortGroupList::Mask (PortGroupList::BUSS | 
					   PortGroupList::SYSTEM | 
					   PortGroupList::OTHER))
	, _io (io)
{
	list<string> our_ports;

	/* Listen for ports changing on the IO */
	if (_offer_inputs) {
		_io->output_changed.connect (mem_fun(*this, &IOSelector::ports_changed));

		const PortSet& ps (_io->outputs());

		for (PortSet::const_iterator i = ps.begin(); i != ps.end(); ++i) {
			our_ports.push_back (i->name());
		}

	} else {
		_io->input_changed.connect (mem_fun(*this, &IOSelector::ports_changed));

		const PortSet& ps (_io->inputs());

		for (PortSet::const_iterator i = ps.begin(); i != ps.end(); ++i) {
			our_ports.push_back (i->name());
		}

	}
	
	set_ports (our_ports);

	setup ();
}

void
IOSelector::ports_changed (ARDOUR::IOChange change, void *src)
{
	ENSURE_GUI_THREAD (bind (mem_fun (*this, &IOSelector::ports_changed), change, src));

	setup ();
}

void
IOSelector::set_state (int r, std::string const & p, bool s, uint32_t keymod)
{
	if (s) {
		if (!_offer_inputs) {
			_io->connect_input (_io->input(r), p, 0);
		} else {
			_io->connect_output (_io->output(r), p, 0);
		}
	} else {
		if (!_offer_inputs) {
			_io->disconnect_input (_io->input(r), p, 0);
		} else {
			_io->disconnect_output (_io->output(r), p, 0);
		}
	}
}

bool
IOSelector::get_state (int r, std::string const & p) const
{
	vector<string> connections;

	if (_offer_inputs) {
		_io->output(r)->get_connections (connections);
	} else {
		_io->input(r)->get_connections (connections);
	}

	return (std::find (connections.begin (), connections.end (), p) != connections.end ());
}

uint32_t
IOSelector::n_rows () const
{
	if (!_offer_inputs) {
		return _io->inputs().num_ports (_io->default_type());
	} else {
		return _io->outputs().num_ports (_io->default_type());
	}
}

uint32_t
IOSelector::maximum_rows () const
{
	if (!_offer_inputs) {
		return _io->input_maximum ().get (_io->default_type());
	} else {
		return _io->output_maximum ().get (_io->default_type());
	}
}


uint32_t
IOSelector::minimum_rows () const
{
	if (!_offer_inputs) {
		return _io->input_minimum ().get (_io->default_type());
	} else {
		return _io->output_minimum ().get (_io->default_type());
	}
}

std::string
IOSelector::row_name (int r) const
{
	string n;
	string::size_type pos;

	if (!_offer_inputs) {
		n = _io->input(r)->name();
	} else {
		n = _io->output(r)->name();
	}
	
	if ((pos = n.find ('/')) != string::npos) {
		return n.substr (pos+1);
	} else {
		return n;
	}
}

void
IOSelector::add_row ()
{
	// The IO selector only works for single typed IOs
	const ARDOUR::DataType t = _io->default_type ();

	if (!_offer_inputs) {

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
IOSelector::remove_row (int r)
{
	// The IO selector only works for single typed IOs
	const ARDOUR::DataType t = _io->default_type ();
	
	if (!_offer_inputs) {
		_io->remove_input_port (_io->input (r), this);
	} else {
		_io->remove_output_port (_io->output (r), this);
	}
}

std::string
IOSelector::row_descriptor () const
{
	return _("port");
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
	add_events (Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK);
	set_name ("IOSelectorWindow2");

	disconnect_button.set_name ("IOSelectorButton");
	disconnect_button.set_image (*Gtk::manage (new Gtk::Image (Gtk::Stock::DISCONNECT, Gtk::ICON_SIZE_BUTTON)));
	get_action_area()->pack_start (disconnect_button, false, false);

	if (_selector.maximum_rows() > _selector.n_rows()) {
		add_button.set_name ("IOSelectorButton");
		add_button.set_image (*Gtk::manage (new Gtk::Image (Gtk::Stock::ADD, Gtk::ICON_SIZE_BUTTON)));
		get_action_area()->pack_start (add_button, false, false);
		add_button.signal_clicked().connect (sigc::mem_fun (_selector, &IOSelector::add_row));
	} 

	if (!for_input) {
		io->output_changed.connect (mem_fun(*this, &IOSelectorWindow::ports_changed));
	} else {
		io->input_changed.connect (mem_fun(*this, &IOSelectorWindow::ports_changed));
	}

	rescan_button.set_name ("IOSelectorButton");
	rescan_button.set_image (*Gtk::manage (new Gtk::Image (Gtk::Stock::REFRESH, Gtk::ICON_SIZE_BUTTON)));
	get_action_area()->pack_start (rescan_button, false, false);

	if (can_cancel) {
		cancel_button.set_name ("IOSelectorButton");
		cancel_button.set_image (*Gtk::manage (new Gtk::Image (Gtk::Stock::CANCEL, Gtk::ICON_SIZE_BUTTON)));
		get_action_area()->pack_start (cancel_button, false, false);
	} else {
		cancel_button.hide();
	}
		
	ok_button.set_name ("IOSelectorButton");
	if (!can_cancel) {
		ok_button.set_image (*Gtk::manage (new Gtk::Image (Gtk::Stock::CLOSE, Gtk::ICON_SIZE_BUTTON)));
	}
	get_action_area()->pack_start (ok_button, false, false);

	get_vbox()->set_spacing (8);
	get_vbox()->pack_start (_selector, true, true);

	suggestion.set_alignment (0.5, 0.5);
	suggestion_box.pack_start (suggestion, true, true);
	get_vbox()->pack_start (suggestion_box, false, false);

	ok_button.signal_clicked().connect (mem_fun(*this, &IOSelectorWindow::accept));
	cancel_button.signal_clicked().connect (mem_fun(*this, &IOSelectorWindow::cancel));
	rescan_button.signal_clicked().connect (mem_fun(*this, &IOSelectorWindow::rescan));

	set_position (Gtk::WIN_POS_MOUSE);

	io_name_changed (this);
	ports_changed (IOChange (0), this);
	leave_scroller ((GdkEventCrossing*) 0);

	show_all ();

	signal_delete_event().connect (bind (sigc::ptr_fun (just_hide_it), this));

	_selector.scrolled_window().add_events (Gdk::ENTER_NOTIFY_MASK|Gdk::LEAVE_NOTIFY_MASK);
	_selector.scrolled_window().signal_enter_notify_event().connect (mem_fun (*this, &IOSelectorWindow::enter_scroller), false);
	_selector.scrolled_window().signal_leave_notify_event().connect (mem_fun (*this, &IOSelectorWindow::leave_scroller), false);
}

IOSelectorWindow::~IOSelectorWindow()
{
	
}

bool
IOSelectorWindow::enter_scroller (GdkEventCrossing* ignored)
{
	cerr << "IN\n";
	suggestion.set_text (_("Click to connect. Ctrl-click to disconnect. Shift-click for cross-connect"));
	return false;
}

bool
IOSelectorWindow::leave_scroller (GdkEventCrossing* ignored)
{
	cerr << "OUT, ev = " << ignored << "\n";
	suggestion.set_text (_("Right-click on individual port names for per-port operations"));
	return false;
}

void
IOSelectorWindow::ports_changed (ARDOUR::IOChange change, void *src)
{
	if (_selector.maximum_rows() > _selector.n_rows()) {
		add_button.set_sensitive (true);
	} else {
		add_button.set_sensitive (false);
	}
}

void
IOSelectorWindow::rescan ()
{
	_selector.setup ();
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
	hbox.pack_start (output_selector, true, true);
	hbox.pack_start (input_selector, true, true);

	pack_start (hbox);
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
	rescan_button.signal_clicked().connect (mem_fun (*this, &PortInsertWindow::rescan));

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
PortInsertWindow::rescan ()
{
	_portinsertui.redisplay ();
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
