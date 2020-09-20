/*
 * Copyright (C) 2006-2016 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2017-2019 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_midi_playlist_h__
#define __ardour_midi_playlist_h__

#include <vector>
#include <list>

#include <boost/utility.hpp>

#include "evoral/Parameter.h"

#include "ardour/ardour.h"
#include "ardour/midi_cursor.h"
#include "ardour/midi_model.h"
#include "ardour/midi_state_tracker.h"
#include "ardour/playlist.h"
#include "evoral/Note.h"
#include "evoral/Parameter.h"
#include "ardour/rt_midibuffer.h"

namespace Evoral {
template<typename Time> class EventSink;
class                         Beats;
}

namespace ARDOUR
{

class BeatsSamplesConverter;
class MidiChannelFilter;
class MidiRegion;
class Session;
class Source;

template<typename T> class MidiRingBuffer;

class LIBARDOUR_API MidiPlaylist : public ARDOUR::Playlist
{
public:
	MidiPlaylist (Session&, const XMLNode&, bool hidden = false);
	MidiPlaylist (Session&, std::string name, bool hidden = false);
	MidiPlaylist (boost::shared_ptr<const MidiPlaylist> other, std::string name, bool hidden = false);

	/** This constructor does NOT notify others (session) */
	MidiPlaylist (boost::shared_ptr<const MidiPlaylist> other,
	              timepos_t const &                     start,
	              timepos_t const &                     cnt,
	              std::string                           name,
	              bool                                  hidden = false);

	~MidiPlaylist ();

	void render (MidiChannelFilter*);
	RTMidiBuffer* rendered();

	int set_state (const XMLNode&, int version);

	bool destroy_region (boost::shared_ptr<Region>);

	void _split_region (boost::shared_ptr<Region>, timepos_t const & position, Thawlist& thawlist);

	void set_note_mode (NoteMode m) { _note_mode = m; }

	std::set<Evoral::Parameter> contained_automation();

  protected:
	void remove_dependents (boost::shared_ptr<Region> region);
	void region_going_away (boost::weak_ptr<Region> region);

  private:
	void dump () const;

	NoteMode     _note_mode;

	RTMidiBuffer _rendered;
};

} /* namespace ARDOUR */

#endif /* __ardour_midi_playlist_h__ */
