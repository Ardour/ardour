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

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <utility>

#include "evoral/EventList.hpp"

#include "ardour/beats_frames_converter.h"
#include "ardour/debug.h"
#include "ardour/midi_model.h"
#include "ardour/midi_playlist.h"
#include "ardour/midi_region.h"
#include "ardour/midi_source.h"
#include "ardour/midi_state_tracker.h"
#include "ardour/session.h"
#include "ardour/types.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

MidiPlaylist::MidiPlaylist (Session& session, const XMLNode& node, bool hidden)
	: Playlist (session, node, DataType::MIDI, hidden)
	, _note_mode(Sustained)
	, _read_end(0)
{
#ifndef NDEBUG
	const XMLProperty* prop = node.property("type");
	assert(prop && DataType(prop->value()) == DataType::MIDI);
#endif

	in_set_state++;
	if (set_state (node, Stateful::loading_state_version)) {
		throw failed_constructor ();
	}
	in_set_state--;

	relayer ();
}

MidiPlaylist::MidiPlaylist (Session& session, string name, bool hidden)
	: Playlist (session, name, DataType::MIDI, hidden)
	, _note_mode(Sustained)
	, _read_end(0)
{
}

MidiPlaylist::MidiPlaylist (boost::shared_ptr<const MidiPlaylist> other, string name, bool hidden)
	: Playlist (other, name, hidden)
	, _note_mode(other->_note_mode)
	, _read_end(0)
{
}

MidiPlaylist::MidiPlaylist (boost::shared_ptr<const MidiPlaylist> other,
                            framepos_t                            start,
                            framecnt_t                            dur,
                            string                                name,
                            bool                                  hidden)
	: Playlist (other, start, dur, name, hidden)
	, _note_mode(other->_note_mode)
	, _read_end(0)
{
}

MidiPlaylist::~MidiPlaylist ()
{
}

template<typename Time>
struct EventsSortByTimeAndType {
    bool operator() (Evoral::Event<Time>* a, Evoral::Event<Time>* b) {
	    if (a->time() == b->time()) {
		    if (parameter_is_midi ((AutomationType)a->event_type()) &&
		        parameter_is_midi ((AutomationType)b->event_type())) {
			    /* negate return value since we must return whether
			     * or not a should sort before b, not b before a
			     */
			    return !MidiBuffer::second_simultaneous_midi_byte_is_first (a->buffer()[0], b->buffer()[0]);
		    }
	    }
	    return a->time() < b->time();
    }
};

