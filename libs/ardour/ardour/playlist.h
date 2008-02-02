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

#include <sys/stat.h>

#include <glib.h>

#include <sigc++/signal.h>

#include <pbd/undo.h>
#include <pbd/stateful.h> 
#include <pbd/statefuldestructible.h> 

#include <ardour/ardour.h>
#include <ardour/session_object.h>
#include <ardour/crossfade_compare.h>
#include <ardour/location.h>
#include <ardour/data_type.h>

namespace ARDOUR  {

class Session;
class Region;

class Playlist : public SessionObject, public boost::enable_shared_from_this<Playlist> {
  public:
	typedef list<boost::shared_ptr<Region> >    RegionList;

	Playlist (Session&, const XMLNode&, DataType type, bool hidden = false);
	Playlist (Session&, string name, DataType type, bool hidden = false);
	Playlist (boost::shared_ptr<const Playlist>, string name, bool hidden = false);
	Playlist (boost::shared_ptr<const Playlist>, nframes_t start, nframes_t cnt, string name, bool hidden = false);

	virtual ~Playlist ();  

	void set_region_ownership ();

	virtual void clear (bool with_signals=true);
	virtual void dump () const;

	void use();
	void release();
	bool used () const { return _refcnt != 0; }

	bool set_name (const string& str);

	const DataType& data_type() const { return _type; }

	bool frozen() const { return _frozen; }
	void set_frozen (bool yn);

	bool hidden() const { return _hidden; }
	bool empty() const;
	uint32_t n_regions() const;
	nframes_t get_maximum_extent () const;
	layer_t top_layer() const;

	EditMode get_edit_mode() const { return _edit_mode; }
	void set_edit_mode (EditMode);

	/* Editing operations */

	void add_region (boost::shared_ptr<Region>, nframes_t position, float times = 1);
	void remove_region (boost::shared_ptr<Region>);
	void get_equivalent_regions (boost::shared_ptr<Region>, std::vector<boost::shared_ptr<Region> >&);
	void get_region_list_equivalent_regions (boost::shared_ptr<Region>, std::vector<boost::shared_ptr<Region> >&);
	void replace_region (boost::shared_ptr<Region> old, boost::shared_ptr<Region> newr, nframes_t pos);
	void split_region (boost::shared_ptr<Region>, nframes_t position);
	void partition (nframes_t start, nframes_t end, bool just_top_level);
	void duplicate (boost::shared_ptr<Region>, nframes_t position, float times);
	void nudge_after (nframes_t start, nframes_t distance, bool forwards);
	void shuffle (boost::shared_ptr<Region>, int dir);
	void update_after_tempo_map_change ();

	boost::shared_ptr<Playlist> cut  (list<AudioRange>&, bool result_is_hidden = true);
	boost::shared_ptr<Playlist> copy (list<AudioRange>&, bool result_is_hidden = true);
	int                         paste (boost::shared_ptr<Playlist>, nframes_t position, float times);

	RegionList*                regions_at (nframes_t frame);
	RegionList*                regions_touched (nframes_t start, nframes_t end);
	RegionList*                regions_to_read (nframes_t start, nframes_t end);
	boost::shared_ptr<Region>  find_region (const PBD::ID&) const;
	boost::shared_ptr<Region>  top_region_at (nframes_t frame);
	boost::shared_ptr<Region>  find_next_region (nframes_t frame, RegionPoint point, int dir);
	nframes64_t                find_next_region_boundary (nframes64_t frame, int dir);
	bool                       region_is_shuffle_constrained (boost::shared_ptr<Region>);

	nframes64_t find_next_transient (nframes64_t position, int dir);

	template<class T> void foreach_region (T *t, void (T::*func)(boost::shared_ptr<Region>, void *), void *arg);
	template<class T> void foreach_region (T *t, void (T::*func)(boost::shared_ptr<Region>));

	XMLNode& get_state ();
	int set_state (const XMLNode&);
	XMLNode& get_template ();

	sigc::signal<void,bool> InUse;
	sigc::signal<void>      Modified;
	sigc::signal<void>      NameChanged;
	sigc::signal<void>      LengthChanged;

	static string bump_name (string old_name, Session&);
	static string bump_name_once (string old_name);

	void freeze ();
	void thaw ();

	void raise_region (boost::shared_ptr<Region>);
	void lower_region (boost::shared_ptr<Region>);
	void raise_region_to_top (boost::shared_ptr<Region>);
	void lower_region_to_bottom (boost::shared_ptr<Region>);

	uint32_t read_data_count() const { return _read_data_count; }

	const PBD::ID& get_orig_diskstream_id () const { return _orig_diskstream_id; }
	void set_orig_diskstream_id (const PBD::ID& did) { _orig_diskstream_id = did; }  

	/* destructive editing */
	
	virtual bool destroy_region (boost::shared_ptr<Region>) = 0;

