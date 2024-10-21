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

#include "midi_cue_background.h"
#include "midi_view.h"

CueMidiBackground::CueMidiBackground (ArdourCanvas::Item* parent)
	: MidiViewBackground (parent)
	, view (nullptr)
	, _width (0.)
	, _height (0.)
{
}

CueMidiBackground::~CueMidiBackground ()
{
}

void
CueMidiBackground::set_size (double w, double h)
{
	_width = w;
	_height = h;

	update_contents_height ();

	HeightChanged (); /* EMIT SIGNAL */
}

double
CueMidiBackground::contents_height() const
{
	return _height;
}

double
CueMidiBackground::height() const
{
	return _height;
}

double
CueMidiBackground::width() const
{
	return _width;
}

uint8_t
CueMidiBackground::get_preferred_midi_channel () const
{
	return 0;
}

void
CueMidiBackground::set_note_highlight (bool yn)
{
}

void
CueMidiBackground::record_layer_check (std::shared_ptr<ARDOUR::Region>, samplepos_t)
{
}

void
CueMidiBackground::set_view (MidiView* mv)
{
	view = mv;
}

void
CueMidiBackground::apply_note_range_to_children ()
{
	if (view) {
		view->apply_note_range (lowest_note(), highest_note());
	}
}
