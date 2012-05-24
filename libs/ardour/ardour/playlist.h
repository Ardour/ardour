/*
    Copyright (C) 2000 Paul Davis

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

#ifndef __ardour_playlist_h__
#define __ardour_playlist_h__

#include <string>
#include <set>
#include <map>
#include <list>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/utility.hpp>

#include <sys/stat.h>

#include <glib.h>
#ifdef HAVE_GLIB_THREADS_RECMUTEX
#include <glibmm/threads.h>
#endif

#include "pbd/undo.h"
#include "pbd/stateful.h"
#include "pbd/statefuldestructible.h"
#include "pbd/sequence_property.h"
#include "pbd/stacktrace.h"

#include "evoral/types.hpp"

#include "ardour/ardour.h"
#include "ardour/session_object.h"
#include "ardour/data_type.h"

namespace ARDOUR  {

class Session;
class Region;
class Playlist;
class Crossfade;

namespace Properties {
	/* fake the type, since regions are handled by SequenceProperty which doesn't
	   care about such things.
	*/
	extern PBD::PropertyDescriptor<bool> regions;
}

class RegionListProperty : public PBD::SequenceProperty<std::list<boost::shared_ptr<Region> > >
{
  public:
	RegionListProperty (Playlist&);

	RegionListProperty* clone () const;
	void get_content_as_xml (boost::shared_ptr<Region>, XMLNode &) const;
	boost::shared_ptr<Region> get_content_from_xml (XMLNode const &) const;

  private:
	RegionListProperty* create () const;

	/* copy construction only by ourselves */
	RegionListProperty (RegionListProperty const & p);

	friend class Playlist;
	/* we live and die with our playlist, no lifetime management needed */
	Playlist& _playlist;
};

class Playlist : public SessionObject , public boost::enable_shared_from_this<Playlist>
{
public:
	static void make_property_quarks ();

	Playlist (Session&, const XMLNode&, DataType type, bool hidden = false);
	Playlist (Session&, std::string name, DataType type, bool hidden = false);
	Playlist (boost::shared_ptr<const Playlist>, std::string name, bool hidden = false);
	Playlist (boost::shared_ptr<const Playlist>, framepos_t start, framecnt_t cnt, std::string name, bool hidden = false);

	virtual ~Playlist ();

	void update (const RegionListProperty::ChangeRecord&);
	void clear_owned_changes ();
	void rdiff (std::vector<Command*>&) const;

	boost::shared_ptr<Region> region_by_id (const PBD::ID&) const;

	uint32_t max_source_level () const;

	void set_region_ownership ();

	virtual void clear (bool with_signals=true);
	virtual void dump () const;

	void use();
	void release();
	bool used () const { return _refcnt != 0; }

	bool set_name (const std::string& str);
	int sort_id() { return _sort_id; }

	const DataType& data_type() const { return _type; }

	bool frozen() const { return _frozen; }
	void set_frozen (bool yn);

	bool hidden() const { return _hidden; }
	bool empty() const;
	uint32_t n_regions() const;
	std::pair<framepos_t, framepos_t> get_extent () const;
	layer_t top_layer() const;

	EditMode get_edit_mode() const { return _edit_mode; }
	void set_edit_mode (EditMode);

	/* Editing operations */

	void add_region (boost::shared_ptr<Region>, framepos_t position, float times = 1, bool auto_partition = false);
	void remove_region (boost::shared_ptr<Region>);
	void remove_region_by_source (boost::shared_ptr<Source>);
	void get_equivalent_regions (boost::shared_ptr<Region>, std::vector<boost::shared_ptr<Region> >&);
	void get_region_list_equivalent_regions (boost::shared_ptr<Region>, std::vector<boost::shared_ptr<Region> >&);
	void replace_region (boost::shared_ptr<Region> old, boost::shared_ptr<Region> newr, framepos_t pos);
	void split_region (boost::shared_ptr<Region>, framepos_t position);
	void split (framepos_t at);
	void shift (framepos_t at, frameoffset_t distance, bool move_intersected, bool ignore_music_glue);
	void partition (framepos_t start, framepos_t end, bool cut = false);
	void duplicate (boost::shared_ptr<Region>, framepos_t position, float times);
	void nudge_after (framepos_t start, framecnt_t distance, bool forwards);
	boost::shared_ptr<Region> combine (const RegionList&);
	void uncombine (boost::shared_ptr<Region>);

	void shuffle (boost::shared_ptr<Region>, int dir);
	void update_after_tempo_map_change ();

	boost::shared_ptr<Playlist> cut  (std::list<AudioRange>&, bool result_is_hidden = true);
	boost::shared_ptr<Playlist> copy (std::list<AudioRange>&, bool result_is_hidden = true);
	int                         paste (boost::shared_ptr<Playlist>, framepos_t position, float times);

	const RegionListProperty& region_list () const { return regions; }

