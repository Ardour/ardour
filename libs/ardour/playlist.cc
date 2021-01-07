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

#include <stdint.h>
#include <set>
#include <algorithm>
#include <string>

#include "pbd/types_convert.h"
#include "pbd/stateful_diff_command.h"
#include "pbd/strsplit.h"
#include "pbd/unwind.h"
#include "pbd/xml++.h"

#include "ardour/debug.h"
#include "ardour/midi_region.h"
#include "ardour/playlist.h"
#include "ardour/playlist_factory.h"
#include "ardour/playlist_source.h"
#include "ardour/region.h"
#include "ardour/midi_region.h"
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
	ShowMeTheList (boost::shared_ptr<Playlist> pl, const string& n) : playlist (pl), name (n) {}
	~ShowMeTheList () {
		cerr << ">>>>" << name << endl; playlist->dump(); cerr << "<<<<" << name << endl << endl;
	};
	boost::shared_ptr<Playlist> playlist;
	string name;
};

void
Playlist::make_property_quarks ()
{
	Properties::regions.property_id = g_quark_from_static_string (X_("regions"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for regions = %1\n",
	                                                Properties::regions.property_id));
}

RegionListProperty::RegionListProperty (Playlist& pl)
	: SequenceProperty<std::list<boost::shared_ptr<Region> > > (Properties::regions.property_id, boost::bind (&Playlist::update, &pl, _1))
	, _playlist (pl)
{

}

RegionListProperty::RegionListProperty (RegionListProperty const & p)
	: PBD::SequenceProperty<std::list<boost::shared_ptr<Region> > > (p)
	, _playlist (p._playlist)
{

}

RegionListProperty *
RegionListProperty::clone () const
{
	return new RegionListProperty (*this);
}

RegionListProperty *
RegionListProperty::create () const
{
	return new RegionListProperty (_playlist);
}

void
RegionListProperty::get_content_as_xml (boost::shared_ptr<Region> region, XMLNode & node) const
{
	/* All regions (even those which are deleted) have their state
	 * saved by other code, so we can just store ID here.
	 */

	node.set_property ("id", region->id());
}

boost::shared_ptr<Region>
RegionListProperty::get_content_from_xml (XMLNode const & node) const
{
	PBD::ID id;
	if (!node.get_property ("id", id)) {
		assert (false);
	}

	boost::shared_ptr<Region> ret = _playlist.region_by_id (id);

	if (!ret) {
		ret = RegionFactory::region_by_id (id);
	}

	return ret;
}

Playlist::Playlist (Session& sess, string nom, DataType type, bool hide)
	: SessionObject(sess, nom)
	, regions (*this)
	, _type(type)
{
	init (hide);
	first_set_state = false;
	_name = nom;
	_set_sort_id ();
}

Playlist::Playlist (Session& sess, const XMLNode& node, DataType type, bool hide)
	: SessionObject(sess, "unnamed playlist")
	, regions (*this)
	, _type(type)
{
#ifndef NDEBUG
	XMLProperty const * prop = node.property("type");
	assert(!prop || DataType(prop->value()) == _type);
#endif

	init (hide);
	_name = "unnamed"; /* reset by set_state */
	_set_sort_id ();

	/* set state called by derived class */
}

Playlist::Playlist (boost::shared_ptr<const Playlist> other, string namestr, bool hide)
	: SessionObject(other->_session, namestr)
	, regions (*this)
	, _type(other->_type)
	, _orig_track_id (other->_orig_track_id)
	, _shared_with_ids (other->_shared_with_ids)
{
	init (hide);

	RegionList tmp;
	ThawList thawlist;
	other->copy_regions (tmp);

	in_set_state++;

	for (list<boost::shared_ptr<Region> >::iterator x = tmp.begin(); x != tmp.end(); ++x) {
		add_region_internal ((*x), (*x)->position(), thawlist);
	}
	thawlist.release ();

	in_set_state--;

	_splicing  = other->_splicing;
	_rippling  = other->_rippling;
	_nudging   = other->_nudging;
	_edit_mode = other->_edit_mode;

	in_set_state = 0;
	first_set_state = false;
	in_flush = false;
	in_partition = false;
	subcnt = 0;
	_frozen = other->_frozen;
}

Playlist::Playlist (boost::shared_ptr<const Playlist> other, samplepos_t start, samplecnt_t cnt, string str, bool hide)
	: SessionObject(other->_session, str)
	, regions (*this)
	, _type(other->_type)
	, _orig_track_id (other->_orig_track_id)
	, _shared_with_ids (other->_shared_with_ids)
{
	RegionReadLock rlock2 (const_cast<Playlist*> (other.get()));

	samplepos_t end = start + cnt - 1;

	init (hide);

	in_set_state++;

	ThawList thawlist;
	for (RegionList::const_iterator i = other->regions.begin(); i != other->regions.end(); ++i) {

		boost::shared_ptr<Region> region;
		boost::shared_ptr<Region> new_region;
		sampleoffset_t offset = 0;
		samplepos_t position = 0;
		samplecnt_t len = 0;
		string    new_name;
		Evoral::OverlapType overlap;

		region = *i;

		overlap = region->coverage (start, end);

		switch (overlap) {
		case Evoral::OverlapNone:
			continue;

		case Evoral::OverlapInternal:
			offset = start - region->position();
			position = 0;
			len = cnt;
			break;

		case Evoral::OverlapStart:
			offset = 0;
			position = region->position() - start;
			len = end - region->position();
			break;

		case Evoral::OverlapEnd:
			offset = start - region->position();
			position = 0;
			len = region->length() - offset;
			break;

		case Evoral::OverlapExternal:
			offset = 0;
			position = region->position() - start;
			len = region->length();
			break;
		}

		RegionFactory::region_name (new_name, region->name(), false);

		PropertyList plist;

		if (_type == DataType::MIDI) {
			boost::shared_ptr<MidiRegion> mregion = boost::dynamic_pointer_cast<MidiRegion> (region);
			if ( mregion && offset ) {
				int32_t division = 1;  /*magic value that ignores the meter (right?)*/
				const double start_quarter_note =_session.tempo_map().exact_qn_at_sample (start, division );
				const double start_offset_quarter_note = start_quarter_note - region->quarter_note();
				const double end_samples = (overlap == Evoral::OverlapStart ?
											end :  /*end the new region at the end of the selection*/
											region->position() + region->length() -1);  /*use the region's end*/
				const double length_quarter_note = _session.tempo_map().exact_qn_at_sample ( end_samples, division ) - start_quarter_note;
				plist.add (Properties::start_beats, mregion->start_beats() + start_offset_quarter_note);
				plist.add (Properties::length_beats, length_quarter_note);
			}
		}

		plist.add (Properties::start, region->start() + offset);
		plist.add (Properties::length, len);
		plist.add (Properties::name, new_name);
		plist.add (Properties::layer, region->layer());
		plist.add (Properties::layering_index, region->layering_index());

		new_region = RegionFactory::create (region, plist);

		add_region_internal (new_region, position, thawlist);
	}

	thawlist.release ();

	//keep track of any dead space at end (for pasting into Ripple or Splice mode)
	//at the end of construction, any length of cnt beyond the extents of the regions is end_space
	_end_space = cnt - (get_extent().second - get_extent().first);

	in_set_state--;
	first_set_state = false;
}

void
Playlist::use ()
{
	++_refcnt;
	InUse (true); /* EMIT SIGNAL */
}

void
Playlist::release ()
{
	if (_refcnt > 0) {
		_refcnt--;
	}

	if (_refcnt == 0) {
		InUse (false); /* EMIT SIGNAL */
	}
}

void
Playlist::copy_regions (RegionList& newlist) const
{
	RegionReadLock rlock (const_cast<Playlist *> (this));

	for (RegionList::const_iterator i = regions.begin(); i != regions.end(); ++i) {
		newlist.push_back (RegionFactory::create (*i, true, true));
	}
}

void
Playlist::init (bool hide)
{
	add_property (regions);
	_xml_node_name = X_("Playlist");

	g_atomic_int_set (&block_notifications, 0);
	g_atomic_int_set (&ignore_state_changes, 0);
	pending_contents_change = false;
	pending_layering = false;
	first_set_state = true;
	_refcnt = 0;
	_hidden = hide;
	_splicing = false;
	_rippling = false;
	_shuffling = false;
	_nudging = false;
	in_set_state = 0;
	in_undo = false;
	_edit_mode = Config->get_edit_mode();
	in_flush = false;
	in_partition = false;
	subcnt = 0;
	_frozen = false;
	_capture_insertion_underway = false;
	_combine_ops = 0;
	_end_space = 0;
	_playlist_shift_active = false;

	_session.history().BeginUndoRedo.connect_same_thread (*this, boost::bind (&Playlist::begin_undo, this));
	_session.history().EndUndoRedo.connect_same_thread (*this, boost::bind (&Playlist::end_undo, this));

	ContentsChanged.connect_same_thread (*this, boost::bind (&Playlist::mark_session_dirty, this));
}

