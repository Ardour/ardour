/*
    Copyright (C) 2011 Paul Davis

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

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include "pbd/error.h"

#include "ardour/midi_playlist.h"
#include "ardour/midi_playlist_source.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

namespace ARDOUR {
class MidiStateTracker;
class Session;
template <typename T> class MidiRingBuffer;
}

namespace Evoral {
template <typename T> class EventSink;
template <typename Time> class Event;
}

/*******************************************************************************
As of May 2011, it appears too complex to support compound regions for MIDI
because of the need to be able to edit the data represented by the region.  It
seems that it would be a better idea to render the consituent regions into a
new MIDI file and create a new region based on that, an operation we have been
calling "consolidate"

This code has been in place as a stub in case anyone gets any brilliant ideas
on other ways to approach this issue.
********************************************************************************/

MidiPlaylistSource::MidiPlaylistSource (Session& s, const ID& orig, const std::string& name, boost::shared_ptr<MidiPlaylist> p,
					uint32_t /*chn*/, frameoffset_t begin, framecnt_t len, Source::Flag flags)
	: Source (s, DataType::MIDI, name)
	, MidiSource (s, name, flags)
	, PlaylistSource (s, orig, name, p, DataType::MIDI, begin, len, flags)
{
}

MidiPlaylistSource::MidiPlaylistSource (Session& s, const XMLNode& node)
	: Source (s, node)
	, MidiSource (s, node)
	, PlaylistSource (s, node)
{
	/* PlaylistSources are never writable, renameable, removable or destructive */
	_flags = Flag (_flags & ~(Writable|CanRename|Removable|RemovableIfEmpty|RemoveAtDestroy|Destructive));

	/* ancestors have already called ::set_state() in their XML-based
	   constructors.
	*/

	if (set_state (node, Stateful::loading_state_version, false)) {
		throw failed_constructor ();
	}
}

MidiPlaylistSource::~MidiPlaylistSource ()
{
}

XMLNode&
MidiPlaylistSource::get_state ()
{
	XMLNode& node (MidiSource::get_state ());

	/* merge PlaylistSource state */

	PlaylistSource::add_state (node);

	return node;
}

int
MidiPlaylistSource::set_state (const XMLNode& node, int version)
{
	return set_state (node, version, true);
}

int
MidiPlaylistSource::set_state (const XMLNode& node, int version, bool with_descendants)
{
	if (with_descendants) {
		if (Source::set_state (node, version) ||
		    MidiSource::set_state (node, version) ||
		    PlaylistSource::set_state (node, version)) {
			return -1;
		}
	}

	return 0;
}

framecnt_t
MidiPlaylistSource::length (framepos_t)  const
{
	pair<framepos_t,framepos_t> extent = _playlist->get_extent();
	return extent.second - extent.first;
}

framecnt_t
MidiPlaylistSource::read_unlocked (Evoral::EventSink<framepos_t>& dst,
				   framepos_t /*position*/,
				   framepos_t start, framecnt_t cnt,
				   MidiStateTracker*) const
{
	boost::shared_ptr<MidiPlaylist> mp = boost::dynamic_pointer_cast<MidiPlaylist> (_playlist);

	if (!mp) {
		return 0;
	}

	return mp->read (dst, start, cnt);
}

framecnt_t
MidiPlaylistSource::write_unlocked (MidiRingBuffer<framepos_t>&,
				    framepos_t,
				    framecnt_t)
{
	fatal << string_compose (_("programming error: %1"), "MidiPlaylistSource::write_unlocked() called - should be impossible") << endmsg;
	/*NOTREACHED*/
	return 0;
}

void
MidiPlaylistSource::append_event_unlocked_beats(const Evoral::Event<Evoral::MusicalTime>& /*ev*/)
{
	fatal << string_compose (_("programming error: %1"), "MidiPlaylistSource::append_event_unlocked_beats() called - should be impossible") << endmsg;
	/*NOTREACHED*/
}

void
MidiPlaylistSource::append_event_unlocked_frames(const Evoral::Event<framepos_t>& /* ev */, framepos_t /*source_start*/)
{
	fatal << string_compose (_("programming error: %1"), "MidiPlaylistSource::append_event_unlocked_frames() called - should be impossible") << endmsg;
	/*NOTREACHED*/
}

void
MidiPlaylistSource::load_model (bool, bool)
{
	/* nothing to do */
}

void
MidiPlaylistSource::destroy_model ()
{
	/* nothing to do */
}

void
MidiPlaylistSource::flush_midi ()
{
}


bool
MidiPlaylistSource::empty () const
{
	return !_playlist || _playlist->empty();
}

