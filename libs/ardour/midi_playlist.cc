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

#include "evoral/EventList.h"
#include "evoral/Control.h"

#include "ardour/debug.h"
#include "ardour/midi_model.h"
#include "ardour/midi_playlist.h"
#include "ardour/midi_region.h"
#include "ardour/midi_source.h"
#include "ardour/midi_state_tracker.h"
#include "ardour/region_factory.h"
#include "ardour/region_sorters.h"
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
{
}

MidiPlaylist::MidiPlaylist (boost::shared_ptr<const MidiPlaylist> other, string name, bool hidden)
	: Playlist (other, name, hidden)
	, _note_mode(other->_note_mode)
{
}

MidiPlaylist::MidiPlaylist (boost::shared_ptr<const MidiPlaylist> other,
                            timepos_t  const &                    start,
                            timepos_t  const &                    dur,
                            string                                name,
                            bool                                  hidden)
	: Playlist (other, start, dur, name, hidden)
	, _note_mode(other->_note_mode)
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
MidiPlaylist::_split_region (boost::shared_ptr<Region> region, timepos_t const & playlist_position, ThawList& thawlist)
{
	if (!region->covers (playlist_position)) {
		return;
	}

	if (region->position() == playlist_position ||
	    region->nt_last() == playlist_position) {
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

	const timecnt_t before = region->position().distance (playlist_position);
	const timecnt_t after = region->length() - before;

	RegionFactory::region_name (before_name, region->name(), false);

	{
		PropertyList plist;

		plist.add (Properties::length, before);
		plist.add (Properties::name, before_name);
		plist.add (Properties::left_of_split, true);
		plist.add (Properties::layering_index, region->layering_index ());
		plist.add (Properties::layer, region->layer ());

		/* note: we must use the version of ::create with an offset here,
		   since it supplies that offset to the Region constructor, which
		   is necessary to get audio region gain envelopes right.
		*/
	        left = RegionFactory::create (region, plist, true, &thawlist);
	}

	RegionFactory::region_name (after_name, region->name(), false);

	{
		PropertyList plist;

		plist.add (Properties::length, after);
		plist.add (Properties::name, after_name);
		plist.add (Properties::right_of_split, true);
		plist.add (Properties::layering_index, region->layering_index ());
		plist.add (Properties::layer, region->layer ());

		/* same note as above */
		right = RegionFactory::create (region, before, plist, true, &thawlist);
	}

	add_region_internal (left, region->position(), thawlist);
	add_region_internal (right, region->position() + before, thawlist);

	remove_region_internal (region, thawlist);
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
	typedef pair<MidiNoteTracker*,samplepos_t> TrackerInfo;

	Playlist::RegionReadLock rl (this);

	DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("---- MidiPlaylist::render (regions: %1)-----\n", regions.size()));

	std::vector< boost::shared_ptr<Region> > regs;

	for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {

		/* check for the case of solo_selection */

		if (_session.solo_selection_active() && SoloSelectedActive() && !SoloSelectedListIncludes ((const Region*) &(**i))) {
			continue;
		}

		if ((*i)->muted()) {
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

	if (regs.empty()) {
		wpr.acquire ();
		_rendered.clear ();
	} else {

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

			DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("render from %1\n", mr->name()));
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
	}

	/* no need to release - RAII with WriteProtectRender takes care of it */

	DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("---- End MidiPlaylist::render, events: %1\n", _rendered.size()));
}

RTMidiBuffer*
MidiPlaylist::rendered ()
{
	return &_rendered;
}

boost::shared_ptr<Region>
MidiPlaylist::combine (RegionList const & rl)
{
	RegionWriteLock rwl (this, true);

	std::cerr << "combine " << rl.size() << " regions\n";

	if (rl.size() < 2) {
		return boost::shared_ptr<Region> ();
	}

	RegionList sorted (rl);
	sorted.sort (RegionSortByLayerAndPosition());

	boost::shared_ptr<Region> first = sorted.front();
	RegionList::const_iterator i = sorted.begin();
	++i;

#ifndef NDEBUG
	for (auto const & r : rl) {
		assert (boost::dynamic_pointer_cast<MidiRegion> (r));
	}
#endif

	boost::shared_ptr<MidiRegion> new_region = boost::dynamic_pointer_cast<MidiRegion> (RegionFactory::create (first, true, true, &rwl.thawlist));

	timepos_t pos (first->position());

	remove_region_internal (first, rwl.thawlist);
	std::cerr << "Remove " << first->name() << std::endl;

	while (i != sorted.end()) {
		new_region->merge (boost::dynamic_pointer_cast<MidiRegion> (*i));
		remove_region_internal (*i, rwl.thawlist);
		std::cerr << "Remove " << (*i)->name() << std::endl;
		++i;
	}

	add_region_internal (new_region, pos, rwl.thawlist);
	std::cerr << "Add " << new_region->name() << std::endl;

	return new_region;
}

void
MidiPlaylist::uncombine (boost::shared_ptr<Region> r)
{
}
