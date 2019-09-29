/*
 * Copyright (C) 2005-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2007-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
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
#include "ardour/panner_manager.h"
#include "ardour/rc_configuration.h"
#include "ardour/send.h"

#include "gui_thread.h"
#include "io_selector.h"
#include "send_ui.h"
#include "timers.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

SendUI::SendUI (Gtk::Window* parent, boost::shared_ptr<Send> s, Session* session)
	: _send (s)
	, _gpm (session, 250)
	, _panners (session)
{
	assert (_send);

	uint32_t const in  = _send->pans_required ();
	uint32_t const out = _send->pan_outs ();
	_panners.set_width (Wide);
	_panners.set_available_panners (PannerManager::instance ().PannerManager::get_available_panners (in, out));
	_panners.set_panner (s->panner_shell (), s->panner ());

	_send->set_metering (true);
	_send->output ()->changed.connect (connections, invalidator (*this), boost::bind (&SendUI::outs_changed, this, _1, _2), gui_context ());

	_gpm.setup_meters ();
	_gpm.set_fader_name (X_("SendUIFader"));
	_gpm.set_controls (boost::shared_ptr<Route> (), s->meter (), s->amp (), s->gain_control ());

	_io = Gtk::manage (new IOSelector (parent, session, s->output ()));

	set_name (X_("SendUIFrame"));

	_hbox.pack_start (_gpm, true, true);

	_vbox.set_spacing (5);
	_vbox.set_border_width (5);
	_vbox.pack_start (_hbox, false, false, false);
	_vbox.pack_start (_panners, false, false);

	pack_start (_vbox, false, false);
	pack_start (*_io, true, true);

	_io->show ();
	_gpm.show_all ();
	_panners.show_all ();
	_vbox.show ();
	_hbox.show ();

	fast_screen_update_connection = Timers::super_rapid_connect (
	    sigc::mem_fun (*this, &SendUI::fast_update));
}

SendUI::~SendUI ()
{
	_send->set_metering (false);

	screen_update_connection.disconnect ();
	fast_screen_update_connection.disconnect ();
}

void
SendUI::outs_changed (IOChange change, void* /*ignored*/)
{
	ENSURE_GUI_THREAD (*this, &SendUI::outs_changed, change, ignored)
	if (change.type & IOChange::ConfigurationChanged) {
		uint32_t const in  = _send->pans_required ();
		uint32_t const out = _send->pan_outs ();
		if (_panners._panner == 0) {
			_panners.set_panner (_send->panner_shell (), _send->panner ());
		}
		_panners.set_available_panners (PannerManager::instance ().PannerManager::get_available_panners (in, out));
		_panners.setup_pan ();
		_panners.show_all ();
		_gpm.setup_meters ();
	}
}

void
SendUI::update ()
{
}

void
SendUI::fast_update ()
{
	if (!is_mapped ()) {
		return;
	}

	if (Config->get_meter_falloff () > 0.0f) {
		_gpm.update_meters ();
	}
}

SendUIWindow::SendUIWindow (boost::shared_ptr<Send> s, Session* session)
	: ArdourWindow (string (_("Send ")) + s->name ())
{
	ui = new SendUI (this, s, session);

	hpacker.pack_start (*ui, true, true);

	add (hpacker);

	set_name ("SendUIWindow");

	ui->show ();
	hpacker.show ();
}

SendUIWindow::~SendUIWindow ()
{
	delete ui;
}
