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

*/

#include <gtkmm2ext/doi.h>

#include "ardour/amp.h"
#include "ardour/io.h"
#include "ardour/return.h"

#include "utils.h"
#include "return_ui.h"
#include "io_selector.h"
#include "ardour_ui.h"
#include "gui_thread.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

ReturnUI::ReturnUI (Gtk::Window* parent, boost::shared_ptr<Return> r, Session& se)
	: _return (r)
	, _session (se)
	, _gpm (se)
{
 	_gpm.set_controls (boost::shared_ptr<Route>(), r->meter(), r->amp());

	_hbox.pack_start (_gpm, true, true);
	set_name ("ReturnUIFrame");

	_vbox.set_spacing (5);
	_vbox.set_border_width (5);

	_vbox.pack_start (_hbox, false, false, false);

	io = manage (new IOSelector (parent, se, r->output()));

	pack_start (_vbox, false, false);

	pack_start (*io, true, true);

	show_all ();

	_return->set_metering (true);
	_return->input()->changed.connect (mem_fun (*this, &ReturnUI::ins_changed));

	_gpm.setup_meters ();
	_gpm.set_fader_name ("ReturnUIFrame");

	// screen_update_connection = ARDOUR_UI::instance()->RapidScreenUpdate.connect (mem_fun (*this, &ReturnUI::update));
	fast_screen_update_connection = ARDOUR_UI::instance()->SuperRapidScreenUpdate.connect (mem_fun (*this, &ReturnUI::fast_update));
}

ReturnUI::~ReturnUI ()
{
	_return->set_metering (false);

	/* XXX not clear that we need to do this */

	screen_update_connection.disconnect();
	fast_screen_update_connection.disconnect();
}

void
ReturnUI::ins_changed (IOChange change, void* ignored)
{
	ENSURE_GUI_THREAD(bind (mem_fun (*this, &ReturnUI::ins_changed), change, ignored));
	if (change & ConfigurationChanged) {
		_gpm.setup_meters ();
	}
}

void
ReturnUI::update ()
{
}

void
ReturnUI::fast_update ()
{
	if (Config->get_meter_falloff() > 0.0f) {
		_gpm.update_meters ();
	}
}

ReturnUIWindow::ReturnUIWindow (boost::shared_ptr<Return> s, Session& ss)
	: ArdourDialog (string("Ardour: return ") + s->name())
{
	ui = new ReturnUI (this, s, ss);

	hpacker.pack_start (*ui, true, true);

	get_vbox()->set_border_width (5);
	get_vbox()->pack_start (hpacker);

	set_name ("ReturnUIWindow");

	going_away_connection = s->GoingAway.connect (mem_fun (*this, &ReturnUIWindow::return_going_away));
	signal_delete_event().connect (bind (sigc::ptr_fun (just_hide_it), reinterpret_cast<Window *> (this)));
}

ReturnUIWindow::~ReturnUIWindow ()
{
	delete ui;
}

void
ReturnUIWindow::return_going_away ()
{
	ENSURE_GUI_THREAD (mem_fun (*this, &ReturnUIWindow::return_going_away));
	delete_when_idle (this);
	going_away_connection.disconnect ();
}

