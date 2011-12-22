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
struct EventsSortByTime {
    bool operator() (Evoral::Event<Time>* a, Evoral::Event<Time>* b) {
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
	DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("++++++ %1 .. %2  +++++++++++++++++++++++++++++++++++++++++++++++\n", start, start + dur));

	framepos_t end = start + dur - 1;

	// relevent regions overlapping start <--> end
	vector< boost::shared_ptr<Region> > regs;
	typedef pair<MidiStateTracker*,framepos_t> TrackerInfo;
	vector<TrackerInfo> tracker_info;
	uint32_t note_cnt = 0;

	for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {
		if ((*i)->coverage (start, end) != OverlapNone) {
			regs.push_back(*i);
		} else {
			NoteTrackers::iterator t = _note_trackers.find ((*i).get());
			if (t != _note_trackers.end()) {

				/* add it the set of trackers we will do note resolution
				   on, and remove it from the list we are keeping
				   around, because we don't need it anymore.

				   if the end of the region (where we want to theoretically resolve notes)
				   is outside the current read range, then just do it at the start
				   of this read range.
				*/

				framepos_t resolve_at = (*i)->last_frame();
				if (resolve_at < start || resolve_at >= end) {
					resolve_at = start;
				}

				tracker_info.push_back (TrackerInfo (t->second, resolve_at));
				DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("time to resolve & remove tracker for %1 @ %2\n", (*i)->name(), resolve_at));
				note_cnt += (t->second->on());
				_note_trackers.erase (t);
			}
		}
	}

	if (note_cnt == 0 && !tracker_info.empty()) {
		/* trackers to dispose of, but they have no notes in them */
		DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("Clearing %1 empty trackers\n", tracker_info.size()));
		for (vector<TrackerInfo>::iterator t = tracker_info.begin(); t != tracker_info.end(); ++t) {
			delete (*t).first;
		}
		tracker_info.clear ();
	}

	if (regs.size() == 1 && tracker_info.empty()) {

		/* just a single region - read directly into dst */

		DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("Single region (%1) read, no out-of-bound region tracking info\n", regs.front()->name()));

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

			if (new_tracker) {
				pair<Region*,MidiStateTracker*> newpair;
				newpair.first = mr.get();
				newpair.second = tracker;
				_note_trackers.insert (newpair);
				DEBUG_TRACE (DEBUG::MidiPlaylistIO, "\tadded tracker to trackers\n");
			}
		}

	} else {

		/* multiple regions and/or note resolution: sort by layer, read into a temporary non-monotonically
		   sorted EventSink, sort and then insert into dst.
		*/

		DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("%1 regions to read, plus %2 trackers\n", regs.size(), tracker_info.size()));

		Evoral::EventList<framepos_t> evlist;

		for (vector<TrackerInfo>::iterator t = tracker_info.begin(); t != tracker_info.end(); ++t) {
			DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("Resolve %1 notes\n", (*t).first->on()));
			(*t).first->resolve_notes (evlist, (*t).second);
			delete (*t).first;
		}

#ifndef NDEBUG
		DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("After resolution we now have %1 events\n",  evlist.size()));
		for (Evoral::EventList<framepos_t>::iterator x = evlist.begin(); x != evlist.end(); ++x) {
			DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("\t%1\n", **x));
		}
#endif

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

			if (new_tracker) {
				pair<Region*,MidiStateTracker*> newpair;
				newpair.first = mr.get();
				newpair.second = tracker;
				_note_trackers.insert (newpair);
				DEBUG_TRACE (DEBUG::MidiPlaylistIO, "\tadded tracker to trackers\n");
			}
		}

		if (!evlist.empty()) {

			/* sort the event list */
			EventsSortByTime<framepos_t> time_cmp;
			evlist.sort (time_cmp);

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


void
MidiPlaylist::refresh_dependents (boost::shared_ptr<Region> /*r*/)
{
	/* MIDI regions have no dependents (crossfades) */
}

void
MidiPlaylist::finalize_split_region (boost::shared_ptr<Region> /*original*/, boost::shared_ptr<Region> /*left*/, boost::shared_ptr<Region> /*right*/)
{
	/* No MIDI crossfading (yet?), so nothing to do here */
}

void
MidiPlaylist::check_dependents (boost::shared_ptr<Region> /*r*/, bool /*norefresh*/)
{
	/* MIDI regions have no dependents (crossfades) */
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

