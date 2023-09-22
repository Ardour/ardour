/*
 * Copyright (C) 2005-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006-2016 David Robillard <d@drobilla.net>
 * Copyright (C) 2006 Sampo Savolainen <v2@iki.fi>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2018 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2014-2018 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015-2017 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2015 André Nusser <andre.nusser@googlemail.com>
 * Copyright (C) 2016-2017 Tim Mayberry <mojofunk@gmail.com>
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
#include <set>
#include <stdint.h>
#include <string>

#include <glibmm/datetime.h>

#include "pbd/stateful_diff_command.h"
#include "pbd/strsplit.h"
#include "pbd/types_convert.h"
#include "pbd/unwind.h"
#include "pbd/xml++.h"

#include "ardour/debug.h"
#include "ardour/midi_region.h"
#include "ardour/playlist.h"
#include "ardour/playlist_factory.h"
#include "ardour/playlist_source.h"
#include "ardour/region.h"
#include "ardour/region_factory.h"
#include "ardour/region_sorters.h"
#include "ardour/session.h"
#include "ardour/session_playlists.h"
#include "ardour/source_factory.h"
#include "ardour/tempo.h"
#include "ardour/transient_detector.h"
#include "ardour/types_convert.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

namespace ARDOUR {
	namespace Properties {
		PBD::PropertyDescriptor<bool> regions;
	}
}

struct ShowMeTheList {
	ShowMeTheList (std::shared_ptr<Playlist> pl, const string& n)
		: playlist (pl)
		, name (n)
	{}

	~ShowMeTheList ()
	{
		cerr << ">>>>" << name << endl;
		playlist->dump ();
		cerr << "<<<<" << name << endl << endl;
	};

	std::shared_ptr<Playlist> playlist;
	string                      name;
};

void
Playlist::make_property_quarks ()
{
	Properties::regions.property_id = g_quark_from_static_string (X_("regions"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for regions = %1\n",
	                                                Properties::regions.property_id));
}

RegionListProperty::RegionListProperty (Playlist& pl)
	: SequenceProperty<std::list<std::shared_ptr<Region> > > (Properties::regions.property_id, boost::bind (&Playlist::update, &pl, _1))
	, _playlist (pl)
{
}

RegionListProperty::RegionListProperty (RegionListProperty const& p)
	: PBD::SequenceProperty<std::list<std::shared_ptr<Region> > > (p)
	, _playlist (p._playlist)
{
}

RegionListProperty*
RegionListProperty::clone () const
{
	return new RegionListProperty (*this);
}

RegionListProperty*
RegionListProperty::create () const
{
	return new RegionListProperty (_playlist);
}

void
RegionListProperty::get_content_as_xml (std::shared_ptr<Region> region, XMLNode& node) const
{
	/* All regions (even those which are deleted) have their state
	 * saved by other code, so we can just store ID here.
	 */

	node.set_property ("id", region->id ());
}

std::shared_ptr<Region>
RegionListProperty::get_content_from_xml (XMLNode const& node) const
{
	PBD::ID id;
	if (!node.get_property ("id", id)) {
		assert (false);
	}

	std::shared_ptr<Region> ret = _playlist.region_by_id (id);

	if (!ret) {
		ret = RegionFactory::region_by_id (id);
	}

	return ret;
}

Playlist::Playlist (Session& sess, string nom, DataType type, bool hide)
	: SessionObject (sess, nom)
	, Temporal::TimeDomainProvider ((type == DataType::AUDIO ? Temporal::AudioTime : Temporal::BeatTime), sess)
	, regions (*this)
	, _type (type)
{
	init (hide);
	first_set_state = false;
	_name           = nom;
	_set_sort_id ();
}

Playlist::Playlist (Session& sess, const XMLNode& node, DataType type, bool hide)
	: SessionObject (sess, "unnamed playlist")
	, Temporal::TimeDomainProvider ((type == DataType::AUDIO ? Temporal::AudioTime : Temporal::BeatTime), sess)
	, regions (*this)
	, _type (type)
{
#ifndef NDEBUG
	XMLProperty const* prop = node.property ("type");
	assert (!prop || DataType (prop->value ()) == _type);
#endif

	init (hide);
	_name = "unnamed"; /* reset by set_state */
	_set_sort_id ();

	/* set state called by derived class */
}

Playlist::Playlist (std::shared_ptr<const Playlist> other, string namestr, bool hide)
	: SessionObject (other->_session, namestr)
	, Temporal::TimeDomainProvider ((other->_type == DataType::AUDIO ? Temporal::AudioTime : Temporal::BeatTime), other->_session)
	, regions (*this)
	, _type (other->_type)
	, _orig_track_id (other->_orig_track_id)
	, _shared_with_ids (other->_shared_with_ids)
{
	init (hide);

	RegionList tmp;
	ThawList   thawlist;
	other->copy_regions (tmp);

	in_set_state++;

	for (list<std::shared_ptr<Region> >::iterator x = tmp.begin(); x != tmp.end(); ++x) {
		add_region_internal ((*x), (*x)->position(), thawlist);
	}
	thawlist.release ();

	in_set_state--;

	_rippling  = other->_rippling;
	_nudging   = other->_nudging;

	in_set_state    = 0;
	first_set_state = false;
	in_flush        = false;
	in_partition    = false;
	subcnt          = 0;
	_frozen         = other->_frozen;
}

Playlist::Playlist (std::shared_ptr<const Playlist> other, timepos_t const & start, timepos_t const & cnt, string str, bool hide)
	: SessionObject(other->_session, str)
	, Temporal::TimeDomainProvider ((other->_type == DataType::AUDIO ? Temporal::AudioTime : Temporal::BeatTime), other->_session)
	, regions (*this)
	, _type (other->_type)
	, _orig_track_id (other->_orig_track_id)
	, _shared_with_ids (other->_shared_with_ids)
{
	RegionReadLock rlock2 (const_cast<Playlist*> (other.get ()));

	timepos_t end = timepos_t (start + cnt);

	init (hide);

	in_set_state++;

	{
		RegionWriteLock rlock (this);
		for (auto const& region : other->regions) {
			std::shared_ptr<Region> new_region;
			timecnt_t offset;
			timepos_t position;
			timecnt_t len;
			string    new_name;
			Temporal::OverlapType overlap;

			overlap = region->coverage (start, end);

			switch (overlap) {
				case Temporal::OverlapNone:
					continue;

				case Temporal::OverlapInternal:
					offset = region->position().distance (start);
					position = timepos_t(start.time_domain());
					len = timecnt_t (cnt);
					break;

				case Temporal::OverlapStart:
					offset = timecnt_t (start.time_domain());
					position = start.distance(region->position());
					len = region->position().distance (end);
					break;

				case Temporal::OverlapEnd:
					offset = region->position().distance (start);
					position = timepos_t (start.time_domain());
					len = region->length() - offset;
					break;

				case Temporal::OverlapExternal:
					offset = timecnt_t (start.time_domain());
					position = start.distance(region->position());
					len = region->length();
					break;
			}

			RegionFactory::region_name (new_name, region->name (), false);

			PropertyList plist (region->derive_properties ());

			plist.add (Properties::start, region->start() + offset);
			plist.add (Properties::length, len);
			plist.add (Properties::name, new_name);

			new_region = RegionFactory::create (region, offset, plist, true, &rlock.thawlist);

			add_region_internal (new_region, position, rlock.thawlist);
		}
	}


	/* keep track of any dead space at end (for pasting into Ripple or
	 * RippleAll mode) at the end of construction, any length of cnt beyond
	 * the extents of the regions is end_space
	 */
	_end_space = timecnt_t (cnt) - (get_extent().first.distance (get_extent().second));


	in_set_state--;
	first_set_state = false;
}

void
Playlist::use ()
{
	if (0 == _refcnt.fetch_add (1)) {
		InUse (true); /* EMIT SIGNAL */
	}
}

void
Playlist::release ()
{
	int oldval = _refcnt.fetch_sub (1);
#ifndef NDEBUG
	if (oldval <= 0) {
		cerr << "Bad Playlist::release for " << name() << endl;
	}
	assert (oldval > 0);
#endif

	if (oldval == 1) {
		InUse (false); /* EMIT SIGNAL */
	}
}

void
Playlist::copy_regions (RegionList& newlist) const
{
	RegionReadLock rlock (const_cast<Playlist*> (this));

	for (auto const & r : regions) {
		newlist.push_back (RegionFactory::create (r, true, true));
	}
}

void
Playlist::init (bool hide)
{
	add_property (regions);
	_xml_node_name = X_("Playlist");

	block_notifications.store (0);
	pending_contents_change     = false;
	pending_layering            = false;
	first_set_state             = true;
	_hidden                     = hide;
	_rippling                   = false;
	_shuffling                  = false;
	_nudging                    = false;
	in_set_state                = 0;
	in_undo                     = false;
	in_flush                    = false;
	in_partition                = false;
	subcnt                      = 0;
	_frozen                     = false;
	_capture_insertion_underway = false;
	_combine_ops                = 0;

	_refcnt.store (0);

	_end_space = timecnt_t (_type == DataType::AUDIO ? Temporal::AudioTime : Temporal::BeatTime);
	_playlist_shift_active = false;

	_session.history ().BeginUndoRedo.connect_same_thread (*this, boost::bind (&Playlist::begin_undo, this));
	_session.history ().EndUndoRedo.connect_same_thread (*this, boost::bind (&Playlist::end_undo, this));

	ContentsChanged.connect_same_thread (*this, boost::bind (&Playlist::mark_session_dirty, this));
}

Playlist::~Playlist ()
{
	DEBUG_TRACE (DEBUG::Destruction, string_compose ("Playlist %1 destructor\n", _name));

	{
		RegionReadLock rl (this);

		for (auto & r : all_regions) {
			r->set_playlist (std::shared_ptr<Playlist> ());
		}
	}

	/* GoingAway must be emitted by derived classes */
}

void
Playlist::_set_sort_id ()
{
	/* Playlists are given names like <track name>.<id>
	 * or <track name>.<edit group name>.<id> where id
	 * is an integer. We extract the id and sort by that.
	 */

	size_t dot_position = _name.val ().find_last_of (".");

	if (dot_position == string::npos) {
		_sort_id = 0;
	} else {
		string t = _name.val ().substr (dot_position + 1);

		if (!string_to_uint32 (t, _sort_id)) {
			_sort_id = 0;
		}
	}
}

bool
Playlist::set_name (const string& str)
{
	bool ret = SessionObject::set_name (str);
	if (ret) {
		_set_sort_id ();
	}
	return ret;
}

/***********************************************************************
 * CHANGE NOTIFICATION HANDLING
 *
 * Notifications must be delayed till the region_lock is released. This
 * is necessary because handlers for the signals may need to acquire
 * the lock (e.g. to read from the playlist).
 ***********************************************************************/

void
Playlist::begin_undo ()
{
	in_undo = true;
	freeze ();
}

void
Playlist::end_undo ()
{
	thaw (true);
	in_undo = false;
}

void
Playlist::freeze ()
{
	/* flush any ongoing reads, paricularly AudioPlaylist::read(),
	 * before beginning to modify the playlist.
	 */
	RegionWriteLock rlock (this, false);
	delay_notifications ();
}

/** @param from_undo true if this thaw is triggered by the end of an undo on this playlist */
void
Playlist::thaw (bool from_undo)
{
	release_notifications (from_undo);
}

