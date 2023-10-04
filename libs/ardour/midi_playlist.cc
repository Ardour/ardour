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

MidiPlaylist::MidiPlaylist (std::shared_ptr<const MidiPlaylist> other, string name, bool hidden)
	: Playlist (other, name, hidden)
	, _note_mode(other->_note_mode)
{
}

MidiPlaylist::MidiPlaylist (std::shared_ptr<const MidiPlaylist> other,
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

template <typename Time>
struct EventsSortByTimeAndType {
	bool operator() (const Evoral::Event<Time>* a, const Evoral::Event<Time>* b)
	{
		if (a->time () == b->time ()) {
			if (a->is_midi () && b->is_midi ()) {
				/* negate return value since we must return whether
				 * or not a should sort before b, not b before a
				 */
				return !MidiBuffer::second_simultaneous_midi_byte_is_first (a->buffer ()[0], b->buffer ()[0]);
			}
			return a->type () < b->type ();
		}
		return a->time () < b->time ();
	}
};

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
	std::shared_ptr<Region> r;

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
MidiPlaylist::destroy_region (std::shared_ptr<Region> region)
{
	std::shared_ptr<MidiRegion> r = std::dynamic_pointer_cast<MidiRegion> (region);

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
MidiPlaylist::_split_region (std::shared_ptr<Region> region, timepos_t const & playlist_position, ThawList& thawlist)
{
	if (!region->covers (playlist_position)) {
		return;
	}

	if (region->position() == playlist_position ||
	    region->nt_last() == playlist_position) {
		return;
	}

	std::shared_ptr<const MidiRegion> mr = std::dynamic_pointer_cast<MidiRegion>(region);

	if (mr == 0) {
		return;
	}

	std::shared_ptr<Region> left;
	std::shared_ptr<Region> right;

	string before_name;
	string after_name;

	const timecnt_t before = region->position().distance (playlist_position);
	const timecnt_t after = region->length() - before;

	RegionFactory::region_name (before_name, region->name(), false);

	{
		PropertyList plist (region->derive_properties (false));

		plist.add (Properties::length, before);
		plist.add (Properties::name, before_name);
		plist.add (Properties::left_of_split, true);

		/* note: we must use the version of ::create with an offset here,
		   since it supplies that offset to the Region constructor, which
		   is necessary to get audio region gain envelopes right.
		*/
		left = RegionFactory::create (region, plist, true, &thawlist);
	}

	RegionFactory::region_name (after_name, region->name(), false);

	{
		PropertyList plist (region->derive_properties (false));

		plist.add (Properties::length, after);
		plist.add (Properties::name, after_name);
		plist.add (Properties::right_of_split, true);
		plist.add (Properties::reg_group, Region::get_region_operation_group_id (region->region_group(), RightOfSplit));

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
		std::shared_ptr<MidiRegion> mr = std::dynamic_pointer_cast<MidiRegion>(*r);

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
	Playlist::RegionReadLock rl (this);

	DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("---- MidiPlaylist::render (regions: %1)-----\n", regions.size()));

	std::list<std::shared_ptr<MidiRegion>> regs;

	for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {

		/* check for the case of solo_selection */

		if (_session.solo_selection_active() && SoloSelectedActive() && !SoloSelectedListIncludes ((const Region*) &(**i))) {
			continue;
		}

		if ((*i)->muted()) {
			continue;
		}

		std::shared_ptr<MidiRegion> mr = std::dynamic_pointer_cast<MidiRegion> (*i);
		if (!mr) {
			continue;
		}

		regs.push_back (mr);
	}

	/* RAII */
	RTMidiBuffer::WriteProtectRender wpr (_rendered);

	if (regs.empty()) {
		wpr.acquire ();
		_rendered.clear ();
		DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("---- End MidiPlaylist::render, events: %1\n", _rendered.size()));
		return;
	}

	if (regs.size() == 1) {
		wpr.acquire ();
		_rendered.clear ();
		std::shared_ptr<MidiRegion> mr = regs.front ();
		DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("render from %1\n", mr->name()));
		mr->render (_rendered, 0, _note_mode, filter);
		DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("---- End MidiPlaylist::render, events: %1\n", _rendered.size()));
		return;
	}

	RegionSortByLayer cmp;
	regs.sort (cmp);

	bool all_transparent = true;
	bool no_layers = true;

	layer_t layer = regs.front()->layer ();

	/* skip bottom-most region, transparency is irrelevant */
	for (auto i = ++regs.begin(); i != regs.end(); ++i) {
		if ((*i)->opaque ()) {
			all_transparent = false;
		}
		if ((*i)->layer () != layer) {
			no_layers = false;
		}
		if (!all_transparent && !no_layers) {
			/* no need to check further */
			break;
		}
	}

	Evoral::EventList<samplepos_t> evlist;

	if (all_transparent || no_layers) {

		DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("\t%1 regions to read\n", regs.size()));

		for (auto i = regs.rbegin(); i != regs.rend(); ++i) {
			std::shared_ptr<MidiRegion> mr = *i;
			DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("render from %1\n", mr->name()));
			mr->render (evlist, 0, _note_mode, filter);
		}
		EventsSortByTimeAndType<samplepos_t> cmp;
		evlist.sort (cmp);

	} else {

		DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("\t%1 layered regions to read\n", regs.size()));

		bool top = true;
		std::vector<samplepos_t> bounds;
		EventsSortByTimeAndType<samplepos_t> cmp;

		/* iterate, top-most region first */
		for (auto i = regs.rbegin(); i != regs.rend(); ++i) {
			std::shared_ptr<MidiRegion> mr = *i;
			DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("maybe render from %1\n", mr->name()));

			if (top) {
				/* render topmost region as-is */
				mr->render (evlist, 0, _note_mode, filter);
				top = false;
			} else {
				Evoral::EventList<samplepos_t> tmp;
				mr->render (tmp, 0, _note_mode, filter);

				/* insert region-bound markers of opaque regions above */
				for (auto const& p : bounds) {
					tmp.write (p, Evoral::NO_EVENT, 0, 0);
				}
				tmp.sort (cmp);

				MidiStateTracker mtr;
				Evoral::EventList<samplepos_t> const slist (evlist);

				for (Evoral::EventList<samplepos_t>::iterator e = tmp.begin(); e != tmp.end(); ++e) {
					Evoral::Event<samplepos_t>* ev (*e);
					timepos_t t (ev->time());

					if (ev->event_type () == Evoral::NO_EVENT) {
						/* reached region bound of an opaque region above this region. */
						mtr.resolve_state (evlist, slist, ev->time());
					} else if (region_is_audible_at (mr, t)) {
						/* no opaque region above this event */
						uint8_t* evbuf = ev->buffer();
						if (3 == ev->size() && (evbuf[0] & 0xf0) == MIDI_CMD_NOTE_OFF && !mtr.active (evbuf[1], evbuf[0] & 0x0f)) {
							; /* skip note off */
						} else {
							evlist.write (ev->time(), ev->event_type(), ev->size(), evbuf);
							mtr.track (evbuf);
						}
					} else {
						/* there is an opaque region above this event, skip this event. */
					}
					delete ev;
				}
			}

			if (mr->opaque ()) {
				bounds.push_back (mr->position ().samples ());
			}

			EventsSortByTimeAndType<samplepos_t> cmp;
			evlist.sort (cmp);
		}
	}

	wpr.acquire ();
	_rendered.clear ();

	/* Copy ordered events from event list to _rendered. */
	for (Evoral::EventList<samplepos_t>::iterator e = evlist.begin(); e != evlist.end(); ++e) {
		Evoral::Event<samplepos_t>* ev (*e);
		_rendered.write (ev->time(), ev->event_type(), ev->size(), ev->buffer());
		delete ev;
	}

	DEBUG_TRACE (DEBUG::MidiPlaylistIO, string_compose ("---- End MidiPlaylist::render, events: %1\n", _rendered.size()));
}

