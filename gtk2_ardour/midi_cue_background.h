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

#ifndef __gtk2_ardour_midi_cue_background_h__
#define __gtk2_ardour_midi_cue_background_h__

#include <cstdint>

#include <gtkmm/adjustment.h>

#include "ardour/types.h"

#include "midi_view_background.h"

class MidiView;

class CueMidiBackground : public MidiViewBackground
{
  public:
	CueMidiBackground (ArdourCanvas::Item* parent);
	~CueMidiBackground ();

	double height() const;
	double contents_height() const;

	uint8_t get_preferred_midi_channel () const;
	void set_note_highlight (bool);
	void record_layer_check (std::shared_ptr<ARDOUR::Region>, samplepos_t);

	void set_size (double w, double h);
	void set_view (MidiView*);

  protected:
	MidiView* view;
	double _width;
	double _height;

	void apply_note_range_to_children();
};


#endif /* __gtk2_ardour_midi_cue_background_h__ */
