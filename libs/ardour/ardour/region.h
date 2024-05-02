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

#include <memory>
#include <vector>

#include <boost/utility.hpp>

#include "temporal/domain_swap.h"
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

namespace PBD {
	class Progress;
}

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
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool>              valid_transients; // used for signal only
	LIBARDOUR_API extern PBD::PropertyDescriptor<timepos_t>         start;
	LIBARDOUR_API extern PBD::PropertyDescriptor<timecnt_t>         length;
	LIBARDOUR_API extern PBD::PropertyDescriptor<timepos_t>         sync_position;
	LIBARDOUR_API extern PBD::PropertyDescriptor<layer_t>           layer;
	LIBARDOUR_API extern PBD::PropertyDescriptor<timepos_t>         ancestral_start;
	LIBARDOUR_API extern PBD::PropertyDescriptor<timecnt_t>         ancestral_length;
	LIBARDOUR_API extern PBD::PropertyDescriptor<float>             stretch;
	LIBARDOUR_API extern PBD::PropertyDescriptor<float>             shift;
	LIBARDOUR_API extern PBD::PropertyDescriptor<uint64_t>          layering_index;
	LIBARDOUR_API extern PBD::PropertyDescriptor<std::string>       tags;
	LIBARDOUR_API extern PBD::PropertyDescriptor<uint64_t>          reg_group;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool>              contents; // type doesn't matter here, used for signal only
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool>              region_fx; // type doesn't matter here, used for signal only
};

class Playlist;
class Filter;
class ExportSpecification;
class Plugin;
class RegionFxPlugin;

enum LIBARDOUR_API RegionEditState {
	EditChangesNothing = 0,
	EditChangesName    = 1,
	EditChangesID      = 2
};

enum LIBARDOUR_API RegionOperationFlag {
	LeftOfSplit    = 0,
	InnerSplit     = 1, // when splitting a Range, there's left/center/right parts of the split
	RightOfSplit   = 2,
	Paste          = 4
};

