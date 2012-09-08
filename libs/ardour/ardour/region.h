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

*/

#ifndef __ardour_region_h__
#define __ardour_region_h__

#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/utility.hpp>

#include "pbd/undo.h"
#include "pbd/signals.h"

#include "ardour/ardour.h"
#include "ardour/data_type.h"
#include "ardour/automatable.h"
#include "ardour/movable.h"
#include "ardour/readable.h"
#include "ardour/session_object.h"
#include "ardour/trimmable.h"

class XMLNode;


namespace ARDOUR {

namespace Properties {
	extern PBD::PropertyDescriptor<bool>              muted;
	extern PBD::PropertyDescriptor<bool>              opaque;
	extern PBD::PropertyDescriptor<bool>              locked;
	extern PBD::PropertyDescriptor<bool>              automatic;
	extern PBD::PropertyDescriptor<bool>              whole_file;
	extern PBD::PropertyDescriptor<bool>              import;
	extern PBD::PropertyDescriptor<bool>              external;
	extern PBD::PropertyDescriptor<bool>              sync_marked;
	extern PBD::PropertyDescriptor<bool>              left_of_split;
	extern PBD::PropertyDescriptor<bool>              right_of_split;
	extern PBD::PropertyDescriptor<bool>              hidden;
	extern PBD::PropertyDescriptor<bool>              position_locked;
	extern PBD::PropertyDescriptor<bool>              valid_transients;
	extern PBD::PropertyDescriptor<framepos_t>        start;
	extern PBD::PropertyDescriptor<framecnt_t>        length;
	extern PBD::PropertyDescriptor<framepos_t>        position;
	extern PBD::PropertyDescriptor<framecnt_t>        sync_position;
	extern PBD::PropertyDescriptor<layer_t>           layer;
	extern PBD::PropertyDescriptor<framepos_t>        ancestral_start;
	extern PBD::PropertyDescriptor<framecnt_t>        ancestral_length;
	extern PBD::PropertyDescriptor<float>             stretch;
	extern PBD::PropertyDescriptor<float>             shift;
	extern PBD::PropertyDescriptor<PositionLockStyle> position_lock_style;
	extern PBD::PropertyDescriptor<uint64_t>          layering_index;
};

class Playlist;
class Filter;
class ExportSpecification;
class Progress;

enum RegionEditState {
	EditChangesNothing = 0,
	EditChangesName    = 1,
	EditChangesID      = 2
};


class Region
	: public SessionObject
	, public boost::enable_shared_from_this<Region>
	, public Readable
	, public Trimmable
	, public Movable
{
  public:
	typedef std::vector<boost::shared_ptr<Source> > SourceList;

	static void make_property_quarks ();

	static PBD::Signal2<void,boost::shared_ptr<ARDOUR::Region>, const PBD::PropertyChange&> RegionPropertyChanged;

	virtual ~Region();

	/** Note: changing the name of a Region does not constitute an edit */
	bool set_name (const std::string& str);

	const DataType& data_type () const { return _type; }

	AnalysisFeatureList transients () { return _transients; };

	/** How the region parameters play together:
	 *
	 * POSITION: first frame of the region along the timeline
	 * START:    first frame of the region within its source(s)
	 * LENGTH:   number of frames the region represents
	 */
	framepos_t position ()  const { return _position; }
	framepos_t start ()     const { return _start; }
	framecnt_t length ()    const { return _length; }
	layer_t    layer ()     const { return _layer; }

	framecnt_t source_length(uint32_t n) const;
	uint32_t   max_source_level () const;

	/* these two are valid ONLY during a StateChanged signal handler */

	framepos_t last_position () const { return _last_position; }
	framecnt_t last_length ()   const { return _last_length; }

	framepos_t ancestral_start ()  const { return _ancestral_start; }
	framecnt_t ancestral_length () const { return _ancestral_length; }

	float stretch () const { return _stretch; }
	float shift ()   const { return _shift; }

	void set_ancestral_data (framepos_t start, framecnt_t length, float stretch, float shift);

	frameoffset_t sync_offset (int& dir) const;
	framepos_t sync_position () const;
	framepos_t sync_point () const;

	framepos_t adjust_to_sync (framepos_t) const;

	/* first_frame() is an alias; last_frame() just hides some math */

	framepos_t first_frame () const { return _position; }
	framepos_t last_frame ()  const { return _position + _length - 1; }

	/** Return the earliest possible value of _position given the
	 *  value of _start within the region's sources
	 */
	framepos_t earliest_possible_position () const;
	/** Return the last possible value of _last_frame given the
	 *  value of _startin the regions's sources
	 */
	framepos_t latest_possible_frame () const;

	Evoral::Range<framepos_t> last_range () const {
		return Evoral::Range<framepos_t> (_last_position, _last_position + _last_length - 1);
	}
	
	Evoral::Range<framepos_t> range () const {
		return Evoral::Range<framepos_t> (first_frame(), last_frame());
	}

	bool hidden ()           const { return _hidden; }
	bool muted ()            const { return _muted; }
	bool opaque ()           const { return _opaque; }
	bool locked ()           const { return _locked; }
	bool position_locked ()  const { return _position_locked; }
	bool valid_transients () const { return _valid_transients; }
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

	bool covers (framepos_t frame) const {
		return first_frame() <= frame && frame <= last_frame();
	}

	/** @return coverage of this region with the given range;
	 *  OverlapInternal: the range is internal to this region.
	 *  OverlapStart:    the range overlaps the start of this region.
	 *  OverlapEnd:      the range overlaps the end of this region.
	 *  OverlapExternal: the range overlaps all of this region.
	 */
	Evoral::OverlapType coverage (framepos_t start, framepos_t end) const {
		return Evoral::coverage (first_frame(), last_frame(), start, end);
	}

	bool equivalent (boost::shared_ptr<const Region>) const;
	bool size_equivalent (boost::shared_ptr<const Region>) const;
	bool overlap_equivalent (boost::shared_ptr<const Region>) const;
	bool region_list_equivalent (boost::shared_ptr<const Region>) const;
	bool source_equivalent (boost::shared_ptr<const Region>) const;
	bool uses_source (boost::shared_ptr<const Source>) const;

	std::string source_string () const;


	/* EDITING OPERATIONS */

	void set_length (framecnt_t);
	void set_start (framepos_t);
	void set_position (framepos_t);
	void special_set_position (framepos_t);
	virtual void update_after_tempo_map_change ();
	void nudge_position (frameoffset_t);

	bool at_natural_position () const;
	void move_to_natural_position ();

	void trim_start (framepos_t new_position);
	void trim_front (framepos_t new_position);
	void trim_end (framepos_t new_position);
	void trim_to (framepos_t position, framecnt_t length);

	void cut_front (framepos_t new_position);
	void cut_end (framepos_t new_position);

	void set_layer (layer_t l); /* ONLY Playlist can call this */
	void raise ();
	void lower ();
	void raise_to_top ();
	void lower_to_bottom ();

	void set_sync_position (framepos_t n);
	void clear_sync_position ();
	void set_hidden (bool yn);
	void set_muted (bool yn);
	void set_whole_file (bool yn);
	void set_automatic (bool yn);
	void set_opaque (bool yn);
	void set_locked (bool yn);
	void set_position_locked (bool yn);

	int apply (Filter &, Progress* progress = 0);

	boost::shared_ptr<ARDOUR::Playlist> playlist () const { return _playlist.lock(); }
	virtual void set_playlist (boost::weak_ptr<ARDOUR::Playlist>);

	void source_deleted (boost::weak_ptr<Source>);

	bool is_compound () const;

	boost::shared_ptr<Source> source (uint32_t n=0) const { return _sources[ (n < _sources.size()) ? n : 0 ]; }
	uint32_t n_channels() const { return _sources.size(); }

	const SourceList& sources ()        const { return _sources; }
	const SourceList& master_sources () const { return _master_sources; }

	std::vector<std::string> master_source_names();
	void set_master_sources (const SourceList&);

	/* automation */

	virtual boost::shared_ptr<Evoral::Control>
	control (const Evoral::Parameter& id, bool create=false) = 0;

	virtual boost::shared_ptr<const Evoral::Control>
	control (const Evoral::Parameter& id) const = 0;

	/* serialization */

	XMLNode&         get_state ();
	virtual XMLNode& state ();
	virtual int      set_state (const XMLNode&, int version);

	virtual boost::shared_ptr<Region> get_parent() const;

	uint64_t layering_index () const { return _layering_index; }
	void set_layering_index (uint64_t when) { _layering_index = when; }

	virtual bool is_dependent() const { return false; }
	virtual bool depends_on (boost::shared_ptr<Region> /*other*/) const { return false; }

	virtual void add_transient (framepos_t) {
		// no transients, but its OK
	}

	virtual int update_transient (framepos_t /* old_position */, framepos_t /* new_position */) {
		// no transients, but its OK
		return 0;
	}

	virtual void remove_transient (framepos_t /* where */) {
		// no transients, but its OK
	}

	virtual int set_transients (AnalysisFeatureList&) {
		// no transients, but its OK
		return 0;
	}

	virtual int get_transients (AnalysisFeatureList&, bool force_new = false) {
		(void) force_new;
		// no transients, but its OK
		return 0;
	}

	virtual int adjust_transients (frameoffset_t /*delta*/) {
		// no transients, but its OK
		return 0;
	}

	virtual int separate_by_channel (ARDOUR::Session&,
			std::vector< boost::shared_ptr<Region> >&) const {
		return 0;
	}

	void invalidate_transients ();

	void drop_sources ();

  protected:
	friend class RegionFactory;

	/** Construct a region from multiple sources*/
	Region (const SourceList& srcs);

	/** Construct a region from another region */
	Region (boost::shared_ptr<const Region>);

	/** Construct a region from another region, at an offset within that region */
	Region (boost::shared_ptr<const Region>, frameoffset_t start_offset);

	/** Construct a region as a copy of another region, but with different sources */
	Region (boost::shared_ptr<const Region>, const SourceList&);

	/** Constructor for derived types only */
	Region (Session& s, framepos_t start, framecnt_t length, const std::string& name, DataType);

	virtual bool can_trim_start_before_source_start () const {
		return false;
	}

  protected:

	void send_change (const PBD::PropertyChange&);
	virtual int _set_state (const XMLNode&, int version, PBD::PropertyChange& what_changed, bool send_signal);
	void post_set (const PBD::PropertyChange&);
	virtual void set_position_internal (framepos_t pos, bool allow_bbt_recompute);
	virtual void set_length_internal (framecnt_t);
	virtual void set_start_internal (framecnt_t);
	
	DataType _type;

	PBD::Property<bool>        _sync_marked;
	PBD::Property<bool>        _left_of_split;
	PBD::Property<bool>        _right_of_split;
	PBD::Property<bool>        _valid_transients;
	PBD::Property<framepos_t>  _start;
	PBD::Property<framecnt_t>  _length;
	PBD::Property<framepos_t>  _position;
	/** Sync position relative to the start of our file */
	PBD::Property<framepos_t>  _sync_position;
	
	SourceList              _sources;
	/** Used when timefx are applied, so we can always use the original source */
	SourceList              _master_sources;

	AnalysisFeatureList     _transients;
	
	boost::weak_ptr<ARDOUR::Playlist> _playlist;
	
  private:
	void mid_thaw (const PBD::PropertyChange&);

	void trim_to_internal (framepos_t position, framecnt_t length);
	void modify_front (framepos_t new_position, bool reset_fade);
	void modify_end (framepos_t new_position, bool reset_fade);

	void maybe_uncopy ();
	void first_edit ();

	bool verify_start (framepos_t);
	bool verify_start_and_length (framepos_t, framecnt_t&);
	bool verify_start_mutable (framepos_t&_start);
	bool verify_length (framecnt_t);

	virtual void recompute_at_start () = 0;
	virtual void recompute_at_end () = 0;

	PBD::Property<bool>        _muted;
	PBD::Property<bool>        _opaque;
	PBD::Property<bool>        _locked;
	PBD::Property<bool>        _automatic;
	PBD::Property<bool>        _whole_file;
	PBD::Property<bool>        _import;
	PBD::Property<bool>        _external;
	PBD::Property<bool>        _hidden;
	PBD::Property<bool>        _position_locked;
	PBD::Property<framepos_t>  _ancestral_start;
	PBD::Property<framecnt_t>  _ancestral_length;
	PBD::Property<float>       _stretch;
	PBD::Property<float>       _shift;
	PBD::EnumProperty<PositionLockStyle> _position_lock_style;
	PBD::Property<uint64_t>    _layering_index;

	framecnt_t              _last_length;
	framepos_t              _last_position;
	mutable RegionEditState _first_edit;
	Timecode::BBT_Time      _bbt_time;
	layer_t                 _layer;

	void register_properties ();

	void use_sources (SourceList const &);
};

} /* namespace ARDOUR */

#endif /* __ardour_region_h__ */
