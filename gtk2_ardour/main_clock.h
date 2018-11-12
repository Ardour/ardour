/*
    Copyright (C) 2012 Paul Davis

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

#ifndef __gtk_ardour_main_clock_h__
#define __gtk_ardour_main_clock_h__

#include "audio_clock.h"

/** A simple subclass of AudioClock that adds a few things to its context menu:
 * `display delta to edit cursor' and edit/change tempo/meter
 */
class MainClock : public AudioClock
{
public:
	MainClock (const std::string& clock_name, const std::string& widget_name, bool primary);
	samplepos_t absolute_time () const;
	void set_session (ARDOUR::Session *s);
	void set (samplepos_t, bool force = false, ARDOUR::samplecnt_t offset = 0);

private:

	// Editor *_editor;

	void build_ops_menu ();
	void set_display_delta_mode (ARDOUR::ClockDeltaMode);
	void edit_current_tempo ();
	void edit_current_meter ();
	void insert_new_tempo ();
	void insert_new_meter ();
	bool _primary;
	bool _suspend_delta_mode_signal;
};

#endif // __gtk_ardour_main_clock_h__
