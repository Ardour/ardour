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

    $Id$
*/

#ifndef __ardour_playlist_h__
#define __ardour_playlist_h__

#include <string>
#include <set>
#include <map>
#include <list>

#include <sys/stat.h>

#include <sigc++/signal.h>
#include <pbd/atomic.h>
#include <pbd/undo.h>

#include <ardour/ardour.h>
#include <ardour/crossfade_compare.h>
#include <ardour/location.h>
#include <ardour/stateful.h>
#include <ardour/source.h>
#include <ardour/state_manager.h>

namespace ARDOUR  {

class Session;
class Region;

class Playlist : public Stateful, public StateManager {
  public:
	typedef list<Region*>    RegionList;

	Playlist (Session&, const XMLNode&, bool hidden = false);
	Playlist (Session&, string name, bool hidden = false);
	Playlist (const Playlist&, string name, bool hidden = false);
	Playlist (const Playlist&, jack_nframes_t start, jack_nframes_t cnt, string name, bool hidden = false);

	virtual jack_nframes_t read (Sample *dst, Sample *mixdown, float *gain_buffer, char * workbuf, jack_nframes_t start, jack_nframes_t cnt, uint32_t chan_n=0) = 0;
	virtual void clear (bool with_delete = false, bool with_save = true);
	virtual void dump () const;
	virtual UndoAction get_memento() const = 0;

	void ref();
	void unref();
	uint32_t refcnt() const { return _refcnt; }

	const string& name() const { return _name; }
	void set_name (const string& str);

	bool frozen() const { return _frozen; }
	void set_frozen (bool yn);

	bool hidden() const { return _hidden; }
	bool empty() const;
	jack_nframes_t get_maximum_extent () const;
	layer_t top_layer() const;

	EditMode get_edit_mode() const { return _edit_mode; }
	void set_edit_mode (EditMode);

	/* Editing operations */

	void add_region (const Region&, jack_nframes_t position, float times = 1, bool with_save = true);
	void remove_region (Region *);
	void replace_region (Region& old, Region& newr, jack_nframes_t pos);
	void split_region (Region&, jack_nframes_t position);
	void partition (jack_nframes_t start, jack_nframes_t end, bool just_top_level);
	void duplicate (Region&, jack_nframes_t position, float times);
	void nudge_after (jack_nframes_t start, jack_nframes_t distance, bool forwards);

	Region* find_region (id_t) const;

	Playlist* cut  (list<AudioRange>&, bool result_is_hidden = true);
	Playlist* copy (list<AudioRange>&, bool result_is_hidden = true);
	int       paste (Playlist&, jack_nframes_t position, float times);

	uint32_t read_data_count() { return _read_data_count; }

	RegionList* regions_at (jack_nframes_t frame);
	RegionList* regions_touched (jack_nframes_t start, jack_nframes_t end);
	Region*     top_region_at (jack_nframes_t frame);

	Region*     find_next_region (jack_nframes_t frame, RegionPoint point, int dir);

	template<class T> void foreach_region (T *t, void (T::*func)(Region *, void *), void *arg);
	template<class T> void foreach_region (T *t, void (T::*func)(Region *));

	XMLNode& get_state ();
	int set_state (const XMLNode&);
	XMLNode& get_template ();

	sigc::signal<void,Region *> RegionAdded;
	sigc::signal<void,Region *> RegionRemoved;

	sigc::signal<void,Playlist*,bool> InUse;
	sigc::signal<void>            Modified;
	sigc::signal<void>            NameChanged;
	sigc::signal<void>            LengthChanged;
	sigc::signal<void>            LayeringChanged;
	sigc::signal<void,Playlist *> GoingAway;
	sigc::signal<void>            StatePushed;

	static sigc::signal<void,Playlist*> PlaylistCreated;

	static string bump_name (string old_name, Session&);
	static string bump_name_once (string old_name);

	void freeze ();
	void thaw ();

	void raise_region (Region&);
	void lower_region (Region&);
	void raise_region_to_top (Region&);
	void lower_region_to_bottom (Region&);

