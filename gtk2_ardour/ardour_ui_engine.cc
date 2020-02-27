/*
 * Copyright (C) 2005-2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2005-2017 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2005-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Karsten Wiese <fzuuzf@googlemail.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2008-2010 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2012-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013-2015 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2013-2016 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2013-2016 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2014-2018 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015 André Nusser <andre.nusser@googlemail.com>
 * Copyright (C) 2016-2018 Len Ovens <len@ovenwerks.net>
 * Copyright (C) 2017 Johannes Mueller <github@johannes-mueller.org>
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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#include "gtk2ardour-version.h"
#endif

#include "pbd/openuri.h"

#include "ardour/audioengine.h"

#include "ardour_message.h"
#include "ardour_ui.h"
#include "engine_dialog.h"
#include "gui_thread.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace ArdourWidgets;
using namespace Gtk;
using namespace std;

void
ARDOUR_UI::audioengine_became_silent ()
{
	ArdourMessageDialog msg (string_compose (_("This is a free/demo copy of %1. It has just switched to silent mode."), PROGRAM_NAME),
	                         true,
	                         Gtk::MESSAGE_WARNING,
	                         Gtk::BUTTONS_NONE,
	                         true);

	msg.set_title (string_compose (_("%1 is now silent"), PROGRAM_NAME));

	Gtk::Label pay_label (string_compose (_("Please consider paying for a copy of %1 - you can pay whatever you want."), PROGRAM_NAME));
	Gtk::Label subscribe_label (_("Better yet become a subscriber - subscriptions start at US$1 per month."));
	Gtk::Button pay_button (_("Pay for a copy (via the web)"));
	Gtk::Button subscribe_button (_("Become a subscriber (via the web)"));
	Gtk::HBox pay_button_box;
	Gtk::HBox subscribe_button_box;

	pay_button_box.pack_start (pay_button, true, false);
	subscribe_button_box.pack_start (subscribe_button, true, false);

	bool (*openuri)(const char*) = PBD::open_uri; /* this forces selection of the const char* variant of PBD::open_uri(), which we need to avoid ambiguity below */

	pay_button.signal_clicked().connect (sigc::hide_return (sigc::bind (sigc::ptr_fun (openuri), (const char*) "https://ardour.org/download")));
	subscribe_button.signal_clicked().connect (sigc::hide_return (sigc::bind (sigc::ptr_fun (openuri), (const char*) "https://community.ardour.org/s/subscribe")));

	msg.get_vbox()->pack_start (pay_label);
	msg.get_vbox()->pack_start (pay_button_box);
	msg.get_vbox()->pack_start (subscribe_label);
	msg.get_vbox()->pack_start (subscribe_button_box);

	msg.get_vbox()->show_all ();

	msg.add_button (_("Remain silent"), Gtk::RESPONSE_CANCEL);
	msg.add_button (_("Save and quit"), Gtk::RESPONSE_NO);
	msg.add_button (_("Give me more time"), Gtk::RESPONSE_YES);

	int r = msg.run ();

	switch (r) {
	case Gtk::RESPONSE_YES:
		AudioEngine::instance()->reset_silence_countdown ();
		break;

	case Gtk::RESPONSE_NO:
		/* save and quit */
		save_state_canfail ("");
		exit (EXIT_SUCCESS);
		break;

	default:
		/* don't reset, save session and exit */
		break;
	}
}

void
ARDOUR_UI::create_xrun_marker (samplepos_t where)
{
	if (_session) {
		Location *location = new Location (*_session, where, where, _("xrun"), Location::IsMark, 0);
		_session->locations()->add (location);
	}
}

void
ARDOUR_UI::halt_on_xrun_message ()
{
	cerr << "HALT on xrun\n";
	ArdourMessageDialog msg (_main_window, _("Recording was stopped because your system could not keep up."));
	msg.run ();
}

void
ARDOUR_UI::xrun_handler (samplepos_t where)
{
	if (!_session) {
		return;
	}

	ENSURE_GUI_THREAD (*this, &ARDOUR_UI::xrun_handler, where)

	if (_session && Config->get_create_xrun_marker() && _session->actively_recording()) {
		create_xrun_marker(where);
	}

	if (_session && Config->get_stop_recording_on_xrun() && _session->actively_recording()) {
		halt_on_xrun_message ();
	}
}

bool
ARDOUR_UI::check_audioengine (Gtk::Window& parent)
{
	if (!AudioEngine::instance()->running()) {
		ArdourMessageDialog msg (parent, string_compose (
		                                 _("%1 is not connected to any audio backend.\n"
		                                 "You cannot open or close sessions in this condition"),
		                                 PROGRAM_NAME));
		msg.run ();
		return false;
	}
	return true;
}
