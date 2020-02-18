/*
 * Copyright (C) 2013-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#include <iostream>
#include "canvas/flag.h"
#include "sys_ex.h"
#include "ui_config.h"

using namespace std;

SysEx::SysEx (
	MidiRegionView&             region,
	ArdourCanvas::Container*    parent,
	string&                     text,
	double                      height,
	double                      x,
	double                      y,
	ARDOUR::MidiModel::SysExPtr sysex)
	: _sysex (sysex)
{
	_flag = new ArdourCanvas::Flag (
		parent,
		height,
		UIConfiguration::instance().color ("midi sysex outline"),
		UIConfiguration::instance().color_mod ("midi sysex fill", "midi sysex fill"),
		ArdourCanvas::Duple (x, y)
		);

	_flag->set_text (text);
}

SysEx::~SysEx()
{
	/* do not delete flag because it was added to a parent/container which
	   will delete it.
	*/
	_flag = 0;
}

bool
SysEx::event_handler (GdkEvent* ev)
{
	switch (ev->type) {
	case GDK_BUTTON_PRESS:
		if (ev->button.button == 3) {
			return true;
		}
		break;

	case GDK_SCROLL:
		if (ev->scroll.direction == GDK_SCROLL_UP) {
			return true;
		} else if (ev->scroll.direction == GDK_SCROLL_DOWN) {
			return true;
		}
		break;

	default:
		break;
	}

	return false;
}

void
SysEx::hide ()
{
	_flag->hide ();
}

void
SysEx::show ()
{
	_flag->show ();
}
