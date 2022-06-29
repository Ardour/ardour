/*
 * Copyright (C) 2006-2016 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2014-2018 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 André Nusser <andre.nusser@googlemail.com>
 * Copyright (C) 2016-2017 Nick Mainsbridge <mainsbridge@gmail.com>
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

#ifndef __ardour_playlist_h__
#define __ardour_playlist_h__

#include <boost/enable_shared_from_this.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>
#include <list>
#include <map>
#include <set>
#include <string>

#include <sys/stat.h>

#include <glib.h>

#include "pbd/sequence_property.h"
#include "pbd/stateful.h"
#include "pbd/statefuldestructible.h"
#include "pbd/undo.h"
#include "pbd/g_atomic_compat.h"

#include "temporal/range.h"

#include "ardour/ardour.h"
#include "ardour/data_type.h"
#include "ardour/region.h"
#include "ardour/session_object.h"
#include "ardour/thawlist.h"

namespace ARDOUR {

class Session;
class Playlist;
class Crossfade;

namespace Properties {
	/* fake the type, since regions are handled by SequenceProperty which doesn't
	 * care about such things.
	 */
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool> regions;
}

class LIBARDOUR_API RegionListProperty : public PBD::SequenceProperty<std::list<boost::shared_ptr<Region> > >
{
public:
	RegionListProperty (Playlist&);

	RegionListProperty*       clone () const;
	void                      get_content_as_xml (boost::shared_ptr<Region>, XMLNode&) const;
	boost::shared_ptr<Region> get_content_from_xml (XMLNode const&) const;

private:
	RegionListProperty* create () const;

	/* copy construction only by ourselves */
	RegionListProperty (RegionListProperty const& p);

	friend class Playlist;
	/* we live and die with our playlist, no lifetime management needed */
	Playlist& _playlist;
};

class LIBARDOUR_API Playlist : public SessionObject, public boost::enable_shared_from_this<Playlist>
{
public:
	static void make_property_quarks ();

	Playlist (Session&, const XMLNode&, DataType type, bool hidden = false);
	Playlist (Session&, std::string name, DataType type, bool hidden = false);
	Playlist (boost::shared_ptr<const Playlist>, std::string name, bool hidden = false);
	Playlist (boost::shared_ptr<const Playlist>, timepos_t const & start, timepos_t const & cnt, std::string name, bool hidden = false);

	virtual ~Playlist ();

	void update (const RegionListProperty::ChangeRecord&);
	void clear_owned_changes ();
	void rdiff (std::vector<Command*>&) const;

	void rdiff_and_add_command (Session*);

	boost::shared_ptr<Region> region_by_id (const PBD::ID&) const;

	uint32_t max_source_level () const;

	bool set_name (const std::string& str);
	void set_region_ownership ();

	Temporal::TimeDomain time_domain() const;

	/*playlist group IDs (pgroup_id) is a group identifier that is implicitly
	 * or explicitly assigned to playlists so they can be associated with each other.
	 *
	 * For example, when you switch a track's playlist, you can choose to
	 *  switch other tracks to the same pgroup_id
	 *
	 * pgroup_id's should be unique; currently we use a timestamp to avoid duplicates.
	 * pgroup_id's are human-readable strings; use string comparison to find matches.
	 *
	 * To be useful, we want every playlist to be assigned a sensible pgroup_id
	 * Some examples of pgroup_id's getting assigned *explicitly* include:
	 *  when the user makes a new playlist for a track or Track Group
	 *  when the user triggers an action like "new playlist for rec-armed tracks"
	 * Some examples of pgroup_id's getting assigned *implicitly* include:
	 *  the user makes the first recording pass ("take") in an empty playlist
	 *  the user imports tracks.
	 */
	static std::string    generate_pgroup_id();
	std::string           pgroup_id()                     { return _pgroup_id; }
	void                  set_pgroup_id(std::string pgid) { _pgroup_id = pgid; PropertyChanged (Properties::name); }

	virtual void clear (bool with_signals = true);
	virtual void dump () const;

	void use ();
	void release ();

	bool            empty () const;
	bool            used () const          { return _refcnt != 0; }
	int             sort_id () const       { return _sort_id; }
	bool            frozen () const        { return _frozen; }
	const DataType& data_type () const     { return _type; }
	bool            hidden () const        { return _hidden; }
	bool            shared () const        { return !_shared_with_ids.empty (); }

	void set_frozen (bool yn);

	void AddToSoloSelectedList (const Region*);
	void RemoveFromSoloSelectedList (const Region*);
	bool SoloSelectedListIncludes (const Region*);
	bool SoloSelectedActive ();