class LIBARDOUR_API Region
	: public SessionObject
	, public std::enable_shared_from_this<Region>
	, public Trimmable
	, public Movable
	, public Temporal::TimeDomainSwapper
{
public:
	typedef std::vector<std::shared_ptr<Source> > SourceList;
	typedef std::list<std::shared_ptr<RegionFxPlugin>> RegionFxList;

	static void make_property_quarks ();

	static PBD::Signal2<void,std::shared_ptr<RegionList>, const PBD::PropertyChange&> RegionsPropertyChanged;

	PBD::Signal0<void> RegionFxChanged;

	typedef std::map <PBD::PropertyChange, RegionList> ChangeMap;

	virtual ~Region();

	/** Note: changing the name of a Region does not constitute an edit */
	bool set_name (const std::string& str);

	PBD::PropertyList derive_properties (bool with_times = true, bool with_envelope = false) const;

	const DataType& data_type () const { return _type; }
	Temporal::TimeDomain time_domain() const;
	void start_domain_bounce (Temporal::DomainBounceInfo&);
	void finish_domain_bounce (Temporal::DomainBounceInfo&);

	/** How the region parameters play together:
	 *
	 * POSITION: first sample of the region along the timeline
	 * START:    first sample of the region within its source(s)
	 * LENGTH:   number of samples the region represents
	 */

	timepos_t position ()  const { return _length.val().position(); }
	timepos_t start ()     const { return _start.val(); }
	timecnt_t length ()    const { return _length.val(); }
	timepos_t end()        const;
	timepos_t nt_last()    const { return end().decrement(); }

	timepos_t source_position () const;
	timecnt_t source_relative_position (Temporal::timepos_t const &) const;
	timecnt_t region_relative_position (Temporal::timepos_t const &) const;

	samplepos_t position_sample ()  const { return position().samples(); }
	samplecnt_t start_sample ()     const { return _start.val().samples(); }
	samplecnt_t length_samples ()   const { return _length.val().samples(); }

	layer_t    layer ()     const { return _layer; }

	void set_selected_for_solo(bool yn);

	timepos_t source_length (uint32_t n) const;
	uint32_t   max_source_level () const;

	/* these two are valid ONLY during a StateChanged signal handler */

	timepos_t last_position () const { return _last_length.position(); }
	timecnt_t last_length ()   const { return _last_length; }

	samplecnt_t ancestral_start_sample ()  const { return _ancestral_start.val().samples(); }
	samplecnt_t ancestral_length_samples () const { return _ancestral_length.val().samples(); }
	timepos_t ancestral_start ()  const { return _ancestral_start.val(); }
	timecnt_t ancestral_length () const { return _ancestral_length.val(); }

	/** Region Groups:
	 * every region has a group-id. regions that have the same group-id (excepting zero) are 'grouped'
	 * if you select a 'grouped' region, then all other regions in the group will be selected
	 * operations like Import, Record, and Paste assign a group-id to the new regions they create
	 * users can explicitly group regions, which implies a stronger connection and gets the 'explicit' flag
	 * users can explicitly ungroup regions, which prevents ardour from applying equivalent-regions logic
	 * regions with no flags and no group-id (prior sessions) will revert to equivalent-regions logic */

	/** RegionGroupRetainer is an RAII construct to retain a group-id for the length of an operation that creates regions */
	struct RegionGroupRetainer {
		RegionGroupRetainer ()
		{
			Glib::Threads::Mutex::Lock lm (_operation_rgroup_mutex);
			if (_retained_group_id == 0) {
				_retained_take_cnt = 0;
				++_next_group_id;
				_operation_rgroup_map.clear ();              // this is used for split & paste operations that honor the region's prior grouping
				_retained_group_id    = _next_group_id << 4; // this is used for newly created regions via recording or importing
				_clear_on_destruction = true;
			} else {
				_clear_on_destruction = false;
			}
		}
		~RegionGroupRetainer ()
		{
			if (_clear_on_destruction) {
				Glib::Threads::Mutex::Lock lm (_operation_rgroup_mutex);
				_retained_group_id = 0;
				_next_group_id += _retained_take_cnt;
				_operation_rgroup_map.clear();
			}
		}
		bool _clear_on_destruction;
	};

	static uint64_t next_group_id () { return _next_group_id; }
	static void set_next_group_id (uint64_t ngid) { _next_group_id = ngid; }

	/* access the retained group-id for actions like Recording, Import.
	 *
	 * Note When a single take creates multiple layered regions (e.g. loop recording)
	 * then the group id need to be bumped for each take
	 */
	static uint64_t get_retained_group_id (uint64_t take = 0) {
		_retained_take_cnt = std::max (_retained_take_cnt, take);
		return _retained_group_id + (take << 4);
	}

	/* access the group-id for an operation on a region, honoring the existing region's group status */
	static uint64_t get_region_operation_group_id (uint64_t old_region_group, RegionOperationFlag flags);

	uint64_t region_group () const { return _reg_group; }
	void set_region_group (uint64_t rg, bool explicitly = false) { _reg_group = rg | (explicitly ? Explicit : NoGroup); }
	void unset_region_group (bool explicitly = false) { _reg_group = (explicitly ? Explicit : NoGroup); }

	bool is_explicitly_grouped()   { return (_reg_group & Explicit) == Explicit; }
	bool is_implicitly_ungrouped() { return (_reg_group == NoGroup); }
	bool is_explicitly_ungrouped() { return (_reg_group == Explicit); }

	float stretch () const { return _stretch; }
	float shift ()   const { return _shift; }

	void set_ancestral_data (timepos_t const & start, timecnt_t const & length, float stretch, float shift);

	timecnt_t sync_offset (int& dir) const;
	timepos_t sync_position () const;

	timepos_t adjust_to_sync (timepos_t const &) const;

	/* first_sample() is an alias; last_sample() just hides some math */

	samplepos_t first_sample () const { return position().samples(); }
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
		return Temporal::TimeRange (last_position(), last_position() + _last_length);
	}

	Temporal::TimeRange range_samples () const {
		return Temporal::TimeRange (timepos_t (first_sample()), timepos_t (first_sample() + length_samples()));
	}

	Temporal::TimeRange range () const {
		return Temporal::TimeRange (position(), position() + length());
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

	Temporal::TimeDomain position_time_domain () const;
	void set_position_time_domain (Temporal::TimeDomain ps);
	void recompute_position_from_time_domain ();

	void suspend_property_changes ();

	bool covers (samplepos_t sample) const {
		return first_sample() <= sample && sample <= last_sample();
	}

	bool covers (timepos_t const & pos) const {
		return position() <= pos && pos <= nt_last();
	}

	/** @return coverage of this region with the given range;
	 *  OverlapInternal: the range is internal to this region.
	 *  OverlapStart:    the range overlaps the start of this region.
	 *  OverlapEnd:      the range overlaps the end of this region.
	 *  OverlapExternal: the range overlaps all of this region.
	 */
	Temporal::OverlapType coverage (timepos_t const & start, timepos_t const & end) const {
		return Temporal::coverage_exclusive_ends (position(), nt_last(), start, end);
	}

	bool exact_equivalent (std::shared_ptr<const Region>) const;
	bool size_equivalent (std::shared_ptr<const Region>) const;
	bool overlap_equivalent (std::shared_ptr<const Region>) const;
	bool enclosed_equivalent (std::shared_ptr<const Region>) const;
	bool layer_and_time_equivalent (std::shared_ptr<const Region>) const;
	bool source_equivalent (std::shared_ptr<const Region>) const;
	bool any_source_equivalent (std::shared_ptr<const Region>) const;
	bool uses_source (std::shared_ptr<const Source>, bool shallow = false) const;
	void deep_sources (std::set<std::shared_ptr<Source> >&) const;

	std::string source_string () const;


	/* EDITING OPERATIONS */

	void set_length (timecnt_t const &);
	void set_start (timepos_t const &);
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

	/* ONLY Playlist can call this */
	void set_layer (layer_t l);
	void set_length_unchecked (timecnt_t const &);
	void set_position_unchecked (timepos_t const &);
	void modify_front_unchecked (timepos_t const & new_position, bool reset_fade);
	void modify_end_unchecked (timepos_t const & new_position, bool reset_fade);

	Temporal::timepos_t region_beats_to_absolute_time(Temporal::Beats beats) const;
	/** Convert a timestamp in beats into timepos_t (both relative to region position) */
	Temporal::timepos_t region_beats_to_region_time (Temporal::Beats beats) const {
		return timepos_t (position().distance (region_beats_to_absolute_time (beats)));
	}
	/** Convert a timestamp in beats relative to region position into beats relative to source start */
	Temporal::Beats region_beats_to_source_beats (Temporal::Beats beats) const {
		return position().distance (region_beats_to_absolute_time (beats)).beats ();
	}
	/** Convert a distance within a region to beats relative to region position */
	Temporal::Beats region_distance_to_region_beats (Temporal::timecnt_t const &) const;

	/** Convert a timestamp in beats measured from source start into absolute beats */
	Temporal::Beats source_beats_to_absolute_beats(Temporal::Beats beats) const;

	/** Convert a timestamp in beats measured from source start into absolute samples */
	Temporal::timepos_t source_beats_to_absolute_time(Temporal::Beats beats) const;

	/** Convert a timestamp in beats measured from source start into region-relative samples */
	Temporal::timepos_t source_beats_to_region_time(Temporal::Beats beats) const {
		return timepos_t (position().distance (source_beats_to_absolute_time (beats)));
	}
	/** Convert a timestamp in absolute time to beats measured from source start*/
	Temporal::Beats absolute_time_to_source_beats(Temporal::timepos_t const &) const;

	Temporal::Beats absolute_time_to_region_beats (Temporal::timepos_t const &) const;

	Temporal::timepos_t absolute_time_to_region_time (Temporal::timepos_t const &) const;

	int apply (Filter &, PBD::Progress* progress = 0);

	std::shared_ptr<ARDOUR::Playlist> playlist () const { return _playlist.lock(); }
	virtual void set_playlist (std::weak_ptr<ARDOUR::Playlist>);

	void source_deleted (std::weak_ptr<Source>);

	bool is_compound () const;

	std::shared_ptr<Source> source (uint32_t n=0) const { return _sources[ (n < _sources.size()) ? n : 0 ]; }

	SourceList& sources_for_edit ()           { return _sources; }
	const SourceList& sources ()        const { return _sources; }
	const SourceList& master_sources () const { return _master_sources; }

	std::vector<std::string> master_source_names();
	void set_master_sources (const SourceList&);

	/* automation */

	virtual std::shared_ptr<Evoral::Control>
		control (const Evoral::Parameter& id, bool create=false) = 0;

	virtual std::shared_ptr<const Evoral::Control>
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

	XMLNode&         get_state () const;
	virtual int      set_state (const XMLNode&, int version);

	virtual bool do_export (std::string const&) const = 0;

	virtual std::shared_ptr<Region> get_parent() const;

	uint64_t layering_index () const { return _layering_index; }
	void set_layering_index (uint64_t when) { _layering_index = when; }

	virtual bool is_dependent() const { return false; }
	virtual bool depends_on (std::shared_ptr<Region> /*other*/) const { return false; }

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

	virtual int separate_by_channel (std::vector< std::shared_ptr<Region> >&) const {
		return -1;
	}

	void maybe_invalidate_transients ();

	void drop_sources ();

	/* Allow to collect RegionsPropertyChanged signal emissions */
	void set_changemap (ChangeMap* changemap) {
		_changemap = changemap;
	}

	void get_cue_markers (CueMarkers&, bool abs = false) const;
	void move_cue_marker (CueMarker const &, timepos_t const & region_relative_position);
	void rename_cue_marker (CueMarker&, std::string const &);

	/* Region Fx */
	bool load_plugin (ARDOUR::PluginType type, std::string const& name);
	bool add_plugin (std::shared_ptr<RegionFxPlugin>, std::shared_ptr<RegionFxPlugin> pos = std::shared_ptr<RegionFxPlugin> ());
	virtual bool remove_plugin (std::shared_ptr<RegionFxPlugin>) { return false; }
	virtual void reorder_plugins (RegionFxList const&);

	bool has_region_fx () const {
		Glib::Threads::RWLock::ReaderLock lm (_fx_lock);
		return !_plugins.empty ();
	}

	std::shared_ptr<RegionFxPlugin> nth_plugin (uint32_t n) const {
		Glib::Threads::RWLock::ReaderLock lm (_fx_lock);
		for (auto const& i : _plugins) {
			if (0 == n--) {
				return i;
			}
		}
		return std::shared_ptr<RegionFxPlugin> ();
	}

	void foreach_plugin (boost::function<void(std::weak_ptr<RegionFxPlugin>)> method) const {
		Glib::Threads::RWLock::ReaderLock lm (_fx_lock);
		for (auto const& i : _plugins) {
			method (std::weak_ptr<RegionFxPlugin> (i));
		}
	}

protected:
	virtual XMLNode& state () const;

	friend class RegionFactory;

	/** Construct a region from multiple sources*/
	Region (const SourceList& srcs);

	/** Construct a region from another region */
	Region (std::shared_ptr<const Region>);

	/** Construct a region from another region, at an offset within that region */
	Region (std::shared_ptr<const Region>, timecnt_t const & start_offset);

	/** Construct a region as a copy of another region, but with different sources */
	Region (std::shared_ptr<const Region>, const SourceList&);

	/** Constructor for derived types only */
	Region (Session& s, timepos_t const & start, timecnt_t const & length, const std::string& name, DataType);

	virtual bool can_trim_start_before_source_start () const {
		return false;
	}

protected:
	virtual bool _add_plugin (std::shared_ptr<RegionFxPlugin>, std::shared_ptr<RegionFxPlugin>, bool) { return false; }
	virtual void fx_latency_changed (bool no_emit);

	virtual void send_change (const PBD::PropertyChange&);
	virtual int _set_state (const XMLNode&, int version, PBD::PropertyChange& what_changed, bool send_signal);
	virtual void set_position_internal (timepos_t const & pos);
	virtual void set_length_internal (timecnt_t const &);
	virtual void set_start_internal (timepos_t const &);
	bool verify_start_and_length (timepos_t const &, timecnt_t&);
	void first_edit ();

	void override_opaqueness (bool yn) {
		_opaque = yn;
	}

	/* This is always using AudioTime. convenient for evenlopes in AudioRegion */
	timepos_t len_as_tpos () const { return timepos_t((samplepos_t)_length.val().samples()); }

	DataType _type;

	mutable Glib::Threads::RWLock _fx_lock;
	uint32_t                      _fx_latency;
	RegionFxList                  _plugins;

	PBD::Property<bool>      _sync_marked;
	PBD::Property<bool>      _left_of_split;
	PBD::Property<bool>      _right_of_split;
	PBD::Property<bool>      _valid_transients;
	PBD::Property<timepos_t> _start;
	PBD::Property<timecnt_t> _length;
	/** Sync position relative to the start of our file */
	PBD::Property<timepos_t> _sync_position;

	SourceList              _sources;
	/** Used when timefx are applied, so we can always use the original source */
	SourceList              _master_sources;

	std::weak_ptr<ARDOUR::Playlist> _playlist;

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

	void trim_to_internal (timepos_t const & position, timecnt_t const & length);

	void maybe_uncopy ();
	void subscribe_to_source_drop ();

	bool verify_start (timepos_t const &);
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
	PBD::Property<timepos_t>   _ancestral_start;
	PBD::Property<timecnt_t>   _ancestral_length;
	PBD::Property<float>       _stretch;
	PBD::Property<float>       _shift;
	PBD::Property<uint64_t>    _layering_index;
	PBD::Property<std::string> _tags;
	PBD::Property<uint64_t>    _reg_group;
	PBD::Property<bool>        _contents; // type is irrelevant

	timecnt_t             _last_length;
	mutable RegionEditState _first_edit;
	layer_t                 _layer;

	ChangeMap* _changemap;

	void register_properties ();

	void use_sources (SourceList const &);

	enum RegionGroupFlags : uint64_t {
		NoGroup   = 0x0, // no flag: implicitly grouped if the id is nonzero; or implicitly 'un-grouped' if the group-id is zero
		Explicit  = 0x1, // the user has explicitly grouped or ungrouped this region. explicitly grouped regions can cross track-group boundaries
	};
	static uint64_t _retained_group_id;
	static uint64_t _retained_take_cnt;
	static uint64_t _next_group_id;

	static Glib::Threads::Mutex         _operation_rgroup_mutex;
	static std::map<uint64_t, uint64_t> _operation_rgroup_map;

	std::atomic<int>          _source_deleted;
	Glib::Threads::Mutex      _source_list_lock;
	PBD::ScopedConnectionList _source_deleted_connections;
};

} /* namespace ARDOUR */

#endif /* __ardour_region_h__ */
