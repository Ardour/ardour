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

#ifndef __ardour_source_h__
#define __ardour_source_h__

#include <string>
#include <set>

#include <glibmm/threads.h>

#include <boost/utility.hpp>
#include "pbd/statefuldestructible.h"

#include "ardour/ardour.h"
#include "ardour/session_object.h"
#include "ardour/data_type.h"

namespace ARDOUR {

class Session;

class LIBARDOUR_API Source : public SessionObject
{
  public:
	enum Flag {
		Writable = 0x1,
		CanRename = 0x2,
		Broadcast = 0x4,
		Removable = 0x8,
		RemovableIfEmpty = 0x10,
		RemoveAtDestroy = 0x20,
		NoPeakFile = 0x40,
		Destructive = 0x80,
		Empty = 0x100, /* used for MIDI only */
		RF64_RIFF = 0x200,
	};

	typedef Glib::Threads::Mutex::Lock Lock;

	Source (Session&, DataType type, const std::string& name, Flag flags=Flag(0));
	Source (Session&, const XMLNode&);

	virtual ~Source ();

	DataType type() { return _type; }

	time_t timestamp() const { return _timestamp; }
	void stamp (time_t when) { _timestamp = when; }

	virtual bool       empty () const = 0;
	virtual samplecnt_t length (samplepos_t pos) const = 0;
	virtual void       update_length (samplecnt_t cnt) = 0;

	virtual samplepos_t natural_position() const { return 0; }

	void mark_for_remove();

	virtual void mark_streaming_write_started (const Lock& lock) {}
	virtual void mark_streaming_write_completed (const Lock& lock) = 0;

	virtual void session_saved() {}

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

	bool         destructive() const       { return (_flags & Destructive); }
	bool         writable () const;

	virtual bool length_mutable() const    { return false; }

	static PBD::Signal1<void,Source*>             SourceCreated;

	bool has_been_analysed() const;
	virtual bool can_be_analysed() const { return false; }
	virtual void set_been_analysed (bool yn);
	virtual bool check_for_analysis_data_on_disk();

	PBD::Signal0<void> AnalysisChanged;

	AnalysisFeatureList transients;
	std::string get_transients_path() const;
	int load_transients (const std::string&);

	samplepos_t    timeline_position() const { return _timeline_position; }
	virtual void set_timeline_position (samplepos_t pos);

	void set_allow_remove_if_empty (bool yn);

        Glib::Threads::Mutex& mutex() { return _lock; }
	Flag         flags() const { return _flags; }

	virtual void inc_use_count ();
	virtual void dec_use_count ();
        int  use_count() const { return g_atomic_int_get (const_cast<gint*>(&_use_count)); }
	bool used() const { return use_count() > 0; }
	uint32_t level() const { return _level; }

	std::string ancestor_name() { return _ancestor_name.empty() ? name() : _ancestor_name; }
	void set_ancestor_name(const std::string& name) { _ancestor_name = name; }

  protected:
	DataType            _type;
	Flag                _flags;
	time_t              _timestamp;
	samplepos_t          _timeline_position;
	bool                _analysed;
        mutable Glib::Threads::Mutex _lock;
        mutable Glib::Threads::Mutex _analysis_lock;
	gint                _use_count; /* atomic */
	uint32_t            _level; /* how deeply nested is this source w.r.t a disk file */
	std::string         _ancestor_name;

  private:
	void fix_writable_flags ();
};

}

#endif /* __ardour_source_h__ */
