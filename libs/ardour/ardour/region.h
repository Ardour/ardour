/*
    Copyright (C) 2000-2001 Paul Davis 

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

#ifndef __ardour_region_h__
#define __ardour_region_h__

#include <vector>

#include <pbd/undo.h>

#include <ardour/ardour.h>
#include <ardour/state_manager.h>

class XMLNode;

namespace ARDOUR {

class Playlist;
class Source;

enum RegionEditState {
	EditChangesNothing = 0,
	EditChangesName    = 1,
	EditChangesID      = 2
};

struct RegionState : public StateManager::State
{
	RegionState (std::string why) : StateManager::State (why) {}

	jack_nframes_t          _start;
	jack_nframes_t          _length;
	jack_nframes_t          _position;
	uint32_t                _flags;
	jack_nframes_t          _sync_position;
	layer_t        	        _layer;
	string                  _name;        
	mutable RegionEditState _first_edit;
};

class Region : public Stateful, public StateManager
{
  public:
	typedef std::vector<Source *> SourceList;

	enum Flag {
		Muted = 0x1,
		Opaque = 0x2,
		EnvelopeActive = 0x4,
		DefaultFadeIn = 0x8,
		DefaultFadeOut = 0x10,
		Locked = 0x20,
		Automatic = 0x40,
		WholeFile = 0x80,
		FadeIn = 0x100,
		FadeOut = 0x200,
		Copied = 0x400,
		Import = 0x800,
		External = 0x1000,
		SyncMarked = 0x2000,
		LeftOfSplit = 0x4000,
		RightOfSplit = 0x8000,
		Hidden = 0x10000,
		DoNotSaveState = 0x20000,
		//
		range_guarantoor = USHRT_MAX
	};

 	static const Flag DefaultFlags = Flag (Opaque|DefaultFadeIn|DefaultFadeOut|FadeIn|FadeOut);

	static Change FadeChanged;
	static Change SyncOffsetChanged;
	static Change MuteChanged;
	static Change OpacityChanged;
	static Change LockChanged;
	static Change LayerChanged;
	static Change HiddenChanged;

	Region (Source& src, jack_nframes_t start, jack_nframes_t length, 
		const string& name, layer_t = 0, Flag flags = DefaultFlags);
	Region (SourceList& srcs, jack_nframes_t start, jack_nframes_t length, 
		const string& name, layer_t = 0, Flag flags = DefaultFlags);
	Region (const Region&, jack_nframes_t start, jack_nframes_t length,
		const string& name, layer_t = 0, Flag flags = DefaultFlags);
	Region (const Region&);
	Region (SourceList& srcs, const XMLNode&);
	Region (Source& src, const XMLNode&);
	virtual ~Region();

	const PBD::ID& id() const { return _id; }

	/* Note: changing the name of a Region does not constitute an edit */

	string name() const { return _name; }
	void set_name (string str);

	jack_nframes_t position () const { return _position; }
	jack_nframes_t start ()    const { return _start; }
	jack_nframes_t length()    const { return _length; }
	layer_t        layer ()    const { return _layer; }
	
	jack_nframes_t sync_offset(int& dir) const;
	jack_nframes_t sync_position() const;

	jack_nframes_t adjust_to_sync (jack_nframes_t);
	
	/* first_frame() is an alias; last_frame() just hides some math */

	jack_nframes_t first_frame() const { return _position; }
	jack_nframes_t last_frame() const { return _position + _length - 1; }

	Flag flags()      const { return _flags; }
	bool hidden()     const { return _flags & Hidden; }
	bool muted()      const { return _flags & Muted; }
	bool opaque ()    const { return _flags & Opaque; }
	bool locked()     const { return _flags & Locked; }
	bool automatic()  const { return _flags & Automatic; }
	bool whole_file() const { return _flags & WholeFile ; }
	bool captured()   const { return !(_flags & (Region::Flag (Region::Import|Region::External))); }

	virtual bool should_save_state () const { return !(_flags & DoNotSaveState); };

	void freeze ();
	void thaw (const string& why);

	bool covers (jack_nframes_t frame) const {
		return _position <= frame && frame < _position + _length;
	}

	OverlapType coverage (jack_nframes_t start, jack_nframes_t end) const {
		return ARDOUR::coverage (_position, _position + _length - 1, start, end);
	}
	
	bool equivalent (const Region&) const;
	bool size_equivalent (const Region&) const;
	bool overlap_equivalent (const Region&) const;
	bool region_list_equivalent (const Region&) const;
	bool source_equivalent (const Region&) const;
	
	/* EDITING OPERATIONS */

	void set_length (jack_nframes_t, void *src);
	void set_start (jack_nframes_t, void *src);
	void set_position (jack_nframes_t, void *src);
	void set_position_on_top (jack_nframes_t, void *src);
	void special_set_position (jack_nframes_t);
	void nudge_position (long, void *src);

	void move_to_natural_position (void *src);

	void trim_start (jack_nframes_t new_position, void *src);
	void trim_front (jack_nframes_t new_position, void *src);
	void trim_end (jack_nframes_t new_position, void *src);
	void trim_to (jack_nframes_t position, jack_nframes_t length, void *src);
	
	void set_layer (layer_t l); /* ONLY Playlist can call this */
	void raise ();
	void lower ();
	void raise_to_top ();
	void lower_to_bottom ();

	void set_sync_position (jack_nframes_t n);
	void clear_sync_position ();
	void set_hidden (bool yn);
	void set_muted (bool yn);
	void set_opaque (bool yn);
	void set_locked (bool yn);

	virtual uint32_t read_data_count() const { return _read_data_count; }

	ARDOUR::Playlist* playlist() const { return _playlist; }

	virtual UndoAction get_memento() const = 0;

	void set_playlist (ARDOUR::Playlist*);

	void lock_sources ();
	void unlock_sources ();
	void source_deleted (Source*);

	Source&  source (uint32_t n=0) const { return *_sources[ (n < _sources.size()) ? n : 0 ]; }
	uint32_t n_channels()          const { return _sources.size(); }

	std::vector<string> master_source_names();


	/* serialization */
	
	XMLNode&         get_state ();
	virtual XMLNode& state (bool);
	virtual int      set_state (const XMLNode&);

	sigc::signal<void,Region*> GoingAway;

	/* This is emitted only when a new id is assigned. Therefore,
	   in a pure Region copy, it will not be emitted.

	   It must be emitted by derived classes, not Region
	   itself, to permit dynamic_cast<> to be used to 
	   infer the type of Region.
	*/

	static sigc::signal<void,Region*> CheckNewRegion;

	Region* get_parent();
	
	uint64_t last_layer_op() const { return _last_layer_op; }
	void set_last_layer_op (uint64_t when);

  protected:
	XMLNode& get_short_state (); /* used only by Session */

	/* state management */

	void send_change (Change);

	/* derived classes need these during their own state management calls */

	void   store_state (RegionState&) const;
	Change restore_and_return_flags (RegionState&);
	
	void trim_to_internal (jack_nframes_t position, jack_nframes_t length, void *src);

	bool copied() const { return _flags & Copied; }
	void maybe_uncopy ();
	void first_edit ();
	
	bool verify_start (jack_nframes_t);
	bool verify_start_and_length (jack_nframes_t, jack_nframes_t);
	bool verify_start_mutable (jack_nframes_t&_start);
	bool verify_length (jack_nframes_t);
	virtual void recompute_at_start () = 0;
	virtual void recompute_at_end () = 0;
	

	PBD::ID                 _id;
	string                  _name;        
	Flag                    _flags;
	jack_nframes_t          _start;
	jack_nframes_t          _length;
	jack_nframes_t          _position;
	jack_nframes_t          _sync_position;
	layer_t                 _layer;
	mutable RegionEditState _first_edit;
	int                     _frozen;
	mutable uint32_t        _read_data_count;  ///< modified in read()
	Change                  _pending_changed;
	uint64_t                _last_layer_op;  ///< timestamp
	Glib::Mutex             _lock;
	ARDOUR::Playlist*       _playlist;
	SourceList              _sources;
	/** Used when timefx are applied, so we can always use the original source */
	SourceList              _master_sources;
};

} /* namespace ARDOUR */

#endif /* __ardour_region_h__ */
