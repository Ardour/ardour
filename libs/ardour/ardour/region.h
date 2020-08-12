/*
 * Copyright (C) 2000-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015-2017 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2018 Ben Loftis <ben@harrisonconsoles.com>
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

#ifndef __ardour_region_h__
#define __ardour_region_h__

#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/utility.hpp>

#include "temporal/timeline.h"
#include "temporal/range.h"

#include "pbd/undo.h"
#include "pbd/signals.h"
#include "ardour/ardour.h"
#include "ardour/data_type.h"
#include "ardour/automatable.h"
#include "ardour/movable.h"
#include "ardour/readable.h"
#include "ardour/session_object.h"
#include "ardour/trimmable.h"
#include "ardour/types_convert.h"

class XMLNode;

namespace ARDOUR {

namespace Properties {
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool>              muted;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool>              opaque;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool>              locked;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool>              video_locked;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool>              automatic;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool>              whole_file;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool>              import;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool>              external;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool>              sync_marked;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool>              left_of_split;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool>              right_of_split;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool>              hidden;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool>              position_locked;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool>              valid_transients;
	LIBARDOUR_API extern PBD::PropertyDescriptor<timecnt_t>         start;
	LIBARDOUR_API extern PBD::PropertyDescriptor<timecnt_t>         length;
	LIBARDOUR_API extern PBD::PropertyDescriptor<timepos_t>         position;
	LIBARDOUR_API extern PBD::PropertyDescriptor<timecnt_t>         sync_position;
	LIBARDOUR_API extern PBD::PropertyDescriptor<layer_t>           layer;
	LIBARDOUR_API extern PBD::PropertyDescriptor<timecnt_t>         ancestral_start;
	LIBARDOUR_API extern PBD::PropertyDescriptor<timecnt_t>         ancestral_length;
	LIBARDOUR_API extern PBD::PropertyDescriptor<float>             stretch;
	LIBARDOUR_API extern PBD::PropertyDescriptor<float>             shift;
	LIBARDOUR_API extern PBD::PropertyDescriptor<PositionLockStyle> position_lock_style;
	LIBARDOUR_API extern PBD::PropertyDescriptor<uint64_t>          layering_index;
	LIBARDOUR_API extern PBD::PropertyDescriptor<std::string>	tags;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool>		contents; // type doesn't matter here
};

class Playlist;
class Filter;
class ExportSpecification;
class Progress;

enum LIBARDOUR_API RegionEditState {
	EditChangesNothing = 0,
	EditChangesName    = 1,
	EditChangesID      = 2
};


class LIBARDOUR_API Region
	: public SessionObject
	, public boost::enable_shared_from_this<Region>
	, public Readable
	, public Trimmable
	, public Movable
{
public:
	typedef std::vector<boost::shared_ptr<Source> > SourceList;

	static void make_property_quarks ();

	static PBD::Signal2<void,boost::shared_ptr<RegionList>, const PBD::PropertyChange&> RegionsPropertyChanged;

	typedef std::map <PBD::PropertyChange, RegionList> ChangeMap;

	virtual ~Region();

	/** Note: changing the name of a Region does not constitute an edit */
	bool set_name (const std::string& str);

	const DataType& data_type () const { return _type; }

	/** How the region parameters play together:
	 *
	 * POSITION: first sample of the region along the timeline
	 * START:    first sample of the region within its source(s)
	 * LENGTH:   number of samples the region represents
	 */

	timepos_t nt_position ()  const { return _position.val(); }
	timecnt_t nt_start ()     const { return _start.val(); }
	timecnt_t nt_length ()    const { return _length.val(); }
	timepos_t nt_end()        const;

	timepos_t source_position () const;
	timepos_t source_relative_position (Temporal::timepos_t const &) const;
	timepos_t region_relative_position (Temporal::timepos_t const &) const;

	samplepos_t position_sample ()  const { return _position.val().samples(); }
	samplecnt_t start_sample ()     const { return _start.val().samples(); }
	samplecnt_t length_samples ()    const { return _length.val().samples(); }

	samplecnt_t readable_length_samples() const { return length_samples(); }

	layer_t    layer ()     const { return _layer; }

	void set_selected_for_solo(bool yn);

	samplecnt_t source_length (uint32_t n) const;
	uint32_t   max_source_level () const;

	/* these two are valid ONLY during a StateChanged signal handler */

	timepos_t last_position () const { return _last_position; }
	timecnt_t last_length ()   const { return _last_length; }

	samplecnt_t ancestral_start_sample ()  const { return _ancestral_start.val().samples(); }
	samplecnt_t ancestral_length_samples () const { return _ancestral_length.val().samples(); }
	timecnt_t ancestral_start ()  const { return _ancestral_start.val(); }
	timecnt_t ancestral_length () const { return _ancestral_length.val(); }

	float stretch () const { return _stretch; }
	float shift ()   const { return _shift; }

	void set_ancestral_data (timecnt_t const & start, timecnt_t const & length, float stretch, float shift);

	timecnt_t sync_offset (int& dir) const;
	timepos_t sync_position () const;

	timepos_t adjust_to_sync (timepos_t const &) const;

	/* first_sample() is an alias; last_sample() just hides some math */

	samplepos_t first_sample () const { return _position.val().samples(); }
	samplepos_t last_sample ()  const { return first_sample() + length_samples() - 1; }

	/** Return the earliest possible value of _position given the
	 *  value of _start within the region's sources
	 */
	timepos_t earliest_possible_position () const;
	/** Return the last possible value of _last_sample given the
	 *  value of _startin the regions's sources
	 */
	samplepos_t latest_possible_sample () const;

	Temporal::TimeRange last_range () const {
		return Temporal::TimeRange (_last_position, _last_position + _last_length);
	}

	Temporal::TimeRange range_samples () const {
		return Temporal::TimeRange (timepos_t::from_samples (first_sample()), timepos_t::from_samples (first_sample() + length_samples()));
	}

	bool hidden ()           const { return _hidden; }
	bool muted ()            const { return _muted; }
	bool opaque ()           const { return _opaque; }
	bool locked ()           const { return _locked; }
	bool position_locked ()  const { return _position_locked; }
	bool video_locked ()     const { return _video_locked; }
	bool automatic ()        const { return _automatic; }
	bool whole_file ()       const { return _whole_file; }
	bool captured ()         const { return !(_import || _external); }
	bool can_move ()         const { return !_position_locked && !_locked; }
	bool sync_marked ()      const { return _sync_marked; }
	bool external ()         const { return _external; }
	bool import ()           const { return _import; }

	Trimmable::CanTrim can_trim () const;

	PositionLockStyle position_lock_style () const { return _position_lock_style; }
	void set_position_lock_style (PositionLockStyle ps);
	void recompute_position_from_lock_style ();

	void suspend_property_changes ();

	bool covers (samplepos_t sample) const {
		return first_sample() <= sample && sample <= last_sample();
	}

	/** @return coverage of this region with the given range;
	 *  OverlapInternal: the range is internal to this region.
	 *  OverlapStart:    the range overlaps the start of this region.
	 *  OverlapEnd:      the range overlaps the end of this region.
	 *  OverlapExternal: the range overlaps all of this region.
	 */
	Evoral::OverlapType coverage (samplepos_t start, samplepos_t end) const {
		return Evoral::coverage (first_sample(), last_sample(), start, end);
	}

	bool exact_equivalent (boost::shared_ptr<const Region>) const;
	bool size_equivalent (boost::shared_ptr<const Region>) const;
	bool overlap_equivalent (boost::shared_ptr<const Region>) const;
	bool enclosed_equivalent (boost::shared_ptr<const Region>) const;
	bool layer_and_time_equivalent (boost::shared_ptr<const Region>) const;
	bool source_equivalent (boost::shared_ptr<const Region>) const;
	bool any_source_equivalent (boost::shared_ptr<const Region>) const;
	bool uses_source (boost::shared_ptr<const Source>, bool shallow = false) const;
	void deep_sources (std::set<boost::shared_ptr<Source> >&) const;

	std::string source_string () const;


	/* EDITING OPERATIONS */

	void set_length (timecnt_t const &);
	void set_start (timecnt_t const &);
	void set_position (timepos_t const &);
	void set_initial_position (timepos_t const &);
	void special_set_position (timepos_t const &);
	virtual void update_after_tempo_map_change (bool send_change = true);
	void nudge_position (timecnt_t const &);

	bool at_natural_position () const;
	void move_to_natural_position ();

	void move_start (timecnt_t const & distance);
	void trim_front (timepos_t const & new_position);
	void trim_end (timepos_t const & new_position);
	void trim_to (timepos_t const & pos,  timecnt_t const & length);

	/* fades are inherently audio in nature and we specify them in samples */
	virtual void fade_range (samplepos_t, samplepos_t) {}

	void cut_front (timepos_t const & new_position);
	void cut_end (timepos_t const & new_position);

	void set_layer (layer_t l); /* ONLY Playlist can call this */
	void raise ();
	void lower ();
	void raise_to_top ();
	void lower_to_bottom ();

	void set_sync_position (timepos_t const & n);
	void clear_sync_position ();
	void set_hidden (bool yn);
	void set_muted (bool yn);
	void set_whole_file (bool yn);
	void set_automatic (bool yn);
	void set_opaque (bool yn);
	void set_locked (bool yn);
	void set_video_locked (bool yn);
	void set_position_locked (bool yn);

	int apply (Filter &, Progress* progress = 0);

	boost::shared_ptr<ARDOUR::Playlist> playlist () const { return _playlist.lock(); }
	virtual void set_playlist (boost::weak_ptr<ARDOUR::Playlist>);

	void source_deleted (boost::weak_ptr<Source>);

	bool is_compound () const;

	boost::shared_ptr<Source> source (uint32_t n=0) const { return _sources[ (n < _sources.size()) ? n : 0 ]; }
	uint32_t n_channels() const { return _sources.size(); }

	SourceList& sources_for_edit ()           { return _sources; }
	const SourceList& sources ()        const { return _sources; }
	const SourceList& master_sources () const { return _master_sources; }

	std::vector<std::string> master_source_names();
	void set_master_sources (const SourceList&);

	/* automation */

	virtual boost::shared_ptr<Evoral::Control>
	control (const Evoral::Parameter& id, bool create=false) = 0;

	virtual boost::shared_ptr<const Evoral::Control>
	control (const Evoral::Parameter& id) const = 0;

	/* tags */

	std::string tags()    const { return _tags; }
	virtual bool set_tags (const std::string& str) {
		if (_tags != str) {
			_tags = str;
			PropertyChanged (PBD::PropertyChange (Properties::tags));
		}
		return true;
	}

	/* serialization */

	XMLNode&         get_state ();
	virtual int      set_state (const XMLNode&, int version);

	virtual boost::shared_ptr<Region> get_parent() const;

	uint64_t layering_index () const { return _layering_index; }
	void set_layering_index (uint64_t when) { _layering_index = when; }

	virtual bool is_dependent() const { return false; }
	virtual bool depends_on (boost::shared_ptr<Region> /*other*/) const { return false; }

	virtual void add_transient (samplepos_t) {
		// no transients, but its OK
	}

	virtual void clear_transients () {
		// no transients, but its OK
	}

	virtual void update_transient (samplepos_t /* old_position */, samplepos_t /* new_position */) {
		// no transients, but its OK
	}

	virtual void remove_transient (samplepos_t /* where */) {
		// no transients, but its OK
	}

	virtual void set_onsets (AnalysisFeatureList&) {
		// no transients, but its OK
	}

	/** merges _onsets and _user_transients into given list
	 * and removed exact duplicates.
	 */
	void transients (AnalysisFeatureList&);

	void captured_xruns (XrunPositions&, bool abs = false) const;

	/** merges _onsets OR _transients with _user_transients into given list
	 * if _onsets and _transients are unset, run analysis.
	 * list is not thinned, duplicates remain in place.
	 *
	 * intended for: Playlist::find_next_transient ()
	 */
	virtual void get_transients (AnalysisFeatureList&) {
		// no transients, but its OK
	}

	/* wrapper to the above for easy access throug Lua */
	AnalysisFeatureList transients () {
		AnalysisFeatureList rv;
		get_transients (rv);
		return rv;
	}

	bool has_transients () const;

	virtual int separate_by_channel (std::vector< boost::shared_ptr<Region> >&) const {
		return -1;
	}

	void maybe_invalidate_transients ();

	void drop_sources ();

	/* Allow to collect RegionsPropertyChanged signal emissions */
	void set_changemap (ChangeMap* changemap) {
		_changemap = changemap;
	}

	void get_cue_markers (CueMarkers&, bool abs = false) const;
	void move_cue_marker (CueMarker const &, samplepos_t region_relative_position);
	void rename_cue_marker (CueMarker&, std::string const &);