framecnt_t
MidiPlaylist::read (Evoral::EventSink<framepos_t>& dst,
                    framepos_t                     start,
                    framecnt_t                     dur,
                    unsigned                       chan_n,
                    MidiChannelFilter*             filter)
{
	typedef pair<MidiStateTracker*,framepos_t> TrackerInfo;

	Playlist::RegionReadLock rl (this);

	DEBUG_TRACE (DEBUG::MidiPlaylistIO,
	             string_compose ("---- MidiPlaylist::read %1 .. %2 (%3 trackers) ----\n",
	                             start, start + dur, _note_trackers.size()));

	/* First, emit any queued edit fixup events at start. */
	for (NoteTrackers::iterator t = _note_trackers.begin(); t != _note_trackers.end(); ++t) {
		t->second->fixer.emit(dst, _read_end, t->second->tracker);
	}

	/* Find relevant regions that overlap [start..end] */
	const framepos_t                         end = start + dur - 1;
	std::vector< boost::shared_ptr<Region> > regs;
	std::vector< boost::shared_ptr<Region> > ended;
	for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {
		switch ((*i)->coverage (start, end)) {
		case Evoral::OverlapStart:
		case Evoral::OverlapInternal:
			regs.push_back (*i);
			break;

		case Evoral::OverlapExternal:
			/* this region is entirely contained in the read range */
			regs.push_back (*i);
			ended.push_back (*i);
			break;

		case Evoral::OverlapEnd:
			/* this region ends within the read range */
			regs.push_back (*i);
			ended.push_back (*i);
			break;

		default:
			/* we don't care */
			break;
		}
	}

	/* If we are reading from a single region, we can read directly into dst.  Otherwise,
	   we read into a temporarily list, sort it, then write that to dst. */
	const bool direct_read = regs.size() == 1 &&
		(ended.empty() || (ended.size() == 1 && ended.front() == regs.front()));

	Evoral::EventList<framepos_t>  evlist;
	Evoral::EventSink<framepos_t>& tgt = direct_read ? dst : evlist;

	DEBUG_TRACE (DEBUG::MidiPlaylistIO,
	             string_compose ("\t%1 regions to read, direct: %2\n", regs.size(), direct_read));

	for (vector<boost::shared_ptr<Region> >::iterator i = regs.begin(); i != regs.end(); ++i) {
		boost::shared_ptr<MidiRegion> mr = boost::dynamic_pointer_cast<MidiRegion>(*i);
		if (!mr) {
			continue;
		}

		/* Get the existing note tracker for this region, or create a new one. */
		NoteTrackers::iterator           t           = _note_trackers.find (mr.get());
		bool                             new_tracker = false;
		boost::shared_ptr<RegionTracker> tracker;
		if (t == _note_trackers.end()) {
			tracker     = boost::shared_ptr<RegionTracker>(new RegionTracker);
			new_tracker = true;
			DEBUG_TRACE (DEBUG::MidiPlaylistIO,
			             string_compose ("\tPre-read %1 (%2 .. %3): new tracker\n",
			                             mr->name(), mr->position(), mr->last_frame()));
		} else {
			tracker = t->second;
			DEBUG_TRACE (DEBUG::MidiPlaylistIO,
			             string_compose ("\tPre-read %1 (%2 .. %3): %4 active notes\n",
			                             mr->name(), mr->position(), mr->last_frame(), tracker->tracker.on()));
		}

		/* Read from region into target. */
		mr->read_at (tgt, start, dur, chan_n, _note_mode, &tracker->tracker, filter);
		DEBUG_TRACE (DEBUG::MidiPlaylistIO,
		             string_compose ("\tPost-read: %1 active notes\n", tracker->tracker.on()));

		if (find (ended.begin(), ended.end(), *i) != ended.end()) {
			/* Region ended within the read range, so resolve any active notes
			   (either stuck notes in the data, or notes that end after the end
			   of the region). */
			DEBUG_TRACE (DEBUG::MidiPlaylistIO,
			             string_compose ("\t%1 ended, resolve notes and delete (%2) tracker\n",
			                             mr->name(), ((new_tracker) ? "new" : "old")));

			tracker->tracker.resolve_notes (tgt, (*i)->last_frame());
			if (!new_tracker) {
				_note_trackers.erase (t);
			}

		} else {

			if (new_tracker) {
				_note_trackers.insert (make_pair (mr.get(), tracker));
				DEBUG_TRACE (DEBUG::MidiPlaylistIO, "\tadded tracker to trackers\n");
			}
		}
	}

	if (!direct_read && !evlist.empty()) {
		/* We've read from multiple regions, sort the event list by time. */
		EventsSortByTimeAndType<framepos_t> cmp;
		evlist.sort (cmp);

		/* Copy ordered events from event list to dst. */
		for (Evoral::EventList<framepos_t>::iterator e = evlist.begin(); e != evlist.end(); ++e) {
			Evoral::Event<framepos_t>* ev (*e);
			dst.write (ev->time(), ev->event_type(), ev->size(), ev->buffer());
			delete ev;
		}
	}

	DEBUG_TRACE (DEBUG::MidiPlaylistIO, "---- End MidiPlaylist::read ----\n");
	_read_end = start + dur;
	return dur;
}