void
Playlist::delay_notifications ()
{
	block_notifications.fetch_add (1);
}

/** @param from_undo true if this release is triggered by the end of an undo on this playlist */
void
Playlist::release_notifications (bool from_undo)
{
	if (PBD::atomic_dec_and_test (block_notifications)) {
		flush_notifications (from_undo);
	}
}

void
Playlist::notify_contents_changed ()
{
	if (holding_state ()) {
		pending_contents_change = true;
	} else {
		pending_contents_change = false;
		ContentsChanged (); /* EMIT SIGNAL */
	}
}

void
Playlist::notify_layering_changed ()
{
	if (holding_state ()) {
		pending_layering = true;
	} else {
		pending_layering = false;
		LayeringChanged (); /* EMIT SIGNAL */
	}
}

void
Playlist::notify_region_removed (std::shared_ptr<Region> r)
{
	if (holding_state ()) {
		pending_removes.insert (r);
		pending_contents_change = true;
	} else {
		/* this might not be true, but we have to act
		   as though it could be.
		*/
		pending_contents_change = false;
		RegionRemoved (std::weak_ptr<Region> (r)); /* EMIT SIGNAL */
		ContentsChanged ();                          /* EMIT SIGNAL */
	}
}

void
Playlist::notify_region_moved (std::shared_ptr<Region> r)
{
	Temporal::RangeMove move (r->last_position (), r->last_length (), r->position ());

	if (holding_state ()) {
		pending_range_moves.push_back (move);

	} else {
		list<Temporal::RangeMove> m;
		m.push_back (move);
		RangesMoved (m, false);
	}
}

void
Playlist::notify_region_start_trimmed (std::shared_ptr<Region> r)
{
	if (r->position() >= r->last_position()) {
		/* trimmed shorter */
		return;
	}

	Temporal::Range const extra (r->position(), r->last_position());

	if (holding_state ()) {
		pending_region_extensions.push_back (extra);

	} else {
		list<Temporal::Range> r;
		r.push_back (extra);
		RegionsExtended (r);
	}
}

void
Playlist::notify_region_end_trimmed (std::shared_ptr<Region> r)
{
	if (r->length() < r->last_length()) {
		/* trimmed shorter */
	}

	Temporal::Range const extra (r->position() + r->last_length(), r->position() + r->length());

	if (holding_state ()) {
		pending_region_extensions.push_back (extra);

	} else {
		list<Temporal::Range> r;
		r.push_back (extra);
		RegionsExtended (r);
	}
}

void
Playlist::notify_region_added (std::shared_ptr<Region> r)
{
	/* the length change might not be true, but we have to act
	 * as though it could be.
	 */

	if (holding_state ()) {
		pending_adds.insert (r);
		pending_contents_change = true;
	} else {
		r->clear_changes ();
		pending_contents_change = false;
		RegionAdded (std::weak_ptr<Region> (r)); /* EMIT SIGNAL */
		ContentsChanged ();                        /* EMIT SIGNAL */
		RegionFactory::CheckNewRegion (r);         /* EMIT SIGNAL */
	}
}

/** @param from_undo true if this flush is triggered by the end of an undo on this playlist */
void
Playlist::flush_notifications (bool from_undo)
{
	set<std::shared_ptr<Region> >::iterator s;
	bool regions_changed = false;

	if (in_flush) {
		return;
	}

	in_flush = true;

	if (!pending_bounds.empty () || !pending_removes.empty () || !pending_adds.empty ()) {
		regions_changed = true;
	}

	/* XXX: it'd be nice if we could use pending_bounds for
	   RegionsExtended and RegionsMoved.
	*/

	/* we have no idea what order the regions ended up in pending
	 * bounds (it could be based on selection order, for example).
	 * so, to preserve layering in the "most recently moved is higher"
	 * model, sort them by existing layer, then timestamp them.
	 */

	// RegionSortByLayer cmp;
	// pending_bounds.sort (cmp);

	list<Temporal::Range> crossfade_ranges;

	for (auto const & r : pending_bounds) {
		crossfade_ranges.push_back (r->last_range ());
		crossfade_ranges.push_back (r->range ());
	}

	std::shared_ptr<RegionList> rl (new RegionList);
	for (s = pending_removes.begin (); s != pending_removes.end (); ++s) {
		crossfade_ranges.push_back ((*s)->range ());
		RegionRemoved (std::weak_ptr<Region> (*s)); /* EMIT SIGNAL */
		rl->push_back (*s);
	}
	if (rl->size () > 0) {
		Region::RegionsPropertyChanged (rl, Properties::hidden);
	}

	for (s = pending_adds.begin (); s != pending_adds.end (); ++s) {
		crossfade_ranges.push_back ((*s)->range ());
		/* don't emit RegionAdded signal until relayering is done,
		   so that the region is fully setup by the time
		   anyone hears that its been added
		*/
	}

	/* notify about contents/region changes first so that layering changes
	 * in a UI will take place on the new contents.
	 */

	if (regions_changed || pending_contents_change) {
		pending_layering = true;
		ContentsChanged (); /* EMIT SIGNAL */
	}

	for (s = pending_adds.begin (); s != pending_adds.end (); ++s) {
		(*s)->clear_changes ();
		RegionAdded (std::weak_ptr<Region> (*s)); /* EMIT SIGNAL */
		RegionFactory::CheckNewRegion (*s);         /* EMIT SIGNAL */
	}

	if ((regions_changed && !in_set_state) || pending_layering) {
		relayer ();
	}

	coalesce_and_check_crossfades (crossfade_ranges);

	if (!pending_range_moves.empty ()) {
		/* We don't need to check crossfades for these as pending_bounds has
		   already covered it.
		*/
		RangesMoved (pending_range_moves, from_undo || _playlist_shift_active);
	}

	if (!pending_region_extensions.empty ()) {
		RegionsExtended (pending_region_extensions);
	}

	clear_pending ();

	in_flush = false;
}

void
Playlist::clear_pending ()
{
	pending_adds.clear ();
	pending_removes.clear ();
	pending_bounds.clear ();
	pending_range_moves.clear ();
	pending_region_extensions.clear ();
	pending_contents_change = false;
	pending_layering        = false;
}

void
Playlist::region_going_away (std::weak_ptr<Region> region)
{
	if (_session.deletion_in_progress ()) {
		return;
	}
	std::shared_ptr<Region> r = region.lock();
	if (r) {
		remove_region (r);
	}
}

/*************************************************************
 * PLAYLIST OPERATIONS
 *************************************************************/

/** Note: this calls set_layer (..., DBL_MAX) so it will reset the layering index of region */
void
Playlist::add_region (std::shared_ptr<Region> region, timepos_t const & position, float times, bool auto_partition)
{
	if (region->whole_file ()) {
		region = RegionFactory::create (region, region->derive_properties ());
	}

	RegionWriteLock rlock (this);
	times = fabs (times);

	int itimes = (int)floor (times);

	timepos_t pos = position;

	if (times == 1 && auto_partition) {
		partition_internal (pos.decrement(), (pos + region->length ()), true, rlock.thawlist);
		for (auto const & r : rlock.thawlist) {
			_session.add_command (new StatefulDiffCommand (r));
		}
	}

	if (itimes >= 1) {
		add_region_internal (region, pos, rlock.thawlist);
		set_layer (region, DBL_MAX);
		pos += region->length();
		--itimes;
	}

	/* note that itimes can be zero if we being asked to just
	 * insert a single fraction of the region.
	 */

	for (int i = 0; i < itimes; ++i) {
		std::shared_ptr<Region> copy = RegionFactory::create (region, true, false, &rlock.thawlist);
		add_region_internal (copy, pos, rlock.thawlist);
		set_layer (copy, DBL_MAX);
		pos += region->length();
	}

	timecnt_t length;

	if (floor (times) != times) {
		length = region->length().scale (times - floor (times));
		string name;
		RegionFactory::region_name (name, region->name (), false);

		{
			PropertyList plist (region->derive_properties ());

			plist.add (Properties::length, length);
			plist.add (Properties::name, name);

			std::shared_ptr<Region> sub = RegionFactory::create (region, plist, true, &rlock.thawlist);
			add_region_internal (sub, pos, rlock.thawlist);

			set_layer (sub, DBL_MAX);
		}
	}
}

void
Playlist::set_region_ownership ()
{
	RegionWriteLock           rl (this);
	RegionList::iterator      i;
	std::weak_ptr<Playlist> pl (shared_from_this ());

	for (i = regions.begin (); i != regions.end (); ++i) {
		(*i)->set_playlist (pl);
	}
}

bool
Playlist::add_region_internal (std::shared_ptr<Region> region, timepos_t const & position, ThawList& thawlist)
{
	if (region->data_type () != _type) {
		return false;
	}

	/* note, this will delay signal emission and trigger Playlist::region_changed_proxy
	 * via PropertyChanged subsciption below :(
	 */
	thawlist.add (region);

	RegionSortByPosition cmp;

	if (!first_set_state) {
		region->set_playlist (std::weak_ptr<Playlist> (shared_from_this()));
	}

	region->set_position_unchecked (position);
	region->set_position_time_domain (time_domain());

	regions.insert (upper_bound (regions.begin (), regions.end (), region, cmp), region);
	all_regions.insert (region);

	if (!holding_state ()) {
		/* layers get assigned from XML state, and are not reset during undo/redo */
		relayer ();
	}

	/* we need to notify the existence of new region before checking dependents. Ick. */

	notify_region_added (region);

	region->PropertyChanged.connect_same_thread (region_state_changed_connections, boost::bind (&Playlist::region_changed_proxy, this, _1, std::weak_ptr<Region> (region)));
	region->DropReferences.connect_same_thread (region_drop_references_connections, boost::bind (&Playlist::region_going_away, this, std::weak_ptr<Region> (region)));

	/* do not handle property changes of newly added regions.
	 * Otherwise this would trigger Playlist::notify_region_moved()
	 * -> RangesMoved() and move automation.
	 */
	region->clear_changes ();

	return true;
}

void
Playlist::replace_region (std::shared_ptr<Region> old, std::shared_ptr<Region> newr, timepos_t const & pos)
{
	if (newr->whole_file ()) {
		newr = RegionFactory::create (newr, newr->derive_properties ());
	}

	RegionWriteLock rlock (this);

	remove_region_internal (old, rlock.thawlist);
	add_region_internal (newr, pos, rlock.thawlist);
	set_layer (newr, old->layer ());
}

void
Playlist::remove_region (std::shared_ptr<Region> region)
{
	RegionWriteLock rlock (this);
	remove_region_internal (region, rlock.thawlist);
}

int
Playlist::remove_region_internal (std::shared_ptr<Region> region, ThawList& thawlist)
{
	RegionList::iterator i;

	if (!in_set_state) {
		/* unset playlist */
		region->set_playlist (std::weak_ptr<Playlist> ());
	}

	/* XXX should probably freeze here .... */

	for (i = regions.begin (); i != regions.end (); ++i) {
		if (*i == region) {

			regions.erase (i);

			if (!holding_state ()) {
				relayer ();
			}

			notify_region_removed (region);
			break;
		}
	}

#if 0
	for (set<std::shared_ptr<Region> >::iterator x = all_regions.begin(); x != all_regions.end(); ++x) {
		if ((*x) == region) {
			all_regions.erase (x);
			break;
		}
	}
#else /* sync_all_regions_with_regions */
	all_regions.clear ();
	for (auto const& r: regions) {
		all_regions.insert (r);
	}
#endif

	return -1;
}

