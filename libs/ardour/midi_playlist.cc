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

#include <cassert>

#include <algorithm>
#include <iostream>

#include <stdlib.h>

#include "pbd/error.h"

#include "evoral/EventList.hpp"

#include "ardour/configuration.h"
#include "ardour/debug.h"
#include "ardour/midi_model.h"
#include "ardour/midi_playlist.h"
#include "ardour/midi_region.h"
#include "ardour/midi_ring_buffer.h"
#include "ardour/session.h"
#include "ardour/types.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

MidiPlaylist::MidiPlaylist (Session& session, const XMLNode& node, bool hidden)
	: Playlist (session, node, DataType::MIDI, hidden)
	, _note_mode(Sustained)
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
{
}

MidiPlaylist::MidiPlaylist (boost::shared_ptr<const MidiPlaylist> other, string name, bool hidden)
	: Playlist (other, name, hidden)
	, _note_mode(other->_note_mode)
{
}

MidiPlaylist::MidiPlaylist (boost::shared_ptr<const MidiPlaylist> other, framepos_t start, framecnt_t dur, string name, bool hidden)
	: Playlist (other, start, dur, name, hidden)
	, _note_mode(other->_note_mode)
{
	/* this constructor does NOT notify others (session) */
}

MidiPlaylist::~MidiPlaylist ()
{
}

template<typename Time>
struct EventsSortByTimeAndType {
    bool operator() (Evoral::Event<Time>* a, Evoral::Event<Time>* b) {
	    if (a->time() == b->time()) {
		    if (EventTypeMap::instance().type_is_midi (a->event_type()) && EventTypeMap::instance().type_is_midi (b->event_type())) {
			    /* negate return value since we must return whether
			     * or not a should sort before b, not b before a
			     */
			    return !MidiBuffer::second_simultaneous_midi_byte_is_first (a->buffer()[0], b->buffer()[0]);
		    }
	    }
	    return a->time() < b->time();
    }
};

/** Returns the number of frames in time duration read (eg could be large when 0 events are read) */
framecnt_t
MidiPlaylist::read (Evoral::EventSink<framepos_t>& dst, framepos_t start, framecnt_t dur, unsigned chan_n)
{
	/* this function is never called from a realtime thread, so
	   its OK to block (for short intervals).
	*/

	Glib::RecMutex::Lock rm (region_lock);
	DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("++++++ %1 .. %2  +++++++ %3 trackers +++++++++++++++++\n", 
							    start, start + dur, _note_trackers.size()));

	framepos_t end = start + dur - 1;

	// relevent regions overlapping start <--> end
	vector< boost::shared_ptr<Region> > regs;
	vector< boost::shared_ptr<Region> > ended;
	typedef pair<MidiStateTracker*,framepos_t> TrackerInfo;
	vector<TrackerInfo> tracker_info;
	NoteTrackers::iterator t;

	for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {

		/* in this call to coverage, the return value indicates the
		 * overlap status of the read range (start...end) WRT to 
		 * the region.
		 */

		switch ((*i)->coverage (start, end)) {
		case Evoral::OverlapStart:
		case Evoral::OverlapInternal:
		case Evoral::OverlapExternal:
			regs.push_back (*i);
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

	if (regs.size() == 1 && 
	    (ended.empty() || (ended.size() == 1 && ended.front() == regs.front()))) {

		/* just a single region - read directly into dst */

		DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("Single region (%1) read, ended during this read %2\n", regs.front()->name(),
								    ended.size()));

		boost::shared_ptr<MidiRegion> mr = boost::dynamic_pointer_cast<MidiRegion>(regs.front());

		if (mr) {

			NoteTrackers::iterator t = _note_trackers.find (mr.get());
			MidiStateTracker* tracker;
			bool new_tracker = false;

			if (t == _note_trackers.end()) {
				tracker = new MidiStateTracker;
				new_tracker = true;
				DEBUG_TRACE (DEBUG::MidiPlaylistIO, "\tBEFORE: new tracker\n");
			} else {
				tracker = t->second;
				DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("\tBEFORE: tracker says there are %1 on notes\n", tracker->on()));
			}

			mr->read_at (dst, start, dur, chan_n, _note_mode, tracker);
			DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("\tAFTER: tracker says there are %1 on notes\n", tracker->on()));

			if (!ended.empty()) {
				DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("\t%1 ended in this read, resolve notes and delete (%2) tracker\n",
										    mr->name(), ((new_tracker) ? "new" : "old")));
				tracker->resolve_notes (dst, mr->last_frame());
				delete tracker;
				if (!new_tracker) {
					_note_trackers.erase (t);
				}
			} else {
				if (new_tracker) {
					pair<Region*,MidiStateTracker*> newpair;
					newpair.first = mr.get();
					newpair.second = tracker;
					_note_trackers.insert (newpair);
					DEBUG_TRACE (DEBUG::MidiPlaylistIO, "\tadded tracker to trackers\n");
				}
			}
		}

	} else {

		/* multiple regions and/or note resolution: sort by layer, read into a temporary non-monotonically
		   sorted EventSink, sort and then insert into dst.
		*/

		DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("%1 regions to read, plus %2 trackers\n", regs.size(), tracker_info.size()));

		Evoral::EventList<framepos_t> evlist;

		DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("for %1 .. %2 we have %3 to consider\n", start, start+dur-1, regs.size()));

		for (vector<boost::shared_ptr<Region> >::iterator i = regs.begin(); i != regs.end(); ++i) {

			boost::shared_ptr<MidiRegion> mr = boost::dynamic_pointer_cast<MidiRegion>(*i);

			if (!mr) {
				continue;
			}

			NoteTrackers::iterator t = _note_trackers.find (mr.get());
			MidiStateTracker* tracker;
			bool new_tracker = false;

			DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("Before %1 (%2 .. %3) we now have %4 events\n", mr->name(), mr->position(), mr->last_frame(), evlist.size()));

			if (t == _note_trackers.end()) {
				tracker = new MidiStateTracker;
				new_tracker = true;
				DEBUG_TRACE (DEBUG::MidiPlaylistIO, "\tBEFORE: new tracker\n");
			} else {
				tracker = t->second;
				DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("\tBEFORE: tracker says there are %1 on notes\n", tracker->on()));
			}


			mr->read_at (evlist, start, dur, chan_n, _note_mode, tracker);

