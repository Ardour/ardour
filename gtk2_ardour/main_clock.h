/*
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 Colin Fletcher <colin.m.fletcher@googlemail.com>
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

#ifndef __gtk_ardour_main_clock_h__
#define __gtk_ardour_main_clock_h__

#include "audio_clock.h"

/** A simple subclass of AudioClock that adds a few things to its context menu:
 * `display delta to edit cursor' and edit/change tempo/meter
 */
class MainClock : public AudioClock
{
public:
	MainClock (const std::string& clock_name, const std::string& widget_name);
	void set_session (ARDOUR::Session *s);
	void set_display_delta_mode (ARDOUR::ClockDeltaMode m);
	void set (Temporal::timepos_t const &, bool force = false);
	sigc::signal<bool, ARDOUR::ClockDeltaMode> change_display_delta_mode_signal;

private:
	void build_ops_menu ();
	void change_display_delta_mode (ARDOUR::ClockDeltaMode);
	void edit_current_tempo ();
	void edit_current_meter ();
	void insert_new_tempo ();
	void insert_new_meter ();
	bool _suspend_delta_mode_signal;
	std::string _widget_name;
	ARDOUR::ClockDeltaMode _delta_mode;
};

#endif // __gtk_ardour_main_clock_h__
