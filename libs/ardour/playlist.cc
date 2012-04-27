/*
    Copyright (C) 2000-2003 Paul Davis

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

#include <stdint.h>
#include <set>
#include <fstream>
#include <algorithm>
#include <unistd.h>
#include <cerrno>
#include <string>
#include <climits>

#include <boost/lexical_cast.hpp>

#include "pbd/convert.h"
#include "pbd/failed_constructor.h"
#include "pbd/stateful_diff_command.h"
#include "pbd/xml++.h"

#include "ardour/debug.h"
#include "ardour/playlist.h"
#include "ardour/session.h"
#include "ardour/region.h"
#include "ardour/region_factory.h"
#include "ardour/region_sorters.h"
#include "ardour/playlist_factory.h"
#include "ardour/playlist_source.h"
#include "ardour/transient_detector.h"
#include "ardour/session_playlists.h"
#include "ardour/source_factory.h"

#include "i18n.h"

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
	/* All regions (even those which are deleted) have their state saved by other
	   code, so we can just store ID here.
	*/

	node.add_property ("id", region->id().to_s ());
}

boost::shared_ptr<Region>
RegionListProperty::get_content_from_xml (XMLNode const & node) const
{
	XMLProperty const * prop = node.property ("id");
	assert (prop);

	PBD::ID id (prop->value ());

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
	const XMLProperty* prop = node.property("type");
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
{
	init (hide);

	RegionList tmp;
	other->copy_regions (tmp);

	in_set_state++;

	for (list<boost::shared_ptr<Region> >::iterator x = tmp.begin(); x != tmp.end(); ++x) {
		add_region_internal( (*x), (*x)->position());
	}

	in_set_state--;

	_splicing  = other->_splicing;
	_nudging   = other->_nudging;
	_edit_mode = other->_edit_mode;

	in_set_state = 0;
	first_set_state = false;
	in_flush = false;
	in_partition = false;
	subcnt = 0;
	_frozen = other->_frozen;
}

Playlist::Playlist (boost::shared_ptr<const Playlist> other, framepos_t start, framecnt_t cnt, string str, bool hide)
	: SessionObject(other->_session, str)
	, regions (*this)
	, _type(other->_type)
	, _orig_track_id (other->_orig_track_id)
{
	RegionLock rlock2 (const_cast<Playlist*> (other.get()));

	framepos_t end = start + cnt - 1;

	init (hide);

	in_set_state++;

	for (RegionList::const_iterator i = other->regions.begin(); i != other->regions.end(); ++i) {

		boost::shared_ptr<Region> region;
		boost::shared_ptr<Region> new_region;
		frameoffset_t offset = 0;
		framepos_t position = 0;
		framecnt_t len = 0;
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

		plist.add (Properties::start, region->start() + offset);
		plist.add (Properties::length, len);
		plist.add (Properties::name, new_name);
		plist.add (Properties::layer, region->layer());
		plist.add (Properties::layering_index, region->layering_index());

		new_region = RegionFactory::RegionFactory::create (region, plist);

		add_region_internal (new_region, position);
	}

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
	RegionLock rlock (const_cast<Playlist *> (this));

	for (RegionList::const_iterator i = regions.begin(); i != regions.end(); ++i) {
		newlist.push_back (RegionFactory::RegionFactory::create (*i, true));
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
	_shuffling = false;
	_nudging = false;
	in_set_state = 0;
	in_undo = false;
	_edit_mode = Config->get_edit_mode();
	in_flush = false;
	in_partition = false;
	subcnt = 0;
	_frozen = false;
	_combine_ops = 0;

	_session.history().BeginUndoRedo.connect_same_thread (*this, boost::bind (&Playlist::begin_undo, this));
	_session.history().EndUndoRedo.connect_same_thread (*this, boost::bind (&Playlist::end_undo, this));

	ContentsChanged.connect_same_thread (*this, boost::bind (&Playlist::mark_session_dirty, this));
}

Playlist::~Playlist ()
{
	DEBUG_TRACE (DEBUG::Destruction, string_compose ("Playlist %1 destructor\n", _name));

	{
		RegionLock rl (this);

		for (set<boost::shared_ptr<Region> >::iterator i = all_regions.begin(); i != all_regions.end(); ++i) {
			(*i)->set_playlist (boost::shared_ptr<Playlist>());
		}
	}

	/* GoingAway must be emitted by derived classes */
}

void
Playlist::_set_sort_id ()
{
	/*
	  Playlists are given names like <track name>.<id>
	  or <track name>.<edit group name>.<id> where id
	  is an integer. We extract the id and sort by that.
	*/

	size_t dot_position = _name.val().find_last_of(".");

	if (dot_position == string::npos) {
		_sort_id = 0;
	} else {
		string t = _name.val().substr(dot_position + 1);

		try {
			_sort_id = boost::lexical_cast<int>(t);
		}

		catch (boost::bad_lexical_cast e) {
			_sort_id = 0;
		}
	}
}

bool
Playlist::set_name (const string& str)
{
	/* in a typical situation, a playlist is being used
	   by one diskstream and also is referenced by the
	   Session. if there are more references than that,
	   then don't change the name.
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
 CHANGE NOTIFICATION HANDLING

 Notifications must be delayed till the region_lock is released. This
 is necessary because handlers for the signals may need to acquire
 the lock (e.g. to read from the playlist).
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
	Evoral::RangeMove<framepos_t> const move (r->last_position (), r->length (), r->position ());

	if (holding_state ()) {

		pending_range_moves.push_back (move);

	} else {

		list< Evoral::RangeMove<framepos_t> > m;
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

	Evoral::Range<framepos_t> const extra (r->position(), r->last_position());

	if (holding_state ()) {

		pending_region_extensions.push_back (extra);

	} else {

		list<Evoral::Range<framepos_t> > r;
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

	Evoral::Range<framepos_t> const extra (r->position() + r->last_length(), r->position() + r->length());

	if (holding_state ()) {

		pending_region_extensions.push_back (extra);

	} else {

		list<Evoral::Range<framepos_t> > r;
		r.push_back (extra);
		RegionsExtended (r);
	}
}


void
Playlist::notify_region_added (boost::shared_ptr<Region> r)
{
	/* the length change might not be true, but we have to act
	   as though it could be.
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
	   bounds (it could be based on selection order, for example).
	   so, to preserve layering in the "most recently moved is higher"
	   model, sort them by existing layer, then timestamp them.
	*/

	// RegionSortByLayer cmp;
	// pending_bounds.sort (cmp);

	list<Evoral::Range<framepos_t> > crossfade_ranges;

	for (RegionList::iterator r = pending_bounds.begin(); r != pending_bounds.end(); ++r) {
		crossfade_ranges.push_back ((*r)->last_range ());
		crossfade_ranges.push_back ((*r)->range ());
	}

	for (s = pending_removes.begin(); s != pending_removes.end(); ++s) {
		crossfade_ranges.push_back ((*s)->range ());
		remove_dependents (*s);
		RegionRemoved (boost::weak_ptr<Region> (*s)); /* EMIT SIGNAL */
	}
	
	for (s = pending_adds.begin(); s != pending_adds.end(); ++s) {
		crossfade_ranges.push_back ((*s)->range ());
		/* don't emit RegionAdded signal until relayering is done,
		   so that the region is fully setup by the time
		   anyone hears that its been added
		*/
	}

	if (((regions_changed || pending_contents_change) && !in_set_state) || pending_layering) {
		relayer ();
	}

	 if (regions_changed || pending_contents_change) {
		 pending_contents_change = false;
		 ContentsChanged (); /* EMIT SIGNAL */
	 }

	 for (s = pending_adds.begin(); s != pending_adds.end(); ++s) {
		 (*s)->clear_changes ();
		 RegionAdded (boost::weak_ptr<Region> (*s)); /* EMIT SIGNAL */
	 }

	 coalesce_and_check_crossfades (crossfade_ranges);

	 if (!pending_range_moves.empty ()) {
		 /* We don't need to check crossfades for these as pending_bounds has
		    already covered it.
		 */
		 RangesMoved (pending_range_moves, from_undo);
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
 }

 /*************************************************************
   PLAYLIST OPERATIONS
  *************************************************************/

/** Note: this calls set_layer (..., DBL_MAX) so it will reset the layering index of region */
 void
 Playlist::add_region (boost::shared_ptr<Region> region, framepos_t position, float times, bool auto_partition)
 {
	 RegionLock rlock (this);
	 times = fabs (times);

	 int itimes = (int) floor (times);

	 framepos_t pos = position;

	 if (times == 1 && auto_partition){
		 partition(pos - 1, (pos + region->length()), true);
	 }

	 if (itimes >= 1) {
		 add_region_internal (region, pos);
		 set_layer (region, DBL_MAX);
		 pos += region->length();
		 --itimes;
	 }


	 /* note that itimes can be zero if we being asked to just
	    insert a single fraction of the region.
	 */

	 for (int i = 0; i < itimes; ++i) {
		 boost::shared_ptr<Region> copy = RegionFactory::create (region, true);
		 add_region_internal (copy, pos);
		 set_layer (copy, DBL_MAX);
		 pos += region->length();
	 }

	 framecnt_t length = 0;

	 if (floor (times) != times) {
		 length = (framecnt_t) floor (region->length() * (times - floor (times)));
		 string name;
		 RegionFactory::region_name (name, region->name(), false);

		 {
			 PropertyList plist;

			 plist.add (Properties::start, region->start());
			 plist.add (Properties::length, length);
			 plist.add (Properties::name, name);
			 plist.add (Properties::layer, region->layer());

			 boost::shared_ptr<Region> sub = RegionFactory::create (region, plist);
			 add_region_internal (sub, pos);
			 set_layer (sub, DBL_MAX);
		 }
	 }

	 possibly_splice_unlocked (position, (pos + length) - position, boost::shared_ptr<Region>());
 }

 void
 Playlist::set_region_ownership ()
 {
	 RegionLock rl (this);
	 RegionList::iterator i;
	 boost::weak_ptr<Playlist> pl (shared_from_this());

	 for (i = regions.begin(); i != regions.end(); ++i) {
		 (*i)->set_playlist (pl);
	 }
 }

 bool
 Playlist::add_region_internal (boost::shared_ptr<Region> region, framepos_t position)
 {
	 if (region->data_type() != _type) {
		 return false;
	 }

	 RegionSortByPosition cmp;

	 if (!first_set_state) {
		 boost::shared_ptr<Playlist> foo (shared_from_this());
		 region->set_playlist (boost::weak_ptr<Playlist>(foo));
	 }

	 region->set_position (position);

	 regions.insert (upper_bound (regions.begin(), regions.end(), region, cmp), region);
	 all_regions.insert (region);

	 possibly_splice_unlocked (position, region->length(), region);

	 if (!holding_state ()) {
		 /* layers get assigned from XML state, and are not reset during undo/redo */
		 relayer ();
	 }

	 /* we need to notify the existence of new region before checking dependents. Ick. */

	 notify_region_added (region);

	 if (!holding_state ()) {
		 check_crossfades (region->range ());
	 }

	 region->PropertyChanged.connect_same_thread (region_state_changed_connections, boost::bind (&Playlist::region_changed_proxy, this, _1, boost::weak_ptr<Region> (region)));

	 return true;
 }

 void
 Playlist::replace_region (boost::shared_ptr<Region> old, boost::shared_ptr<Region> newr, framepos_t pos)
 {
	 RegionLock rlock (this);

	 bool old_sp = _splicing;
	 _splicing = true;

	 remove_region_internal (old);
	 add_region_internal (newr, pos);
	 set_layer (newr, old->layer ());

	 _splicing = old_sp;

	 possibly_splice_unlocked (pos, old->length() - newr->length());
 }

 void
 Playlist::remove_region (boost::shared_ptr<Region> region)
 {
	 RegionLock rlock (this);
	 remove_region_internal (region);
 }

 int
 Playlist::remove_region_internal (boost::shared_ptr<Region> region)
 {
	 RegionList::iterator i;

	 if (!in_set_state) {
		 /* unset playlist */
		 region->set_playlist (boost::weak_ptr<Playlist>());
	 }

	 /* XXX should probably freeze here .... */

	 for (i = regions.begin(); i != regions.end(); ++i) {
		 if (*i == region) {

			 framepos_t pos = (*i)->position();
			 framecnt_t distance = (*i)->length();

			 regions.erase (i);

			 possibly_splice_unlocked (pos, -distance);

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
	 if (Config->get_use_overlap_equivalency()) {
		 for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {
			 if ((*i)->overlap_equivalent (other)) {
				 results.push_back (*i);
			 }
		 }
	 } else {
		 for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {
			 if ((*i)->equivalent (other)) {
				 results.push_back (*i);
			 }
		 }
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
 Playlist::partition (framepos_t start, framepos_t end, bool cut)
 {
	 RegionList thawlist;

	 partition_internal (start, end, cut, thawlist);

	 for (RegionList::iterator i = thawlist.begin(); i != thawlist.end(); ++i) {
		 (*i)->resume_property_changes ();
	 }
 }

/** Go through each region on the playlist and cut them at start and end, removing the section between
 *  start and end if cutting == true.  Regions that lie entirely within start and end are always
 *  removed.
 */

 void
 Playlist::partition_internal (framepos_t start, framepos_t end, bool cutting, RegionList& thawlist)
 {
	 RegionList new_regions;

	 {
		 RegionLock rlock (this);

		 boost::shared_ptr<Region> region;
		 boost::shared_ptr<Region> current;
		 string new_name;
		 RegionList::iterator tmp;
		 Evoral::OverlapType overlap;
		 framepos_t pos1, pos2, pos3, pos4;

		 in_partition = true;

		 /* need to work from a copy, because otherwise the regions we add during the process
		    get operated on as well.
		 */

		 RegionList copy = regions.rlist();

		 for (RegionList::iterator i = copy.begin(); i != copy.end(); i = tmp) {

			 tmp = i;
			 ++tmp;

			 current = *i;

			 if (current->first_frame() >= start && current->last_frame() < end) {

				 if (cutting) {
					 remove_region_internal (current);
				 }

				 continue;
			 }

			 /* coverage will return OverlapStart if the start coincides
			    with the end point. we do not partition such a region,
			    so catch this special case.
			 */

			 if (current->first_frame() >= end) {
				 continue;
			 }

			 if ((overlap = current->coverage (start, end)) == Evoral::OverlapNone) {
				 continue;
			 }

			 pos1 = current->position();
			 pos2 = start;
			 pos3 = end;
			 pos4 = current->last_frame();

			 if (overlap == Evoral::OverlapInternal) {
				 /* split: we need 3 new regions, the front, middle and end.
				    cut:   we need 2 regions, the front and end.
				 */

				 /*
					  start                 end
			   ---------------*************************------------
					  P1  P2              P3  P4
			   SPLIT:
			   ---------------*****++++++++++++++++====------------
			   CUT
			   ---------------*****----------------====------------

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

					 region = RegionFactory::create (current, plist);
					 add_region_internal (region, start);
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

				 region = RegionFactory::create (current, plist);

				 add_region_internal (region, end);
				 new_regions.push_back (region);

				 /* "front" ***** */

				 current->suspend_property_changes ();
				 thawlist.push_back (current);
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

					 region = RegionFactory::create (current, plist);

					 add_region_internal (region, start);
					 new_regions.push_back (region);
				 }

				 /* front ****** */

				 current->suspend_property_changes ();
				 thawlist.push_back (current);
				 current->cut_end (pos2 - 1);

			 } else if (overlap == Evoral::OverlapStart) {

				 /* split: we need 2 regions: the front and the end.
				    cut: just trim current to skip the cut area
				 */

				 /*
							 start           end
				     ---------------*************************------------
					P2          P1 P3                   P4

				     SPLIT:
				     ---------------****+++++++++++++++++++++------------
				     CUT:
				     -------------------*********************------------

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

					 region = RegionFactory::create (current, plist);

					 add_region_internal (region, pos1);
					 new_regions.push_back (region);
				 }

				 /* end */

				 current->suspend_property_changes ();
				 thawlist.push_back (current);
				 current->trim_front (pos3);
			 } else if (overlap == Evoral::OverlapExternal) {

				 /* split: no split required.
				    cut: remove the region.
				 */

				 /*
					start                                      end
				     ---------------*************************------------
					P2          P1 P3                   P4

				     SPLIT:
				     ---------------*************************------------
				     CUT:
				     ----------------------------------------------------

				 */

				 if (cutting) {
					 remove_region_internal (current);
				 }

				 new_regions.push_back (current);
			 }
		 }

		 in_partition = false;
	 }

	 check_crossfades (Evoral::Range<framepos_t> (start, end));
 }

 boost::shared_ptr<Playlist>
 Playlist::cut_copy (boost::shared_ptr<Playlist> (Playlist::*pmf)(framepos_t, framecnt_t,bool), list<AudioRange>& ranges, bool result_is_hidden)
 {
	 boost::shared_ptr<Playlist> ret;
	 boost::shared_ptr<Playlist> pl;
	 framepos_t start;

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
			    offset to reflect the start of the first range we
			    chopped.
			 */

			 ret->paste (pl, (*i).start - start, 1.0f);
		 }
	 }

	 return ret;
 }

 boost::shared_ptr<Playlist>
 Playlist::cut (list<AudioRange>& ranges, bool result_is_hidden)
 {
	 boost::shared_ptr<Playlist> (Playlist::*pmf)(framepos_t,framecnt_t,bool) = &Playlist::cut;
	 return cut_copy (pmf, ranges, result_is_hidden);
 }

 boost::shared_ptr<Playlist>
 Playlist::copy (list<AudioRange>& ranges, bool result_is_hidden)
 {
	 boost::shared_ptr<Playlist> (Playlist::*pmf)(framepos_t,framecnt_t,bool) = &Playlist::copy;
	 return cut_copy (pmf, ranges, result_is_hidden);
 }

 boost::shared_ptr<Playlist>
 Playlist::cut (framepos_t start, framecnt_t cnt, bool result_is_hidden)
 {
	 boost::shared_ptr<Playlist> the_copy;
	 RegionList thawlist;
	 char buf[32];

	 snprintf (buf, sizeof (buf), "%" PRIu32, ++subcnt);
	 string new_name = _name;
	 new_name += '.';
	 new_name += buf;

	 if ((the_copy = PlaylistFactory::create (shared_from_this(), start, cnt, new_name, result_is_hidden)) == 0) {
		 return boost::shared_ptr<Playlist>();
	 }

	 partition_internal (start, start+cnt-1, true, thawlist);

	 for (RegionList::iterator i = thawlist.begin(); i != thawlist.end(); ++i) {
		 (*i)->resume_property_changes();
	 }

	 return the_copy;
 }

 boost::shared_ptr<Playlist>
 Playlist::copy (framepos_t start, framecnt_t cnt, bool result_is_hidden)
 {
	 char buf[32];

	 snprintf (buf, sizeof (buf), "%" PRIu32, ++subcnt);
	 string new_name = _name;
	 new_name += '.';
	 new_name += buf;

	 cnt = min (_get_extent().second - start, cnt);
	 return PlaylistFactory::create (shared_from_this(), start, cnt, new_name, result_is_hidden);
 }

 int
 Playlist::paste (boost::shared_ptr<Playlist> other, framepos_t position, float times)
 {
	 times = fabs (times);

	 {
		 RegionLock rl1 (this);
		 RegionLock rl2 (other.get());

		 int itimes = (int) floor (times);
		 framepos_t pos = position;
		 framecnt_t const shift = other->_get_extent().second;
		 layer_t top = top_layer ();

		 while (itimes--) {
			 for (RegionList::iterator i = other->regions.begin(); i != other->regions.end(); ++i) {
				 boost::shared_ptr<Region> copy_of_region = RegionFactory::create (*i, true);

				 /* put these new regions on top of all existing ones, but preserve
				    the ordering they had in the original playlist.
				 */

				 add_region_internal (copy_of_region, (*i)->position() + pos);
				 set_layer (copy_of_region, copy_of_region->layer() + top);
			 }
			 pos += shift;
		 }
	 }

	 return 0;
 }


 void
 Playlist::duplicate (boost::shared_ptr<Region> region, framepos_t position, float times)
 {
	 times = fabs (times);

	 RegionLock rl (this);
	 int itimes = (int) floor (times);
	 framepos_t pos = position + 1;

	 while (itimes--) {
		 boost::shared_ptr<Region> copy = RegionFactory::create (region, true);
		 add_region_internal (copy, pos);
		 set_layer (copy, DBL_MAX);
		 pos += region->length();
	 }

	 if (floor (times) != times) {
		 framecnt_t length = (framecnt_t) floor (region->length() * (times - floor (times)));
		 string name;
		 RegionFactory::region_name (name, region->name(), false);

		 {
			 PropertyList plist;

			 plist.add (Properties::start, region->start());
			 plist.add (Properties::length, length);
			 plist.add (Properties::name, name);

			 boost::shared_ptr<Region> sub = RegionFactory::create (region, plist);
			 add_region_internal (sub, pos);
			 set_layer (sub, DBL_MAX);
		 }
	 }
 }

 void
 Playlist::shift (framepos_t at, frameoffset_t distance, bool move_intersected, bool ignore_music_glue)
 {
	 RegionLock rlock (this);
	 RegionList copy (regions.rlist());
	 RegionList fixup;

	 for (RegionList::iterator r = copy.begin(); r != copy.end(); ++r) {

		 if ((*r)->last_frame() < at) {
			 /* too early */
			 continue;
		 }

		 if (at > (*r)->first_frame() && at < (*r)->last_frame()) {
			 /* intersected region */
			 if (!move_intersected) {
				 continue;
			 }
		 }

		 /* do not move regions glued to music time - that
		    has to be done separately.
		 */

		 if (!ignore_music_glue && (*r)->position_lock_style() != AudioTime) {
			 fixup.push_back (*r);
			 continue;
		 }

		 (*r)->set_position ((*r)->position() + distance);
	 }

	 /* XXX: may not be necessary; Region::post_set should do this, I think */
	 for (RegionList::iterator r = fixup.begin(); r != fixup.end(); ++r) {
		 (*r)->recompute_position_from_lock_style ();
	 }
 }

 void
 Playlist::split (framepos_t at)
 {
	 RegionLock rlock (this);
	 RegionList copy (regions.rlist());

	 /* use a copy since this operation can modify the region list
	  */

	 for (RegionList::iterator r = copy.begin(); r != copy.end(); ++r) {
		 _split_region (*r, at);
	 }
 }

 void
 Playlist::split_region (boost::shared_ptr<Region> region, framepos_t playlist_position)
 {
	 RegionLock rl (this);
	 _split_region (region, playlist_position);
 }

 void
 Playlist::_split_region (boost::shared_ptr<Region> region, framepos_t playlist_position)
 {
	 if (!region->covers (playlist_position)) {
		 return;
	 }

	 if (region->position() == playlist_position ||
	     region->last_frame() == playlist_position) {
		 return;
	 }

	 boost::shared_ptr<Region> left;
	 boost::shared_ptr<Region> right;
	 frameoffset_t before;
	 frameoffset_t after;
	 string before_name;
	 string after_name;

	 /* split doesn't change anything about length, so don't try to splice */

	 bool old_sp = _splicing;
	 _splicing = true;

	 before = playlist_position - region->position();
	 after = region->length() - before;

	 RegionFactory::region_name (before_name, region->name(), false);

	 {
		 PropertyList plist;

		 plist.add (Properties::position, region->position ());
		 plist.add (Properties::length, before);
		 plist.add (Properties::name, before_name);
		 plist.add (Properties::left_of_split, true);
		 plist.add (Properties::layering_index, region->layering_index ());
		 plist.add (Properties::layer, region->layer ());

		 /* note: we must use the version of ::create with an offset here,
		    since it supplies that offset to the Region constructor, which
		    is necessary to get audio region gain envelopes right.
		 */
		 left = RegionFactory::create (region, 0, plist);
	 }

	 RegionFactory::region_name (after_name, region->name(), false);

	 {
		 PropertyList plist;

		 plist.add (Properties::position, region->position() + before);
		 plist.add (Properties::length, after);
		 plist.add (Properties::name, after_name);
		 plist.add (Properties::right_of_split, true);
		 plist.add (Properties::layering_index, region->layering_index ());
		 plist.add (Properties::layer, region->layer ());

		 /* same note as above */
		 right = RegionFactory::create (region, before, plist);
	 }

	 add_region_internal (left, region->position());
	 add_region_internal (right, region->position() + before);
	 remove_region_internal (region);

	 _splicing = old_sp;
 }

 void
 Playlist::possibly_splice (framepos_t at, framecnt_t distance, boost::shared_ptr<Region> exclude)
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
 Playlist::possibly_splice_unlocked (framepos_t at, framecnt_t distance, boost::shared_ptr<Region> exclude)
 {
	 if (_splicing || in_set_state) {
		 /* don't respond to splicing moves or state setting */
		 return;
	 }

	 if (_edit_mode == Splice) {
		 splice_unlocked (at, distance, exclude);
	 }
 }

 void
 Playlist::splice_locked (framepos_t at, framecnt_t distance, boost::shared_ptr<Region> exclude)
 {
	 {
		 RegionLock rl (this);
		 core_splice (at, distance, exclude);
	 }
 }

 void
 Playlist::splice_unlocked (framepos_t at, framecnt_t distance, boost::shared_ptr<Region> exclude)
 {
	 core_splice (at, distance, exclude);
 }

 void
 Playlist::core_splice (framepos_t at, framecnt_t distance, boost::shared_ptr<Region> exclude)
 {
	 _splicing = true;

	 for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {

		 if (exclude && (*i) == exclude) {
			 continue;
		 }

		 if ((*i)->position() >= at) {
			 framepos_t new_pos = (*i)->position() + distance;
			 if (new_pos < 0) {
				 new_pos = 0;
			 } else if (new_pos >= max_framepos - (*i)->length()) {
				 new_pos = max_framepos - (*i)->length();
			 }

			 (*i)->set_position (new_pos);
		 }
	 }

	 _splicing = false;

	 notify_contents_changed ();
 }

 void
 Playlist::region_bounds_changed (const PropertyChange& what_changed, boost::shared_ptr<Region> region)
 {
	 if (in_set_state || _splicing || _nudging || _shuffling) {
		 return;
	 }

	 if (what_changed.contains (Properties::position)) {

		 /* remove it from the list then add it back in
		    the right place again.
		 */

		 RegionSortByPosition cmp;

		 RegionList::iterator i = find (regions.begin(), regions.end(), region);

		 if (i == regions.end()) {
			 /* the region bounds are being modified but its not currently
			    in the region list. we will use its bounds correctly when/if
			    it is added
			 */
			 return;
		 }

		 regions.erase (i);
		 regions.insert (upper_bound (regions.begin(), regions.end(), region, cmp), region);
	 }

	 if (what_changed.contains (Properties::position) || what_changed.contains (Properties::length)) {

		 frameoffset_t delta = 0;

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
			 list<Evoral::Range<framepos_t> > xf;
			 xf.push_back (Evoral::Range<framepos_t> (region->last_range()));
			 xf.push_back (Evoral::Range<framepos_t> (region->range()));
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
	 PropertyChange pos_and_length;
	 bool save = false;

	 if (in_set_state || in_flush) {
		 return false;
	 }

	 our_interests.add (Properties::muted);
	 our_interests.add (Properties::layer);
	 our_interests.add (Properties::opaque);

	 bounds.add (Properties::start);
	 bounds.add (Properties::position);
	 bounds.add (Properties::length);

	 pos_and_length.add (Properties::position);
	 pos_and_length.add (Properties::length);

	 if (what_changed.contains (bounds)) {
		 region_bounds_changed (what_changed, region);
		 save = !(_splicing || _nudging);
	 }

	 if (what_changed.contains (our_interests) && !what_changed.contains (pos_and_length)) {
		 check_crossfades (region->range ());
	 }

	 if (what_changed.contains (Properties::position) && !what_changed.contains (Properties::length)) {
		 notify_region_moved (region);
	 } else if (!what_changed.contains (Properties::position) && what_changed.contains (Properties::length)) {
		 notify_region_end_trimmed (region);
	 } else if (what_changed.contains (Properties::position) && what_changed.contains (Properties::length)) {
		 notify_region_start_trimmed (region);
	 }

	 /* don't notify about layer changes, since we are the only object that can initiate
	    them, and we notify in ::relayer()
	 */

	 if (what_changed.contains (our_interests)) {
		 save = true;
	 }

	 return save;
 }

 void
 Playlist::drop_regions ()
 {
	 RegionLock rl (this);
	 regions.clear ();
	 all_regions.clear ();
 }

 void
 Playlist::sync_all_regions_with_regions ()
 {
	 RegionLock rl (this);

	 all_regions.clear ();

	 for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {
		 all_regions.insert (*i);
	 }
 }

 void
 Playlist::clear (bool with_signals)
 {
	 {
		 RegionLock rl (this);

		 region_state_changed_connections.drop_connections ();

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

 /***********************************************************************
  FINDING THINGS
  **********************************************************************/

boost::shared_ptr<RegionList>
Playlist::regions_at (framepos_t frame)
{
	RegionLock rlock (this);
	return find_regions_at (frame);
}

 uint32_t
 Playlist::count_regions_at (framepos_t frame) const
 {
	 RegionLock rlock (const_cast<Playlist*>(this));
	 uint32_t cnt = 0;

	 for (RegionList::const_iterator i = regions.begin(); i != regions.end(); ++i) {
		 if ((*i)->covers (frame)) {
			 cnt++;
		 }
	 }

	 return cnt;
 }

 boost::shared_ptr<Region>
 Playlist::top_region_at (framepos_t frame)

 {
	 RegionLock rlock (this);
	 boost::shared_ptr<RegionList> rlist = find_regions_at (frame);
	 boost::shared_ptr<Region> region;

	 if (rlist->size()) {
		 RegionSortByLayer cmp;
		 rlist->sort (cmp);
		 region = rlist->back();
	 }

	 return region;
 }

 boost::shared_ptr<Region>
 Playlist::top_unmuted_region_at (framepos_t frame)

 {
	 RegionLock rlock (this);
	 boost::shared_ptr<RegionList> rlist = find_regions_at (frame);

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
Playlist::find_regions_at (framepos_t frame)
{
	/* Caller must hold lock */
	
	boost::shared_ptr<RegionList> rlist (new RegionList);
	
	for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {
		if ((*i)->covers (frame)) {
			rlist->push_back (*i);
		}
	}
	
	return rlist;
}

boost::shared_ptr<RegionList>
Playlist::regions_with_start_within (Evoral::Range<framepos_t> range)
{
	RegionLock rlock (this);
	boost::shared_ptr<RegionList> rlist (new RegionList);

	for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {
		if ((*i)->first_frame() >= range.from && (*i)->first_frame() <= range.to) {
			rlist->push_back (*i);
		}
	}

	return rlist;
}

boost::shared_ptr<RegionList>
Playlist::regions_with_end_within (Evoral::Range<framepos_t> range)
{
	RegionLock rlock (this);
	boost::shared_ptr<RegionList> rlist (new RegionList);

	for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {
		if ((*i)->last_frame() >= range.from && (*i)->last_frame() <= range.to) {
			rlist->push_back (*i);
		}
	}

	return rlist;
}

/** @param start Range start.
 *  @param end Range end.
 *  @return regions which have some part within this range.
 */
boost::shared_ptr<RegionList>
Playlist::regions_touched (framepos_t start, framepos_t end)
{
	RegionLock rlock (this);
	boost::shared_ptr<RegionList> rlist (new RegionList);
	
	for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {
		if ((*i)->coverage (start, end) != Evoral::OverlapNone) {
			rlist->push_back (*i);
		}
	}
	
	return rlist;
}

 framepos_t
 Playlist::find_next_transient (framepos_t from, int dir)
 {
	 RegionLock rlock (this);
	 AnalysisFeatureList points;
	 AnalysisFeatureList these_points;

	 for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {
		 if (dir > 0) {
			 if ((*i)->last_frame() < from) {
				 continue;
			 }
		 } else {
			 if ((*i)->first_frame() > from) {
				 continue;
			 }
		 }

		 (*i)->get_transients (these_points);

		 /* add first frame, just, err, because */

		 these_points.push_back ((*i)->first_frame());

		 points.insert (points.end(), these_points.begin(), these_points.end());
		 these_points.clear ();
	 }

	 if (points.empty()) {
		 return -1;
	 }

	 TransientDetector::cleanup_transients (points, _session.frame_rate(), 3.0);
	 bool reached = false;

	 if (dir > 0) {
		 for (AnalysisFeatureList::iterator x = points.begin(); x != points.end(); ++x) {
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
 Playlist::find_next_region (framepos_t frame, RegionPoint point, int dir)
 {
	 RegionLock rlock (this);
	 boost::shared_ptr<Region> ret;
	 framepos_t closest = max_framepos;

	 bool end_iter = false;

	 for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {

		 if(end_iter) break;

		 frameoffset_t distance;
		 boost::shared_ptr<Region> r = (*i);
		 framepos_t pos = 0;

		 switch (point) {
		 case Start:
			 pos = r->first_frame ();
			 break;
		 case End:
			 pos = r->last_frame ();
			 break;
		 case SyncPoint:
			 pos = r->sync_position ();
			 break;
		 }

		 switch (dir) {
		 case 1: /* forwards */

			 if (pos > frame) {
				 if ((distance = pos - frame) < closest) {
					 closest = distance;
					 ret = r;
					 end_iter = true;
				 }
			 }

			 break;

		 default: /* backwards */

			 if (pos < frame) {
				 if ((distance = frame - pos) < closest) {
					 closest = distance;
					 ret = r;
				 }
			 }
			 else {
				 end_iter = true;
			 }

			 break;
		 }
	 }

	 return ret;
 }

 framepos_t
 Playlist::find_next_region_boundary (framepos_t frame, int dir)
 {
	 RegionLock rlock (this);

	 framepos_t closest = max_framepos;
	 framepos_t ret = -1;

	 if (dir > 0) {

		 for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {

			 boost::shared_ptr<Region> r = (*i);
			 frameoffset_t distance;

			 if (r->first_frame() > frame) {

				 distance = r->first_frame() - frame;

				 if (distance < closest) {
					 ret = r->first_frame();
					 closest = distance;
				 }
			 }

			 if (r->last_frame () > frame) {

				 distance = r->last_frame () - frame;

				 if (distance < closest) {
					 ret = r->last_frame ();
					 closest = distance;
				 }
			 }
		 }

	 } else {

		 for (RegionList::reverse_iterator i = regions.rbegin(); i != regions.rend(); ++i) {

			 boost::shared_ptr<Region> r = (*i);
			 frameoffset_t distance;

			 if (r->last_frame() < frame) {

				 distance = frame - r->last_frame();

				 if (distance < closest) {
					 ret = r->last_frame();
					 closest = distance;
				 }
			 }

			 if (r->first_frame() < frame) {

				 distance = frame - r->first_frame();

				 if (distance < closest) {
					 ret = r->first_frame();
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
	 if (!in_set_state && !holding_state ()) {
		 _session.set_dirty();
	 }
 }

 void
 Playlist::rdiff (vector<Command*>& cmds) const
 {
	 RegionLock rlock (const_cast<Playlist *> (this));
	 Stateful::rdiff (cmds);
 }

 void
 Playlist::clear_owned_changes ()
 {
	 RegionLock rlock (this);
	 Stateful::clear_owned_changes ();
 }

 void
 Playlist::update (const RegionListProperty::ChangeRecord& change)
 {
	 DEBUG_TRACE (DEBUG::Properties, string_compose ("Playlist %1 updates from a change record with %2 adds %3 removes\n",
							 name(), change.added.size(), change.removed.size()));

	 freeze ();
	 /* add the added regions */
	 for (RegionListProperty::ChangeContainer::iterator i = change.added.begin(); i != change.added.end(); ++i) {
		 add_region_internal ((*i), (*i)->position());
	 }
	 /* remove the removed regions */
	 for (RegionListProperty::ChangeContainer::iterator i = change.removed.begin(); i != change.removed.end(); ++i) {
		 remove_region (*i);
	 }

	 thaw ();
 }

 int
 Playlist::set_state (const XMLNode& node, int version)
 {
	 XMLNode *child;
	 XMLNodeList nlist;
	 XMLNodeConstIterator niter;
	 XMLPropertyList plist;
	 XMLPropertyConstIterator piter;
	 XMLProperty *prop;
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

	 plist = node.properties();

	 set_id (node);

	 for (piter = plist.begin(); piter != plist.end(); ++piter) {

		 prop = *piter;

		 if (prop->name() == X_("name")) {
			 _name = prop->value();
			 _set_sort_id ();
		 } else if (prop->name() == X_("orig-diskstream-id")) {
			 /* XXX legacy session: fix up later */
			 _orig_track_id = prop->value ();
		 } else if (prop->name() == X_("orig-track-id")) {
			 _orig_track_id = prop->value ();
		 } else if (prop->name() == X_("frozen")) {
			 _frozen = string_is_affirmative (prop->value());
		 } else if (prop->name() == X_("combine-ops")) {
			 _combine_ops = atoi (prop->value());
		 }
	 }

	 clear (true);

	 nlist = node.children();

	 for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		 child = *niter;

		 if (child->name() == "Region") {

			 seen_region_nodes = true;

			 if ((prop = child->property ("id")) == 0) {
				 error << _("region state node has no ID, ignored") << endmsg;
				 continue;
			 }

			 ID id = prop->value ();

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
				 RegionLock rlock (this);
				 add_region_internal (region, region->position());
			 }
			
			region->resume_property_changes ();

		}
	}

	if (seen_region_nodes && regions.empty()) {
		ret = -1;
	} else {

		/* update dependents, which was not done during add_region_internal
		   due to in_set_state being true
		*/
		
		for (RegionList::iterator r = regions.begin(); r != regions.end(); ++r) {
			check_crossfades ((*r)->range ());
		}
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
	char buf[64];

	node->add_property (X_("id"), id().to_s());
	node->add_property (X_("name"), _name);
	node->add_property (X_("type"), _type.to_string());

	_orig_track_id.print (buf, sizeof (buf));
	node->add_property (X_("orig-track-id"), buf);
	node->add_property (X_("frozen"), _frozen ? "yes" : "no");

	if (full_state) {
		RegionLock rlock (this, false);

		snprintf (buf, sizeof (buf), "%u", _combine_ops);
		node->add_property ("combine-ops", buf);

		for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {
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
	RegionLock rlock (const_cast<Playlist *>(this), false);
	return regions.empty();
}

uint32_t
Playlist::n_regions() const
{
	RegionLock rlock (const_cast<Playlist *>(this), false);
	return regions.size();
}

pair<framepos_t, framepos_t>
Playlist::get_extent () const
{
	RegionLock rlock (const_cast<Playlist *>(this), false);
	return _get_extent ();
}

pair<framepos_t, framepos_t>
Playlist::_get_extent () const
{
	pair<framepos_t, framepos_t> ext (max_framepos, 0);

	if (regions.empty()) {
		ext.first = 0;
		return ext;
	}

	for (RegionList::const_iterator i = regions.begin(); i != regions.end(); ++i) {
		pair<framepos_t, framepos_t> const e ((*i)->position(), (*i)->position() + (*i)->length());
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
	} while (session.playlists->by_name (newname)!=NULL);

	return newname;
}


layer_t
Playlist::top_layer() const
{
	RegionLock rlock (const_cast<Playlist *> (this));
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
	/* Remove the layer we are setting from our region list, and sort it */
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
	list<Evoral::Range<framepos_t> > xf;

	for (RegionList::const_iterator k = regions.begin(); k != regions.end(); ++k) {
		(*k)->set_layering_index (j++);

		Evoral::Range<framepos_t> r ((*k)->first_frame(), (*k)->last_frame());
		xf.push_back (r);
	}

	/* now recheck the entire playlist for crossfades */

	coalesce_and_check_crossfades (xf);
}

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
	   each of which represent some period of time on some layer.  The idea
	   is to avoid having to search the entire region list to establish whether
	   each region overlaps another */

	/* how many pieces to divide this playlist's time up into */
	int const divisions = 512;

	/* find the start and end positions of the regions on this playlist */
	framepos_t start = INT64_MAX;
	framepos_t end = 0;
	for (RegionList::const_iterator i = regions.begin(); i != regions.end(); ++i) {
		start = min (start, (*i)->position());
		end = max (end, (*i)->position() + (*i)->length());
	}

	/* hence the size of each time division */
	double const division_size = (end - start) / double (divisions);

	vector<vector<RegionList> > layers;
	layers.push_back (vector<RegionList> (divisions));

	/* Sort our regions into layering index order */
	RegionList copy = regions.rlist();
	copy.sort (RelayerSort ());

	DEBUG_TRACE (DEBUG::Layering, "relayer() using:\n");
	for (RegionList::iterator i = copy.begin(); i != copy.end(); ++i) {
		DEBUG_TRACE (DEBUG::Layering, string_compose ("\t%1 %2\n", (*i)->name(), (*i)->layering_index()));
	}

	for (RegionList::iterator i = copy.begin(); i != copy.end(); ++i) {

		/* find the time divisions that this region covers; if there are no regions on the list,
		   division_size will equal 0 and in this case we'll just say that
		   start_division = end_division = 0.
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
			   that is already on that layer
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
	   relayering because we just removed the only region on the top layer, nothing will
	   appear to have changed, but the StreamView must still sort itself out.  We could
	   probably keep a note of the top layer last time we relayered, and check that,
	   but premature optimisation &c...
	*/
	notify_layering_changed ();

	/* This relayer() may have been called as a result of a region removal, in which
	   case we need to setup layering indices so account for the one that has just
	   gone away.
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
Playlist::nudge_after (framepos_t start, framecnt_t distance, bool forwards)
{
	RegionList::iterator i;
	bool moved = false;

	_nudging = true;

	{
		RegionLock rlock (const_cast<Playlist *> (this));

		for (i = regions.begin(); i != regions.end(); ++i) {

			if ((*i)->position() >= start) {

				framepos_t new_pos;

				if (forwards) {

					if ((*i)->last_frame() > max_framepos - distance) {
						new_pos = max_framepos - (*i)->length();
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
Playlist::uses_source (boost::shared_ptr<const Source> src) const
{
	RegionLock rlock (const_cast<Playlist*> (this));

	for (set<boost::shared_ptr<Region> >::iterator r = all_regions.begin(); r != all_regions.end(); ++r) {
		if ((*r)->uses_source (src)) {
			return true;
		}
	}

	return false;
}

boost::shared_ptr<Region>
Playlist::find_region (const ID& id) const
{
	RegionLock rlock (const_cast<Playlist*> (this));

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
	RegionLock rlock (const_cast<Playlist*> (this));
	uint32_t cnt = 0;

	for (RegionList::const_iterator i = regions.begin(); i != regions.end(); ++i) {
		if ((*i) == r) {
			cnt++;
		}
	}

	return cnt;
}

boost::shared_ptr<Region>
Playlist::region_by_id (const ID& id) const
{
	/* searches all regions ever added to this playlist */

	for (set<boost::shared_ptr<Region> >::iterator i = all_regions.begin(); i != all_regions.end(); ++i) {
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

	Evoral::Range<framepos_t> old_range = region->range ();

	{
		RegionLock rlock (const_cast<Playlist*> (this));


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

						framepos_t new_pos;

						if ((*next)->position() != region->last_frame() + 1) {
							/* they didn't used to touch, so after shuffle,
							   just have them swap positions.
							*/
							new_pos = (*next)->position();
						} else {
							/* they used to touch, so after shuffle,
							   make sure they still do. put the earlier
							   region where the later one will end after
							   it is moved.
							*/
							new_pos = region->position() + (*next)->length();
						}

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

						framepos_t new_pos;
						if (region->position() != (*prev)->last_frame() + 1) {
							/* they didn't used to touch, so after shuffle,
							   just have them swap positions.
							*/
							new_pos = region->position();
						} else {
							/* they used to touch, so after shuffle,
							   make sure they still do. put the earlier
							   one where the later one will end after
							*/
							new_pos = (*prev)->position() + region->length();
						}

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
	RegionLock rlock (const_cast<Playlist*> (this));

	if (regions.size() > 1) {
		return true;
	}

	return false;
}

void
Playlist::update_after_tempo_map_change ()
{
	RegionLock rlock (const_cast<Playlist*> (this));
	RegionList copy (regions.rlist());

	freeze ();

	for (RegionList::iterator i = copy.begin(); i != copy.end(); ++i) {
		(*i)->update_after_tempo_map_change ();
	}

	thaw ();
}

void
Playlist::foreach_region (boost::function<void(boost::shared_ptr<Region>)> s)
{
	RegionLock rl (this, false);
	for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {
		s (*i);
	}
}

bool
Playlist::has_region_at (framepos_t const p) const
{
	RegionLock (const_cast<Playlist *> (this));

	RegionList::const_iterator i = regions.begin ();
	while (i != regions.end() && !(*i)->covers (p)) {
		++i;
	}

	return (i != regions.end());
}

/** Remove any region that uses a given source */
void
Playlist::remove_region_by_source (boost::shared_ptr<Source> s)
{
	RegionLock rl (this);

	RegionList::iterator i = regions.begin();
	while (i != regions.end()) {
		RegionList::iterator j = i;
		++j;

		if ((*i)->uses_source (s)) {
			remove_region_internal (*i);
		}

		i = j;
	}
}

/** Look from a session frame time and find the start time of the next region
 *  which is on the top layer of this playlist.
 *  @param t Time to look from.
 *  @return Position of next top-layered region, or max_framepos if there isn't one.
 */
framepos_t
Playlist::find_next_top_layer_position (framepos_t t) const
{
	RegionLock rlock (const_cast<Playlist *> (this));

	layer_t const top = top_layer ();

	RegionList copy = regions.rlist ();
	copy.sort (RegionSortByPosition ());

	for (RegionList::const_iterator i = copy.begin(); i != copy.end(); ++i) {
		if ((*i)->position() >= t && (*i)->layer() == top) {
			return (*i)->position();
		}
	}

	return max_framepos;
}

boost::shared_ptr<Region>
Playlist::combine (const RegionList& r)
{
	PropertyList plist;
	uint32_t channels = 0;
	uint32_t layer = 0;
	framepos_t earliest_position = max_framepos;
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

	for (RegionList::const_iterator i = r.begin(); i != r.end(); ++i) {

		/* copy the region */

		boost::shared_ptr<Region> original_region = (*i);
		boost::shared_ptr<Region> copied_region = RegionFactory::create (original_region, false);

		old_and_new_regions.push_back (TwoRegions (original_region,copied_region));
		originals.push_back (original_region);
		copies.push_back (copied_region);

		RegionFactory::add_compound_association (original_region, copied_region);

		/* make position relative to zero */

		pl->add_region (copied_region, original_region->position() - earliest_position);

		/* use the maximum number of channels for any region */

		channels = max (channels, original_region->n_channels());

		/* it will go above the layer of the highest existing region */

		layer = max (layer, original_region->layer());
	}

	pl->in_partition = false;

	pre_combine (copies);

	/* now create a new PlaylistSource for each channel in the new playlist */

	SourceList sources;
	pair<framepos_t,framepos_t> extent = pl->get_extent();

	for (uint32_t chn = 0; chn < channels; ++chn) {
		sources.push_back (SourceFactory::createFromPlaylist (_type, _session, pl, id(), parent_name, chn, 0, extent.second, false, false));

	}

	/* now a new whole-file region using the list of sources */

	plist.add (Properties::start, 0);
	plist.add (Properties::length, extent.second);
	plist.add (Properties::name, parent_name);
	plist.add (Properties::whole_file, true);

	boost::shared_ptr<Region> parent_region = RegionFactory::create (sources, plist, true);

	/* now the non-whole-file region that we will actually use in the
	 * playlist
	 */

	plist.clear ();
	plist.add (Properties::start, 0);
	plist.add (Properties::length, extent.second);
	plist.add (Properties::name, child_name);
	plist.add (Properties::layer, layer+1);

	boost::shared_ptr<Region> compound_region = RegionFactory::create (parent_region, plist, true);

	/* remove all the selected regions from the current playlist
	 */

	freeze ();

	for (RegionList::const_iterator i = r.begin(); i != r.end(); ++i) {
		remove_region (*i);
	}

	/* do type-specific stuff with the originals and the new compound
	   region
	*/

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

	framepos_t adjusted_start = 0; // gcc isn't smart enough
	framepos_t adjusted_end = 0;   // gcc isn't smart enough

	/* the leftmost (earliest) edge of the compound region
	   starts at zero in its source, or larger if it
	   has been trimmed or content-scrolled.

	   the rightmost (latest) edge of the compound region
	   relative to its source is the starting point plus
	   the length of the region.
	*/

	// (2) get all the original regions

	const RegionList& rl (pl->region_list().rlist());
	RegionFactory::CompoundAssociations& cassocs (RegionFactory::compound_associations());
	frameoffset_t move_offset = 0;

	/* there are two possibilities here:
	   1) the playlist that the playlist source was based on
	   is us, so just add the originals (which belonged to
	   us anyway) back in the right place.

	   2) the playlist that the playlist source was based on
	   is NOT us, so we need to make copies of each of
	   the original regions that we find, and add them
	   instead.
	*/
	bool same_playlist = (pls->original() == id());

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

		if (!same_playlist) {
			framepos_t pos = original->position();
			/* make a copy, but don't announce it */
			original = RegionFactory::create (original, false);
			/* the pure copy constructor resets position() to zero,
			   so fix that up.
			*/
			original->set_position (pos);
		}

		/* check to see how the original region (in the
		 * playlist before compounding occured) overlaps
		 * with the new state of the compound region.
		 */

		original->clear_changes ();
		modified_region = false;

		switch (original->coverage (adjusted_start, adjusted_end)) {
		case Evoral::OverlapNone:
			/* original region does not cover any part
			   of the current state of the compound region
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
			/* overlap fully covers original, so leave it
			   as is
			*/
			break;

		case Evoral::OverlapEnd:
			/* overlap starts within but covers end,
			   so trim the front of the region
			*/
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
			/* fix the position to match any movement of the compound region.
			 */
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
	}

	in_partition = false;
	thaw ();
}

uint32_t
Playlist::max_source_level () const
{
	RegionLock rlock (const_cast<Playlist *> (this));
	uint32_t lvl = 0;

	for (RegionList::const_iterator i = regions.begin(); i != regions.end(); ++i) {
		lvl = max (lvl, (*i)->max_source_level());
	}

	return lvl;
}

void
Playlist::set_orig_track_id (const PBD::ID& id)
{
	_orig_track_id = id;
}

void
Playlist::coalesce_and_check_crossfades (list<Evoral::Range<framepos_t> > ranges)
{
	/* XXX: it's a shame that this coalesce algorithm also exists in
	   TimeSelection::consolidate().
	*/

	/* XXX: xfade: this is implemented in Evoral::RangeList */

restart:
	for (list<Evoral::Range<framepos_t> >::iterator i = ranges.begin(); i != ranges.end(); ++i) {
		for (list<Evoral::Range<framepos_t> >::iterator j = ranges.begin(); j != ranges.end(); ++j) {

			if (i == j) {
				continue;
			}

			if (Evoral::coverage (i->from, i->to, j->from, j->to) != Evoral::OverlapNone) {
				i->from = min (i->from, j->from);
				i->to = max (i->to, j->to);
				ranges.erase (j);
				goto restart;
			}
		}
	}

	for (list<Evoral::Range<framepos_t> >::iterator i = ranges.begin(); i != ranges.end(); ++i) {
		check_crossfades (*i);
	}
}
