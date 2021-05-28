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

#include "ardour_message.h"
#include "ardour_ui.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace std;

void
ARDOUR_UI::launch_chat ()
{
	ArdourMessageDialog dialog(_("<b>Just ask and wait for an answer.\nIt may take from minutes to hours.</b>"), true);

	dialog.set_title (_("About the Chat"));
	dialog.set_secondary_text (_("When you're inside the chat just ask your question and wait for an answer. The chat is occupied by real people with real lives so many of them are passively online and might not read your question before minutes or hours later.\nSo please be patient and wait for an answer.\n\nYou should just leave the chat window open and check back regularly until someone has answered your question."));

	switch (dialog.run()) {
	case RESPONSE_OK:
		open_uri("http://ardour.org/chat");
		break;
	default:
		break;
	}
}

void
ARDOUR_UI::launch_tutorial ()
{
	PBD::open_uri (Config->get_tutorial_manual_url());
}

void
ARDOUR_UI::launch_reference ()
{
	PBD::open_uri (Config->get_reference_manual_url());
}

void
ARDOUR_UI::launch_tracker ()
{
	PBD::open_uri ("http://tracker.ardour.org");
}

void
ARDOUR_UI::launch_subscribe ()
{
	PBD::open_uri ("https://community.ardour.org/s/subscribe");
}

void
ARDOUR_UI::launch_cheat_sheet ()
{
#ifdef __APPLE__
	PBD::open_uri ("http://manual.ardour.org/files/a3_mnemonic_cheat_sheet_osx.pdf");
#else
	PBD::open_uri ("http://manual.ardour.org/files/a3_mnemonic_cheatsheet.pdf");
#endif
}

void
ARDOUR_UI::launch_website ()
{
	PBD::open_uri ("http://ardour.org");
}

void
ARDOUR_UI::launch_website_dev ()
{
	PBD::open_uri ("http://ardour.org/development.html");
}

void
ARDOUR_UI::launch_forums ()
{
	PBD::open_uri ("https://community.ardour.org/forums");
}

void
ARDOUR_UI::launch_howto_report ()
{
	PBD::open_uri ("http://ardour.org/reporting_bugs");
}