	boost::shared_ptr<RegionList> regions_at (framepos_t frame);
	uint32_t                   count_regions_at (framepos_t) const;
	uint32_t                   count_joined_regions () const;
	boost::shared_ptr<RegionList> regions_touched (framepos_t start, framepos_t end);
	boost::shared_ptr<RegionList> regions_with_start_within (Evoral::Range<framepos_t>);
	boost::shared_ptr<RegionList> regions_with_end_within (Evoral::Range<framepos_t>);
	uint32_t                   region_use_count (boost::shared_ptr<Region>) const;
	boost::shared_ptr<Region>  find_region (const PBD::ID&) const;
	boost::shared_ptr<Region>  top_region_at (framepos_t frame);
	boost::shared_ptr<Region>  top_unmuted_region_at (framepos_t frame);
	boost::shared_ptr<Region>  find_next_region (framepos_t frame, RegionPoint point, int dir);
	framepos_t                 find_next_region_boundary (framepos_t frame, int dir);
	bool                       region_is_shuffle_constrained (boost::shared_ptr<Region>);
	bool                       has_region_at (framepos_t const) const;

	bool uses_source (boost::shared_ptr<const Source> src) const;

	framepos_t find_next_transient (framepos_t position, int dir);

	void foreach_region (boost::function<void (boost::shared_ptr<Region>)>);

	XMLNode& get_state ();
	virtual int set_state (const XMLNode&, int version);
	XMLNode& get_template ();

	PBD::Signal1<void,bool> InUse;
	PBD::Signal0<void>      ContentsChanged;
	PBD::Signal1<void,boost::weak_ptr<Region> > RegionAdded;
	PBD::Signal1<void,boost::weak_ptr<Region> > RegionRemoved;
	PBD::Signal0<void>      NameChanged;
	PBD::Signal0<void>      LayeringChanged;

	/** Emitted when regions have moved (not when regions have only been trimmed) */
	PBD::Signal2<void,std::list< Evoral::RangeMove<framepos_t> > const &, bool> RangesMoved;

	/** Emitted when regions are extended; the ranges passed are the new extra time ranges
	    that these regions now occupy.
	*/
	PBD::Signal1<void,std::list< Evoral::Range<framepos_t> > const &> RegionsExtended;

	static std::string bump_name (std::string old_name, Session&);

	void freeze ();
	void thaw (bool from_undo = false);

	void raise_region (boost::shared_ptr<Region>);
	void lower_region (boost::shared_ptr<Region>);
	void raise_region_to_top (boost::shared_ptr<Region>);
	void lower_region_to_bottom (boost::shared_ptr<Region>);

	const PBD::ID& get_orig_track_id () const { return _orig_track_id; }
	void set_orig_track_id (const PBD::ID& did);

	/* destructive editing */

	virtual bool destroy_region (boost::shared_ptr<Region>) = 0;

	void sync_all_regions_with_regions ();

	/* special case function used by UI selection objects, which have playlists that actually own the regions
	   within them.
	*/

	void drop_regions ();

	virtual boost::shared_ptr<Crossfade> find_crossfade (const PBD::ID &) const {
		return boost::shared_ptr<Crossfade> ();
	}

	framepos_t find_next_top_layer_position (framepos_t) const;
	uint32_t combine_ops() const { return _combine_ops; }

	uint64_t highest_layering_index () const;

	void set_layer (boost::shared_ptr<Region>, double);
	
  protected:
	friend class Session;

  protected:
    class RegionReadLock : public Glib::RWLock::ReaderLock {
    public:
        RegionReadLock (Playlist *pl) : Glib::RWLock::ReaderLock (pl->region_lock) {}
        ~RegionReadLock() {}
    };

    class RegionWriteLock : public Glib::RWLock::WriterLock {
    public:
	    RegionWriteLock (Playlist *pl, bool do_block_notify = true) 
                    : Glib::RWLock::WriterLock (pl->region_lock)
                    , playlist (pl)
                    , block_notify (do_block_notify) {
                    if (block_notify) {
                            playlist->delay_notifications();
                    }
            }

        ~RegionWriteLock() {
                Glib::RWLock::WriterLock::release ();
                if (block_notify) {
                        playlist->release_notifications ();
                }
        }
        Playlist *playlist;
        bool block_notify;
    };

	RegionListProperty   regions;  /* the current list of regions in the playlist */
	std::set<boost::shared_ptr<Region> > all_regions; /* all regions ever added to this playlist */
	PBD::ScopedConnectionList region_state_changed_connections;
	DataType        _type;
	int             _sort_id;
	mutable gint    block_notifications;
	mutable gint    ignore_state_changes;
	std::set<boost::shared_ptr<Region> > pending_adds;
	std::set<boost::shared_ptr<Region> > pending_removes;
	RegionList       pending_bounds;
	bool             pending_contents_change;
	bool             pending_layering;