void
Playlist::remove_gaps (timecnt_t const & gap_threshold, timecnt_t const & leave_gap, boost::function<void (timepos_t, timecnt_t)> gap_callback)
{
	bool closed = false;

	{
		RegionWriteLock rlock (this);
		RegionList::iterator i;
		RegionList::iterator nxt (regions.end());

		if (regions.size() < 2) {
			return;
		}

		for (i = regions.begin(); i != regions.end(); ++i) {

			nxt = i;
			++nxt;

			if (nxt == regions.end()) {
				break;
			}

			timepos_t end_of_this_region = (*i)->end();

			if (end_of_this_region >= (*nxt)->position()) {
				continue;
			}

			const timecnt_t gap = end_of_this_region.distance ((*nxt)->position());

			if (gap < gap_threshold) {
				continue;
			}

			const timecnt_t shift = gap - leave_gap;

			(void) ripple_unlocked ((*nxt)->position(), -shift, 0, rlock.thawlist);

			gap_callback ((*nxt)->position(), shift);

			closed = true;
		}
	}

	if (closed) {
		notify_contents_changed ();
	}
}

void
Playlist::get_equivalent_regions (std::shared_ptr<Region> basis, vector<std::shared_ptr<Region>>& results)
{
	if (basis->is_explicitly_ungrouped ()) {
		/*user explicitly ungrouped this region, so we bail */
		return;
	}

	if (basis->is_implicitly_ungrouped ()) {
		/* no group defined ... we must guess ... fallback to 'region equivalence' behavior */

		switch (Config->get_region_equivalence ()) {
			case Exact:
				for (auto const& r : regions) {
					if (r->exact_equivalent (basis)) {
						results.push_back (r);
					}
				}
				break;
			case LayerTime:
				for (auto const& r : regions) {
					if (r->layer_and_time_equivalent (basis)) {
						results.push_back (r);
					}
				}
				break;
			case Enclosed:
				for (auto const& r : regions) {
					if (r->enclosed_equivalent (basis)) {
						results.push_back (r);
					}
				}
				break;
			case Overlap:
				for (auto const& r : regions) {
					if (r->overlap_equivalent (basis)) {
						results.push_back (r);
					}
				}
				break;
		}
		return;
	}

	/* region has an implicit or explicit group-id; return all regions with the same group-id */
	for (auto const& r : regions) {
		if (r->region_group () == basis->region_group ()) {
			results.push_back (r);
		}
	}
}

void
Playlist::partition (timepos_t const & start, timepos_t const & end, bool cut)
{
	RegionWriteLock lock (this);
	partition_internal (start, end, cut, lock.thawlist);
}

/** Go through each region on the playlist and cut them at start and end, removing the section between
 *  start and end if cutting == true.  Regions that lie entirely within start and end are always
 *  removed.
 */
void
Playlist::partition_internal (timepos_t const & start, timepos_t const & end, bool cutting, ThawList& thawlist)
{
	RegionList new_regions;

	{
		std::shared_ptr<Region> region;
		std::shared_ptr<Region> current;
		string new_name;
		RegionList::iterator tmp;
		Temporal::OverlapType overlap;
		timepos_t pos1, pos2, pos3, pos4;

		in_partition = true;

		/* need to work from a copy, because otherwise the regions we add
		 * during the process get operated on as well.
		 */

		RegionList copy = regions.rlist ();

		for (RegionList::iterator i = copy.begin (); i != copy.end (); i = tmp) {
			tmp = i;
			++tmp;

			current = *i;

			if (start < current->position() && end >= current->nt_last()) {

				if (cutting) {
					remove_region_internal (current, thawlist);
				}

				continue;
			}

			/* coverage will return OverlapStart if the start coincides
			 * with the end point. we do not partition such a region,
			 * so catch this special case.
			 */

			if (end < current->position()) {
				continue;
			}

			if ((overlap = current->coverage (start, end)) == Temporal::OverlapNone) {
				continue;
			}

			pos1 = current->position();
			pos2 = start;
			pos3 = end;
			pos4 = current->nt_last ();

			if (overlap == Temporal::OverlapInternal) {
				/* split: we need 3 new regions, the front, middle and end.
				 * cut:   we need 2 regions, the front and end.
				 *
				 *
				 *                start                 end
				 * ---------------*************************------------
				 *                P1  P2              P3  P4
				 * SPLIT:
				 * ---------------*****++++++++++++++++====------------
				 * CUT
				 * ---------------*****----------------====------------
				 */

				if (!cutting) {
					/* "middle" ++++++ */

					RegionFactory::region_name (new_name, current->name (), false);

					PropertyList plist (current->derive_properties ());

					plist.add (Properties::start, current->start() + pos1.distance (pos2));
					plist.add (Properties::length, pos2.distance (pos3));
					plist.add (Properties::name, new_name);
					plist.add (Properties::automatic, true);
					plist.add (Properties::left_of_split, true);
					plist.add (Properties::right_of_split, true);
					plist.add (Properties::reg_group, Region::get_region_operation_group_id (current->region_group(), InnerSplit));

					/* see note in ::_split_region()
					 */
					region = RegionFactory::create (current, pos1.distance (pos2), plist, true, &thawlist);
					add_region_internal (region, start, thawlist);
					new_regions.push_back (region);
				}

				/* "end" ====== */

				RegionFactory::region_name (new_name, current->name (), false);

				PropertyList plist (current->derive_properties ());

				plist.add (Properties::start, current->start() + pos1.distance (pos3));
				plist.add (Properties::length, pos3.distance (pos4));
				plist.add (Properties::name, new_name);
				plist.add (Properties::automatic, true);
				plist.add (Properties::right_of_split, true);
				plist.add (Properties::reg_group, Region::get_region_operation_group_id (current->region_group(), RightOfSplit));

				region = RegionFactory::create (current, pos1.distance (pos3), plist, true, &thawlist );

				add_region_internal (region, end, thawlist);
				new_regions.push_back (region);

				/* "front" ***** */

				current->clear_changes ();
				thawlist.add (current);

				current->modify_end_unchecked (pos2.decrement(), true);

			} else if (overlap == Temporal::OverlapEnd) {

				/*
				 *              start           end
				 * ---------------*************************------------
				 * P1           P2         P4   P3
				 * SPLIT:
				 * ---------------**************+++++++++++------------
				 * CUT:
				 * ---------------**************-----------------------
				*/

				if (!cutting) {
					/* end +++++ */

					RegionFactory::region_name (new_name, current->name (), false);

					PropertyList plist (current->derive_properties ());

					plist.add (Properties::start, current->start() + pos1.distance (pos2));
					plist.add (Properties::length, pos2.distance (pos4));
					plist.add (Properties::name, new_name);
					plist.add (Properties::automatic, true);
					plist.add (Properties::left_of_split, true);

					region = RegionFactory::create (current, pos1.distance (pos2), plist, true, &thawlist);

					add_region_internal (region, start, thawlist);
					new_regions.push_back (region);
				}

				/* front ****** */

				current->clear_changes ();
				thawlist.add (current);

				current->modify_end_unchecked (pos2.decrement(), true);

			} else if (overlap == Temporal::OverlapStart) {

				/* split: we need 2 regions: the front and the end.
				 * cut: just trim current to skip the cut area
				 *
				 *
				 * start           end
				 * ---------------*************************------------
				 * P2          P1 P3                   P4
				 *
				 * SPLIT:
				 * ---------------****+++++++++++++++++++++------------
				 * CUT:
				 * -------------------*********************------------
				 */

				if (!cutting) {
					/* front **** */
					RegionFactory::region_name (new_name, current->name (), false);

					PropertyList plist (current->derive_properties ());

					plist.add (Properties::length, pos1.distance (pos3));
					plist.add (Properties::name, new_name);
					plist.add (Properties::automatic, true);
					plist.add (Properties::right_of_split, true);
					plist.add (Properties::reg_group, Region::get_region_operation_group_id (current->region_group(), RightOfSplit));

					region = RegionFactory::create (current, plist, true, &thawlist);

					add_region_internal (region, pos1, thawlist);
					new_regions.push_back (region);
				}

				/* end */

				current->clear_changes ();
				thawlist.add (current);
				current->modify_front_unchecked (pos3, false);

			} else if (overlap == Temporal::OverlapExternal) {

				/* split: no split required.
				 * cut: remove the region.
				 *
				 *
				 * start                                      end
				 * ---------------*************************------------
				 * P2          P1 P3                   P4
				 *
				 *
				 * SPLIT:
				 * ---------------*************************------------
				 * CUT:
				 * ----------------------------------------------------
				 *
				 */

				if (cutting) {
					remove_region_internal (current, thawlist);
				}

				new_regions.push_back (current);
			}
		}

		in_partition = false;
	}

	/* keep track of any dead space at end (for pasting into Ripple or RippleAll mode) */
	const timecnt_t wanted_length = start.distance (end);
	_end_space = wanted_length - _get_extent().first.distance (_get_extent().second);
	// cout << "PL: " << name() << " END SPACE: " << _end_space << " WANTED LEN: " << wanted_length << " EXT: " << _get_extent().first << " to " << _get_extent().second << "\n";
}

std::shared_ptr<Playlist>
Playlist::cut_copy (std::shared_ptr<Playlist> (Playlist::*pmf)(timepos_t const &, timecnt_t const &), list<TimelineRange>& ranges)
{
	std::shared_ptr<Playlist> ret;
	std::shared_ptr<Playlist> pl;
	timepos_t start;

	if (ranges.empty ()) {
		return std::shared_ptr<Playlist> ();
	}

	start = ranges.front().start();

	for (list<TimelineRange>::iterator i = ranges.begin(); i != ranges.end(); ++i) {

		pl = (this->*pmf)((*i).start(), (*i).length());

		if (i == ranges.begin ()) {
			ret = pl;
		} else {
			/* paste the next section into the nascent playlist,
			 * offset to reflect the start of the first range we
			 * chopped.
			 */

			ret->paste (pl, (*i).start().earlier (timecnt_t (start, start)), 1.0f);
		}
	}

	return ret;
}

std::shared_ptr<Playlist>
Playlist::cut (list<TimelineRange>& ranges)
{
	std::shared_ptr<Playlist> (Playlist::*pmf) (timepos_t const & , timecnt_t const &) = &Playlist::cut;
	return cut_copy (pmf, ranges);
}

std::shared_ptr<Playlist>
Playlist::copy (list<TimelineRange>& ranges )
{
	std::shared_ptr<Playlist> (Playlist::*pmf) (timepos_t const &, timecnt_t const &) = &Playlist::copy;
	return cut_copy (pmf, ranges);
}

std::shared_ptr<Playlist>
Playlist::cut (timepos_t const & start, timecnt_t const & cnt)
{
	std::shared_ptr<Playlist> the_copy;
	char                        buf[32];

	snprintf (buf, sizeof (buf), "%" PRIu32, ++subcnt);
	string new_name = _name;
	new_name += '.';
	new_name += buf;

	if ((the_copy = PlaylistFactory::create (shared_from_this(), start, timepos_t (cnt), new_name, true)) == 0) {
		return std::shared_ptr<Playlist>();
	}

	{
		RegionWriteLock rlock (this);
		partition_internal (start, (start+cnt).decrement(), true, rlock.thawlist);
	}

	return the_copy;
}