Playlist::~Playlist ()
{
	DEBUG_TRACE (DEBUG::Destruction, string_compose ("Playlist %1 destructor\n", _name));

	{
		RegionReadLock rl (this);

		for (set<boost::shared_ptr<Region> >::iterator i = all_regions.begin(); i != all_regions.end(); ++i) {
			(*i)->set_playlist (boost::shared_ptr<Playlist>());
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

	size_t dot_position = _name.val().find_last_of(".");

	if (dot_position == string::npos) {
		_sort_id = 0;
	} else {
		string t = _name.val().substr(dot_position + 1);

		if (!string_to_uint32 (t, _sort_id)) {
			_sort_id = 0;
		}
	}
}

bool
Playlist::set_name (const string& str)
{
	/* in a typical situation, a playlist is being used
	 * by one diskstream and also is referenced by the
	 * Session. if there are more references than that,
	 * then don't change the name.
	 */

	if (_refcnt > 2) {
		return false;
	}

	bool ret =  SessionObject::set_name(str);
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
	delay_notifications ();
	g_atomic_int_inc (&ignore_state_changes);
}

/** @param from_undo true if this thaw is triggered by the end of an undo on this playlist */
void
Playlist::thaw (bool from_undo)
{
	g_atomic_int_dec_and_test (&ignore_state_changes);
	release_notifications (from_undo);
}


void
Playlist::delay_notifications ()
{
	g_atomic_int_inc (&block_notifications);
}

/** @param from_undo true if this release is triggered by the end of an undo on this playlist */
void
Playlist::release_notifications (bool from_undo)
{
	if (g_atomic_int_dec_and_test (&block_notifications)) {
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
		ContentsChanged(); /* EMIT SIGNAL */
	}
}

void
Playlist::notify_layering_changed ()
{
	if (holding_state ()) {
		pending_layering = true;
	} else {
		pending_layering = false;
		LayeringChanged(); /* EMIT SIGNAL */
	}
}

void
Playlist::notify_region_removed (boost::shared_ptr<Region> r)
{
	if (holding_state ()) {
		pending_removes.insert (r);
		pending_contents_change = true;
	} else {
		/* this might not be true, but we have to act
		   as though it could be.
		*/
		pending_contents_change = false;
		RegionRemoved (boost::weak_ptr<Region> (r)); /* EMIT SIGNAL */
		ContentsChanged (); /* EMIT SIGNAL */
	}
}

void
Playlist::notify_region_moved (boost::shared_ptr<Region> r)
{
	Evoral::RangeMove<samplepos_t> const move (r->last_position (), r->length (), r->position ());

	if (holding_state ()) {

		pending_range_moves.push_back (move);

	} else {

		list< Evoral::RangeMove<samplepos_t> > m;
		m.push_back (move);
		RangesMoved (m, false);
	}

}

void
Playlist::notify_region_start_trimmed (boost::shared_ptr<Region> r)
{
	if (r->position() >= r->last_position()) {
		/* trimmed shorter */
		return;
	}

	Evoral::Range<samplepos_t> const extra (r->position(), r->last_position());

	if (holding_state ()) {

		pending_region_extensions.push_back (extra);

	} else {

		list<Evoral::Range<samplepos_t> > r;
		r.push_back (extra);
		RegionsExtended (r);

	}
}

void
Playlist::notify_region_end_trimmed (boost::shared_ptr<Region> r)
{
	if (r->length() < r->last_length()) {
		/* trimmed shorter */
	}

	Evoral::Range<samplepos_t> const extra (r->position() + r->last_length(), r->position() + r->length());

	if (holding_state ()) {

		pending_region_extensions.push_back (extra);

	} else {

		list<Evoral::Range<samplepos_t> > r;
		r.push_back (extra);
		RegionsExtended (r);
	}
}


void
Playlist::notify_region_added (boost::shared_ptr<Region> r)
{
	/* the length change might not be true, but we have to act
	 * as though it could be.
	 */

	if (holding_state()) {
		pending_adds.insert (r);
		pending_contents_change = true;
	} else {
		r->clear_changes ();
		pending_contents_change = false;
		RegionAdded (boost::weak_ptr<Region> (r)); /* EMIT SIGNAL */
		ContentsChanged (); /* EMIT SIGNAL */

	}
}

/** @param from_undo true if this flush is triggered by the end of an undo on this playlist */
void
Playlist::flush_notifications (bool from_undo)
{
	set<boost::shared_ptr<Region> >::iterator s;
	bool regions_changed = false;

	if (in_flush) {
		return;
	}

	in_flush = true;

	if (!pending_bounds.empty() || !pending_removes.empty() || !pending_adds.empty()) {
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

	list<Evoral::Range<samplepos_t> > crossfade_ranges;

	for (RegionList::iterator r = pending_bounds.begin(); r != pending_bounds.end(); ++r) {
		crossfade_ranges.push_back ((*r)->last_range ());
		crossfade_ranges.push_back ((*r)->range ());
	}

	for (s = pending_removes.begin(); s != pending_removes.end(); ++s) {
		crossfade_ranges.push_back ((*s)->range ());
		remove_dependents (*s);
		RegionRemoved (boost::weak_ptr<Region> (*s)); /* EMIT SIGNAL */
		Region::RegionPropertyChanged(*s, Properties::hidden);
	}

	for (s = pending_adds.begin(); s != pending_adds.end(); ++s) {
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

	for (s = pending_adds.begin(); s != pending_adds.end(); ++s) {
		(*s)->clear_changes ();
		RegionAdded (boost::weak_ptr<Region> (*s)); /* EMIT SIGNAL */
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
	pending_layering = false;
}

/*************************************************************
 * PLAYLIST OPERATIONS
 *************************************************************/

/** Note: this calls set_layer (..., DBL_MAX) so it will reset the layering index of region */
void
Playlist::add_region (boost::shared_ptr<Region> region, samplepos_t position, float times, bool auto_partition, int32_t sub_num, double quarter_note, bool for_music)
{
	RegionWriteLock rlock (this);
	times = fabs (times);

	int itimes = (int) floor (times);

	samplepos_t pos = position;

	if (times == 1 && auto_partition){
		RegionList thawlist;
		partition_internal (pos - 1, (pos + region->length()), true, rlock.thawlist);
		for (RegionList::iterator i = thawlist.begin(); i != thawlist.end(); ++i) {
			(*i)->resume_property_changes ();
			_session.add_command (new StatefulDiffCommand (*i));
		}
	}

	if (itimes >= 1) {
		add_region_internal (region, pos, rlock.thawlist, sub_num, quarter_note, for_music);
		set_layer (region, DBL_MAX);
		pos += region->length();
		--itimes;
	}

	/* note that itimes can be zero if we being asked to just
	 * insert a single fraction of the region.
	 */

	for (int i = 0; i < itimes; ++i) {
		boost::shared_ptr<Region> copy = RegionFactory::create (region, true);
		add_region_internal (copy, pos, rlock.thawlist, sub_num);
		set_layer (copy, DBL_MAX);
		pos += region->length();
	}

	samplecnt_t length = 0;

	if (floor (times) != times) {
		length = (samplecnt_t) floor (region->length() * (times - floor (times)));
		string name;
		RegionFactory::region_name (name, region->name(), false);

		{
			PropertyList plist;

			plist.add (Properties::start, region->start());
			plist.add (Properties::length, length);
			plist.add (Properties::name, name);
			plist.add (Properties::layer, region->layer());

			boost::shared_ptr<Region> sub = RegionFactory::create (region, plist);
			add_region_internal (sub, pos, rlock.thawlist, sub_num);
			set_layer (sub, DBL_MAX);
		}
	}

	possibly_splice_unlocked (position, (pos + length) - position, region, rlock.thawlist);
}

void
Playlist::set_region_ownership ()
{
	RegionWriteLock rl (this);
	RegionList::iterator i;
	boost::weak_ptr<Playlist> pl (shared_from_this());

	for (i = regions.begin(); i != regions.end(); ++i) {
		(*i)->set_playlist (pl);
	}
}

bool
Playlist::add_region_internal (boost::shared_ptr<Region> region, samplepos_t position, ThawList& thawlist, int32_t sub_num, double quarter_note, bool for_music)
{
	if (region->data_type() != _type) {
		return false;
	}

	/* note, this will delay signal emission and trigger Playlist::region_changed_proxy
	 * via PropertyChanged subsciption below :(
	 */
	thawlist.add (region);

	RegionSortByPosition cmp;

	if (!first_set_state) {
		boost::shared_ptr<Playlist> foo (shared_from_this());
		region->set_playlist (boost::weak_ptr<Playlist>(foo));
	}
	if (for_music) {
		region->set_position_music (quarter_note);
	} else {
		region->set_position (position, sub_num);
	}

	regions.insert (upper_bound (regions.begin(), regions.end(), region, cmp), region);
	all_regions.insert (region);

	possibly_splice_unlocked (position, region->length(), region, thawlist);

	if (!holding_state ()) {
		/* layers get assigned from XML state, and are not reset during undo/redo */
		relayer ();
	}

	/* we need to notify the existence of new region before checking dependents. Ick. */

	notify_region_added (region);

	region->PropertyChanged.connect_same_thread (region_state_changed_connections, boost::bind (&Playlist::region_changed_proxy, this, _1, boost::weak_ptr<Region> (region)));
	region->DropReferences.connect_same_thread (region_drop_references_connections, boost::bind (&Playlist::region_going_away, this, boost::weak_ptr<Region> (region)));

	return true;
}

void
Playlist::replace_region (boost::shared_ptr<Region> old, boost::shared_ptr<Region> newr, samplepos_t pos)
{
	RegionWriteLock rlock (this);

	bool old_sp = _splicing;
	_splicing = true;

	remove_region_internal (old, rlock.thawlist);
	add_region_internal (newr, pos, rlock.thawlist);
	set_layer (newr, old->layer ());

	_splicing = old_sp;

	possibly_splice_unlocked (pos, old->length() - newr->length(), boost::shared_ptr<Region>(), rlock.thawlist);
}

void
Playlist::remove_region (boost::shared_ptr<Region> region)
{
	RegionWriteLock rlock (this);
	remove_region_internal (region, rlock.thawlist);
}

int
Playlist::remove_region_internal (boost::shared_ptr<Region> region, ThawList& thawlist)
{
	RegionList::iterator i;

	if (!in_set_state) {
		/* unset playlist */
		region->set_playlist (boost::weak_ptr<Playlist>());
	}

	/* XXX should probably freeze here .... */

	for (i = regions.begin(); i != regions.end(); ++i) {
		if (*i == region) {

			samplepos_t pos = (*i)->position();
			samplecnt_t distance = (*i)->length();

			regions.erase (i);

			possibly_splice_unlocked (pos, -distance, boost::shared_ptr<Region>(), thawlist);

			if (!holding_state ()) {
				relayer ();
				remove_dependents (region);
			}

			notify_region_removed (region);
			break;
		}
	}

	return -1;
}

void
Playlist::get_equivalent_regions (boost::shared_ptr<Region> other, vector<boost::shared_ptr<Region> >& results)
{
	switch (Config->get_region_equivalence()) {
		 case Exact:
			 for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {
				 if ((*i)->exact_equivalent (other)) {
					 results.push_back (*i);
				 }
			 }
			 break;
		 case LayerTime:
			 for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {
				 if ((*i)->layer_and_time_equivalent (other)) {
					 results.push_back (*i);
				 }
			 }
			 break;
		 case Enclosed:
			 for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {
				 if ((*i)->enclosed_equivalent (other)) {
					 results.push_back (*i);
				 }
			 }
			 break;
		 case Overlap:
			 for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {
				 if ((*i)->overlap_equivalent (other)) {
					 results.push_back (*i);
				 }
			 }
			 break;
	}
}

void
Playlist::get_region_list_equivalent_regions (boost::shared_ptr<Region> other, vector<boost::shared_ptr<Region> >& results)
{
	for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {

		if ((*i) && (*i)->region_list_equivalent (other)) {
			results.push_back (*i);
		}
	}
}

void
Playlist::get_source_equivalent_regions (boost::shared_ptr<Region> other, vector<boost::shared_ptr<Region> >& results)
{
	for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {

		if ((*i) && (*i)->any_source_equivalent (other)) {
			results.push_back (*i);
		}
	}
}

void
Playlist::partition (samplepos_t start, samplepos_t end, bool cut)
{
	RegionWriteLock lock(this);
	partition_internal (start, end, cut, lock.thawlist);
}

/* If a MIDI region is locked to musical-time, Properties::start is ignored
 * and _start is overwritten using Properties::start_beats in
 * add_region_internal() -> Region::set_position() -> MidiRegion::set_position_internal()
 */
static void maybe_add_start_beats (TempoMap const& tm, PropertyList& plist, boost::shared_ptr<Region> r, samplepos_t start, samplepos_t end)
{
	boost::shared_ptr<MidiRegion> mr = boost::dynamic_pointer_cast<MidiRegion>(r);
	if (!mr) {
		return;
	}
	double delta_beats = tm.quarter_notes_between_samples (start, end);
	plist.add (Properties::start_beats, mr->start_beats () + delta_beats);
}

/** Go through each region on the playlist and cut them at start and end, removing the section between
 *  start and end if cutting == true.  Regions that lie entirely within start and end are always
 *  removed.
 */
void
Playlist::partition_internal (samplepos_t start, samplepos_t end, bool cutting, ThawList& thawlist)
{
	RegionList new_regions;

	{

		boost::shared_ptr<Region> region;
		boost::shared_ptr<Region> current;
		string new_name;
		RegionList::iterator tmp;
		Evoral::OverlapType overlap;
		samplepos_t pos1, pos2, pos3, pos4;

		in_partition = true;

		/* need to work from a copy, because otherwise the regions we add
		 * during the process get operated on as well.
		 */

		RegionList copy = regions.rlist();

		for (RegionList::iterator i = copy.begin(); i != copy.end(); i = tmp) {

			tmp = i;
			++tmp;

			current = *i;

			if (current->first_sample() >= start && current->last_sample() < end) {

				if (cutting) {
					remove_region_internal (current, thawlist);
				}

				continue;
			}

			/* coverage will return OverlapStart if the start coincides
			 * with the end point. we do not partition such a region,
			 * so catch this special case.
			 */

			if (current->first_sample() >= end) {
				continue;
			}

			if ((overlap = current->coverage (start, end)) == Evoral::OverlapNone) {
				continue;
			}

			pos1 = current->position();
			pos2 = start;
			pos3 = end;
			pos4 = current->last_sample();

			if (overlap == Evoral::OverlapInternal) {
				/* split: we need 3 new regions, the front, middle and end.
				 * cut:   we need 2 regions, the front and end.
				 *
				 *
				 * start                 end
				 * ---------------*************************------------
				 * P1  P2              P3  P4
				 * SPLIT:
				 * ---------------*****++++++++++++++++====------------
				 * CUT
				 * ---------------*****----------------====------------
				 */

				if (!cutting) {
					/* "middle" ++++++ */

					RegionFactory::region_name (new_name, current->name(), false);

					PropertyList plist;

					plist.add (Properties::start, current->start() + (pos2 - pos1));
					plist.add (Properties::length, pos3 - pos2);
					plist.add (Properties::name, new_name);
					plist.add (Properties::layer, current->layer ());
					plist.add (Properties::layering_index, current->layering_index ());
					plist.add (Properties::automatic, true);
					plist.add (Properties::left_of_split, true);
					plist.add (Properties::right_of_split, true);
					maybe_add_start_beats (_session.tempo_map(), plist, current, current->start(), current->start() + (pos2 - pos1));

					/* see note in :_split_region()
					 * for MusicSample is needed to offset region-gain
					 */
					region = RegionFactory::create (current, MusicSample (pos2 - pos1, 0), plist);
					add_region_internal (region, start, thawlist);
					new_regions.push_back (region);
				}

				/* "end" ====== */

				RegionFactory::region_name (new_name, current->name(), false);

				PropertyList plist;

				plist.add (Properties::start, current->start() + (pos3 - pos1));
				plist.add (Properties::length, pos4 - pos3);
				plist.add (Properties::name, new_name);
				plist.add (Properties::layer, current->layer ());
				plist.add (Properties::layering_index, current->layering_index ());
				plist.add (Properties::automatic, true);
				plist.add (Properties::right_of_split, true);
				maybe_add_start_beats (_session.tempo_map(), plist, current, current->start(), current->start() + (pos3 - pos1));

				region = RegionFactory::create (current, MusicSample (pos3 - pos1, 0), plist);

				add_region_internal (region, end, thawlist);
				new_regions.push_back (region);

				/* "front" ***** */

				current->clear_changes ();
				thawlist.add (current);
				current->cut_end (pos2 - 1);

			} else if (overlap == Evoral::OverlapEnd) {

				/*
				  start           end
				  ---------------*************************------------
				  P1           P2         P4   P3
				  SPLIT:
				  ---------------**************+++++++++++------------
				  CUT:
				  ---------------**************-----------------------
				*/

				if (!cutting) {

					/* end +++++ */

					RegionFactory::region_name (new_name, current->name(), false);

					PropertyList plist;

					plist.add (Properties::start, current->start() + (pos2 - pos1));
					plist.add (Properties::length, pos4 - pos2);
					plist.add (Properties::name, new_name);
					plist.add (Properties::layer, current->layer ());
					plist.add (Properties::layering_index, current->layering_index ());
					plist.add (Properties::automatic, true);
					plist.add (Properties::left_of_split, true);
					maybe_add_start_beats (_session.tempo_map(), plist, current, current->start(), current->start() + (pos2 - pos1));

					region = RegionFactory::create (current, MusicSample(pos2 - pos1, 0), plist);

					add_region_internal (region, start, thawlist);
					new_regions.push_back (region);
				}

				/* front ****** */

				current->clear_changes ();
				thawlist.add (current);
				current->cut_end (pos2 - 1);

			} else if (overlap == Evoral::OverlapStart) {

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
					RegionFactory::region_name (new_name, current->name(), false);

					PropertyList plist;

					plist.add (Properties::start, current->start());
					plist.add (Properties::length, pos3 - pos1);
					plist.add (Properties::name, new_name);
					plist.add (Properties::layer, current->layer ());
					plist.add (Properties::layering_index, current->layering_index ());
					plist.add (Properties::automatic, true);
					plist.add (Properties::right_of_split, true);
					maybe_add_start_beats (_session.tempo_map(), plist, current, current->start(), current->start());

					region = RegionFactory::create (current, plist);

					add_region_internal (region, pos1, thawlist);
					new_regions.push_back (region);
				}

				/* end */

				current->clear_changes ();
				thawlist.add (current);
				current->trim_front (pos3);
			} else if (overlap == Evoral::OverlapExternal) {

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

	//keep track of any dead space at end (for pasting into Ripple or Splice mode)
	samplepos_t wanted_length = end-start;
	_end_space = wanted_length - _get_extent().second - _get_extent().first;
}

boost::shared_ptr<Playlist>
Playlist::cut_copy (boost::shared_ptr<Playlist> (Playlist::*pmf)(samplepos_t, samplecnt_t,bool), list<AudioRange>& ranges, bool result_is_hidden)
{
	boost::shared_ptr<Playlist> ret;
	boost::shared_ptr<Playlist> pl;
	samplepos_t start;

	if (ranges.empty()) {
		return boost::shared_ptr<Playlist>();
	}

	start = ranges.front().start;

	for (list<AudioRange>::iterator i = ranges.begin(); i != ranges.end(); ++i) {

		pl = (this->*pmf)((*i).start, (*i).length(), result_is_hidden);

		if (i == ranges.begin()) {
			ret = pl;
		} else {

			/* paste the next section into the nascent playlist,
			 * offset to reflect the start of the first range we
			 * chopped.
			 */

			ret->paste (pl, (*i).start - start, 1.0f, 0);
		}
	}

	return ret;
}

boost::shared_ptr<Playlist>
Playlist::cut (list<AudioRange>& ranges, bool result_is_hidden)
{
	boost::shared_ptr<Playlist> (Playlist::*pmf)(samplepos_t,samplecnt_t,bool) = &Playlist::cut;
	return cut_copy (pmf, ranges, result_is_hidden);
}

boost::shared_ptr<Playlist>
Playlist::copy (list<AudioRange>& ranges, bool result_is_hidden)
{
	boost::shared_ptr<Playlist> (Playlist::*pmf)(samplepos_t,samplecnt_t,bool) = &Playlist::copy;
	return cut_copy (pmf, ranges, result_is_hidden);
}

boost::shared_ptr<Playlist>
Playlist::cut (samplepos_t start, samplecnt_t cnt, bool result_is_hidden)
{
	boost::shared_ptr<Playlist> the_copy;
	char buf[32];

	snprintf (buf, sizeof (buf), "%" PRIu32, ++subcnt);
	string new_name = _name;
	new_name += '.';
	new_name += buf;

	if ((the_copy = PlaylistFactory::create (shared_from_this(), start, cnt, new_name, result_is_hidden)) == 0) {
		return boost::shared_ptr<Playlist>();
	}

	{
		RegionWriteLock rlock (this);
		partition_internal (start, start+cnt-1, true, rlock.thawlist);
	}

	return the_copy;
}

boost::shared_ptr<Playlist>
Playlist::copy (samplepos_t start, samplecnt_t cnt, bool result_is_hidden)
{
	char buf[32];

	snprintf (buf, sizeof (buf), "%" PRIu32, ++subcnt);
	string new_name = _name;
	new_name += '.';
	new_name += buf;

	// cnt = min (_get_extent().second - start, cnt);  (We need the full range length when copy/pasting in Ripple.  Why was this limit here?  It's not in CUT... )

	return PlaylistFactory::create (shared_from_this(), start, cnt, new_name, result_is_hidden);
}

int
Playlist::paste (boost::shared_ptr<Playlist> other, samplepos_t position, float times, const int32_t sub_num)
{
	times = fabs (times);

	{
		RegionReadLock rl2 (other.get());

		int itimes = (int) floor (times);
		samplepos_t pos = position;
		samplecnt_t const shift = other->_get_extent().second;
		layer_t top = top_layer ();

		{
			RegionWriteLock rl1 (this);
			while (itimes--) {
				for (RegionList::iterator i = other->regions.begin(); i != other->regions.end(); ++i) {
					boost::shared_ptr<Region> copy_of_region = RegionFactory::create (*i, true);

					/* put these new regions on top of all existing ones, but preserve
					   the ordering they had in the original playlist.
					*/

					add_region_internal (copy_of_region, (*i)->position() + pos, rl1.thawlist, sub_num);
					set_layer (copy_of_region, copy_of_region->layer() + top);
				}
				pos += shift;
			}
		}
	}

	return 0;
}


void
Playlist::duplicate (boost::shared_ptr<Region> region, samplepos_t position, float times)
{
	duplicate(region, position, region->length(), times);
}

/** @param gap from the beginning of the region to the next beginning */
void
Playlist::duplicate (boost::shared_ptr<Region> region, samplepos_t position, samplecnt_t gap, float times)
{
	times = fabs (times);

	RegionWriteLock rl (this);
	int itimes = (int) floor (times);

	while (itimes--) {
		boost::shared_ptr<Region> copy = RegionFactory::create (region, true);
		add_region_internal (copy, position, rl.thawlist);
		set_layer (copy, DBL_MAX);
		position += gap;
	}

	if (floor (times) != times) {
		samplecnt_t length = (samplecnt_t) floor (region->length() * (times - floor (times)));
		string name;
		RegionFactory::region_name (name, region->name(), false);

		{
			PropertyList plist;

			plist.add (Properties::start, region->start());
			plist.add (Properties::length, length);
			plist.add (Properties::name, name);

			boost::shared_ptr<Region> sub = RegionFactory::create (region, plist);
			add_region_internal (sub, position, rl.thawlist);
			set_layer (sub, DBL_MAX);
		}
	}
}

/** @param gap from the beginning of the region to the next beginning */
/** @param end the first sample that does _not_ contain a duplicated sample */
void
Playlist::duplicate_until (boost::shared_ptr<Region> region, samplepos_t position, samplecnt_t gap, samplepos_t end)
{
	 RegionWriteLock rl (this);

	 while (position + region->length() - 1 < end) {
		 boost::shared_ptr<Region> copy = RegionFactory::create (region, true);
		 add_region_internal (copy, position, rl.thawlist);
		 set_layer (copy, DBL_MAX);
		 position += gap;
	 }

	 if (position < end) {
		 samplecnt_t length = min (region->length(), end - position);
		 string name;
		 RegionFactory::region_name (name, region->name(), false);

		 {
			 PropertyList plist;

			 plist.add (Properties::start, region->start());
			 plist.add (Properties::length, length);
			 plist.add (Properties::name, name);

			 boost::shared_ptr<Region> sub = RegionFactory::create (region, plist);
			 add_region_internal (sub, position, rl.thawlist);
			 set_layer (sub, DBL_MAX);
		 }
	 }
}

void
Playlist::duplicate_range (AudioRange& range, float times)
{
	boost::shared_ptr<Playlist> pl = copy (range.start, range.length(), true);
	samplecnt_t offset = range.end - range.start;
	paste (pl, range.start + offset, times, 0);
}

void
Playlist::duplicate_ranges (std::list<AudioRange>& ranges, float times)
{
	if (ranges.empty()) {
		return;
	}

	samplepos_t min_pos = max_samplepos;
	samplepos_t max_pos = 0;

	for (std::list<AudioRange>::const_iterator i = ranges.begin();
	     i != ranges.end();
	     ++i) {
		min_pos = min (min_pos, (*i).start);
		max_pos = max (max_pos, (*i).end);
	}

	samplecnt_t offset = max_pos - min_pos;

	int count = 1;
	int itimes = (int) floor (times);
	while (itimes--) {
		for (list<AudioRange>::iterator i = ranges.begin (); i != ranges.end (); ++i) {
			boost::shared_ptr<Playlist> pl = copy ((*i).start, (*i).length (), true);
			paste (pl, (*i).start + (offset * count), 1.0f, 0);
		}
		++count;
	}
}

void
Playlist::shift (samplepos_t at, sampleoffset_t distance, bool move_intersected, bool ignore_music_glue)
{
	PBD::Unwinder<bool> uw (_playlist_shift_active, true);
	RegionWriteLock rlock (this);
	RegionList copy (regions.rlist());
	RegionList fixup;

	for (RegionList::iterator r = copy.begin(); r != copy.end(); ++r) {

		if ((*r)->last_sample() < at) {
			/* too early */
			continue;
		}

		if (at > (*r)->first_sample() && at < (*r)->last_sample()) {
			/* intersected region */
			if (!move_intersected) {
				continue;
			}
		}

		/* do not move regions glued to music time - that
		 * has to be done separately.
		 */

		if (!ignore_music_glue && (*r)->position_lock_style() != AudioTime) {
			fixup.push_back (*r);
			continue;
		}

		rlock.thawlist.add (*r);
		(*r)->set_position ((*r)->position() + distance);
	}

	/* XXX: may not be necessary; Region::post_set should do this, I think */
	for (RegionList::iterator r = fixup.begin(); r != fixup.end(); ++r) {
		(*r)->recompute_position_from_lock_style (0);
	}
}

void
Playlist::split (const MusicSample& at)
{
	RegionWriteLock rlock (this);
	RegionList copy (regions.rlist());

	/* use a copy since this operation can modify the region list */

	for (RegionList::iterator r = copy.begin(); r != copy.end(); ++r) {
		_split_region (*r, at, rlock.thawlist);
	}
}

void
Playlist::split_region (boost::shared_ptr<Region> region, const MusicSample& playlist_position)
{
	RegionWriteLock rl (this);
	_split_region (region, playlist_position, rl.thawlist);
}

void
Playlist::_split_region (boost::shared_ptr<Region> region, const MusicSample& playlist_position, ThawList& thawlist)
{
	if (!region->covers (playlist_position.sample)) {
		return;
	}

	if (region->position() == playlist_position.sample ||
			region->last_sample() == playlist_position.sample) {
		return;
	}

	boost::shared_ptr<Region> left;
	boost::shared_ptr<Region> right;

	MusicSample before (playlist_position.sample - region->position(), playlist_position.division);
	MusicSample after (region->length() - before.sample, 0);
	string before_name;
	string after_name;

	/* split doesn't change anything about length, so don't try to splice */

	bool old_sp = _splicing;
	_splicing = true;

	RegionFactory::region_name (before_name, region->name(), false);

	{
		PropertyList plist;

		plist.add (Properties::length, before.sample);
		plist.add (Properties::name, before_name);
		plist.add (Properties::left_of_split, true);
		plist.add (Properties::layering_index, region->layering_index ());
		plist.add (Properties::layer, region->layer ());

		/* note: we must use the version of ::create with an offset here,
		 * since it supplies that offset to the Region constructor, which
		 * is necessary to get audio region gain envelopes right.
		 */
		left = RegionFactory::create (region, MusicSample (0, 0), plist, true);
	}

	RegionFactory::region_name (after_name, region->name(), false);

	{
		PropertyList plist;

		plist.add (Properties::length, after.sample);
		plist.add (Properties::name, after_name);
		plist.add (Properties::right_of_split, true);
		plist.add (Properties::layering_index, region->layering_index ());
		plist.add (Properties::layer, region->layer ());

		/* same note as above */
		right = RegionFactory::create (region, before, plist, true);
	}

	add_region_internal (left, region->position(), thawlist, 0);
	add_region_internal (right, region->position() + before.sample, thawlist, before.division);

	remove_region_internal (region, thawlist);

	_splicing = old_sp;
}

void
Playlist::AddToSoloSelectedList(const Region* r)
{
	_soloSelectedRegions.insert (r);
}


void
Playlist::RemoveFromSoloSelectedList(const Region* r)
{
	_soloSelectedRegions.erase (r);
}


bool
Playlist::SoloSelectedListIncludes(const Region* r)
{
	std::set<const Region*>::iterator i = _soloSelectedRegions.find(r);

	return ( i != _soloSelectedRegions.end() );
}

bool
Playlist::SoloSelectedActive()
{
	return !_soloSelectedRegions.empty();
}


void
Playlist::possibly_splice (samplepos_t at, samplecnt_t distance, boost::shared_ptr<Region> exclude)
{
	if (_splicing || in_set_state) {
		/* don't respond to splicing moves or state setting */
		return;
	}

	if (_edit_mode == Splice) {
		splice_locked (at, distance, exclude);
	}
}

void
Playlist::possibly_splice_unlocked (samplepos_t at, samplecnt_t distance, boost::shared_ptr<Region> exclude, ThawList& thawlist)
{
	if (_splicing || in_set_state) {
		/* don't respond to splicing moves or state setting */
		return;
	}

	if (_edit_mode == Splice) {
		splice_unlocked (at, distance, exclude, thawlist);
	}
}

void
Playlist::splice_locked (samplepos_t at, samplecnt_t distance, boost::shared_ptr<Region> exclude)
{
	RegionWriteLock rl (this);
	splice_unlocked (at, distance, exclude, rl.thawlist);
}

void
Playlist::splice_unlocked (samplepos_t at, samplecnt_t distance, boost::shared_ptr<Region> exclude, ThawList& thawlist)
{
	_splicing = true;

	for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {

		if (exclude && (*i) == exclude) {
			continue;
		}

		if ((*i)->position() >= at) {
			samplepos_t new_pos = (*i)->position() + distance;
			if (new_pos < 0) {
				new_pos = 0;
			} else if (new_pos >= max_samplepos - (*i)->length()) {
				new_pos = max_samplepos - (*i)->length();
			}

			thawlist.add (*i);
			(*i)->set_position (new_pos);
		}
	}

	_splicing = false;

	notify_contents_changed ();
}

void
Playlist::ripple_locked (samplepos_t at, samplecnt_t distance, RegionList *exclude)
{
	RegionWriteLock rl (this);
	ripple_unlocked (at, distance, exclude, rl.thawlist);
}

void
Playlist::ripple_unlocked (samplepos_t at, samplecnt_t distance, RegionList *exclude, ThawList& thawlist)
{
	if (distance == 0) {
		return;
	}

	_rippling = true;
	RegionListProperty copy = regions;
	for (RegionList::iterator i = copy.begin(); i != copy.end(); ++i) {
		assert (i != copy.end());

		if (exclude) {
			if (std::find(exclude->begin(), exclude->end(), (*i)) != exclude->end()) {
				continue;
			}
		}

		if ((*i)->position() >= at) {
			samplepos_t new_pos = (*i)->position() + distance;
			samplepos_t limit = max_samplepos - (*i)->length();
			if (new_pos < 0) {
				new_pos = 0;
			} else if (new_pos >= limit ) {
				new_pos = limit;
			}

			thawlist.add (*i);
			(*i)->set_position (new_pos);
		}
	}

	_rippling = false;
	notify_contents_changed ();
}


void
Playlist::region_bounds_changed (const PropertyChange& what_changed, boost::shared_ptr<Region> region)
{
	if (in_set_state || _splicing || _rippling || _nudging || _shuffling) {
		return;
	}

	if (what_changed.contains (Properties::position)) {

		/* remove it from the list then add it back in
		 * the right place again.
		 */

		RegionSortByPosition cmp;

		RegionList::iterator i = find (regions.begin(), regions.end(), region);

		if (i == regions.end()) {
			/* the region bounds are being modified but its not currently
			 * in the region list. we will use its bounds correctly when/if
			 * it is added
			 */
			return;
		}

		regions.erase (i);
		regions.insert (upper_bound (regions.begin(), regions.end(), region, cmp), region);
	}

	if (what_changed.contains (Properties::position) || what_changed.contains (Properties::length)) {

		sampleoffset_t delta = 0;

		if (what_changed.contains (Properties::position)) {
			delta = region->position() - region->last_position();
		}

		if (what_changed.contains (Properties::length)) {
			delta += region->length() - region->last_length();
		}

		if (delta) {
			possibly_splice (region->last_position() + region->last_length(), delta, region);
		}

		if (holding_state ()) {
			pending_bounds.push_back (region);
		} else {
			notify_contents_changed ();
			relayer ();
			list<Evoral::Range<samplepos_t> > xf;
			xf.push_back (Evoral::Range<samplepos_t> (region->last_range()));
			xf.push_back (Evoral::Range<samplepos_t> (region->range()));
			coalesce_and_check_crossfades (xf);
		}
	}
}

void
Playlist::region_changed_proxy (const PropertyChange& what_changed, boost::weak_ptr<Region> weak_region)
{
	boost::shared_ptr<Region> region (weak_region.lock());

	if (!region) {
		return;
	}

	/* this makes a virtual call to the right kind of playlist ... */

	region_changed (what_changed, region);
}

bool
Playlist::region_changed (const PropertyChange& what_changed, boost::shared_ptr<Region> region)
{
	PropertyChange our_interests;
	PropertyChange bounds;
	bool save = false;

	if (in_set_state || in_flush) {
		return false;
	}

	our_interests.add (Properties::muted);
	our_interests.add (Properties::layer);
	our_interests.add (Properties::opaque);
	our_interests.add (Properties::contents);

	bounds.add (Properties::start);
	bounds.add (Properties::position);
	bounds.add (Properties::length);

	bool send_contents = false;

	if (what_changed.contains (bounds)) {
		region_bounds_changed (what_changed, region);
		save = !(_splicing || _nudging);
		send_contents = true;
	}

	if (what_changed.contains (Properties::contents)) {
		send_contents = true;
	}

	if (what_changed.contains (Properties::position) && !what_changed.contains (Properties::length)) {
		notify_region_moved (region);
	} else if (!what_changed.contains (Properties::position) && what_changed.contains (Properties::length)) {
		notify_region_end_trimmed (region);
	} else if (what_changed.contains (Properties::position) && what_changed.contains (Properties::length)) {
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

	for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {
		all_regions.insert (*i);
	}
}

void
Playlist::clear (bool with_signals)
{
	{
		RegionWriteLock rl (this);

		region_state_changed_connections.drop_connections ();
		region_drop_references_connections.drop_connections ();

		for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {
			pending_removes.insert (*i);
		}

		regions.clear ();

		for (set<boost::shared_ptr<Region> >::iterator s = pending_removes.begin(); s != pending_removes.end(); ++s) {
			remove_dependents (*s);
		}
	}

	if (with_signals) {

		for (set<boost::shared_ptr<Region> >::iterator s = pending_removes.begin(); s != pending_removes.end(); ++s) {
			RegionRemoved (boost::weak_ptr<Region> (*s)); /* EMIT SIGNAL */
		}

		pending_removes.clear ();
		pending_contents_change = false;
		ContentsChanged ();
	}

}

/* *********************************************************************
FINDING THINGS
**********************************************************************/

boost::shared_ptr<RegionList>
Playlist::region_list()
{
	RegionReadLock rlock (this);
	boost::shared_ptr<RegionList> rlist (new RegionList (regions.rlist ()));
	return rlist;
}

void
Playlist::deep_sources (std::set<boost::shared_ptr<Source> >& sources) const
{
	RegionReadLock rlock (const_cast<Playlist*>(this));

	for (RegionList::const_iterator i = regions.begin(); i != regions.end(); ++i) {
		(*i)->deep_sources (sources);
	}
}

boost::shared_ptr<RegionList>
Playlist::regions_at (samplepos_t sample)
{
	RegionReadLock rlock (this);
	return find_regions_at (sample);
}

uint32_t
Playlist::count_regions_at (samplepos_t sample) const
{
	RegionReadLock rlock (const_cast<Playlist*>(this));
	uint32_t cnt = 0;

	for (RegionList::const_iterator i = regions.begin(); i != regions.end(); ++i) {
		if ((*i)->covers (sample)) {
			cnt++;
		}
	}

	return cnt;
}

boost::shared_ptr<Region>
Playlist::top_region_at (samplepos_t sample)
{
	RegionReadLock rlock (this);
	boost::shared_ptr<RegionList> rlist = find_regions_at (sample);
	boost::shared_ptr<Region> region;

	if (rlist->size()) {
		RegionSortByLayer cmp;
		rlist->sort (cmp);
		region = rlist->back();
	}

	return region;
}

boost::shared_ptr<Region>
Playlist::top_unmuted_region_at (samplepos_t sample)
{
	RegionReadLock rlock (this);
	boost::shared_ptr<RegionList> rlist = find_regions_at (sample);

	for (RegionList::iterator i = rlist->begin(); i != rlist->end(); ) {

		RegionList::iterator tmp = i;

		++tmp;

		if ((*i)->muted()) {
			rlist->erase (i);
		}

		i = tmp;
	}

	boost::shared_ptr<Region> region;

	if (rlist->size()) {
		RegionSortByLayer cmp;
		rlist->sort (cmp);
		region = rlist->back();
	}

	return region;
}

boost::shared_ptr<RegionList>
Playlist::find_regions_at (samplepos_t sample)
{
	/* Caller must hold lock */

	boost::shared_ptr<RegionList> rlist (new RegionList);

	for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {
		if ((*i)->covers (sample)) {
			rlist->push_back (*i);
		}
	}

	return rlist;
}

boost::shared_ptr<RegionList>
Playlist::regions_with_start_within (Evoral::Range<samplepos_t> range)
{
	RegionReadLock rlock (this);
	boost::shared_ptr<RegionList> rlist (new RegionList);

	for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {
		if ((*i)->first_sample() >= range.from && (*i)->first_sample() <= range.to) {
			rlist->push_back (*i);
		}
	}

	return rlist;
}

boost::shared_ptr<RegionList>
Playlist::regions_with_end_within (Evoral::Range<samplepos_t> range)
{
	RegionReadLock rlock (this);
	boost::shared_ptr<RegionList> rlist (new RegionList);

	for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {
		if ((*i)->last_sample() >= range.from && (*i)->last_sample() <= range.to) {
			rlist->push_back (*i);
		}
	}

	return rlist;
}

boost::shared_ptr<RegionList>
Playlist::regions_touched (samplepos_t start, samplepos_t end)
{
	RegionReadLock rlock (this);
	return regions_touched_locked (start, end);
}

boost::shared_ptr<RegionList>
Playlist::regions_touched_locked (samplepos_t start, samplepos_t end)
{
	boost::shared_ptr<RegionList> rlist (new RegionList);

	for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {
		if ((*i)->coverage (start, end) != Evoral::OverlapNone) {
			rlist->push_back (*i);
		}
	}

	return rlist;
}

samplepos_t
Playlist::find_next_transient (samplepos_t from, int dir)
{
	RegionReadLock rlock (this);
	AnalysisFeatureList points;
	AnalysisFeatureList these_points;

	for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {
		if (dir > 0) {
			if ((*i)->last_sample() < from) {
				continue;
			}
		} else {
			if ((*i)->first_sample() > from) {
				continue;
			}
		}

		(*i)->get_transients (these_points);

		/* add first sample, just, err, because */

		these_points.push_back ((*i)->first_sample());

		points.insert (points.end(), these_points.begin(), these_points.end());
		these_points.clear ();
	}

	if (points.empty()) {
		return -1;
	}

	TransientDetector::cleanup_transients (points, _session.sample_rate(), 3.0);
	bool reached = false;

	if (dir > 0) {
		for (AnalysisFeatureList::const_iterator x = points.begin(); x != points.end(); ++x) {
			if ((*x) >= from) {
				reached = true;
			}

			if (reached && (*x) > from) {
				return *x;
			}
		}
	} else {
		for (AnalysisFeatureList::reverse_iterator x = points.rbegin(); x != points.rend(); ++x) {
			if ((*x) <= from) {
				reached = true;
			}

			if (reached && (*x) < from) {
				return *x;
			}
		}
	}

	return -1;
}

boost::shared_ptr<Region>
Playlist::find_next_region (samplepos_t sample, RegionPoint point, int dir)
{
	RegionReadLock rlock (this);
	boost::shared_ptr<Region> ret;
	samplepos_t closest = max_samplepos;

	bool end_iter = false;

	for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {

		if(end_iter) break;

		sampleoffset_t distance;
		boost::shared_ptr<Region> r = (*i);
		samplepos_t pos = 0;

		switch (point) {
		case Start:
			pos = r->first_sample ();
			break;
		case End:
			pos = r->last_sample ();
			break;
		case SyncPoint:
			pos = r->sync_position ();
			break;
		}

		switch (dir) {
		case 1: /* forwards */

			if (pos > sample) {
				if ((distance = pos - sample) < closest) {
					closest = distance;
					ret = r;
					end_iter = true;
				}
			}

			break;

		default: /* backwards */

			if (pos < sample) {
				if ((distance = sample - pos) < closest) {
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

samplepos_t
Playlist::find_next_region_boundary (samplepos_t sample, int dir)
{
	RegionReadLock rlock (this);

	samplepos_t closest = max_samplepos;
	samplepos_t ret = -1;

	if (dir > 0) {

		for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {

			boost::shared_ptr<Region> r = (*i);
			sampleoffset_t distance;
			const samplepos_t first_sample = r->first_sample();
			const samplepos_t last_sample = r->last_sample();

			if (first_sample > sample) {

				distance = first_sample - sample;

				if (distance < closest) {
					ret = first_sample;
					closest = distance;
				}
			}

			if (last_sample > sample) {

				distance = last_sample - sample;

				if (distance < closest) {
					ret = last_sample;
					closest = distance;
				}
			}
		}

	} else {

		for (RegionList::reverse_iterator i = regions.rbegin(); i != regions.rend(); ++i) {

			boost::shared_ptr<Region> r = (*i);
			sampleoffset_t distance;
			const samplepos_t first_sample = r->first_sample();
			const samplepos_t last_sample = r->last_sample();

			if (last_sample < sample) {

				distance = sample - last_sample;

				if (distance < closest) {
					ret = last_sample;
					closest = distance;
				}
			}

			if (first_sample < sample) {

				distance = sample - first_sample;

				if (distance < closest) {
					ret = first_sample;
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
		_session.set_dirty();
	}
}

void
Playlist::rdiff (vector<Command*>& cmds) const
{
	RegionReadLock rlock (const_cast<Playlist *> (this));
	Stateful::rdiff (cmds);
}

void
Playlist::clear_owned_changes ()
{
	RegionReadLock rlock (this);
	Stateful::clear_owned_changes ();
}

void
Playlist::update (const RegionListProperty::ChangeRecord& change)
{
	DEBUG_TRACE (DEBUG::Properties, string_compose ("Playlist %1 updates from a change record with %2 adds %3 removes\n",
				name(), change.added.size(), change.removed.size()));

	freeze ();
	{
		RegionWriteLock rlock (this);
		/* add the added regions */
		for (RegionListProperty::ChangeContainer::const_iterator i = change.added.begin(); i != change.added.end(); ++i) {
			add_region_internal ((*i), (*i)->position(), rlock.thawlist);
		}
		/* remove the removed regions */
		for (RegionListProperty::ChangeContainer::const_iterator i = change.removed.begin(); i != change.removed.end(); ++i) {
			remove_region_internal (*i, rlock.thawlist);
		}
	}

	thaw ();
}

int
Playlist::set_state (const XMLNode& node, int version)
{
	XMLNode *child;
	XMLNodeList nlist;
	XMLNodeConstIterator niter;
	XMLPropertyConstIterator piter;
	boost::shared_ptr<Region> region;
	string region_name;
	bool seen_region_nodes = false;
	int ret = 0;

	in_set_state++;

	if (node.name() != "Playlist") {
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

	node.get_property (X_("combine-ops"), _combine_ops);

	string shared_ids;
	if (node.get_property (X_("shared-with-ids"), shared_ids)) {
		if (!shared_ids.empty()) {
			vector<string> result;
			::split (shared_ids, result, ',');
			vector<string>::iterator it = result.begin();
			for (; it != result.end(); ++it) {
				_shared_with_ids.push_back (PBD::ID(*it));
			}
		}
	}

	clear (true);

	nlist = node.children();

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		child = *niter;

		if (child->name() == "Region") {

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

	if (seen_region_nodes && regions.empty()) {
		ret = -1;
	}

	thaw ();
	notify_contents_changed ();

	in_set_state--;
	first_set_state = false;

	return ret;
}

XMLNode&
Playlist::get_state()
{
	return state (true);
}

XMLNode&
Playlist::get_template()
{
	return state (false);
}

/** @param full_state true to include regions in the returned state, otherwise false.
 */
XMLNode&
Playlist::state (bool full_state)
{
	XMLNode *node = new XMLNode (X_("Playlist"));

	node->set_property (X_("id"), id());
	node->set_property (X_("name"), name());
	node->set_property (X_("type"), _type);
	node->set_property (X_("orig-track-id"), _orig_track_id);

	string shared_ids;
	list<PBD::ID>::const_iterator it = _shared_with_ids.begin();
	for (; it != _shared_with_ids.end(); ++it) {
		shared_ids += "," + (*it).to_s();
	}
	if (!shared_ids.empty()) {
		shared_ids.erase(0,1);
	}

	node->set_property (X_("shared-with-ids"), shared_ids);
	node->set_property (X_("frozen"), _frozen);

	if (full_state) {
		RegionReadLock rlock (this);

		node->set_property ("combine-ops", _combine_ops);

		for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {
			assert ((*i)->sources().size() > 0 && (*i)->master_sources().size() > 0);
			node->add_child_nocopy ((*i)->get_state());
		}
	}

	if (_extra_xml) {
		node->add_child_copy (*_extra_xml);
	}

	return *node;
}

bool
Playlist::empty() const
{
	RegionReadLock rlock (const_cast<Playlist *>(this));
	return regions.empty();
}

uint32_t
Playlist::n_regions() const
{
	RegionReadLock rlock (const_cast<Playlist *>(this));
	return regions.size();
}

/** @return true if the all_regions list is empty, ie this playlist
 *  has never had a region added to it.
 */
bool
Playlist::all_regions_empty() const
{
	RegionReadLock rl (const_cast<Playlist *> (this));
	return all_regions.empty();
}

pair<samplepos_t, samplepos_t>
Playlist::get_extent () const
{
	if (_cached_extent) {
		return _cached_extent.value ();
	}

	RegionReadLock rlock (const_cast<Playlist *>(this));
	_cached_extent = _get_extent ();
	return _cached_extent.value ();
}

pair<samplepos_t, samplepos_t>
Playlist::get_extent_with_endspace () const
{
	pair<samplepos_t, samplepos_t> l = get_extent();
	l.second += _end_space;
	return l;
}

pair<samplepos_t, samplepos_t>
Playlist::_get_extent () const
{
	pair<samplepos_t, samplepos_t> ext (max_samplepos, 0);

	if (regions.empty()) {
		ext.first = 0;
		return ext;
	}

	for (RegionList::const_iterator i = regions.begin(); i != regions.end(); ++i) {
		pair<samplepos_t, samplepos_t> const e ((*i)->position(), (*i)->position() + (*i)->length());
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
Playlist::bump_name (string name, Session &session)
{
	string newname = name;

	do {
		newname = bump_name_once (newname, '.');
	} while (session.playlists()->by_name (newname)!=NULL);

	return newname;
}


layer_t
Playlist::top_layer() const
{
	RegionReadLock rlock (const_cast<Playlist *> (this));
	layer_t top = 0;

	for (RegionList::const_iterator i = regions.begin(); i != regions.end(); ++i) {
		top = max (top, (*i)->layer());
	}
	return top;
}

void
Playlist::set_edit_mode (EditMode mode)
{
	_edit_mode = mode;
}

struct RelayerSort {
	bool operator () (boost::shared_ptr<Region> a, boost::shared_ptr<Region> b) {
		return a->layering_index() < b->layering_index();
	}
};

/** Set a new layer for a region.  This adjusts the layering indices of all
 *  regions in the playlist to put the specified region in the appropriate
 *  place.  The actual layering will be fixed up when relayer() happens.
 */
void
Playlist::set_layer (boost::shared_ptr<Region> region, double new_layer)
{
	/* Remove the layer we are setting from our region list, and sort it
	 *  using the layer indeces.
	 */

	RegionList copy = regions.rlist();
	copy.remove (region);
	copy.sort (RelayerSort ());

	/* Put region back in the right place */
	RegionList::iterator i = copy.begin();
	while (i != copy.end ()) {
		if ((*i)->layer() > new_layer) {
			break;
		}
		++i;
	}

	copy.insert (i, region);

	setup_layering_indices (copy);
}

void
Playlist::setup_layering_indices (RegionList const & regions)
{
	uint64_t j = 0;

	for (RegionList::const_iterator k = regions.begin(); k != regions.end(); ++k) {
		(*k)->set_layering_index (j++);
	}
}

struct LaterHigherSort {
	bool operator () (boost::shared_ptr<Region> a, boost::shared_ptr<Region> b) {
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

	/* Build up a new list of regions on each layer, stored in a set of lists
	 * each of which represent some period of time on some layer.  The idea
	 * is to avoid having to search the entire region list to establish whether
	 * each region overlaps another */

	/* how many pieces to divide this playlist's time up into */
	int const divisions = 512;

	/* find the start and end positions of the regions on this playlist */
	samplepos_t start = INT64_MAX;
	samplepos_t end = 0;
	for (RegionList::const_iterator i = regions.begin(); i != regions.end(); ++i) {
		start = min (start, (*i)->position());
		end = max (end, (*i)->position() + (*i)->length());
	}

	/* hence the size of each time division */
	double const division_size = (end - start) / double (divisions);

	vector<vector<RegionList> > layers;
	layers.push_back (vector<RegionList> (divisions));

	/* Sort our regions into layering index order (for manual layering) or position order (for later is higher)*/
	RegionList copy = regions.rlist();
	switch (Config->get_layer_model()) {
		case LaterHigher:
			copy.sort (LaterHigherSort ());
			break;
		case Manual:
			copy.sort (RelayerSort ());
			break;
	}

	DEBUG_TRACE (DEBUG::Layering, "relayer() using:\n");
	for (RegionList::iterator i = copy.begin(); i != copy.end(); ++i) {
		DEBUG_TRACE (DEBUG::Layering, string_compose ("\t%1 %2\n", (*i)->name(), (*i)->layering_index()));
	}

	for (RegionList::iterator i = copy.begin(); i != copy.end(); ++i) {

		/* find the time divisions that this region covers; if there are no regions on the list,
		 * division_size will equal 0 and in this case we'll just say that
		 * start_division = end_division = 0.
		 */
		int start_division = 0;
		int end_division = 0;

		if (division_size > 0) {
			start_division = floor ( ((*i)->position() - start) / division_size);
			end_division = floor ( ((*i)->position() + (*i)->length() - start) / division_size );
			if (end_division == divisions) {
				end_division--;
			}
		}

		assert (divisions == 0 || end_division < divisions);

		/* find the lowest layer that this region can go on */
		size_t j = layers.size();
		while (j > 0) {
			/* try layer j - 1; it can go on if it overlaps no other region
			 * that is already on that layer
			 */

			bool overlap = false;
			for (int k = start_division; k <= end_division; ++k) {
				RegionList::iterator l = layers[j-1][k].begin ();
				while (l != layers[j-1][k].end()) {
					if ((*l)->overlap_equivalent (*i)) {
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

		if (j == layers.size()) {
			/* we need a new layer for this region */
			layers.push_back (vector<RegionList> (divisions));
		}

		/* put a reference to this region in each of the divisions that it exists in */
		for (int k = start_division; k <= end_division; ++k) {
			layers[j][k].push_back (*i);
		}

		(*i)->set_layer (j);
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
Playlist::raise_region (boost::shared_ptr<Region> region)
{
	set_layer (region, region->layer() + 1.5);
	relayer ();
}

void
Playlist::lower_region (boost::shared_ptr<Region> region)
{
	set_layer (region, region->layer() - 1.5);
	relayer ();
}

void
Playlist::raise_region_to_top (boost::shared_ptr<Region> region)
{
	set_layer (region, DBL_MAX);
	relayer ();
}

void
Playlist::lower_region_to_bottom (boost::shared_ptr<Region> region)
{
	set_layer (region, -0.5);
	relayer ();
}

void
Playlist::nudge_after (samplepos_t start, samplecnt_t distance, bool forwards)
{
	RegionList::iterator i;
	bool moved = false;

	_nudging = true;

	{
		RegionWriteLock rlock (const_cast<Playlist *> (this));

		for (i = regions.begin(); i != regions.end(); ++i) {

			if ((*i)->position() >= start) {

				samplepos_t new_pos;

				if (forwards) {

					if ((*i)->last_sample() > max_samplepos - distance) {
						new_pos = max_samplepos - (*i)->length();
					} else {
						new_pos = (*i)->position() + distance;
					}

				} else {

					if ((*i)->position() > distance) {
						new_pos = (*i)->position() - distance;
					} else {
						new_pos = 0;
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
Playlist::uses_source (boost::shared_ptr<const Source> src, bool shallow) const
{
	RegionReadLock rlock (const_cast<Playlist*> (this));

	for (set<boost::shared_ptr<Region> >::const_iterator r = all_regions.begin(); r != all_regions.end(); ++r) {
		/* Note: passing the second argument as false can cause at best
		 * incredibly deep and time-consuming recursion, and at worst
		 * cycles if the user has managed to create cycles of reference
		 * between compound regions. We generally only this during
		 * cleanup, and @param shallow is passed as true.
		 */
		if ((*r)->uses_source (src, shallow)) {
			return true;
		}
	}

	return false;
}


boost::shared_ptr<Region>
Playlist::find_region (const ID& id) const
{
	RegionReadLock rlock (const_cast<Playlist*> (this));

	/* searches all regions currently in use by the playlist */

	for (RegionList::const_iterator i = regions.begin(); i != regions.end(); ++i) {
		if ((*i)->id() == id) {
			return *i;
		}
	}

	return boost::shared_ptr<Region> ();
}

uint32_t
Playlist::region_use_count (boost::shared_ptr<Region> r) const
{
	RegionReadLock rlock (const_cast<Playlist*> (this));
	uint32_t cnt = 0;

	for (RegionList::const_iterator i = regions.begin(); i != regions.end(); ++i) {
		if ((*i) == r) {
			cnt++;
		}
	}

	RegionFactory::CompoundAssociations& cassocs (RegionFactory::compound_associations());
	for (RegionFactory::CompoundAssociations::iterator it = cassocs.begin(); it != cassocs.end(); ++it) {
		/* check if region is used in a compound */
		if (it->second == r) {
			/* region is referenced as 'original' of a compound */
			++cnt;
			break;
		}
		if (r->whole_file() && r->max_source_level() > 0) {
			/* region itself ia a compound.
			 * the compound regions are not referenced -> check regions inside compound
			 */
			const SourceList& sl = r->sources();
			for (SourceList::const_iterator s = sl.begin(); s != sl.end(); ++s) {
				boost::shared_ptr<PlaylistSource> ps = boost::dynamic_pointer_cast<PlaylistSource>(*s);
				if (!ps) continue;
				if (ps->playlist()->region_use_count(it->first)) {
					// break out of both loops
					return ++cnt;
				}
			}
		}
	}
	return cnt;
}

boost::shared_ptr<Region>
Playlist::region_by_id (const ID& id) const
{
	/* searches all regions ever added to this playlist */

	for (set<boost::shared_ptr<Region> >::const_iterator i = all_regions.begin(); i != all_regions.end(); ++i) {
		if ((*i)->id() == id) {
			return *i;
		}
	}
	return boost::shared_ptr<Region> ();
}

void
Playlist::dump () const
{
	boost::shared_ptr<Region> r;

	cerr << "Playlist \"" << _name << "\" " << endl
	     << regions.size() << " regions "
	     << endl;

	for (RegionList::const_iterator i = regions.begin(); i != regions.end(); ++i) {
		r = *i;
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
Playlist::shuffle (boost::shared_ptr<Region> region, int dir)
{
	bool moved = false;

	if (region->locked()) {
		return;
	}

	_shuffling = true;

	{
		RegionWriteLock rlock (const_cast<Playlist*> (this));


		if (dir > 0) {

			RegionList::iterator next;

			for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {
				if ((*i) == region) {
					next = i;
					++next;

					if (next != regions.end()) {

						if ((*next)->locked()) {
							break;
						}

						samplepos_t new_pos;

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

						regions.erase (i); // removes the region from the list */
						next++;
						regions.insert (next, region); // adds it back after next

						moved = true;
					}
					break;
				}
			}
		} else {

			RegionList::iterator prev = regions.end();

			for (RegionList::iterator i = regions.begin(); i != regions.end(); prev = i, ++i) {
				if ((*i) == region) {

					if (prev != regions.end()) {

						if ((*prev)->locked()) {
							break;
						}

						samplepos_t new_pos;
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

						regions.erase (i); // remove region
						regions.insert (prev, region); // insert region before prev

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
		notify_contents_changed();
	}

}

bool
Playlist::region_is_shuffle_constrained (boost::shared_ptr<Region>)
{
	RegionReadLock rlock (const_cast<Playlist*> (this));

	if (regions.size() > 1) {
		return true;
	}

	return false;
}

void
Playlist::ripple (samplepos_t at, samplecnt_t distance, RegionList *exclude)
{
	ripple_locked (at, distance, exclude);
}

void
Playlist::update_after_tempo_map_change ()
{
	RegionWriteLock rlock (const_cast<Playlist*> (this));
	RegionList copy (regions.rlist());

	freeze ();

	for (RegionList::iterator i = copy.begin(); i != copy.end(); ++i) {
		rlock.thawlist.add (*i);
		(*i)->update_after_tempo_map_change ();
	}
	/* possibly causes a contents changed notification (flush_notifications()) */
	thaw ();
}

void
Playlist::foreach_region (boost::function<void(boost::shared_ptr<Region>)> s)
{
	RegionReadLock rl (this);
	for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {
		s (*i);
	}
}

bool
Playlist::has_region_at (samplepos_t const p) const
{
	RegionReadLock (const_cast<Playlist *> (this));

	RegionList::const_iterator i = regions.begin ();
	while (i != regions.end() && !(*i)->covers (p)) {
		++i;
	}

	return (i != regions.end());
}

/** Look from a session sample time and find the start time of the next region
 *  which is on the top layer of this playlist.
 *  @param t Time to look from.
 *  @return Position of next top-layered region, or max_samplepos if there isn't one.
 */
samplepos_t
Playlist::find_next_top_layer_position (samplepos_t t) const
{
	RegionReadLock rlock (const_cast<Playlist *> (this));

	layer_t const top = top_layer ();

	RegionList copy = regions.rlist ();
	copy.sort (RegionSortByPosition ());

	for (RegionList::const_iterator i = copy.begin(); i != copy.end(); ++i) {
		if ((*i)->position() >= t && (*i)->layer() == top) {
			return (*i)->position();
		}
	}

	return max_samplepos;
}

boost::shared_ptr<Region>
Playlist::combine (const RegionList& r)
{
	PropertyList plist;
	uint32_t channels = 0;
	uint32_t layer = 0;
	samplepos_t earliest_position = max_samplepos;
	vector<TwoRegions> old_and_new_regions;
	vector<boost::shared_ptr<Region> > originals;
	vector<boost::shared_ptr<Region> > copies;
	string parent_name;
	string child_name;
	uint32_t max_level = 0;

	/* find the maximum depth of all the regions we're combining */

	for (RegionList::const_iterator i = r.begin(); i != r.end(); ++i) {
		max_level = max (max_level, (*i)->max_source_level());
	}

	parent_name = RegionFactory::compound_region_name (name(), combine_ops(), max_level, true);
	child_name = RegionFactory::compound_region_name (name(), combine_ops(), max_level, false);

	boost::shared_ptr<Playlist> pl = PlaylistFactory::create (_type, _session, parent_name, true);

	for (RegionList::const_iterator i = r.begin(); i != r.end(); ++i) {
		earliest_position = min (earliest_position, (*i)->position());
	}

	/* enable this so that we do not try to create xfades etc. as we add
	 * regions
	 */

	pl->in_partition = true;

	/* sort by position then layer.
	 * route_time_axis passes 'selected_regions' - which is not sorted.
	 * here we need the top-most first, then every layer's region sorted by position.
	 */
	RegionList sorted(r);
	sorted.sort(RegionSortByLayerAndPosition());

	for (RegionList::const_iterator i = sorted.begin(); i != sorted.end(); ++i) {

		/* copy the region */

		boost::shared_ptr<Region> original_region = (*i);
		boost::shared_ptr<Region> copied_region = RegionFactory::create (original_region, false);

		old_and_new_regions.push_back (TwoRegions (original_region,copied_region));
		originals.push_back (original_region);
		copies.push_back (copied_region);

		RegionFactory::add_compound_association (original_region, copied_region);

		/* make position relative to zero */

		pl->add_region (copied_region, original_region->position() - earliest_position);
		copied_region->set_layer (original_region->layer ());

		/* use the maximum number of channels for any region */

		channels = max (channels, original_region->n_channels());

		/* it will go above the layer of the highest existing region */

		layer = max (layer, original_region->layer());
	}

	pl->in_partition = false;

	pre_combine (copies);

	/* now create a new PlaylistSource for each channel in the new playlist */

	SourceList sources;
	pair<samplepos_t,samplepos_t> extent = pl->get_extent();

	for (uint32_t chn = 0; chn < channels; ++chn) {
		sources.push_back (SourceFactory::createFromPlaylist (_type, _session, pl, id(), parent_name, chn, 0, extent.second, false, false));

	}

	/* now a new whole-file region using the list of sources */

	plist.add (Properties::start, 0);
	plist.add (Properties::length, extent.second);
	plist.add (Properties::name, parent_name);
	plist.add (Properties::whole_file, true);

	boost::shared_ptr<Region> parent_region = RegionFactory::create (sources, plist, true);

	/* now the non-whole-file region that we will actually use in the playlist */

	plist.clear ();
	plist.add (Properties::start, 0);
	plist.add (Properties::length, extent.second);
	plist.add (Properties::name, child_name);
	plist.add (Properties::layer, layer+1);

	boost::shared_ptr<Region> compound_region = RegionFactory::create (parent_region, plist, true);

	for (SourceList::iterator s = sources.begin(); s != sources.end(); ++s) {
		boost::dynamic_pointer_cast<PlaylistSource>(*s)->set_owner (compound_region->id());
	}

	/* remove all the selected regions from the current playlist */

	freeze ();

	for (RegionList::const_iterator i = r.begin(); i != r.end(); ++i) {
		remove_region (*i);
	}

	/* do type-specific stuff with the originals and the new compound region */

	post_combine (originals, compound_region);

	/* add the new region at the right location */

	add_region (compound_region, earliest_position);

	_combine_ops++;

	thaw ();

	return compound_region;
}

void
Playlist::uncombine (boost::shared_ptr<Region> target)
{
	boost::shared_ptr<PlaylistSource> pls;
	boost::shared_ptr<const Playlist> pl;
	vector<boost::shared_ptr<Region> > originals;
	vector<TwoRegions> old_and_new_regions;

	// (1) check that its really a compound region

	if ((pls = boost::dynamic_pointer_cast<PlaylistSource>(target->source (0))) == 0) {
		return;
	}

	pl = pls->playlist();

	samplepos_t adjusted_start = 0; // gcc isn't smart enough
	samplepos_t adjusted_end = 0;   // gcc isn't smart enough

	/* the leftmost (earliest) edge of the compound region
	 * starts at zero in its source, or larger if it
	 * has been trimmed or content-scrolled.
	 *
	 * the rightmost (latest) edge of the compound region
	 * relative to its source is the starting point plus
	 * the length of the region.
	 */

	// (2) get all the original regions

	const RegionList& rl (pl->region_list_property().rlist());
	RegionFactory::CompoundAssociations& cassocs (RegionFactory::compound_associations());
	sampleoffset_t move_offset = 0;

	/* there are three possibilities here:
	   1) the playlist that the playlist source was based on
	   is us, so just add the originals (which belonged to
	   us anyway) back in the right place.

	   2) the playlist that the playlist source was based on
	   is NOT us, so we need to make copies of each of
	   the original regions that we find, and add them
	   instead.

	   3) target region is a copy of a compount region previously
	   created. In this case we will also need to make copies ot each of
	   the original regions, and add them instead.
	*/

	const bool need_copies = (boost::dynamic_pointer_cast<PlaylistSource> (pls)->owner() != target->id()) ||
		(pls->original() != id());

	for (RegionList::const_iterator i = rl.begin(); i != rl.end(); ++i) {

		boost::shared_ptr<Region> current (*i);

		RegionFactory::CompoundAssociations::iterator ca = cassocs.find (*i);

		if (ca == cassocs.end()) {
			continue;
		}

		boost::shared_ptr<Region> original (ca->second);

		bool modified_region;

		if (i == rl.begin()) {
			move_offset = (target->position() - original->position()) - target->start();
			adjusted_start = original->position() + target->start();
			adjusted_end = adjusted_start + target->length();
		}

		if (need_copies) {
			samplepos_t pos = original->position();
			/* make a copy, but don't announce it */
			original = RegionFactory::create (original, false);
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
		case Evoral::OverlapNone:
			/* original region does not cover any part
			 * of the current state of the compound region
			 */
			continue;

		case Evoral::OverlapInternal:
			/* overlap is just a small piece inside the
			 * original so trim both ends
			 */
			original->trim_to (adjusted_start, adjusted_end - adjusted_start);
			modified_region = true;
			break;

		case Evoral::OverlapExternal:
			/* overlap fully covers original, so leave it as is */
			break;

		case Evoral::OverlapEnd:
			/* overlap starts within but covers end, so trim the front of the region */
			original->trim_front (adjusted_start);
			modified_region = true;
			break;

		case Evoral::OverlapStart:
			/* overlap covers start but ends within, so
			 * trim the end of the region.
			 */
			original->trim_end (adjusted_end);
			modified_region = true;
			break;
		}

		if (move_offset) {
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
		old_and_new_regions.push_back (TwoRegions (*i, original));
	}

	pre_uncombine (originals, target);

	in_partition = true;
	freeze ();

	// (3) remove the compound region

	remove_region (target);

	// (4) add the constituent regions

	for (vector<boost::shared_ptr<Region> >::iterator i = originals.begin(); i != originals.end(); ++i) {
		add_region ((*i), (*i)->position());
		set_layer((*i), (*i)->layer());
		if (!RegionFactory::region_by_id((*i)->id())) {
			RegionFactory::map_add(*i);
		}
	}

	in_partition = false;
	thaw ();
}

void
Playlist::fade_range (list<AudioRange>& ranges)
{
	RegionReadLock rlock (this);
	for (list<AudioRange>::iterator r = ranges.begin(); r != ranges.end(); ) {
		list<AudioRange>::iterator tmpr = r;
		++tmpr;
		for (RegionList::const_iterator i = regions.begin(); i != regions.end(); ) {
			RegionList::const_iterator tmpi = i;
			++tmpi;
			(*i)->fade_range ((*r).start, (*r).end);
			i = tmpi;
		}
		r = tmpr;
	}
}

uint32_t
Playlist::max_source_level () const
{
	RegionReadLock rlock (const_cast<Playlist *> (this));
	uint32_t lvl = 0;

	for (RegionList::const_iterator i = regions.begin(); i != regions.end(); ++i) {
		lvl = max (lvl, (*i)->max_source_level());
	}

	return lvl;
}

void
Playlist::set_orig_track_id (const PBD::ID& id)
{
	if (shared_with(id)) {
		// Swap 'shared_id' / origin_track_id
		unshare_with (id);
		share_with (_orig_track_id);
	}
	_orig_track_id = id;
}

void
Playlist::share_with (const PBD::ID& id)
{
	if (!shared_with(id)) {
		_shared_with_ids.push_back (id);
	}
}

void
Playlist::unshare_with (const PBD::ID& id)
{
	list<PBD::ID>::iterator it = _shared_with_ids.begin ();
	while (it != _shared_with_ids.end()) {
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
	bool shared = false;
	list<PBD::ID>::const_iterator it = _shared_with_ids.begin ();
	while (it != _shared_with_ids.end() && !shared) {
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
	_shared_with_ids.clear();
}

/** Take a list of ranges, coalesce any that can be coalesced, then call
 *  check_crossfades for each one.
 */
void
Playlist::coalesce_and_check_crossfades (list<Evoral::Range<samplepos_t> > ranges)
{
	/* XXX: it's a shame that this coalesce algorithm also exists in
	 * TimeSelection::consolidate().
	 */

	/* XXX: xfade: this is implemented in Evoral::RangeList */

restart:
	for (list<Evoral::Range<samplepos_t> >::iterator i = ranges.begin(); i != ranges.end(); ++i) {
		for (list<Evoral::Range<samplepos_t> >::iterator j = ranges.begin(); j != ranges.end(); ++j) {

			if (i == j) {
				continue;
			}

			// XXX i->from can be > i->to - is this right? coverage() will return OverlapNone in this case
			if (Evoral::coverage (i->from, i->to, j->from, j->to) != Evoral::OverlapNone) {
				i->from = min (i->from, j->from);
				i->to = max (i->to, j->to);
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
