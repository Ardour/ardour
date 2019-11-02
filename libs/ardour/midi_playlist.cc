/*
 * Copyright (C) 2006-2016 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
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

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <utility>

#include "evoral/EventList.hpp"
#include "evoral/Control.hpp"

#include "ardour/beats_samples_converter.h"
#include "ardour/debug.h"
#include "ardour/midi_model.h"
#include "ardour/midi_playlist.h"
#include "ardour/midi_region.h"
#include "ardour/midi_source.h"
#include "ardour/midi_state_tracker.h"
#include "ardour/region_factory.h"
#include "ardour/rt_midibuffer.h"
#include "ardour/session.h"
#include "ardour/tempo.h"
#include "ardour/types.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

MidiPlaylist::MidiPlaylist (Session& session, const XMLNode& node, bool hidden)
	: Playlist (session, node, DataType::MIDI, hidden)
	, _note_mode(Sustained)
	, _read_end(0)
{
#ifndef NDEBUG
	XMLProperty const * prop = node.property("type");
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
                            samplepos_t                            start,
                            samplecnt_t                            dur,
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
    bool operator() (const Evoral::Event<Time>* a, const Evoral::Event<Time>* b) {
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

samplecnt_t
MidiPlaylist::read (Evoral::EventSink<samplepos_t>& dst,
                    samplepos_t                     start,
                    samplecnt_t                     dur,
                    Evoral::Range<samplepos_t>*     loop_range,
                    unsigned                       chan_n,
                    MidiChannelFilter*             filter)
{
	typedef pair<MidiStateTracker*,samplepos_t> TrackerInfo;

	Playlist::RegionReadLock rl (this);

	DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("---- MidiPlaylist::read %1 .. %2 ----\n", start, start + dur));

	/* Find relevant regions that overlap [start..end] */
	const samplepos_t                         end = start + dur - 1;
	std::vector< boost::shared_ptr<Region> > regs;
	std::vector< boost::shared_ptr<Region> > ended;
	for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {

		/* check for the case of solo_selection */
		bool force_transparent = ( _session.solo_selection_active() && SoloSelectedActive() && !SoloSelectedListIncludes( (const Region*) &(**i) ) );
		if ( force_transparent )
			continue;

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

	Evoral::EventList<samplepos_t>  evlist;
	Evoral::EventSink<samplepos_t>& tgt = direct_read ? dst : evlist;

	DEBUG_TRACE (DEBUG::MidiPlaylistIO,
	             string_compose ("\t%1 regions to read, direct: %2\n", regs.size(), direct_read));

	for (vector<boost::shared_ptr<Region> >::iterator i = regs.begin(); i != regs.end(); ++i) {
		boost::shared_ptr<MidiRegion> mr = boost::dynamic_pointer_cast<MidiRegion>(*i);
		if (!mr) {
			continue;
		}

		MidiCursor cursor; // XXX remove me

		/* Read from region into target. */
		DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("read from %1 at %2 for %3 LR %4 .. %5\n",
		                                                    mr->name(), start, dur,
		                                                    (loop_range ? loop_range->from : -1),
		                                                    (loop_range ? loop_range->to : -1)));
		mr->read_at (tgt, start, dur, loop_range, cursor, chan_n, _note_mode, 0, filter);

		if (find (ended.begin(), ended.end(), *i) != ended.end()) {
			/* Region ended within the read range, so resolve any active notes
			   (either stuck notes in the data, or notes that end after the end
			   of the region). */
			DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("\t%1 ended, resolve notes and delete\n", mr->name()));
		}
	}

	if (!direct_read && !evlist.empty()) {
		/* We've read from multiple regions, sort the event list by time. */
		EventsSortByTimeAndType<samplepos_t> cmp;
		evlist.sort (cmp);

		/* Copy ordered events from event list to dst. */
		for (Evoral::EventList<samplepos_t>::iterator e = evlist.begin(); e != evlist.end(); ++e) {
			Evoral::Event<samplepos_t>* ev (*e);
			dst.write (ev->time(), ev->event_type(), ev->size(), ev->buffer());
			delete ev;
		}
	}

	DEBUG_TRACE (DEBUG::MidiPlaylistIO, "---- End MidiPlaylist::read ----\n");
	_read_end = start + dur;
	return dur;
}

void
MidiPlaylist::reset_note_trackers ()
{
}

void
MidiPlaylist::resolve_note_trackers (Evoral::EventSink<samplepos_t>& dst, samplepos_t time)
{
}

void
MidiPlaylist::remove_dependents (boost::shared_ptr<Region> region)
{
}

