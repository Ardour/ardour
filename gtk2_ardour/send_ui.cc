/*
    Copyright (C) 2002 Paul Davis 

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

    $Id$
*/

#include <ardour/send.h>
#include <gtkmm2ext/doi.h>

#include "utils.h"
#include "send_ui.h"
#include "io_selector.h"
#include "ardour_ui.h"
#include "gui_thread.h"

using namespace ARDOUR;
using namespace PBD;

SendUI::SendUI (boost::shared_ptr<Send> s, Session& se)
	: _send (s),
	  _session (se),
	  gpm (s, se),
	  panners (s, se)
{
	hbox.pack_start (gpm, true, true);
	set_name ("SendUIFrame");
	
	vbox.set_spacing (5);
	vbox.set_border_width (5);

	vbox.pack_start (hbox, false, false, false);
	vbox.pack_start (panners, false,false);

	io = new IOSelector (se, s, false);
	
	pack_start (vbox, false, false);

	pack_start (*io, true, true);

	show_all ();

	_send->set_metering (true);

	_send->output_changed.connect (mem_fun (*this, &SendUI::ins_changed));
	_send->output_changed.connect (mem_fun (*this, &SendUI::outs_changed));
	
	panners.set_width (Wide);
	panners.setup_pan ();

	gpm.setup_meters ();
	gpm.set_fader_name ("SendUIFrame");

	// screen_update_connection = ARDOUR_UI::instance()->RapidScreenUpdate.connect (mem_fun (*this, &SendUI::update));
	fast_screen_update_connection = ARDOUR_UI::instance()->SuperRapidScreenUpdate.connect (mem_fun (*this, &SendUI::fast_update));
}

SendUI::~SendUI ()
{
	_send->set_metering (false);
	
	/* XXX not clear that we need to do this */

	screen_update_connection.disconnect();
	fast_screen_update_connection.disconnect();
}

void
SendUI::ins_changed (IOChange change, void* ignored)
{
	ENSURE_GUI_THREAD(bind (mem_fun (*this, &SendUI::ins_changed), change, ignored));
	if (change & ConfigurationChanged) {
		panners.setup_pan ();
	}
}

void
SendUI::outs_changed (IOChange change, void* ignored)
{
	ENSURE_GUI_THREAD(bind (mem_fun (*this, &SendUI::outs_changed), change, ignored));
	if (change & ConfigurationChanged) {
		panners.setup_pan ();
		gpm.setup_meters ();
	}
}

void
SendUI::send_going_away (Redirect *ignored)
{
	ENSURE_GUI_THREAD (bind (mem_fun (*this, &SendUI::send_going_away), ignored));

	delete this;
}

void
SendUI::update ()
{
}

void
SendUI::fast_update ()
{
	if (_session.meter_falloff() > 0.0f) {
		gpm.update_meters ();
	}
}
	
SendUIWindow::SendUIWindow (boost::shared_ptr<Send> s, Session& ss)
{
	ui = new SendUI (s, ss);

	vpacker.set_border_width (5);

	hpacker.pack_start (*ui, true, true);

	vpacker.pack_start (hpacker);

	add (vpacker);
	set_name ("SendUIWindow");

	s->GoingAway.connect (mem_fun (*this, &SendUIWindow::send_going_away));

	signal_delete_event().connect (bind (ptr_fun (just_hide_it), reinterpret_cast<Window *> (this)));

}

SendUIWindow::~SendUIWindow ()
{
	delete ui;
}

void
SendUIWindow::send_going_away (Redirect *ignored)
{
	ENSURE_GUI_THREAD(bind (mem_fun (*this, &SendUIWindow::send_going_away), ignored));
	
	delete this;
}

