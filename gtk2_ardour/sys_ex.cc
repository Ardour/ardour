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

#include "gtkmm2ext/keyboard.h"

#include "editor.h"
#include "midi_region_view.h"
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
	, _region (region)
{
	_flag = new ArdourCanvas::Flag (
		parent,
		height,
		UIConfiguration::instance().color ("midi sysex outline"),
		UIConfiguration::instance().color_mod ("midi sysex fill", "midi sysex fill"),
		ArdourCanvas::Duple (x, y)
		);

	_flag->Event.connect (sigc::mem_fun (*this, &SysEx::event_handler));
	_flag->set_font_description (UIConfiguration::instance ().get_SmallFont ());
	_flag->set_text (text);
}

SysEx::~SysEx()
{
	delete _flag;
}

bool
SysEx::event_handler (GdkEvent* ev)
{
	/* XXX: icky dcast */
	Editor* e = dynamic_cast<Editor*> (&_region.get_time_axis_view ().editor ());

	if (!e->internal_editing ()) {
		return false;
	}

	switch (ev->type) {
		case GDK_BUTTON_PRESS:
			if (Gtkmm2ext::Keyboard::is_delete_event (&ev->button)) {
				_region.delete_sysex (this);
				return true;
			}
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