	void share_with (const PBD::ID&);
	void unshare_with (const PBD::ID&);
	bool shared_with (const PBD::ID&) const;
	void reset_shares ();

	uint32_t n_regions() const;
	bool all_regions_empty() const;
	std::pair<timepos_t, timepos_t> get_extent () const;
	std::pair<timepos_t, timepos_t> get_extent_with_endspace() const;
	layer_t top_layer() const;

	/* Editing operations */

	void add_region (boost::shared_ptr<Region>, timepos_t const & position, float times = 1, bool auto_partition = false);
	void remove_region (boost::shared_ptr<Region>);
	void get_equivalent_regions (boost::shared_ptr<Region>, std::vector<boost::shared_ptr<Region> >&);

	void get_region_list_equivalent_regions (boost::shared_ptr<Region>, std::vector<boost::shared_ptr<Region> >&);
	void get_source_equivalent_regions (boost::shared_ptr<Region>, std::vector<boost::shared_ptr<Region> >&);
	void replace_region (boost::shared_ptr<Region> old, boost::shared_ptr<Region> newr, timepos_t const & pos);
	void split_region (boost::shared_ptr<Region>, timepos_t const & position);
	void split (timepos_t const & at);
	void shift (timepos_t const & at, timecnt_t const & distance, bool move_intersected, bool ignore_music_glue);
	void partition (timepos_t const & start, timepos_t const & end, bool cut = false);
	void duplicate (boost::shared_ptr<Region>, timepos_t & position, float times);
	void duplicate (boost::shared_ptr<Region>, timepos_t & position, timecnt_t const & gap, float times);
	void duplicate_until (boost::shared_ptr<Region>, timepos_t & position, timecnt_t const & gap, timepos_t const & end);
	void duplicate_range (TimelineRange&, float times);
	void duplicate_ranges (std::list<TimelineRange>&, float times);
	void nudge_after (timepos_t const & start, timecnt_t const & distance, bool forwards);
	virtual boost::shared_ptr<Region> combine (const RegionList&);
	virtual void uncombine (boost::shared_ptr<Region>);
	void fade_range (std::list<TimelineRange>&);
	void remove_gaps (timecnt_t const & gap_threshold, timecnt_t const & leave_gap, boost::function<void (timepos_t, timecnt_t)> gap_callback);

	void shuffle (boost::shared_ptr<Region>, int dir);

	void ripple (timepos_t const & at, timecnt_t const & distance, RegionList *exclude);
	void ripple (timepos_t const & at, timecnt_t const & distance, boost::shared_ptr<Region> exclude) {
		 RegionList el;
		 if (exclude) {
			 el.push_back (exclude);
		 }
		 ripple (at, distance, &el);
	}

	void update_after_tempo_map_change ();

	boost::shared_ptr<Playlist> cut  (std::list<TimelineRange>&, bool result_is_hidden = true);
	boost::shared_ptr<Playlist> copy (std::list<TimelineRange>&, bool result_is_hidden = true);
	int                         paste (boost::shared_ptr<Playlist>, timepos_t const & position, float times);

	const RegionListProperty& region_list_property () const
	{
		return regions;
	}

	boost::shared_ptr<RegionList> region_list ();

	boost::shared_ptr<RegionList> regions_at (timepos_t const & sample);
	uint32_t                      count_regions_at (timepos_t const &) const;

	/** @param start Range start.
	 *  @param end Range end.
	 *  @return regions which have some part within this range.
	 */
	boost::shared_ptr<RegionList> regions_touched (timepos_t const & start, timepos_t const & end);
	boost::shared_ptr<RegionList> regions_with_start_within (Temporal::TimeRange);
	boost::shared_ptr<RegionList> regions_with_end_within (Temporal::TimeRange);
	uint32_t                   region_use_count (boost::shared_ptr<Region>) const;
	boost::shared_ptr<Region>  find_region (const PBD::ID&) const;
	boost::shared_ptr<Region>  top_region_at (timepos_t const &);
	boost::shared_ptr<Region>  top_unmuted_region_at (timepos_t const &);
	boost::shared_ptr<Region>  find_next_region (timepos_t const &, RegionPoint point, int dir);
	timepos_t                  find_next_region_boundary (timepos_t const &, int dir);
	bool                       region_is_shuffle_constrained (boost::shared_ptr<Region>);
	bool                       has_region_at (timepos_t const &) const;

	timepos_t                  find_prev_region_start (timepos_t const & sample);

	bool uses_source (boost::shared_ptr<const Source> src, bool shallow = false) const;
	void deep_sources (std::set<boost::shared_ptr<Source> >&) const;