	uint32_t read_data_count() const { return _read_data_count; }

	Session& session() { return _session; }

	id_t get_orig_diskstream_id () const { return _orig_diskstream_id; }
	void set_orig_diskstream_id (id_t did) { _orig_diskstream_id = did; }  

	/* destructive editing */
	
	virtual bool destroy_region (Region *) = 0;

  protected:
	friend class Session;
	virtual ~Playlist ();  /* members of the public use unref() */

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

	RegionList       regions;
	string          _name;
	Session&        _session;
	atomic_t         block_notifications;
	atomic_t         ignore_state_changes;
	mutable PBD::NonBlockingLock region_lock;
	RegionList       pending_removals;
	RegionList       pending_adds;
	RegionList       pending_bounds;
	bool             pending_modified;
	bool             pending_length;
	bool             save_on_thaw;
	string           last_save_reason;
	bool             in_set_state;
	bool            _hidden;
	bool            _splicing;
	bool            _nudging;
	uint32_t        _refcnt;
	EditMode        _edit_mode;
	bool             in_flush;
	bool             in_partition;
	bool            _frozen;
	uint32_t         subcnt;
	uint32_t        _read_data_count;
	id_t            _orig_diskstream_id;
	uint64_t         layer_op_counter;
	jack_nframes_t   freeze_length;

	void init (bool hide);

	bool holding_state () const { 
		return atomic_read (&block_notifications) != 0 ||
			atomic_read (&ignore_state_changes) != 0;
	}

	/* prevent the compiler from ever generating these */

	Playlist (const Playlist&);
	Playlist (Playlist&);

	void delay_notifications ();
	void release_notifications ();
	virtual void flush_notifications ();

	void notify_region_removed (Region *);
	void notify_region_added (Region *);
	void notify_length_changed ();
	void notify_layering_changed ();
	void notify_modified ();
	void notify_state_changed (Change);

	void mark_session_dirty();

	void region_changed_proxy (Change, Region*);
	virtual bool region_changed (Change, Region*);

	void region_bounds_changed (Change, Region *);
	void region_deleted (Region *);

	void sort_regions ();

	void possibly_splice ();
	void possibly_splice_unlocked();
	void core_splice ();
	void splice_locked ();
	void splice_unlocked ();


	virtual void finalize_split_region (Region *original, Region *left, Region *right) {}
	
	virtual void check_dependents (Region& region, bool norefresh) {}
	virtual void refresh_dependents (Region& region) {}
	virtual void remove_dependents (Region& region) {}

	virtual XMLNode& state (bool);

	/* override state_manager::save_state so we can check in_set_state() */

	void save_state (std::string why);
	void maybe_save_state (std::string why);

	void add_region_internal (Region *, jack_nframes_t position, bool delay_sort = false);

	int remove_region_internal (Region *, bool delay_sort = false);
	RegionList *find_regions_at (jack_nframes_t frame);
	void copy_regions (RegionList&) const;
	void partition_internal (jack_nframes_t start, jack_nframes_t end, bool cutting, RegionList& thawlist);

	jack_nframes_t _get_maximum_extent() const;

	Playlist* cut_copy (Playlist* (Playlist::*pmf)(jack_nframes_t, jack_nframes_t, bool), 
			    list<AudioRange>& ranges, bool result_is_hidden);
	Playlist *cut (jack_nframes_t start, jack_nframes_t cnt, bool result_is_hidden);
	Playlist *copy (jack_nframes_t start, jack_nframes_t cnt, bool result_is_hidden);


	int move_region_to_layer (layer_t, Region& r, int dir);
	void relayer ();

	static Playlist* copyPlaylist (const Playlist&, jack_nframes_t start, jack_nframes_t length,
				       string name, bool result_is_hidden);
	
	void unset_freeze_parent (Playlist*);
	void unset_freeze_child (Playlist*);

	void timestamp_layer_op (Region&);
};

} /* namespace ARDOUR */

#endif	/* __ardour_playlist_h__ */


