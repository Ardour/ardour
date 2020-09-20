/*
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2006-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2016-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2018-2019 Ben Loftis <ben@harrisonconsoles.com>
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

#ifndef __ardour_source_h__
#define __ardour_source_h__

#include <string>
#include <set>

#include <glibmm/threads.h>

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/utility.hpp>

#include "pbd/statefuldestructible.h"
#include "pbd/g_atomic_compat.h"

#include "ardour/ardour.h"
#include "ardour/session_object.h"
#include "ardour/data_type.h"

namespace ARDOUR {

class Session;

class LIBARDOUR_API Source : public SessionObject,
		public boost::enable_shared_from_this<ARDOUR::Source>
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
		/* No longer in use but kept to allow loading of older sessions */
		Destructive = 0x80,
		Empty = 0x100, /* used for MIDI only */
		RF64_RIFF = 0x200,
		Missing = 0x400, /* used for MIDI only */
	};

	typedef Glib::Threads::Mutex::Lock Lock;

	Source (Session&, DataType type, const std::string& name, Flag flags=Flag(0));
	Source (Session&, const XMLNode&);

	virtual ~Source ();

	DataType type() { return _type; }

	time_t timestamp() const { return _timestamp; }
	void stamp (time_t when) { _timestamp = when; }

	timecnt_t length() const;

	virtual bool        empty () const;
	virtual samplecnt_t length_samples (timepos_t const & pos) const { return _length.samples(); };
	virtual void        update_length (timecnt_t const & cnt) {}

	void                 set_take_id (std::string id) { _take_id =id; }
	const std::string&   take_id ()        const { return _take_id; }

	void mark_for_remove();

	virtual void mark_streaming_write_started (const Lock& lock) {}
	virtual void mark_streaming_write_completed (const Lock& lock) = 0;

	virtual void session_saved() {}

	XMLNode& get_state ();
	int set_state (XMLNode const &, int version);

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

	size_t n_captured_xruns () const { return _xruns.size (); }
	XrunPositions const& captured_xruns () const { return _xruns; }
	void set_captured_xruns (XrunPositions const& xruns) { _xruns = xruns; }

	CueMarkers const & cue_markers() const { return _cue_markers; }
	bool add_cue_marker (CueMarker const &);
	bool move_cue_marker (CueMarker const &, timepos_t const & source_relative_position);
	bool remove_cue_marker (CueMarker const &);
	bool rename_cue_marker (CueMarker&, std::string const &);
	bool clear_cue_markers ();
	PBD::Signal0<void> CueMarkersChanged;

	virtual timepos_t natural_position() const { return _natural_position; }
	virtual void set_natural_position (timepos_t const & pos);

	bool have_natural_position() const { return _have_natural_position; }

	void set_allow_remove_if_empty (bool yn);

	Glib::Threads::Mutex& mutex() { return _lock; }
	Flag flags() const { return _flags; }

	virtual void inc_use_count ();
	virtual void dec_use_count ();
	int  use_count() const { return g_atomic_int_get (&_use_count); }
	bool used() const { return use_count() > 0; }

	uint32_t level() const { return _level; }

	std::string ancestor_name() { return _ancestor_name.empty() ? name() : _ancestor_name; }
	void set_ancestor_name(const std::string& name) { _ancestor_name = name; }

	void set_captured_for (std::string str) { _captured_for = str; }
	std::string captured_for() const { return _captured_for; }

  protected:
	DataType            _type;
	Flag                _flags;
	time_t              _timestamp;
	std::string         _take_id;
	timepos_t           _natural_position;
	bool                _have_natural_position;
	bool                _analysed;
	GATOMIC_QUAL gint _use_count; /* atomic */
	uint32_t            _level; /* how deeply nested is this source w.r.t a disk file */
	std::string         _ancestor_name;
	std::string        _captured_for;
	timecnt_t           _length;
	XrunPositions      _xruns;

	mutable Glib::Threads::Mutex _lock;
	mutable Glib::Threads::Mutex _analysis_lock;

  private:
	void fix_writable_flags ();

	XMLNode& get_cue_state () const;
	int set_cue_state (XMLNode const &, int);
};

}

#endif /* __ardour_source_h__ */