	samplepos_t find_next_transient (timepos_t const & position, int dir);

	void foreach_region (boost::function<void(boost::shared_ptr<Region>)>);

	XMLNode&    get_state () const;
	virtual int set_state (const XMLNode&, int version);
	XMLNode&    get_template ();

	PBD::Signal1<void, bool>                     InUse;
	PBD::Signal0<void>                           ContentsChanged;
	PBD::Signal1<void, boost::weak_ptr<Region> > RegionAdded;
	PBD::Signal1<void, boost::weak_ptr<Region> > RegionRemoved;
	PBD::Signal0<void>                           NameChanged;
	PBD::Signal0<void>                           LayeringChanged;

	/** Emitted when regions have moved (not when regions have only been trimmed) */
	PBD::Signal2<void,std::list< Temporal::RangeMove> const &, bool> RangesMoved;

	/** Emitted when regions are extended; the ranges passed are the new extra time ranges
	    that these regions now occupy.
	*/
	PBD::Signal1<void,std::list< Temporal::Range> const &> RegionsExtended;

	static std::string bump_name (std::string old_name, Session&);

	void freeze ();
	void thaw (bool from_undo = false);

	void raise_region (boost::shared_ptr<Region>);
	void lower_region (boost::shared_ptr<Region>);
	void raise_region_to_top (boost::shared_ptr<Region>);
	void lower_region_to_bottom (boost::shared_ptr<Region>);

	void set_orig_track_id (const PBD::ID& did);
	const PBD::ID& get_orig_track_id () const { return _orig_track_id; }

	/* destructive editing */

	virtual bool destroy_region (boost::shared_ptr<Region>) = 0;

	void sync_all_regions_with_regions ();

	/* special case function used by UI selection objects, which have playlists that actually own the regions
	   within them.
	*/

	void drop_regions ();

	virtual boost::shared_ptr<Crossfade> find_crossfade (const PBD::ID&) const
	{
		return boost::shared_ptr<Crossfade> ();
	}

	timepos_t find_next_top_layer_position (timepos_t const &) const;
	uint32_t combine_ops() const { return _combine_ops; }

	void set_layer (boost::shared_ptr<Region>, double);

	void set_capture_insertion_in_progress (bool yn);

protected:
	friend class Session;

protected:
	class RegionReadLock : public Glib::Threads::RWLock::ReaderLock
	{
	public:
		RegionReadLock (Playlist const * pl)
		    : Glib::Threads::RWLock::ReaderLock (pl->region_lock)
		{
		}
		~RegionReadLock () {}
	};

	class RegionWriteLock : public Glib::Threads::RWLock::WriterLock
	{
	public:
		RegionWriteLock (Playlist* pl, bool do_block_notify = true)
		    : Glib::Threads::RWLock::WriterLock (pl->region_lock)
		    , playlist (pl)
		    , block_notify (do_block_notify)
		{
			if (block_notify) {
				playlist->delay_notifications ();
			}
		}

		~RegionWriteLock ()
		{
			Glib::Threads::RWLock::WriterLock::release ();
			thawlist.release ();
			if (block_notify) {
				playlist->release_notifications ();
			}
		}

		ThawList  thawlist;
		Playlist* playlist;
		bool      block_notify;
	};

	RegionListProperty                   regions;     /* the current list of regions in the playlist */
	std::set<boost::shared_ptr<Region> > all_regions; /* all regions ever added to this playlist */
	PBD::ScopedConnectionList            region_state_changed_connections;
	PBD::ScopedConnectionList            region_drop_references_connections;
	DataType                             _type;
	uint32_t                             _sort_id;
	mutable GATOMIC_QUAL gint            block_notifications;
	mutable GATOMIC_QUAL gint            ignore_state_changes;
	std::set<boost::shared_ptr<Region> > pending_adds;
	std::set<boost::shared_ptr<Region> > pending_removes;
	RegionList                           pending_bounds;
	bool                                 pending_contents_change;
	bool                                 pending_layering;

	std::set<const Region*> _soloSelectedRegions;

	/** Movements of time ranges caused by region moves; note that
	 *  region trims are not included in this list; it is used to
	 *  do automation-follows-regions.
	 */

	std::list<Temporal::RangeMove> pending_range_moves;

	/** Extra sections added to regions during trims */
	std::list<Temporal::TimeRange> pending_region_extensions;

	uint32_t           in_set_state;
	bool               in_undo;
	bool               first_set_state;
	bool               _hidden;
	bool               _rippling;
	bool               _shuffling;
	bool               _nudging;
	uint32_t           _refcnt;
	bool               in_flush;
	bool               in_partition;
	bool               _frozen;
	bool               _capture_insertion_underway;
	uint32_t           subcnt;
	PBD::ID            _orig_track_id;
	uint32_t           _combine_ops;