	/** Movements of time ranges caused by region moves; note that
	 *  region trims are not included in this list; it is used to
	 *  do automation-follows-regions.
	 */
	std::list< Evoral::RangeMove<framepos_t> > pending_range_moves;
	/** Extra sections added to regions during trims */
	std::list< Evoral::Range<framepos_t> >     pending_region_extensions;
	bool             save_on_thaw;
	std::string      last_save_reason;
	uint32_t         in_set_state;
	bool             in_undo;
	bool             first_set_state;
	bool            _hidden;
	bool            _splicing;
	bool            _shuffling;
	bool            _nudging;
	uint32_t        _refcnt;
	EditMode        _edit_mode;
	bool             in_flush;
	bool             in_partition;
	bool            _frozen;
	uint32_t         subcnt;
	PBD::ID         _orig_track_id;
	bool             auto_partition;
	uint32_t        _combine_ops;

	void init (bool hide);

	bool holding_state () const {
		return g_atomic_int_get (&block_notifications) != 0 ||
			g_atomic_int_get (&ignore_state_changes) != 0;
	}

	void delay_notifications ();
	void release_notifications (bool from_undo = false);
	virtual void flush_notifications (bool from_undo = false);
	void clear_pending ();

	void _set_sort_id ();

	boost::shared_ptr<RegionList> regions_touched_locked (framepos_t start, framepos_t end);

	void notify_region_removed (boost::shared_ptr<Region>);
	void notify_region_added (boost::shared_ptr<Region>);
	void notify_layering_changed ();
	void notify_contents_changed ();
	void notify_state_changed (const PBD::PropertyChange&);
	void notify_region_moved (boost::shared_ptr<Region>);
	void notify_region_start_trimmed (boost::shared_ptr<Region>);
	void notify_region_end_trimmed (boost::shared_ptr<Region>);

	void mark_session_dirty();

	void region_changed_proxy (const PBD::PropertyChange&, boost::weak_ptr<Region>);
	virtual bool region_changed (const PBD::PropertyChange&, boost::shared_ptr<Region>);

	void region_bounds_changed (const PBD::PropertyChange&, boost::shared_ptr<Region>);
	void region_deleted (boost::shared_ptr<Region>);

	void sort_regions ();

	void possibly_splice (framepos_t at, framecnt_t distance, boost::shared_ptr<Region> exclude = boost::shared_ptr<Region>());
	void possibly_splice_unlocked(framepos_t at, framecnt_t distance, boost::shared_ptr<Region> exclude = boost::shared_ptr<Region>());

	void core_splice (framepos_t at, framecnt_t distance, boost::shared_ptr<Region> exclude);
	void splice_locked (framepos_t at, framecnt_t distance, boost::shared_ptr<Region> exclude);
	void splice_unlocked (framepos_t at, framecnt_t distance, boost::shared_ptr<Region> exclude);

	virtual void check_crossfades (Evoral::Range<framepos_t>) {}
	virtual void remove_dependents (boost::shared_ptr<Region> /*region*/) {}

	virtual XMLNode& state (bool);

	bool add_region_internal (boost::shared_ptr<Region>, framepos_t position);

	int remove_region_internal (boost::shared_ptr<Region>);
	void copy_regions (RegionList&) const;
	void partition_internal (framepos_t start, framepos_t end, bool cutting, RegionList& thawlist);

	std::pair<framepos_t, framepos_t> _get_extent() const;

	boost::shared_ptr<Playlist> cut_copy (boost::shared_ptr<Playlist> (Playlist::*pmf)(framepos_t, framecnt_t, bool),
					      std::list<AudioRange>& ranges, bool result_is_hidden);
	boost::shared_ptr<Playlist> cut (framepos_t start, framecnt_t cnt, bool result_is_hidden);
	boost::shared_ptr<Playlist> copy (framepos_t start, framecnt_t cnt, bool result_is_hidden);

	void relayer ();

	void begin_undo ();
	void end_undo ();
	void unset_freeze_parent (Playlist*);
	void unset_freeze_child (Playlist*);

	void _split_region (boost::shared_ptr<Region>, framepos_t position);

	typedef std::pair<boost::shared_ptr<Region>, boost::shared_ptr<Region> > TwoRegions;

	/* this is called before we create a new compound region */
	virtual void pre_combine (std::vector<boost::shared_ptr<Region> >&) {}
	/* this is called before we create a new compound region */
	virtual void post_combine (std::vector<boost::shared_ptr<Region> >&, boost::shared_ptr<Region>) {}
	/* this is called before we remove a compound region and replace it
	   with its constituent regions
	*/
	virtual void pre_uncombine (std::vector<boost::shared_ptr<Region> >&, boost::shared_ptr<Region>) {}

  private:
	friend class RegionReadLock;
	friend class RegionWriteLock;
	mutable Glib::RWLock region_lock;

  private:
	void setup_layering_indices (RegionList const &);
	void coalesce_and_check_crossfades (std::list<Evoral::Range<framepos_t> >);
	boost::shared_ptr<RegionList> find_regions_at (framepos_t);
};

} /* namespace ARDOUR */

#endif	/* __ardour_playlist_h__ */


