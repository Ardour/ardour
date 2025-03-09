/*
 * Copyright (C) 2025 Paul Davis <paul@linuxaudiosystems.com>
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

#pragma once

#include "ardour_window.h"

namespace ARDOUR {
	class MidiRegion;
	class Track;
}

class Pianoroll;
class RegionEditor;

class PianorollWindow : public ArdourWindow
{
  public:
	PianorollWindow (std::string const & name, ARDOUR::Session&);
	~PianorollWindow ();

	void set (std::shared_ptr<ARDOUR::MidiTrack>, std::shared_ptr<ARDOUR::MidiRegion>);
	bool on_key_press_event (GdkEventKey*);
	bool on_delete_event (GdkEventAny*);

 private:
	Gtk::HBox hpacker;
	Pianoroll* pianoroll;
	RegionEditor* region_editor;
};