std::shared_ptr<Playlist>
Playlist::copy (timepos_t const & start, timecnt_t const & cnt)
{
	char buf[32];

	snprintf (buf, sizeof (buf), "%" PRIu32, ++subcnt);
	string new_name = _name;
	new_name += '.';
	new_name += buf;

	// cnt = min (_get_extent().second - start, cnt);  (We need the full range length when copy/pasting in Ripple.  Why was this limit here?  It's not in CUT... )

	return PlaylistFactory::create (shared_from_this (), start, timepos_t (cnt), new_name, true);
}

int
Playlist::paste (std::shared_ptr<Playlist> other, timepos_t const & position, float times)
{
	times = fabs (times);

	{
		RegionReadLock rl2 (other.get ());

		int itimes = (int) floor (times);
		timepos_t pos = position;
		timecnt_t const shift (other->_get_extent().second, other->_get_extent().first);
		layer_t top = top_layer () + 1;

		{
			RegionWriteLock rl1 (this);
			while (itimes--) {
				for (auto const & r : other->regions) {
					std::shared_ptr<Region> copy_of_region = RegionFactory::create (r, true, false, &rl1.thawlist);

					/* we want newly-pasted regions to share one (implicit) group-id */
					copy_of_region->set_region_group(false);

					/* put these new regions on top of all existing ones, but preserve
					   the ordering they had in the original playlist.
					*/

					add_region_internal (copy_of_region, r->position() + pos, rl1.thawlist);
					set_layer (copy_of_region, copy_of_region->layer() + top);
				}
				pos += shift;
			}
		}
	}
	return 0;
}

void
Playlist::duplicate (std::shared_ptr<Region> region, timepos_t & position, float times)
{
	duplicate(region, position, region->length(), times);
}

/** @param gap from the beginning of the region to the next beginning */
void
Playlist::duplicate (std::shared_ptr<Region> region, timepos_t & position, timecnt_t const & gap, float times)
{
	times = fabs (times);

	RegionWriteLock rl (this);
	int             itimes = (int)floor (times);

	while (itimes--) {
		std::shared_ptr<Region> copy = RegionFactory::create (region, true, false, &rl.thawlist);
		add_region_internal (copy, position, rl.thawlist);
		set_layer (copy, DBL_MAX);
		position += gap;
	}

	if (floor (times) != times) {
		timecnt_t length = region->length().scale (times - floor (times));
		string name;
		RegionFactory::region_name (name, region->name(), false);

		{
			PropertyList plist (region->derive_properties ());
			plist.add (Properties::length, length);

			std::shared_ptr<Region> sub = RegionFactory::create (region, plist, true, &rl.thawlist);
			add_region_internal (sub, position, rl.thawlist);
			set_layer (sub, DBL_MAX);
		}
	}
}

/** @param gap from the beginning of the region to the next beginning */
/** @param end the first sample that does _not_ contain a duplicated sample */
void
Playlist::duplicate_until (std::shared_ptr<Region> region, timepos_t & position, timecnt_t const & gap, timepos_t const & end)
{
	RegionWriteLock rl (this);

	while ((position + region->length()).decrement() < end) {
		std::shared_ptr<Region> copy = RegionFactory::create (region, true, false, &rl.thawlist);
		add_region_internal (copy, position, rl.thawlist);
		set_layer (copy, DBL_MAX);
		position += gap;
	}
	if (position < end) {
		timecnt_t length = min (region->length(), position.distance (end));
		string name;
		RegionFactory::region_name (name, region->name(), false);

		{
			PropertyList plist (region->derive_properties ());
			plist.add (Properties::length, length);

			std::shared_ptr<Region> sub = RegionFactory::create (region, plist, false, &rl.thawlist);
			add_region_internal (sub, position, rl.thawlist);
			set_layer (sub, DBL_MAX);
		}
	}
}

void
Playlist::duplicate_range (TimelineRange& range, float times)
{
	std::shared_ptr<Playlist> pl = copy (range.start(), range.length());
	paste (pl, range.end(), times);
}

void
Playlist::duplicate_ranges (std::list<TimelineRange>& ranges, float times)
{
	if (ranges.empty ()) {
		return;
	}

	timepos_t min_pos = timepos_t::max (ranges.front().start().time_domain());
	timepos_t max_pos = timepos_t (min_pos.time_domain());

	for (std::list<TimelineRange>::const_iterator i = ranges.begin();
	     i != ranges.end();
	     ++i) {
		min_pos = min (min_pos, (*i).start());
		max_pos = max (max_pos, (*i).end());
	}

	timecnt_t offset = min_pos.distance (max_pos);

	int count  = 1;
	int itimes = (int)floor (times);
	while (itimes--) {
		for (list<TimelineRange>::iterator i = ranges.begin (); i != ranges.end (); ++i) {
			std::shared_ptr<Playlist> pl = copy ((*i).start(), (*i).length ());
			paste (pl, (*i).start() + (offset.scale (count)), 1.0f);
		}
		++count;
	}
}

void
Playlist::shift (timepos_t const & at, timecnt_t const & distance, bool move_intersected, bool ignore_music_glue)
{
	PBD::Unwinder<bool> uw (_playlist_shift_active, true);
	RegionWriteLock rlock (this);
	RegionList copy (regions.rlist());
	RegionList fixup;

	for (auto & r : copy) {

		if (r->nt_last() < at) {
			/* too early */
			continue;
		}

		if (at > r->position() && at < r->nt_last()) {
			/* intersected region */
			if (!move_intersected) {
				continue;
			}
		}

		/* do not move regions glued to music time - that
		 * has to be done separately.
		 */

		if (!ignore_music_glue && r->position().time_domain() != Temporal::AudioTime) {
			fixup.push_back (r);
			continue;
		}

		rlock.thawlist.add (r);
		r->set_position (r->position() + distance);
	}

	/* XXX: may not be necessary; Region::post_set should do this, I think */
	for (auto & r : fixup) {
		r->recompute_position_from_time_domain ();
	}
}

void
Playlist::split (timepos_t const & at)
{
	RegionWriteLock rlock (this);
	RegionList      copy (regions.rlist ());

	/* use a copy since this operation can modify the region list */

	for (auto & r : copy) {
		_split_region (r, at, rlock.thawlist);
	}
}

void
Playlist::split_region (std::shared_ptr<Region> region, timepos_t const & playlist_position)
{
	RegionWriteLock rl (this);
	_split_region (region, playlist_position, rl.thawlist);
}