protected:
	virtual XMLNode& state ();

	friend class RegionFactory;

	/** Construct a region from multiple sources*/
	Region (const SourceList& srcs);

	/** Construct a region from another region */
	Region (boost::shared_ptr<const Region>);

	/** Construct a region from another region, at an offset within that region */
	Region (boost::shared_ptr<const Region>, timecnt_t const & start_offset);

	/** Construct a region as a copy of another region, but with different sources */
	Region (boost::shared_ptr<const Region>, const SourceList&);

	/** Constructor for derived types only */
	Region (Session& s, timecnt_t const & start, timecnt_t const & length, const std::string& name, DataType);

	virtual bool can_trim_start_before_source_start () const {
		return false;
	}

protected:

	void send_change (const PBD::PropertyChange&);
	virtual int _set_state (const XMLNode&, int version, PBD::PropertyChange& what_changed, bool send_signal);
	void post_set (const PBD::PropertyChange&);
	virtual void set_position_internal (timepos_t const & pos);
	virtual void set_length_internal (timecnt_t const &);
	virtual void set_start_internal (timecnt_t const &);
	bool verify_start_and_length (timecnt_t const &, timecnt_t&);
	void first_edit ();

	DataType _type;

	PBD::Property<bool>      _sync_marked;
	PBD::Property<bool>      _left_of_split;
	PBD::Property<bool>      _right_of_split;
	PBD::Property<bool>      _valid_transients;
	PBD::Property<timecnt_t> _start;
	PBD::Property<timecnt_t> _length;
	PBD::Property<timepos_t> _position;
	/** Sync position relative to the start of our file */
	PBD::Property<timecnt_t> _sync_position;

	SourceList              _sources;
	/** Used when timefx are applied, so we can always use the original source */
	SourceList              _master_sources;

	boost::weak_ptr<ARDOUR::Playlist> _playlist;

	void merge_features (AnalysisFeatureList&, const AnalysisFeatureList&, const sampleoffset_t) const;

	AnalysisFeatureList     _onsets; // used by the Ferret (Aubio OnsetDetector)

	// _transient_user_start is covered by  _valid_transients
	AnalysisFeatureList     _user_transients; // user added
	samplepos_t             _transient_user_start; // region's _start relative to user_transients

	// these are used by Playlist::find_next_transient() in absence of onsets
	AnalysisFeatureList     _transients; // Source Analysis (QM Transient), user read-only
	samplepos_t             _transient_analysis_start;
	samplepos_t             _transient_analysis_end;

	bool                    _soloSelected;

