/*
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2014 Robin Gareus <robin@gareus.org>
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

#include <gtkmm2ext/doi.h>

#include "ardour/io.h"
#include "ardour/rc_configuration.h"
#include "ardour/return.h"

#include "return_ui.h"
#include "io_selector.h"
#include "gui_thread.h"
#include "timers.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

ReturnUI::ReturnUI (Gtk::Window* parent, boost::shared_ptr<Return> r, Session* session)
	:_return (r)
	, _gpm (session, 250)
{
	_gpm.set_controls (boost::shared_ptr<Route>(), r->meter(), r->amp(), r->gain_control());

	_hbox.pack_start (_gpm, true, true);
	set_name (X_("ReturnUIFrame"));

	_vbox.set_spacing (5);
	_vbox.set_border_width (5);

	_vbox.pack_start (_hbox, false, false, false);

	io = Gtk::manage (new IOSelector (parent, session, r->output()));

	pack_start (_vbox, false, false);

	pack_start (*io, true, true);

	show_all ();

	_return->set_metering (true);
	_return->input()->changed.connect (input_change_connection, invalidator (*this), boost::bind (&ReturnUI::ins_changed, this, _1, _2), gui_context());

	_gpm.setup_meters ();
	_gpm.set_fader_name (X_("ReturnUIFader"));

	// screen_update_connection = Timers::rapid_connect (sigc::mem_fun (*this, &ReturnUI::update));
	fast_screen_update_connection = Timers::super_rapid_connect (sigc::mem_fun (*this, &ReturnUI::fast_update));
}

ReturnUI::~ReturnUI ()
{
	_return->set_metering (false);

	/* XXX not clear that we need to do this */

	screen_update_connection.disconnect();
	fast_screen_update_connection.disconnect();
}

void
ReturnUI::ins_changed (IOChange change, void* /*ignored*/)
{
	ENSURE_GUI_THREAD (*this, &ReturnUI::ins_changed, change, ignored)
	if (change.type & IOChange::ConfigurationChanged) {
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

ReturnUIWindow::ReturnUIWindow (boost::shared_ptr<Return> r, ARDOUR::Session* s)
	: ArdourWindow (string(_("Return ")) + r->name())
{
	ui = new ReturnUI (this, r, s);

	hpacker.pack_start (*ui, true, true);

	add (hpacker);

	set_name ("ReturnUIWindow");

}

ReturnUIWindow::~ReturnUIWindow ()
{
	delete ui;
}