void
MidiPlaylist::region_edited(boost::shared_ptr<Region>         region,
                            const MidiModel::NoteDiffCommand* cmd)
{
	typedef MidiModel::NoteDiffCommand Command;

	boost::shared_ptr<MidiRegion> mr = boost::dynamic_pointer_cast<MidiRegion>(region);
	if (!mr || !_session.transport_rolling()) {
		return;
	}

	/* Take write lock to prevent concurrency with read(). */
	Playlist::RegionWriteLock lock(this);

	NoteTrackers::iterator t = _note_trackers.find(mr.get());
	if (t == _note_trackers.end()) {
		return; /* Region is not currently active, nothing to do. */
	}

	/* Queue any necessary edit compensation events. */
	t->second->fixer.prepare(
		_session.tempo_map(), cmd, mr->position() - mr->start(),
		_read_end, mr->midi_source()->model()->active_notes());
}

void
MidiPlaylist::reset_note_trackers ()
{
	Playlist::RegionWriteLock rl (this, false);

	DEBUG_TRACE (DEBUG::MidiTrackers, string_compose ("%1 reset all note trackers\n", name()));
	_note_trackers.clear ();
}

void
MidiPlaylist::resolve_note_trackers (Evoral::EventSink<framepos_t>& dst, framepos_t time)
{
	Playlist::RegionWriteLock rl (this, false);

	for (NoteTrackers::iterator n = _note_trackers.begin(); n != _note_trackers.end(); ++n) {
		n->second->tracker.resolve_notes(dst, time);
	}
	DEBUG_TRACE (DEBUG::MidiTrackers, string_compose ("%1 resolve all note trackers\n", name()));
	_note_trackers.clear ();
}

void
MidiPlaylist::remove_dependents (boost::shared_ptr<Region> region)
{
	/* MIDI regions have no dependents (crossfades) but we might be tracking notes */
	_note_trackers.erase(region.get());
}

int
MidiPlaylist::set_state (const XMLNode& node, int version)
{
	in_set_state++;
	freeze ();

	if (Playlist::set_state (node, version)) {
		return -1;
	}

	thaw();
	in_set_state--;

	return 0;
}

void
MidiPlaylist::dump () const
{
	boost::shared_ptr<Region> r;

	cerr << "Playlist \"" << _name << "\" " << endl
	<< regions.size() << " regions "
	<< endl;

	for (RegionList::const_iterator i = regions.begin(); i != regions.end(); ++i) {
		r = *i;
		cerr << "  " << r->name() << " @ " << r << " ["
		<< r->start() << "+" << r->length()
		<< "] at "
		<< r->position()
		<< " on layer "
		<< r->layer ()
		<< endl;
	}
}

bool
MidiPlaylist::destroy_region (boost::shared_ptr<Region> region)
{
	boost::shared_ptr<MidiRegion> r = boost::dynamic_pointer_cast<MidiRegion> (region);

	if (!r) {
		return false;
	}

	bool changed = false;

	{
		RegionWriteLock rlock (this);
		RegionList::iterator i;
		RegionList::iterator tmp;

		for (i = regions.begin(); i != regions.end(); ) {

			tmp = i;
			++tmp;

			if ((*i) == region) {
				regions.erase (i);
				changed = true;
			}

			i = tmp;
		}
	}


	if (changed) {
		/* overload this, it normally means "removed", not destroyed */
		notify_region_removed (region);
	}

	return changed;
}

set<Evoral::Parameter>
MidiPlaylist::contained_automation()
{
	/* this function is never called from a realtime thread, so
	   its OK to block (for short intervals).
	*/

	Playlist::RegionReadLock rl (this);
	set<Evoral::Parameter> ret;

	for (RegionList::const_iterator r = regions.begin(); r != regions.end(); ++r) {
		boost::shared_ptr<MidiRegion> mr = boost::dynamic_pointer_cast<MidiRegion>(*r);

		for (Automatable::Controls::iterator c = mr->model()->controls().begin();
				c != mr->model()->controls().end(); ++c) {
			ret.insert(c->first);
		}
	}

	return ret;
}
