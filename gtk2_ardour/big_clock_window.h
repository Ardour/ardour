/*
 * Copyright (C) 2013-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __bigclock_window_h__
#define __bigclock_window_h__

#include "ardour_window.h"

class AudioClock;

class BigClockWindow : public ArdourWindow
{
public:
	BigClockWindow (AudioClock&);

private:
	AudioClock& clock;
	Gtk::Requisition default_size;

	void clock_size_reallocated (Gtk::Allocation&);
	void on_realize ();
	void on_unmap ();
	bool on_key_press_event (GdkEventKey*);
};

#endif // __ardour_window_h__