	/* special case function used by UI selection objects, which have playlists that actually own the regions
	   within them.
	*/

	void drop_regions ();

  protected:
	friend class Session;

  protected:
	struct RegionLock {
	    RegionLock (Playlist *pl, bool do_block_notify = true) : playlist (pl), block_notify (do_block_notify) {
		    playlist->region_lock.lock();
		    if (block_notify) {
			    playlist->delay_notifications();
		    }
	    }
	    ~RegionLock() { 
		    playlist->region_lock.unlock();
		    if (block_notify) {
			    playlist->release_notifications ();
		    }
	    }
	    Playlist *playlist;
	    bool block_notify;
	};

	friend class RegionLock;

	RegionList       regions;  /* the current list of regions in the playlist */
	std::set<boost::shared_ptr<Region> > all_regions; /* all regions ever added to this playlist */
	DataType        _type;
	mutable gint    block_notifications;
	mutable gint    ignore_state_changes;
	mutable Glib::Mutex region_lock;
	std::set<boost::shared_ptr<Region> > pending_adds;
	std::set<boost::shared_ptr<Region> > pending_removes;
	RegionList       pending_bounds;
	bool             pending_modified;
	bool             pending_length;
	bool             save_on_thaw;
	string           last_save_reason;
	uint32_t         in_set_state;
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
	uint32_t        _read_data_count;
	PBD::ID         _orig_diskstream_id;
	uint64_t         layer_op_counter;
	nframes_t   freeze_length;

	void init (bool hide);

	bool holding_state () const { 
		return g_atomic_int_get (&block_notifications) != 0 ||
			g_atomic_int_get (&ignore_state_changes) != 0;
	}

	/* prevent the compiler from ever generating these */

	Playlist (const Playlist&);
	Playlist (Playlist&);

	void delay_notifications ();
	void release_notifications ();
	virtual void flush_notifications ();

	void notify_region_removed (boost::shared_ptr<Region>);
	void notify_region_added (boost::shared_ptr<Region>);
	void notify_length_changed ();
	void notify_layering_changed ();
	void notify_modified ();
	void notify_state_changed (Change);

	void mark_session_dirty();

	void region_changed_proxy (Change, boost::weak_ptr<Region>);
	virtual bool region_changed (Change, boost::shared_ptr<Region>);

	void region_bounds_changed (Change, boost::shared_ptr<Region>);
	void region_deleted (boost::shared_ptr<Region>);

	void sort_regions ();

	void possibly_splice (nframes_t at, nframes64_t distance, boost::shared_ptr<Region> exclude = boost::shared_ptr<Region>());
	void possibly_splice_unlocked(nframes_t at, nframes64_t distance, boost::shared_ptr<Region> exclude = boost::shared_ptr<Region>());

	void core_splice (nframes_t at, nframes64_t distance, boost::shared_ptr<Region> exclude);
	void splice_locked (nframes_t at, nframes64_t distance, boost::shared_ptr<Region> exclude);
	void splice_unlocked (nframes_t at, nframes64_t distance, boost::shared_ptr<Region> exclude);

	virtual void finalize_split_region (boost::shared_ptr<Region> original, boost::shared_ptr<Region> left, boost::shared_ptr<Region> right) {}
	
	virtual void check_dependents (boost::shared_ptr<Region> region, bool norefresh) {}
	virtual void refresh_dependents (boost::shared_ptr<Region> region) {}
	virtual void remove_dependents (boost::shared_ptr<Region> region) {}

	virtual XMLNode& state (bool);

	boost::shared_ptr<Region> region_by_id (PBD::ID);

	void add_region_internal (boost::shared_ptr<Region>, nframes_t position);
	
	int remove_region_internal (boost::shared_ptr<Region>);
	RegionList *find_regions_at (nframes_t frame);
	void copy_regions (RegionList&) const;
	void partition_internal (nframes_t start, nframes_t end, bool cutting, RegionList& thawlist);

	nframes_t _get_maximum_extent() const;

	boost::shared_ptr<Playlist> cut_copy (boost::shared_ptr<Playlist> (Playlist::*pmf)(nframes_t, nframes_t, bool), 
					      list<AudioRange>& ranges, bool result_is_hidden);
	boost::shared_ptr<Playlist> cut (nframes_t start, nframes_t cnt, bool result_is_hidden);
	boost::shared_ptr<Playlist> copy (nframes_t start, nframes_t cnt, bool result_is_hidden);

	int move_region_to_layer (layer_t, boost::shared_ptr<Region> r, int dir);
	void relayer ();
	
	void unset_freeze_parent (Playlist*);
	void unset_freeze_child (Playlist*);

	void timestamp_layer_op (boost::shared_ptr<Region>);
};

} /* namespace ARDOUR */

#endif	/* __ardour_playlist_h__ */


