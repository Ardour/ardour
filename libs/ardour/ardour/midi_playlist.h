/*
    Copyright (C) 2006 Paul Davis
    Author: David Robillard

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

#ifndef __ardour_midi_playlist_h__
#define __ardour_midi_playlist_h__

#include <vector>
#include <list>

#include <boost/utility.hpp>

#include "ardour/ardour.h"
#include "ardour/midi_model.h"
#include "ardour/midi_state_tracker.h"
#include "ardour/note_fixer.h"
#include "ardour/playlist.h"
#include "evoral/Beats.hpp"
#include "evoral/Note.hpp"
#include "evoral/Parameter.hpp"

namespace Evoral {
template<typename Time> class EventSink;
}

namespace ARDOUR
{

class BeatsFramesConverter;
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
	              framepos_t                            start,
	              framecnt_t                            cnt,
	              std::string                           name,
	              bool                                  hidden = false);

	~MidiPlaylist ();

	/** Read a range from the playlist into an event sink.
	 *
	 * @param buf Destination for events.
	 * @param start First frame of read range.
	 * @param cnt Number of frames in read range.
	 * @param chan_n Must be 0 (this is the audio-style "channel", where each
	 * channel is backed by a separate region, not MIDI channels, which all
	 * exist in the same region and are not handled here).
	 * @return The number of frames read (time, not an event count).
	 */
	framecnt_t read (Evoral::EventSink<framepos_t>& buf,
	                 framepos_t                     start,
	                 framecnt_t                     cnt,
	                 uint32_t                       chan_n = 0,
	                 MidiChannelFilter*             filter = NULL);

	int set_state (const XMLNode&, int version);

	bool destroy_region (boost::shared_ptr<Region>);

	void set_note_mode (NoteMode m) { _note_mode = m; }

	std::set<Evoral::Parameter> contained_automation();

	/** Handle a region edit during read.
	 *
	 * This must be called before the command is applied to the model.  Events
	 * are injected into the playlist output to compensate for edits to active
	 * notes and maintain coherent output and tracker state.
	 */
	void region_edited(boost::shared_ptr<Region>         region,
	                   const MidiModel::NoteDiffCommand* cmd);

	/** Clear all note trackers. */
	void reset_note_trackers ();

	/** Resolve all pending notes and clear all note trackers.
	 *
	 * @param dst Sink to write note offs to.
	 * @param time Time stamp of all written note offs.
	 */
	void resolve_note_trackers (Evoral::EventSink<framepos_t>& dst, framepos_t time);

protected:
	void remove_dependents (boost::shared_ptr<Region> region);

private:
	typedef Evoral::Note<Evoral::Beats> Note;
	typedef Evoral::Event<framepos_t>   Event;

	struct RegionTracker : public boost::noncopyable {
		MidiStateTracker tracker;  ///< Active note tracker
		NoteFixer        fixer;    ///< Edit compensation
	};

	typedef std::map< Region*, boost::shared_ptr<RegionTracker> > NoteTrackers;

	void dump () const;

	NoteTrackers _note_trackers;
	NoteMode     _note_mode;
	framepos_t   _read_end;
};

} /* namespace ARDOUR */

#endif	/* __ardour_midi_playlist_h__ */


