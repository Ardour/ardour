/*
 * Copyright (C) 2006-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2012 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2015-2017 Nick Mainsbridge <mainsbridge@gmail.com>
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

#ifndef __gtk_ardour_midi_cue_view_h__
#define __gtk_ardour_midi_cue_view_h__

#include "midi_view.h"

class MidiCueView : public MidiView
{
  public:
	MidiCueView (std::shared_ptr<ARDOUR::MidiTrack> mt,
	          ArdourCanvas::Item&      parent,
	          EditingContext&          ec,
	          MidiViewBackground&      bg,
	          uint32_t                 basic_color);

	bool canvas_event (GdkEvent*);
	void set_samples_per_pixel (double);
	void set_height (double);

	ArdourCanvas::Item* drag_group() const;

  protected:
	bool scroll (GdkEventScroll* ev);

	std::shared_ptr<Temporal::TempoMap const> tempo_map;
	ArdourCanvas::Rectangle* event_rect;
};


#endif /* __gtk_ardour_midi_cue_view_h__ */
