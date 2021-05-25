/*
 * Copyright (C) 2011-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2011-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
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

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include "pbd/error.h"

#include "ardour/midi_playlist.h"
#include "ardour/midi_playlist_source.h"

#include "pbd/i18n.h"

using namespace std;
using namespace PBD;

namespace Evoral {
template <typename T> class EventSink;
template <typename Time> class Event;
}

namespace ARDOUR {

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
					uint32_t /*chn*/, sampleoffset_t begin, samplecnt_t len, Source::Flag flags)
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
	/* PlaylistSources are never writable, renameable or removable */
	_flags = Flag (_flags & ~(Writable|CanRename|Removable|RemovableIfEmpty|RemoveAtDestroy));

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

samplecnt_t
MidiPlaylistSource::length (samplepos_t)  const
{
	pair<samplepos_t,samplepos_t> extent = _playlist->get_extent();
	return extent.second - extent.first;
}

samplecnt_t
MidiPlaylistSource::read_unlocked (const Lock& lock,
                                   Evoral::EventSink<samplepos_t>& dst,
                                   samplepos_t /*position*/,
                                   samplepos_t start,
                                   samplecnt_t cnt,
                                   Evoral::Range<samplepos_t>* loop_range,
                                   MidiStateTracker*,
                                   MidiChannelFilter*) const
{
	boost::shared_ptr<MidiPlaylist> mp = boost::dynamic_pointer_cast<MidiPlaylist> (_playlist);

	if (!mp) {
		return 0;
	}

	/* XXX paul says on Oct 26 2019:

	   rgareus: so to clarify now that i have better perspective: the API i want to get rid of is MidiPlaylist::read() ; everything that used it (i.e. the DiskReader) should use MidiPlaylist::rendered()->read()
	   rgareus: but a "read" operation is also a "write" operation: you have to put the data somewhere
	   rgareus: the only other user of MidiPlaylist::read() was MidiPlaylistSource (unsurprisingly), which as I noted is not even (really) used
	   rgareus: ::rendered() returns a ptr-to-RT_MidiBuffer, which has a read method which expects to write into a MidiBuffer, using push_back()
	   rgareus: but MidiPlaylistSource::read() is given an EventSink<samplepos_t> as the destination, and this does not (currently) have ::push_back(), only ::write() (which is willing to deal with inserts rather than appends)
	   rgareus: so, this is the API "mess" I'm trying to clean up. simple solution: since we don't use MidiPlaylistSource just comment out the line and forget about it for now, then remove MidiPlaylist::read() and move on

	   This represents that decision, for now.
	*/

	return cnt; // mp->read (dst, start, cnt, loop_range);
}

samplecnt_t
MidiPlaylistSource::write_unlocked (const Lock&,
                                    MidiRingBuffer<samplepos_t>&,
                                    samplepos_t,
                                    samplecnt_t)
{
	fatal << string_compose (_("programming error: %1"), "MidiPlaylistSource::write_unlocked() called - should be impossible") << endmsg;
	abort(); /*NOTREACHED*/
	return 0;
}

void
MidiPlaylistSource::append_event_beats(const Glib::Threads::Mutex::Lock& /*lock*/, const Evoral::Event<Temporal::Beats>& /*ev*/)
{
	fatal << string_compose (_("programming error: %1"), "MidiPlaylistSource::append_event_beats() called - should be impossible") << endmsg;
	abort(); /*NOTREACHED*/
}

void
MidiPlaylistSource::append_event_samples(const Glib::Threads::Mutex::Lock& /*lock*/, const Evoral::Event<samplepos_t>& /* ev */, samplepos_t /*source_start*/)
{
	fatal << string_compose (_("programming error: %1"), "MidiPlaylistSource::append_event_samples() called - should be impossible") << endmsg;
	abort(); /*NOTREACHED*/
}

void
MidiPlaylistSource::load_model (const Glib::Threads::Mutex::Lock&, bool)
{
	/* nothing to do */
}

void
MidiPlaylistSource::destroy_model (const Glib::Threads::Mutex::Lock&)
{
	/* nothing to do */
}

void
MidiPlaylistSource::flush_midi (const Lock& lock)
{
}


bool
MidiPlaylistSource::empty () const
{
	return !_playlist || _playlist->empty();
}

} // namespace ARDOUR