#ifndef NDEBUG
			DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("After %1 (%2 .. %3) we now have %4\n", mr->name(), mr->position(), mr->last_frame(), evlist.size()));
			for (Evoral::EventList<framepos_t>::iterator x = evlist.begin(); x != evlist.end(); ++x) {
				DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("\t%1\n", **x));
			}
			DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("\tAFTER: tracker says there are %1 on notes\n", tracker->on()));
#endif
			if (find (ended.begin(), ended.end(), *i) != ended.end()) {

				/* the region ended within the read range, so
				 * resolve any dangling notes (i.e. notes whose
				 * end is beyond the end of the region).
				 */
				
				DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("\t%1 ended in this read, resolve notes and delete (%2) tracker\n",
										    mr->name(), ((new_tracker) ? "new" : "old")));

				tracker->resolve_notes (evlist, (*i)->last_frame());
				delete tracker;
				if (!new_tracker) {
					_note_trackers.erase (t);
				}

			} else {

				if (new_tracker) {
					pair<Region*,MidiStateTracker*> newpair;
					newpair.first = mr.get();
					newpair.second = tracker;
					_note_trackers.insert (newpair).first;
					DEBUG_TRACE (DEBUG::MidiPlaylistIO, "\tadded tracker to trackers\n");
				}
			}
		}

		if (!evlist.empty()) {

			/* sort the event list */
			EventsSortByTimeAndType<framepos_t> cmp;
			evlist.sort (cmp);

#ifndef NDEBUG
			DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("Final we now have %1 events\n",  evlist.size()));
			for (Evoral::EventList<framepos_t>::iterator x = evlist.begin(); x != evlist.end(); ++x) {
				DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("\t%1\n", **x));
			}
#endif
			/* write into dst */
			for (Evoral::EventList<framepos_t>::iterator e = evlist.begin(); e != evlist.end(); ++e) {
				Evoral::Event<framepos_t>* ev (*e);
				dst.write (ev->time(), ev->event_type(), ev->size(), ev->buffer());
				delete ev;
			}

		}
	}

	DEBUG_TRACE (DEBUG::MidiPlaylistIO, "-------------------------------------------------------------\n");
	return dur;
}

void
MidiPlaylist::clear_note_trackers ()
{
	Glib::RecMutex::Lock rm (region_lock);
	for (NoteTrackers::iterator n = _note_trackers.begin(); n != _note_trackers.end(); ++n) {
		delete n->second;
	}
	DEBUG_TRACE (DEBUG::MidiTrackers, string_compose ("%1 clears all note trackers\n", name()));
	_note_trackers.clear ();
}

void
MidiPlaylist::remove_dependents (boost::shared_ptr<Region> region)
{
	/* MIDI regions have no dependents (crossfades) but we might be tracking notes */
	NoteTrackers::iterator t = _note_trackers.find (region.get());

	/* GACK! THREAD SAFETY! */

	if (t != _note_trackers.end()) {
		delete t->second;
		_note_trackers.erase (t);
	}
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
		RegionLock rlock (this);
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

	Glib::RecMutex::Lock rm (region_lock);

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


bool
MidiPlaylist::region_changed (const PBD::PropertyChange& what_changed, boost::shared_ptr<Region> region)
{
	if (in_flush || in_set_state) {
		return false;
	}

	PBD::PropertyChange our_interests;
	our_interests.add (Properties::midi_data);

	bool parent_wants_notify = Playlist::region_changed (what_changed, region);

	if (parent_wants_notify || what_changed.contains (our_interests)) {
		notify_contents_changed ();
	}

	return true;
}