void
Playlist::_split_region (std::shared_ptr<Region> region, timepos_t const &  playlist_position, ThawList& thawlist)
{
	if (!region->covers (playlist_position)) {
		return;
	}

	if (region->position() == playlist_position ||
	    region->nt_last() == playlist_position) {
		return;
	}

	std::shared_ptr<Region> left;
	std::shared_ptr<Region> right;

	timecnt_t before (region->position().distance (playlist_position));
	timecnt_t after (region->length() - before);
	string before_name;
	string after_name;

	RegionFactory::region_name (before_name, region->name (), false);

	{
		PropertyList plist (region->derive_properties (false));

		plist.add (Properties::length, before);
		plist.add (Properties::name, before_name);
		plist.add (Properties::left_of_split, true);

		/* note: we must use the version of ::create with an offset here,
		 * since it supplies that offset to the Region constructor, which
		 * is necessary to get audio region gain envelopes right.
		 */
		left = RegionFactory::create (region, timecnt_t (before.time_domain()), plist, true, &thawlist);
	}

	RegionFactory::region_name (after_name, region->name (), false);

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

void
Playlist::AddToSoloSelectedList (const Region* r)
{
	_soloSelectedRegions.insert (r);
}

void
Playlist::RemoveFromSoloSelectedList (const Region* r)
{
	_soloSelectedRegions.erase (r);
}

bool
Playlist::SoloSelectedListIncludes (const Region* r)
{
	std::set<const Region*>::iterator i = _soloSelectedRegions.find (r);

	return (i != _soloSelectedRegions.end ());
}

bool
Playlist::SoloSelectedActive ()
{
	return !_soloSelectedRegions.empty ();
}

void
Playlist::ripple_locked (timepos_t const & at, timecnt_t const & distance, RegionList *exclude)
{
	RegionWriteLock rl (this);
	ripple_unlocked (at, distance, exclude, rl.thawlist);
}

void
Playlist::ripple_unlocked (timepos_t const & at, timecnt_t const & distance, RegionList *exclude, ThawList& thawlist, bool notify)
{
	if (distance.is_zero()) {
		return;
	}

	_rippling               = true;
	RegionListProperty copy = regions;

	for (auto & r : copy) {

		if (exclude) {
			if (std::find (exclude->begin (), exclude->end (), r) != exclude->end ()) {
				continue;
			}
		}

		if (r->position() >= at) {
			timepos_t new_pos = r->position() + distance;
			timepos_t limit = timepos_t::max (new_pos.time_domain()).earlier (r->length());
			if (new_pos < 0) {
				new_pos = timepos_t (new_pos.time_domain());
			} else if (new_pos >= limit ) {
				new_pos = limit;
			}

			thawlist.add (r);
			r->set_position (new_pos);
		}
	}

	_rippling = false;

	if (notify) {
		notify_contents_changed ();
	}
}

void
Playlist::region_bounds_changed (const PropertyChange& what_changed, std::shared_ptr<Region> region)
{
	if (in_set_state || _rippling || _nudging || _shuffling) {
		return;
	}

	if (what_changed.contains (Properties::length)) {
		/* remove it from the list then add it back in
		 * the right place again.
		 */

		RegionSortByPosition cmp;

		RegionList::iterator i = find (regions.begin (), regions.end (), region);

		if (i == regions.end ()) {
			/* the region bounds are being modified but its not currently
			 * in the region list. we will use its bounds correctly when/if
			 * it is added
			 */
			return;
		}

		regions.erase (i);
		regions.insert (upper_bound (regions.begin (), regions.end (), region, cmp), region);

		if (holding_state ()) {
			pending_bounds.push_back (region);
		} else {
			notify_contents_changed ();
			relayer ();
			list<Temporal::Range> xf;
			xf.push_back (Temporal::Range (region->last_range()));
			xf.push_back (Temporal::Range (region->range()));
			coalesce_and_check_crossfades (xf);
		}
	}
}

void
Playlist::region_changed_proxy (const PropertyChange& what_changed, std::weak_ptr<Region> weak_region)
{
	std::shared_ptr<Region> region (weak_region.lock ());

	if (!region) {
		return;
	}

	/* this makes a virtual call to the right kind of playlist ... */

	region_changed (what_changed, region);
}

bool
Playlist::region_changed (const PropertyChange& what_changed, std::shared_ptr<Region> region)
{
	PropertyChange our_interests;
	PropertyChange bounds;
	bool           save = false;

	if (in_set_state || in_flush) {
		return false;
	}

	our_interests.add (Properties::muted);
	our_interests.add (Properties::layer);
	our_interests.add (Properties::opaque);
	our_interests.add (Properties::contents);
	our_interests.add (Properties::time_domain);

	bounds.add (Properties::start);
	bounds.add (Properties::length);

	bool send_contents = false;

	if (what_changed.contains (bounds)) {
		region_bounds_changed (what_changed, region);
		save          = !_nudging;
		send_contents = true;
	}

	if (what_changed.contains (Properties::contents)) {
		send_contents = true;
	}

	/* since cc6c0f1263ab272707b0241ec333776036515fc2 can no longer distinguish between
	 * position and length changes, so we have to assume that all have happened :(
	 */
	if (what_changed.contains (Properties::length)) {
		notify_region_moved (region);
		notify_region_end_trimmed (region);
		notify_region_start_trimmed (region);
	}

	/* don't notify about layer changes, since we are the only object that can initiate
	 * them, and we notify in ::relayer()
	 */

	if (what_changed.contains (our_interests)) {
		save = true;
	}

	if (send_contents || save) {
		notify_contents_changed ();
	}

	mark_session_dirty ();

	return save;
}

void
Playlist::drop_regions ()
{
	RegionWriteLock rl (this);
	regions.clear ();
	all_regions.clear ();
}

void
Playlist::sync_all_regions_with_regions ()
{
	RegionWriteLock rl (this);

	all_regions.clear ();

	for (auto & r: regions) {
		all_regions.insert (r);
	}
}

void
Playlist::clear (bool with_signals)
{
	{
		RegionWriteLock rl (this);

		region_state_changed_connections.drop_connections ();
		region_drop_references_connections.drop_connections ();

		for (auto & r : regions) {
			pending_removes.insert (r);
		}

		regions.clear ();
	}

	if (with_signals) {
		for (auto & r : pending_removes) {
			RegionRemoved (std::weak_ptr<Region> (r)); /* EMIT SIGNAL */
		}

		pending_removes.clear ();
		pending_contents_change = false;
		ContentsChanged ();
	}
}

/* *********************************************************************
FINDING THINGS
**********************************************************************/

std::shared_ptr<RegionList>
Playlist::region_list ()
{
	RegionReadLock              rlock (this);
	std::shared_ptr<RegionList> rlist (new RegionList (regions.rlist ()));
	return rlist;
}

void
Playlist::deep_sources (std::set<std::shared_ptr<Source> >& sources) const
{
	RegionReadLock rlock (const_cast<Playlist*> (this));

	for (auto const & r : regions){
		r->deep_sources (sources);
	}
}

std::shared_ptr<RegionList>
Playlist::regions_at (timepos_t const & pos)
{
	RegionReadLock rlock (this);
	return find_regions_at (pos);
}

uint32_t
Playlist::count_regions_at (timepos_t const & pos) const
{
	RegionReadLock rlock (const_cast<Playlist*> (this));
	uint32_t       cnt = 0;

	for (auto const & r : regions) {
		if (r->covers (pos)) {
			cnt++;
		}
	}

	return cnt;
}

std::shared_ptr<Region>
Playlist::top_region_at (timepos_t const & pos)
{
	RegionReadLock rlock (this);
	std::shared_ptr<RegionList> rlist = find_regions_at (pos);
	std::shared_ptr<Region> region;

	if (rlist->size ()) {
		RegionSortByLayer cmp;
		rlist->sort (cmp);
		region = rlist->back ();
	}

	return region;
}

std::shared_ptr<Region>
Playlist::top_unmuted_region_at (timepos_t const & pos)
{
	RegionReadLock rlock (this);
	std::shared_ptr<RegionList> rlist = find_regions_at (pos);

	for (RegionList::iterator i = rlist->begin (); i != rlist->end ();) {
		RegionList::iterator tmp = i;

		++tmp;

		if ((*i)->muted ()) {
			rlist->erase (i);
		}

		i = tmp;
	}

	std::shared_ptr<Region> region;

	if (rlist->size ()) {
		RegionSortByLayer cmp;
		rlist->sort (cmp);
		region = rlist->back ();
	}

	return region;
}

std::shared_ptr<RegionList>
Playlist::audible_regions_at (timepos_t const& pos)
{
	std::shared_ptr<RegionList> rv (new RegionList);
	std::shared_ptr<RegionList> rlist;
	{
		RegionReadLock rlock (this);
		rlist = find_regions_at (pos);
	}

	if (!rlist->size ()) {
		return rv;
	}

	RegionSortByLayer cmp;
	rlist->sort (cmp);

	for (auto i = rlist->rbegin (); i != rlist->rend (); ++i) {
		if ((*i)->muted ()) {
			continue;
		}
		rv->push_back (*i);
		if ((*i)->opaque ()) {
			break;
		}
	}

	return rv;
}

bool
Playlist::region_is_audible_at (std::shared_ptr<Region> r, timepos_t const& pos)
{
	std::shared_ptr<RegionList> rlist;
	{
		RegionReadLock rlock (this);
		rlist = find_regions_at (pos);
	}

	if (!rlist->size ()) {
		return false;
	}

	RegionSortByLayer cmp;
	rlist->sort (cmp);

	for (auto i = rlist->rbegin (); i != rlist->rend (); ++i) {
		if ((*i)->muted ()) {
			continue;
		}
		if (r == *i) {
			return true;
		}
		if ((*i)->opaque ()) {
			break;
		}
	}
	return false;
}

std::shared_ptr<RegionList>
Playlist::find_regions_at (timepos_t const & pos)
{
	/* Caller must hold lock */

	std::shared_ptr<RegionList> rlist (new RegionList);

	for (auto & r : regions) {
		if (r->covers (pos)) {
			rlist->push_back (r);
		}
	}

	return rlist;
}

std::shared_ptr<RegionList>
Playlist::regions_with_start_within (Temporal::Range range)
{
	RegionReadLock              rlock (this);
	std::shared_ptr<RegionList> rlist (new RegionList);

	for (auto & r : regions) {
		if (r->position() >= range.start() && r->position() < range.end()) {
			rlist->push_back (r);
		}
	}

	return rlist;
}

std::shared_ptr<RegionList>
Playlist::regions_with_end_within (Temporal::Range range)
{
	RegionReadLock              rlock (this);
	std::shared_ptr<RegionList> rlist (new RegionList);

	for (auto & r : regions) {
		if (r->nt_last() >= range.start() && r->nt_last() < range.end()) {
			rlist->push_back (r);
		}
	}

	return rlist;
}

std::shared_ptr<RegionList>
Playlist::regions_touched (timepos_t const & start, timepos_t const & end)
{
	RegionReadLock rlock (this);
	return regions_touched_locked (start, end);
}

std::shared_ptr<RegionList>
Playlist::regions_touched_locked (timepos_t const & start, timepos_t const & end)
{
	std::shared_ptr<RegionList> rlist (new RegionList);

	for (auto & r : regions) {
		if (r->coverage (start, end) != Temporal::OverlapNone) {
			rlist->push_back (r);
		}
	}

	return rlist;
}

samplepos_t
Playlist::find_next_transient (timepos_t const & from, int dir)
{
	RegionReadLock      rlock (this);
	AnalysisFeatureList points;
	AnalysisFeatureList these_points;

	for (auto const & r : regions) {
		if (dir > 0) {
			if (r->nt_last() < from) {
				continue;
			}
		} else {
			if (r->position() > from) {
				continue;
			}
		}

		r->get_transients (these_points);

		these_points.push_back (r->position_sample());

		points.insert (points.end (), these_points.begin (), these_points.end ());
		these_points.clear ();
	}

	if (points.empty ()) {
		return -1;
	}

	TransientDetector::cleanup_transients (points, _session.sample_rate (), 3.0);
	bool reached = false;

	if (dir > 0) {
		for (AnalysisFeatureList::const_iterator x = points.begin(); x != points.end(); ++x) {
			if ((*x) >= from.samples()) {
				reached = true;
			}

			if (reached && (*x) > from.samples()) {
				return *x;
			}
		}
	} else {
		for (AnalysisFeatureList::reverse_iterator x = points.rbegin(); x != points.rend(); ++x) {
			if ((*x) <= from.samples()) {
				reached = true;
			}

			if (reached && (*x) < from.samples()) {
				return *x;
			}
		}
	}

	return -1;
}

std::shared_ptr<Region>
Playlist::find_next_region (timepos_t const & pos, RegionPoint point, int dir)
{
	RegionReadLock          rlock (this);
	std::shared_ptr<Region> ret;
	timecnt_t closest = timecnt_t::max (pos.time_domain());

	bool end_iter = false;

	for (auto const & r : regions) {

		if(end_iter) break;

		timecnt_t distance;
		timepos_t rpos;

		switch (point) {
		case Start:
			rpos = r->position ();
			break;
		case End:
			rpos = r->nt_last ();
			break;
		case SyncPoint:
			rpos = r->sync_position ();
			break;
		}

		switch (dir) {
		case 1: /* forwards */

			if (rpos > pos) {
				if ((distance = rpos.distance (pos)) < closest) {
					closest = distance;
					ret = r;
					end_iter = true;
				}
			}

			break;

		default: /* backwards */

			if (rpos < pos) {
				if ((distance = rpos.distance (pos)) < closest) {
					closest = distance;
					ret = r;
				}
			} else {
				end_iter = true;
			}

			break;
		}
	}

	return ret;
}

timepos_t
Playlist::find_prev_region_start (timepos_t const & at)
{
	RegionReadLock rlock (this);

	timecnt_t closest = timecnt_t::max (at.time_domain());
	timepos_t ret     = timepos_t::max (at.time_domain());;

	for (RegionList::reverse_iterator i = regions.rbegin (); i != regions.rend (); ++i) {
		std::shared_ptr<Region> r = (*i);
		timecnt_t       distance;
		const timepos_t first_sample = r->position();

		if (first_sample == at) {
			/* region at the given position - ignore */
			continue;
		}

		if (first_sample < at) {
			distance = first_sample.distance (at);

			if (distance < closest) {
				ret     = first_sample;
				closest = distance;
			}
		}

		/* XXX may be able to break out of loop here if first_sample >=
		   at, since regions should be sorted by position. Check this.
		*/
	}

	if (ret == timepos_t::max (at.time_domain())) {
		/* no earlier region found */
		ret = timepos_t (at.time_domain());
	}

	return ret;
}

timepos_t
Playlist::find_next_region_boundary (timepos_t const & pos, int dir)
{
	RegionReadLock rlock (this);

	timecnt_t closest = timecnt_t::max (pos.time_domain());
	timepos_t ret = timepos_t::max (pos.time_domain ());

	if (dir > 0) {
		for (RegionList::iterator i = regions.begin (); i != regions.end (); ++i) {
			std::shared_ptr<Region> r = (*i);
			timecnt_t distance;

			if (r->position() > pos) {

				distance = pos.distance (r->position());

				if (distance < closest) {
					ret = r->position ();
					closest = distance;
				}
			}

			if (r->nt_last() > pos) {

				distance = pos.distance (r->nt_last());

				if (distance < closest) {
					ret = r->nt_last();
					closest = distance;
				}
			}
		}

	} else {
		for (RegionList::reverse_iterator i = regions.rbegin (); i != regions.rend (); ++i) {
			std::shared_ptr<Region> r = (*i);
			timecnt_t distance;

			if (r->nt_last() < pos) {

				distance = r->nt_last().distance (pos);

				if (distance < closest) {
					ret = r->nt_last();
					closest = distance;
				}
			}

			if (r->position() < pos) {

				distance = r->position().distance (pos);

				if (distance < closest) {
					ret = r->position();
					closest = distance;
				}
			}
		}
	}

	return ret;
}

/***********************************************************************/

void
Playlist::mark_session_dirty ()
{
	_cached_extent.reset ();

	if (!in_set_state && !holding_state ()) {
		_session.set_dirty ();
	}
}

void
Playlist::rdiff (vector<Command*>& cmds) const
{
	RegionReadLock rlock (const_cast<Playlist*> (this));
	Stateful::rdiff (cmds);
}

void
Playlist::clear_owned_changes ()
{
	RegionReadLock rlock (this);
	Stateful::clear_owned_changes ();
}

string
Playlist::generate_pgroup_id ()
{
	time_t now;
	time (&now);
	Glib::DateTime tm (Glib::DateTime::create_now_local (now));
	string gid;
	gid = (tm.format ("%F %H.%M.%S"));
	return gid;
}

void
Playlist::update (const RegionListProperty::ChangeRecord& change)
{
	DEBUG_TRACE (DEBUG::Properties, string_compose ("Playlist %1 updates from a change record with %2 adds %3 removes\n",
	                                                name (), change.added.size (), change.removed.size ()));

	RegionWriteLock rlock (this);
	/* add the added regions */
	for (RegionListProperty::ChangeContainer::const_iterator i = change.added.begin(); i != change.added.end(); ++i) {
		add_region_internal ((*i), (*i)->position(), rlock.thawlist);
	}
	/* remove the removed regions */
	for (RegionListProperty::ChangeContainer::const_iterator i = change.removed.begin (); i != change.removed.end (); ++i) {
		remove_region_internal (*i, rlock.thawlist);
	}
}

int
Playlist::set_state (const XMLNode& node, int version)
{
	XMLNode*                  child;
	XMLNodeList               nlist;
	XMLNodeConstIterator      niter;
	std::shared_ptr<Region> region;
	string                    region_name;
	bool                      seen_region_nodes = false;
	int                       ret               = 0;

	in_set_state++;

	if (node.name () != "Playlist") {
		in_set_state--;
		return -1;
	}

	freeze ();

	set_id (node);

	std::string name;
	if (node.get_property (X_("name"), name)) {
		_name = name;
		_set_sort_id ();
	}

	/* XXX legacy session: fix up later - :: update_orig_2X() */
	node.get_property (X_("orig-diskstream-id"), _orig_track_id);
	node.get_property (X_("orig_diskstream_id"), _orig_track_id);

	node.get_property (X_("orig-track-id"), _orig_track_id);
	node.get_property (X_("frozen"), _frozen);

	node.get_property (X_("pgroup-id"), _pgroup_id);

	node.get_property (X_("combine-ops"), _combine_ops);

	string shared_ids;
	if (node.get_property (X_("shared-with-ids"), shared_ids)) {
		if (!shared_ids.empty ()) {
			vector<string> result;
			::split (shared_ids, result, ',');
			vector<string>::iterator it = result.begin ();
			for (; it != result.end (); ++it) {
				_shared_with_ids.push_back (PBD::ID (*it));
			}
		}
	}

	clear (true);

	nlist = node.children ();

	for (niter = nlist.begin (); niter != nlist.end (); ++niter) {
		child = *niter;

		if (child->name () == "Region") {
			seen_region_nodes = true;

			ID id;
			if (!child->get_property ("id", id)) {
				error << _("region state node has no ID, ignored") << endmsg;
				continue;
			}

			if ((region = region_by_id (id))) {
				region->suspend_property_changes ();

				if (region->set_state (*child, version)) {
					region->resume_property_changes ();
					continue;
				}

			} else if ((region = RegionFactory::create (_session, *child, true)) != 0) {
				region->suspend_property_changes ();
			} else {
				error << _("Playlist: cannot create region from XML") << endmsg;
				return -1;
			}

			{
				RegionWriteLock rlock (this);
				add_region_internal (region, region->position(), rlock.thawlist);
			}

			region->resume_property_changes ();
		}
	}

	if (seen_region_nodes && regions.empty ()) {
		ret = -1;
	}

	thaw ();
	notify_contents_changed ();

	in_set_state--;
	first_set_state = false;

	return ret;
}

XMLNode&
Playlist::get_state () const
{
	return state (true);
}

XMLNode&
Playlist::get_template ()
{
	return state (false);
}

/** @param full_state true to include regions in the returned state, otherwise false.
 */
XMLNode&
Playlist::state (bool full_state) const
{
	XMLNode* node = new XMLNode (X_("Playlist"));

	node->set_property (X_("id"), id ());
	node->set_property (X_("name"), name ());
	node->set_property (X_("type"), _type);
	node->set_property (X_("orig-track-id"), _orig_track_id);
	node->set_property (X_("pgroup-id"), _pgroup_id);

	string                        shared_ids;
	list<PBD::ID>::const_iterator it = _shared_with_ids.begin ();
	for (; it != _shared_with_ids.end (); ++it) {
		shared_ids += "," + (*it).to_s ();
	}
	if (!shared_ids.empty ()) {
		shared_ids.erase (0, 1);
	}

	node->set_property (X_("shared-with-ids"), shared_ids);
	node->set_property (X_("frozen"), _frozen);

	if (full_state) {
		RegionReadLock rlock (this);

		node->set_property ("combine-ops", _combine_ops);

		for (auto const & r : regions) {
			assert (r->sources ().size () > 0 && r->master_sources ().size () > 0);
			node->add_child_nocopy (r->get_state ());
		}
	}

	if (_extra_xml) {
		node->add_child_copy (*_extra_xml);
	}

	return *node;
}

bool
Playlist::empty () const
{
	RegionReadLock rlock (const_cast<Playlist*> (this));
	return regions.empty ();
}

uint32_t
Playlist::n_regions () const
{
	RegionReadLock rlock (const_cast<Playlist*> (this));
	return regions.size ();
}

/** @return true if the all_regions list is empty, ie this playlist
 *  has never had a region added to it.
 */
bool
Playlist::all_regions_empty () const
{
	RegionReadLock rl (const_cast<Playlist*> (this));
	return all_regions.empty ();
}

pair<timepos_t, timepos_t>
Playlist::get_extent () const
{
	if (_cached_extent) {
		return _cached_extent.value ();
	}

	RegionReadLock rlock (const_cast<Playlist*> (this));
	_cached_extent = _get_extent ();
	return _cached_extent.value ();
}

pair<timepos_t, timepos_t>
Playlist::get_extent_with_endspace () const
{
	pair<timepos_t, timepos_t> l = get_extent();
	l.second += _end_space;
	return l;
}

pair<timepos_t, timepos_t>
Playlist::_get_extent () const
{
	Temporal::TimeDomain time_domain = Temporal::AudioTime;

	if (regions.empty()) {
		/* use time domain guess based on data type */
		time_domain = (_type == DataType::AUDIO ? Temporal::AudioTime : Temporal::BeatTime);
	}

	pair<timepos_t, timepos_t> ext (timepos_t::max (time_domain), timepos_t (time_domain));

	if (regions.empty()) {
		return ext;
	}

	/* use time domain of first region's position */

	time_domain = regions.front()->position().time_domain();

	for (auto const & r : regions) {
		pair<timepos_t, timepos_t> const e (r->position(), r->position() + r->length());
		if (e.first < ext.first) {
			ext.first = e.first;
		}
		if (e.second > ext.second) {
			ext.second = e.second;
		}
	}

	return ext;
}

string
Playlist::bump_name (string name, Session& session)
{
	string newname = name;

	do {
		newname = bump_name_once (newname, '.');
	} while (session.playlists ()->by_name (newname) != NULL);

	return newname;
}

layer_t
Playlist::top_layer () const
{
	RegionReadLock rlock (const_cast<Playlist*> (this));
	layer_t        top = 0;

	for (auto const & r : regions) {
		top = max (top, r->layer ());
	}
	return top;
}

struct RelayerSort {
	bool operator() (std::shared_ptr<Region> a, std::shared_ptr<Region> b)
	{
		return a->layering_index () < b->layering_index ();
	}
};

/** Set a new layer for a region.  This adjusts the layering indices of all
 *  regions in the playlist to put the specified region in the appropriate
 *  place.  The actual layering will be fixed up when relayer() happens.
 */
void
Playlist::set_layer (std::shared_ptr<Region> region, double new_layer)
{
	/* Remove the layer we are setting from our region list, and sort it
	 *  using the layer indeces.
	 */

	RegionList copy = regions.rlist ();
	copy.remove (region);
	copy.sort (RelayerSort ());

	/* Put region back in the right place */
	RegionList::iterator i = copy.begin ();
	while (i != copy.end ()) {
		if ((*i)->layer () > new_layer) {
			break;
		}
		++i;
	}

	copy.insert (i, region);

	setup_layering_indices (copy);
}

void
Playlist::setup_layering_indices (RegionList const& regions)
{
	uint64_t j = 0;

	for (auto const & r : regions) {
		r->set_layering_index (j++);
	}
}

struct LaterHigherSort {
	bool operator () (std::shared_ptr<Region> a, std::shared_ptr<Region> b) {
		return a->position() < b->position();
	}
};

/** Take the layering indices of each of our regions, compute the layers
 *  that they should be on, and write the layers back to the regions.
 */
void
Playlist::relayer ()
{
	/* never compute layers when setting from XML */

	if (in_set_state) {
		return;
	}

	if (regions.empty()) {
		/* nothing to do */
		return;
	}

	/* Build up a new list of regions on each layer, stored in a set of lists
	 * each of which represent some period of time on some layer.  The idea
	 * is to avoid having to search the entire region list to establish whether
	 * each region overlaps another */

	/* how many pieces to divide this playlist's time up into */
	int const divisions = 512;

	/* find the start and end positions of the regions on this playlist */
	timepos_t start = timepos_t::max (regions.front()->position().time_domain());
	timepos_t end = timepos_t (start.time_domain());

	for (auto const & r : regions) {
		start = min (start, r->position());
		end = max (end, r->position() + r->length());
	}

	/* hence the size of each time division */
	double const division_size = (end.samples() - start.samples()) / double (divisions);

	vector<vector<RegionList> > layers;
	layers.push_back (vector<RegionList> (divisions));

	/* Sort our regions into layering index order (for manual layering) or position order (for later is higher)*/
	RegionList copy = regions.rlist ();
	switch (Config->get_layer_model ()) {
		case LaterHigher:
			copy.sort (LaterHigherSort ());
			break;
		case Manual:
			copy.sort (RelayerSort ());
			break;
	}

	DEBUG_TRACE (DEBUG::Layering, "relayer() using:\n");
#ifndef NDEBUG
	for (auto const & r : copy) {
		DEBUG_TRACE (DEBUG::Layering, string_compose ("\t%1 %2\n", r->name (), r->layering_index ()));
	}
#endif

	for (auto const & r : copy) {
		/* find the time divisions that this region covers; if there are no regions on the list,
		 * division_size will equal 0 and in this case we'll just say that
		 * start_division = end_division = 0.
		 */
		int start_division = 0;
		int end_division   = 0;

		if (division_size > 0) {
			start_division = start.distance (r->position ()).samples () / division_size;
			end_division = start.distance (r->position () + r->length ()).samples () / division_size;
			if (end_division == divisions) {
				end_division--;
			}
		}

		assert (divisions == 0 || end_division < divisions);

		/* find the lowest layer that this region can go on */
		size_t j = layers.size ();
		while (j > 0) {
			/* try layer j - 1; it can go on if it overlaps no other region
			 * that is already on that layer
			 */

			bool overlap = false;
			for (int k = start_division; k <= end_division; ++k) {
				RegionList::iterator l = layers[j - 1][k].begin ();
				while (l != layers[j - 1][k].end ()) {
					if ((*l)->overlap_equivalent (r)) {
						overlap = true;
						break;
					}
					l++;
				}

				if (overlap) {
					break;
				}
			}

			if (overlap) {
				/* overlap, so we must use layer j */
				break;
			}

			--j;
		}

		if (j == layers.size ()) {
			/* we need a new layer for this region */
			layers.push_back (vector<RegionList> (divisions));
		}

		/* put a reference to this region in each of the divisions that it exists in */
		for (int k = start_division; k <= end_division; ++k) {
			layers[j][k].push_back (r);
		}

		r->set_layer (j);
	}

	/* It's a little tricky to know when we could avoid calling this; e.g. if we are
	 * relayering because we just removed the only region on the top layer, nothing will
	 * appear to have changed, but the StreamView must still sort itself out.  We could
	 * probably keep a note of the top layer last time we relayered, and check that,
	 * but premature optimisation &c...
	 */
	notify_layering_changed ();

	/* This relayer() may have been called as a result of a region removal, in which
	 * case we need to setup layering indices to account for the one that has just
	 * gone away.
	 */
	setup_layering_indices (copy);
}

void
Playlist::raise_region (std::shared_ptr<Region> region)
{
	set_layer (region, region->layer () + 1.5);
	relayer ();
}

void
Playlist::lower_region (std::shared_ptr<Region> region)
{
	set_layer (region, region->layer () - 1.5);
	relayer ();
}

void
Playlist::raise_region_to_top (std::shared_ptr<Region> region)
{
	set_layer (region, DBL_MAX);
	relayer ();
}

void
Playlist::lower_region_to_bottom (std::shared_ptr<Region> region)
{
	set_layer (region, -0.5);
	relayer ();
}

void
Playlist::nudge_after (timepos_t const & start, timecnt_t const & distance, bool forwards)
{
	RegionList::iterator i;
	bool                 moved = false;

	_nudging = true;

	{
		RegionWriteLock rlock (const_cast<Playlist *> (this));

		for (i = regions.begin(); i != regions.end(); ++i) {

			if ((*i)->position() >= start) {

				timepos_t new_pos;

				if (forwards) {

					if ((*i)->nt_last() > timepos_t::max ((*i)->position().time_domain()).earlier (distance)) {
						new_pos = timepos_t::max ((*i)->position().time_domain()).earlier ((*i)->length());
					} else {
						new_pos = (*i)->position() + distance;
					}

				} else {

					if ((*i)->position() > distance) {
						new_pos = (*i)->position().earlier (distance);
					} else {
						new_pos = timepos_t ((*i)->position().time_domain());;
					}
				}

				rlock.thawlist.add (*i);
				(*i)->set_position (new_pos);
				moved = true;
			}
		}
	}

	if (moved) {
		_nudging = false;
		notify_contents_changed ();
	}
}

bool
Playlist::uses_source (std::shared_ptr<const Source> src, bool shallow) const
{
	RegionReadLock rlock (const_cast<Playlist*> (this));

	for (auto const & r : all_regions) {
		/* Note: passing the second argument as false can cause at best
		 * incredibly deep and time-consuming recursion, and at worst
		 * cycles if the user has managed to create cycles of reference
		 * between compound regions. We generally only this during
		 * cleanup, and @p shallow is passed as true.
		 */
		if (r->uses_source (src, shallow)) {
			return true;
		}
	}

	return false;
}

std::shared_ptr<Region>
Playlist::find_region (const ID& id) const
{
	RegionReadLock rlock (const_cast<Playlist*> (this));

	/* searches all regions currently in use by the playlist */

	for (auto const & r : regions) {
		if (r->id () == id) {
			return r;
		}
	}

	return std::shared_ptr<Region> ();
}

uint32_t
Playlist::region_use_count (std::shared_ptr<Region> r) const
{
	RegionReadLock rlock (const_cast<Playlist*> (this));
	uint32_t       cnt = 0;

	for (RegionList::const_iterator i = regions.begin (); i != regions.end (); ++i) {
		if ((*i) == r) {
			cnt++;
		}
	}

	RegionFactory::CompoundAssociations& cassocs (RegionFactory::compound_associations ());
	for (RegionFactory::CompoundAssociations::iterator it = cassocs.begin (); it != cassocs.end (); ++it) {
		/* check if region is used in a compound */
		if (it->second == r) {
			/* region is referenced as 'original' of a compound */
			++cnt;
			break;
		}
		if (r->whole_file () && r->max_source_level () > 0) {
			/* region itself ia a compound.
			 * the compound regions are not referenced -> check regions inside compound
			 */
			const SourceList& sl = r->sources ();
			for (SourceList::const_iterator s = sl.begin (); s != sl.end (); ++s) {
				std::shared_ptr<PlaylistSource> ps = std::dynamic_pointer_cast<PlaylistSource> (*s);
				if (!ps)
					continue;
				if (ps->playlist ()->region_use_count (it->first)) {
					/* break out of both loops */
					return ++cnt;
				}
			}
		}
	}
	return cnt;
}

std::shared_ptr<Region>
Playlist::region_by_id (const ID& id) const
{
	/* searches all regions ever added to this playlist */

	for (auto const & r : all_regions) {
		if (r->id () == id) {
			return r;
		}
	}
	return std::shared_ptr<Region> ();
}

void
Playlist::dump () const
{
	std::shared_ptr<Region> r;

	cerr << "Playlist \"" << _name << "\" " << endl
	     << regions.size () << " regions "
	     << endl;

	for (auto const & r : regions) {
		cerr << "  " << r->name() << " ["
		     << r->start() << "+" << r->length()
		     << "] at "
		     << r->position()
		     << " on layer "
		     << r->layer ()
		     << endl;
	}
}

void
Playlist::set_frozen (bool yn)
{
	_frozen = yn;
}

void
Playlist::shuffle (std::shared_ptr<Region> region, int dir)
{
	bool moved = false;

	if (region->locked ()) {
		return;
	}

	_shuffling = true;

	{
		RegionWriteLock rlock (const_cast<Playlist*> (this));

		if (dir > 0) {
			RegionList::iterator next;

			for (RegionList::iterator i = regions.begin (); i != regions.end (); ++i) {
				if ((*i) == region) {
					next = i;
					++next;

					if (next != regions.end ()) {
						if ((*next)->locked ()) {
							break;
						}

						timepos_t new_pos;

						if ((*next)->position() != region->last_sample() + 1) {
							/* they didn't used to touch, so after shuffle,
							 * just have them swap positions.
							 */
							new_pos = (*next)->position();
						} else {
							/* they used to touch, so after shuffle,
							 * make sure they still do. put the earlier
							 * region where the later one will end after
							 * it is moved.
							 */
							new_pos = region->position() + (*next)->length();
						}

						rlock.thawlist.add (*next);
						rlock.thawlist.add (region);

						(*next)->set_position (region->position());
						region->set_position (new_pos);

						/* avoid a full sort */

						regions.erase (i); /* removes the region from the list */
						next++;
						regions.insert (next, region); /* adds it back after next */

						moved = true;
					}
					break;
				}
			}
		} else {
			RegionList::iterator prev = regions.end ();

			for (RegionList::iterator i = regions.begin (); i != regions.end (); prev = i, ++i) {
				if ((*i) == region) {
					if (prev != regions.end ()) {
						if ((*prev)->locked ()) {
							break;
						}

						timepos_t new_pos;
						if (region->position() != (*prev)->last_sample() + 1) {
							/* they didn't used to touch, so after shuffle,
							 * just have them swap positions.
							 */
							new_pos = region->position();
						} else {
							/* they used to touch, so after shuffle,
							 * make sure they still do. put the earlier
							 * one where the later one will end after
							 */
							new_pos = (*prev)->position() + region->length();
						}

						rlock.thawlist.add (region);
						rlock.thawlist.add (*prev);

						region->set_position ((*prev)->position());
						(*prev)->set_position (new_pos);

						/* avoid a full sort */

						regions.erase (i);             /* remove region */
						regions.insert (prev, region); /* insert region before prev */

						moved = true;
					}

					break;
				}
			}
		}
	}

	_shuffling = false;

	if (moved) {
		relayer ();
		notify_contents_changed ();
	}
}

bool
Playlist::region_is_shuffle_constrained (std::shared_ptr<Region>)
{
	RegionReadLock rlock (const_cast<Playlist*> (this));

	if (regions.size () > 1) {
		return true;
	}

	return false;
}

void
Playlist::ripple (timepos_t const & at, timecnt_t const & distance, RegionList *exclude)
{
	ripple_locked (at, distance, exclude);
}

void
Playlist::update_after_tempo_map_change ()
{
	RegionWriteLock rlock (const_cast<Playlist*> (this));
	RegionList      copy (regions.rlist ());

	for (auto & r : copy) {
		rlock.thawlist.add (r);
		r->update_after_tempo_map_change ();
	}
}

void
Playlist::foreach_region (boost::function<void(std::shared_ptr<Region>)> func)
{
	RegionReadLock rl (this);
	for (auto & r : regions) {
		func (r);
	}
}

bool
Playlist::has_region_at (timepos_t const & p) const
{
	RegionReadLock (const_cast<Playlist*> (this));

	RegionList::const_iterator i = regions.begin ();
	while (i != regions.end () && !(*i)->covers (p)) {
		++i;
	}

	return (i != regions.end ());
}

/** Look from a session sample time and find the start time of the next region
 *  which is on the top layer of this playlist.
 *  @param t Time to look from.
 *  @return Position of next top-layered region, or max_samplepos if there isn't one.
 */
timepos_t
Playlist::find_next_top_layer_position (timepos_t const & t) const
{
	RegionReadLock rlock (const_cast<Playlist*> (this));

	layer_t const top = top_layer ();

	RegionList copy = regions.rlist ();
	copy.sort (RegionSortByPosition ());

	for (auto const & r : copy) {
		if (r->position() >= t && r->layer() == top) {
			return r->position();
		}
	}

	return timepos_t::max (t.time_domain());
}

std::shared_ptr<Region>
Playlist::combine (const RegionList& rl, std::shared_ptr<Track>)
{
	if (rl.empty()) {
		return std::shared_ptr<Region>();
	}

	ThawList                           thawlist;
	PropertyList                       plist;
	SourceList::size_type              channels          = 0;
	uint32_t                           layer             = 0;
	timepos_t                          earliest_position = timepos_t::max (rl.front()->position().time_domain());
	vector<TwoRegions>                 old_and_new_regions;
	vector<std::shared_ptr<Region> > originals;
	vector<std::shared_ptr<Region> > copies;
	string parent_name;
	string child_name;
	uint32_t max_level = 0;

	/* find the maximum depth of all the regions we're combining */

	for (RegionList::const_iterator i = rl.begin(); i != rl.end(); ++i) {
		max_level = max (max_level, (*i)->max_source_level());
	}

	parent_name = RegionFactory::compound_region_name (name(), combine_ops(), max_level, true);
	child_name = RegionFactory::compound_region_name (name(), combine_ops(), max_level, false);

	std::shared_ptr<Playlist> pl = PlaylistFactory::create (_type, _session, parent_name, true);

	for (auto const & r : rl) {
		earliest_position = min (earliest_position, r->position());
	}

	/* enable this so that we do not try to create xfades etc. as we add
	 * regions
	 */

	pl->in_partition = true;

	/* sort by position then layer.
	 * route_time_axis passes 'selected_regions' - which is not sorted.
	 * here we need the top-most first, then every layer's region sorted by position.
	 */
	RegionList sorted(rl);
	sorted.sort(RegionSortByLayerAndPosition());

	for (auto const & original_region : sorted) {

		/* copy the region */

		std::shared_ptr<Region> copied_region   = RegionFactory::create (original_region, false, false, &thawlist);

		old_and_new_regions.push_back (TwoRegions (original_region,copied_region));
		originals.push_back (original_region);
		copies.push_back (copied_region);

		RegionFactory::add_compound_association (original_region, copied_region);

		/* make position relative to zero */

		pl->add_region_internal (copied_region, original_region->position().earlier (timecnt_t (earliest_position, earliest_position)), thawlist);

		/* use the maximum number of channels for any region */

		channels = max (channels, original_region->sources().size());

		/* it will go above the layer of the highest existing region */

		layer = max (layer, original_region->layer());
	}

	pl->in_partition = false;

	/* pre-process. e.g. disable audio region fades */
	pre_combine (copies);

	/* now create a new PlaylistSource for each channel in the new playlist */


	SourceList                     sources;
	pair<timepos_t,timepos_t> extent = pl->get_extent();
	timepos_t zero (_type == DataType::AUDIO ? Temporal::AudioTime : Temporal::BeatTime);

	for (uint32_t chn = 0; chn < channels; ++chn) {
		sources.push_back (SourceFactory::createFromPlaylist (_type, _session, pl, id(), parent_name, chn, zero, extent.second, false, false));
	}

	/* now a new whole-file region using the list of sources */

	plist.add (Properties::start, timecnt_t (0, zero));
	plist.add (Properties::length, timecnt_t  (extent.second, extent.first));
	plist.add (Properties::name, parent_name);
	plist.add (Properties::whole_file, true);

	std::shared_ptr<Region> parent_region = RegionFactory::create (sources, plist, true, &thawlist);

	/* now the non-whole-file region that we will actually use in the playlist */

	plist.clear ();
	plist.add (Properties::start, zero);
	plist.add (Properties::length, extent.second);
	plist.add (Properties::name, child_name);
	plist.add (Properties::layer, layer+1);

	std::shared_ptr<Region> compound_region = RegionFactory::create (parent_region, plist, true, &thawlist);

	for (auto src : sources) {
		std::dynamic_pointer_cast<PlaylistSource>(src)->set_owner (compound_region->id());
	}

	/* remove all the selected regions from the current playlist */

	freeze ();

	for (auto const & reg : rl) {
		remove_region (reg);
	}

	/* do type-specific stuff with the originals and the new compound region */

	post_combine (originals, compound_region);

	/* add the new region at the right location */

	add_region (compound_region, earliest_position);

	_combine_ops++;

	thawlist.release ();
	thaw ();

	return compound_region;
}

void
Playlist::uncombine (std::shared_ptr<Region> target)
{
	std::shared_ptr<PlaylistSource>  pls;
	std::shared_ptr<const Playlist>  pl;
	vector<std::shared_ptr<Region> > originals;
	vector<TwoRegions>                 old_and_new_regions;

	/* (1) check that its really a compound region */

	if ((pls = std::dynamic_pointer_cast<PlaylistSource> (target->source (0))) == 0) {
		return;
	}

	pl = pls->playlist ();

	timepos_t adjusted_start;
	timepos_t adjusted_end;

	/* the leftmost (earliest) edge of the compound region
	 * starts at zero in its source, or larger if it
	 * has been trimmed or content-scrolled.
	 *
	 * the rightmost (latest) edge of the compound region
	 * relative to its source is the starting point plus
	 * the length of the region.
	 */

	/* (2) get all the original regions */

	const RegionList& rl (pl->region_list_property().rlist());
	RegionFactory::CompoundAssociations& cassocs (RegionFactory::compound_associations());
	timecnt_t move_offset;

	/* there are three possibilities here:
	   1) the playlist that the playlist source was based on
	   is us, so just add the originals (which belonged to
	   us anyway) back in the right place.

	   2) the playlist that the playlist source was based on
	   is NOT us, so we need to make copies of each of
	   the original regions that we find, and add them
	   instead.

	   3) target region is a copy of a compound region previously
	   created. In this case we will also need to make copies ot each of
	   the original regions, and add them instead.
	*/

	const bool need_copies = (std::dynamic_pointer_cast<PlaylistSource> (pls)->owner() != target->id()) ||
		(pls->original() != id());


	ThawList thawlist;

	for (auto const & current : rl) {

		RegionFactory::CompoundAssociations::iterator ca = cassocs.find (current);

		if (ca == cassocs.end()) {
			continue;
		}

		std::shared_ptr<Region> original (ca->second);

		bool modified_region;

		if (current == rl.front()) {
			move_offset = original->position().distance (target->position()) - timecnt_t (target->start(), target->position());
			adjusted_start = original->position() + target->start();
			adjusted_end = adjusted_start + target->length();
		}

		if (!need_copies) {
			thawlist.add (original);
		} else {
			timepos_t pos = original->position();
			/* make a copy, but don't announce it */
			original = RegionFactory::create (original, false, false, &thawlist);
			/* the pure copy constructor resets position() to zero, so fix that up.  */
			original->set_position (pos);
		}

		/* check to see how the original region (in the
		 * playlist before compounding occurred) overlaps
		 * with the new state of the compound region.
		 */

		original->clear_changes ();
		modified_region = false;

		switch (original->coverage (adjusted_start, adjusted_end)) {
		case Temporal::OverlapNone:
			/* original region does not cover any part
			 * of the current state of the compound region
			 */
			continue;

		case Temporal::OverlapInternal:
			/* overlap is just a small piece inside the
			 * original so trim both ends
			 */
			original->trim_to (adjusted_start, adjusted_start.distance (adjusted_end));
			modified_region = true;
			break;

		case Temporal::OverlapExternal:
			/* overlap fully covers original, so leave it as is */
			break;

		case Temporal::OverlapEnd:
			/* overlap starts within but covers end, so trim the front of the region */
			original->trim_front (adjusted_start);
			modified_region = true;
			break;

		case Temporal::OverlapStart:
			/* overlap covers start but ends within, so
			 * trim the end of the region.
			 */
			original->trim_end (adjusted_end);
			modified_region = true;
			break;
		}

		if (!move_offset.is_zero()) {
			/* fix the position to match any movement of the compound region. */
			original->set_position (original->position() + move_offset);
			modified_region = true;
		}

		if (modified_region) {
			_session.add_command (new StatefulDiffCommand (original));
		}

		/* and add to the list of regions waiting to be
		 * re-inserted
		 */

		originals.push_back (original);
		old_and_new_regions.push_back (TwoRegions (current, original));
	}

	pre_uncombine (originals, target);

	in_partition = true;
	freeze ();

	// (3) remove the compound region

	remove_region (target);

	// (4) add the constituent regions

	for (vector<std::shared_ptr<Region> >::iterator i = originals.begin(); i != originals.end(); ++i) {
		add_region ((*i), (*i)->position());
		set_layer((*i), (*i)->layer());
		if (!RegionFactory::region_by_id((*i)->id())) {
			RegionFactory::map_add(*i);
		}
	}

	in_partition = false;
	thaw ();
	thawlist.release ();
}

void
Playlist::fade_range (list<TimelineRange>& ranges)
{
	RegionReadLock rlock (this);
	for (list<TimelineRange>::iterator r = ranges.begin(); r != ranges.end(); ) {
		list<TimelineRange>::iterator tmpr = r;
		++tmpr;
		for (RegionList::const_iterator i = regions.begin (); i != regions.end ();) {
			RegionList::const_iterator tmpi = i;
			++tmpi;
			(*i)->fade_range ((*r).start().samples(), (*r).end().samples());
			i = tmpi;
		}
		r = tmpr;
	}
}

uint32_t
Playlist::max_source_level () const
{
	RegionReadLock rlock (const_cast<Playlist*> (this));
	uint32_t       lvl = 0;

	for (RegionList::const_iterator i = regions.begin (); i != regions.end (); ++i) {
		lvl = max (lvl, (*i)->max_source_level ());
	}

	return lvl;
}

void
Playlist::set_orig_track_id (const PBD::ID& id)
{
	if (shared_with (id)) {
		/* Swap 'shared_id' / origin_track_id */
		unshare_with (id);
		share_with (_orig_track_id);
	}
	_orig_track_id = id;
}

void
Playlist::share_with (const PBD::ID& id)
{
	if (!shared_with (id)) {
		_shared_with_ids.push_back (id);
	}
}

void
Playlist::unshare_with (const PBD::ID& id)
{
	list<PBD::ID>::iterator it = _shared_with_ids.begin ();
	while (it != _shared_with_ids.end ()) {
		if (*it == id) {
			_shared_with_ids.erase (it);
			break;
		}
		++it;
	}
}

bool
Playlist::shared_with (const PBD::ID& id) const
{
	bool                          shared = false;
	list<PBD::ID>::const_iterator it     = _shared_with_ids.begin ();
	while (it != _shared_with_ids.end () && !shared) {
		if (*it == id) {
			shared = true;
		}
		++it;
	}

	return shared;
}

void
Playlist::reset_shares ()
{
	_shared_with_ids.clear ();
}

/** Take a list of ranges, coalesce any that can be coalesced, then call
 *  check_crossfades for each one.
 */
void
Playlist::coalesce_and_check_crossfades (list<Temporal::Range> ranges)
{
	/* XXX: it's a shame that this coalesce algorithm also exists in
	 * TimeSelection::consolidate().
	 */

	/* XXX: xfade: this is implemented in Evoral::RangeList */

restart:
	for (list<Temporal::Range>::iterator i = ranges.begin(); i != ranges.end(); ++i) {
		for (list<Temporal::Range>::iterator j = ranges.begin(); j != ranges.end(); ++j) {

			if (i == j) {
				continue;
			}

			// XXX i->from can be > i->to - is this right? coverage() will return OverlapNone in this case
			if (Temporal::coverage_exclusive_ends (i->start(), i->end(), j->start(), j->start()) != Temporal::OverlapNone) {
				i->set_start (min (i->start(), j->start()));
				i->set_end (max (i->end(), j->end()));
				ranges.erase (j);
				goto restart;
			}
		}
	}
}

void
Playlist::set_capture_insertion_in_progress (bool yn)
{
	_capture_insertion_underway = yn;
}

void
Playlist::rdiff_and_add_command (Session* session)
{

	vector<Command*> cmds;
	rdiff (cmds);
	session->add_commands (cmds);
	session->add_command (new StatefulDiffCommand (shared_from_this ()));
}

void
Playlist::start_domain_bounce (Temporal::DomainBounceInfo& cmd)
{
	RegionReadLock rlock (this);
	for (auto & region  : regions) {
		region->start_domain_bounce (cmd);
	}
}


void
Playlist::finish_domain_bounce (Temporal::DomainBounceInfo& cmd)
{
	ThawList thawlist;

	clear_changes ();

	{
		RegionWriteLock rlock (this);
		for (auto & region  : regions) {
			thawlist.add (region);
			region->finish_domain_bounce (cmd);
		}
	}

	thawlist.release ();
	rdiff_and_add_command (&_session);
}

void
Playlist::time_domain_changed ()
{
#if 0
	using namespace Temporal;

	TimeDomainProvider::time_domain_changed ();

	Temporal::TimeDomain to = time_domain();
	Temporal::TimeDomain from = (to == AudioTime ? BeatTime : AudioTime);

	for (auto & region  : regions) {
		region->swap_time_domain (from, to);
	}
#endif
}