RTMidiBuffer*
MidiPlaylist::rendered ()
{
	return &_rendered;
}

std::shared_ptr<Region>
MidiPlaylist::combine (RegionList const & rl, std::shared_ptr<Track> trk)
{
	RegionWriteLock rwl (this, true);

	if (rl.size() < 2) {
		return std::shared_ptr<Region> ();
	}

	RegionList sorted (rl);
	sorted.sort (RegionSortByLayerAndPosition());

	std::shared_ptr<Region> first = sorted.front();

	std::shared_ptr<MidiSource> ms (session().create_midi_source_by_stealing_name (trk));
	std::shared_ptr<MidiRegion> new_region = std::dynamic_pointer_cast<MidiRegion> (RegionFactory::create (ms, first->derive_properties (false), true, &rwl.thawlist));

	timepos_t earliest (first->position ());
	timepos_t latest (Temporal::BeatTime);

	new_region->set_position (earliest);

	for (auto const & r : sorted) {
		if (r->position() < earliest) {
			earliest = r->position();
		}
		if (r->end() > latest) {
			latest = r->end();
		}
		/* We need to explicit set the end, to terminate any MIDI notes
		 * that extend beyond the end of the region at the region boundary.
		 */
		new_region->set_length_unchecked (earliest.distance (r->end()));
		new_region->merge (std::dynamic_pointer_cast<MidiRegion> (r));
		remove_region_internal (r, rwl.thawlist);
	}

	new_region->set_length_unchecked (earliest.distance (latest));

	/* thin automation.
	 * Combining MIDI regions plays back automation, the compound
	 * will have individual points just like automation was played back
	 * and recorded. So it has to be thinned it like after a write-pass.
	 */
	for (auto& l: new_region->model ()->controls()) {
		l.second->list()->thin (Config->get_automation_thinning_factor ());
	}

	/* write MIDI to disk */

	new_region->midi_source (0)->session_saved ();

	add_region_internal (new_region, earliest, rwl.thawlist);

	return new_region;
}

void
MidiPlaylist::uncombine (std::shared_ptr<Region> r)
{
}