private:
	void mid_thaw (const PBD::PropertyChange&);

	virtual void trim_to_internal (timepos_t const & position, timecnt_t const & length);
	void modify_front (timepos_t const & new_position, bool reset_fade);
	void modify_end (timepos_t const & new_position, bool reset_fade);

	void maybe_uncopy ();

	bool verify_start (timecnt_t const &);
	bool verify_start_mutable (timecnt_t&);
	bool verify_length (timecnt_t&);

	virtual void recompute_at_start () = 0;
	virtual void recompute_at_end () = 0;

	PBD::Property<bool>        _muted;
	PBD::Property<bool>        _opaque;
	PBD::Property<bool>        _locked;
	PBD::Property<bool>        _video_locked;
	PBD::Property<bool>        _automatic;
	PBD::Property<bool>        _whole_file;
	PBD::Property<bool>        _import;
	PBD::Property<bool>        _external;
	PBD::Property<bool>        _hidden;
	PBD::Property<bool>        _position_locked;
	PBD::Property<timecnt_t>   _ancestral_start;
	PBD::Property<timecnt_t>   _ancestral_length;
	PBD::Property<float>       _stretch;
	PBD::Property<float>       _shift;
	PBD::EnumProperty<PositionLockStyle> _position_lock_style;
	PBD::Property<uint64_t>    _layering_index;
	PBD::Property<std::string> _tags;
	PBD::Property<bool>        _contents; // type is irrelevant

	timecnt_t             _last_length;
	timepos_t             _last_position;
	mutable RegionEditState _first_edit;
	layer_t                 _layer;

	ChangeMap* _changemap;

	void register_properties ();

	void use_sources (SourceList const &);
};

} /* namespace ARDOUR */

#endif /* __ardour_region_h__ */