void
MidiPlaylist::region_going_away (boost::weak_ptr<Region> region)
{
	boost::shared_ptr<Region> r = region.lock();
	if (r) {
		remove_dependents(r);
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
void
MidiPlaylist::_split_region (boost::shared_ptr<Region> region, const MusicSample& playlist_position)
{
	if (!region->covers (playlist_position.sample)) {
		return;
	}

	if (region->position() == playlist_position.sample ||
	    region->last_sample() == playlist_position.sample) {
		return;
	}

	boost::shared_ptr<const MidiRegion> mr = boost::dynamic_pointer_cast<MidiRegion>(region);

	if (mr == 0) {
		return;
	}

	boost::shared_ptr<Region> left;
	boost::shared_ptr<Region> right;

	string before_name;
	string after_name;
	const double before_qn = _session.tempo_map().exact_qn_at_sample (playlist_position.sample, playlist_position.division) - region->quarter_note();
	const double after_qn = mr->length_beats() - before_qn;
	MusicSample before (playlist_position.sample - region->position(), playlist_position.division);
	MusicSample after (region->length() - before.sample, playlist_position.division);

	/* split doesn't change anything about length, so don't try to splice */
	bool old_sp = _splicing;
	_splicing = true;

	RegionFactory::region_name (before_name, region->name(), false);

	{
		PropertyList plist;

		plist.add (Properties::length, before.sample);
		plist.add (Properties::length_beats, before_qn);
		plist.add (Properties::name, before_name);
		plist.add (Properties::left_of_split, true);
		plist.add (Properties::layering_index, region->layering_index ());
		plist.add (Properties::layer, region->layer ());

		/* note: we must use the version of ::create with an offset here,
		   since it supplies that offset to the Region constructor, which
		   is necessary to get audio region gain envelopes right.
		*/
		left = RegionFactory::create (region, MusicSample (0, 0), plist, true);
	}

	RegionFactory::region_name (after_name, region->name(), false);

	{
		PropertyList plist;

		plist.add (Properties::length, after.sample);
		plist.add (Properties::length_beats, after_qn);
		plist.add (Properties::name, after_name);
		plist.add (Properties::right_of_split, true);
		plist.add (Properties::layering_index, region->layering_index ());
		plist.add (Properties::layer, region->layer ());

		/* same note as above */
		right = RegionFactory::create (region, before, plist, true);
	}

	add_region_internal (left, region->position(), 0, region->quarter_note(), true);
	add_region_internal (right, region->position() + before.sample, before.division, region->quarter_note() + before_qn, true);

	remove_region_internal (region);

	_splicing = old_sp;
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
			if (c->second->list()->size() > 0) {
				ret.insert(c->first);
			}
		}
	}

	return ret;
}

void
MidiPlaylist::render (MidiChannelFilter* filter)
{
	typedef pair<MidiStateTracker*,samplepos_t> TrackerInfo;

	Playlist::RegionReadLock rl (this);

	DEBUG_TRACE (DEBUG::MidiPlaylistIO, "---- MidiPlaylist::dump-----\n");

	std::vector< boost::shared_ptr<Region> > regs;

	for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {

		/* check for the case of solo_selection */

		if (_session.solo_selection_active() && SoloSelectedActive() && !SoloSelectedListIncludes ((const Region*) &(**i))) {
			continue;
		}

		regs.push_back (*i);
	}

	/* If we are reading from a single region, we can read directly into _rendered.  Otherwise,
	   we read into a temporarily list, sort it, then write that to _rendered.
	*/
	Evoral::EventList<samplepos_t>  evlist;
	Evoral::EventSink<samplepos_t>* tgt;

	/* RAII */
	RTMidiBuffer::WriteProtectRender wpr (_rendered);

	if (regs.size() == 1) {
		tgt = &_rendered;
		wpr.acquire ();
		_rendered.clear ();
	} else {
		tgt = &evlist;
	}

	DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("\t%1 regions to read, direct: %2\n", regs.size(), (regs.size() == 1)));

	for (vector<boost::shared_ptr<Region> >::iterator i = regs.begin(); i != regs.end(); ++i) {

		boost::shared_ptr<MidiRegion> mr = boost::dynamic_pointer_cast<MidiRegion>(*i);

		if (!mr) {
			continue;
		}

		DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("dump from %1 at %2\n", mr->name()));
		mr->render (*tgt, 0, _note_mode, filter);
	}

	if (!evlist.empty()) {
		/* We've read from multiple regions into evlist, sort the event list by time. */
		EventsSortByTimeAndType<samplepos_t> cmp;
		evlist.sort (cmp);

		/* Copy ordered events from event list to _rendered. */

		wpr.acquire ();
		_rendered.clear ();

		for (Evoral::EventList<samplepos_t>::iterator e = evlist.begin(); e != evlist.end(); ++e) {
			Evoral::Event<samplepos_t>* ev (*e);
			_rendered.write (ev->time(), ev->event_type(), ev->size(), ev->buffer());
			delete ev;
		}
	}


	/* no need to release - RAII with WriteProtectRender takes care of it */

	DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("---- End MidiPlaylist::render, events: %1\n", _rendered.size()));
}

RTMidiBuffer*
MidiPlaylist::rendered ()
{
	return &_rendered;
}
