/*
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
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

#include "ardour/smf_source.h"
#include "ardour/midi_region.h"

#include "midi_view.h"
#include "pianoroll.h"
#include "pianoroll_background.h"

PianorollMidiBackground::PianorollMidiBackground (ArdourCanvas::Item* parent, Pianoroll& pr)
	: MidiViewBackground (parent, pr)
	, pianoroll (pr)
	, _width (0)
	, _height (0)
{
}

PianorollMidiBackground::~PianorollMidiBackground ()
{
}

void
PianorollMidiBackground::set_size (int w, int h)
{
	_width = w;
	_height = h;

	update_contents_height ();

	HeightChanged (); /* EMIT SIGNAL */
}

int
PianorollMidiBackground::contents_height() const
{
	return _height;
}

int
PianorollMidiBackground::height() const
{
	return _height;
}

int
PianorollMidiBackground::width() const
{
	return _width;
}

ARDOUR::InstrumentInfo*
PianorollMidiBackground::instrument_info () const
{
	return pianoroll.instrument_info ();
}

uint8_t
PianorollMidiBackground::get_preferred_midi_channel () const
{
	return pianoroll.visible_channel ();
}

void
PianorollMidiBackground::set_note_highlight (uint8_t note)
{
	pianoroll.set_note_highlight (note);
}

void
PianorollMidiBackground::record_layer_check (std::shared_ptr<ARDOUR::Region>, samplepos_t)
{
}

void
PianorollMidiBackground::apply_note_range_to_children ()
{
	pianoroll. apply_note_range (lowest_note(), highest_note());
}

std::shared_ptr<ARDOUR::MidiTrack>
PianorollMidiBackground::midi_track() const
{
	return pianoroll.midi_track();
}