	std::list<PBD::ID> _shared_with_ids;

	void init (bool hide);

	bool holding_state () const
	{
		return g_atomic_int_get (&block_notifications) != 0 ||
		       g_atomic_int_get (&ignore_state_changes) != 0;
	}

	void         delay_notifications ();
	void         release_notifications (bool from_undo = false);
	virtual void flush_notifications (bool from_undo = false);
	void         clear_pending ();

	void _set_sort_id ();

	boost::shared_ptr<RegionList> regions_touched_locked (timepos_t const & start, timepos_t const & end);

	void notify_region_removed (boost::shared_ptr<Region>);
	void notify_region_added (boost::shared_ptr<Region>);
	void notify_layering_changed ();
	void notify_contents_changed ();
	void notify_state_changed (const PBD::PropertyChange&);
	void notify_region_moved (boost::shared_ptr<Region>);
	void notify_region_start_trimmed (boost::shared_ptr<Region>);
	void notify_region_end_trimmed (boost::shared_ptr<Region>);

	void mark_session_dirty ();

	void         region_changed_proxy (const PBD::PropertyChange&, boost::weak_ptr<Region>);
	virtual bool region_changed (const PBD::PropertyChange&, boost::shared_ptr<Region>);

	void region_bounds_changed (const PBD::PropertyChange&, boost::shared_ptr<Region>);
	void region_deleted (boost::shared_ptr<Region>);

	void sort_regions ();

	void ripple_locked (timepos_t const & at, timecnt_t const & distance, RegionList *exclude);
	void ripple_unlocked (timepos_t const & at, timecnt_t const & distance, RegionList *exclude, ThawList& thawlist, bool notify = true);

	virtual void remove_dependents (boost::shared_ptr<Region> /*region*/) {}
	virtual void region_going_away (boost::weak_ptr<Region> /*region*/) {}

	virtual XMLNode& state (bool) const;

	bool add_region_internal (boost::shared_ptr<Region>, timepos_t const & position, ThawList& thawlist);

	int  remove_region_internal (boost::shared_ptr<Region>, ThawList& thawlist);
	void copy_regions (RegionList&) const;

	void partition_internal (timepos_t const & start, timepos_t const & end, bool cutting, ThawList& thawlist);

	std::pair<timepos_t, timepos_t> _get_extent() const;

	boost::shared_ptr<Playlist> cut_copy (boost::shared_ptr<Playlist> (Playlist::*pmf)(timepos_t const &, timecnt_t const &, bool),
	                                      std::list<TimelineRange>& ranges, bool result_is_hidden);
	boost::shared_ptr<Playlist> cut (timepos_t const & start, timecnt_t const & cnt, bool result_is_hidden);
	boost::shared_ptr<Playlist> copy (timepos_t const & start, timecnt_t const & cnt, bool result_is_hidden);

	void relayer ();

	void begin_undo ();
	void end_undo ();

	virtual void _split_region (boost::shared_ptr<Region>, timepos_t const & position, ThawList& thawlist);

	typedef std::pair<boost::shared_ptr<Region>, boost::shared_ptr<Region> > TwoRegions;

	/* this is called before we create a new compound region */
	virtual void pre_combine (std::vector<boost::shared_ptr<Region> >&) {}
	/* this is called before we create a new compound region */
	virtual void post_combine (std::vector<boost::shared_ptr<Region> >&, boost::shared_ptr<Region>) {}
	/* this is called before we remove a compound region and replace it
	 * with its constituent regions
	 */
	virtual void pre_uncombine (std::vector<boost::shared_ptr<Region> >&, boost::shared_ptr<Region>) {}

private:
	friend class RegionReadLock;
	friend class RegionWriteLock;

	mutable Glib::Threads::RWLock region_lock;

private:
	void freeze_locked ();
	void setup_layering_indices (RegionList const &);
	void coalesce_and_check_crossfades (std::list<Temporal::TimeRange>);
	boost::shared_ptr<RegionList> find_regions_at (timepos_t const &);

	mutable boost::optional<std::pair<timepos_t, timepos_t> > _cached_extent;
	timepos_t _end_space;  //this is used when we are pasting a range with extra space at the end
	bool _playlist_shift_active;

	std::string _pgroup_id; // when we make multiple playlists in one action, they will share the same pgroup_id
};

} /* namespace ARDOUR */

#endif /* __ardour_playlist_h__ */

