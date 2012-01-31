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

#ifndef __ardour_overlap_h__
#define __ardour_overlap_h__

#include <vector>
#include <algorithm>
#include <boost/shared_ptr.hpp>


#include "pbd/undo.h"
#include "pbd/statefuldestructible.h"

#include "ardour/ardour.h"
#include "ardour/audioregion.h"
#include "evoral/Curve.hpp"

namespace ARDOUR {
	namespace Properties {
		/* "active" is defined elsewhere but we use it with crossfade also */
		extern PBD::PropertyDescriptor<bool> active;
		extern PBD::PropertyDescriptor<bool> follow_overlap;
	}

enum AnchorPoint {
	StartOfIn,
	EndOfIn,
	EndOfOut
};

class Playlist;

class Crossfade : public ARDOUR::AudioRegion
{
  public:

	class NoCrossfadeHere: std::exception {
	public:
		virtual const char *what() const throw() { return "no crossfade should be constructed here"; }
	};

	/* constructor for "fixed" xfades at each end of an internal overlap */

	Crossfade (boost::shared_ptr<ARDOUR::AudioRegion> in, boost::shared_ptr<ARDOUR::AudioRegion> out,
		   framecnt_t initial_length,
		   AnchorPoint);

	/* constructor for xfade between two regions that are overlapped in any way
	   except the "internal" case.
	*/

	Crossfade (boost::shared_ptr<ARDOUR::AudioRegion> in, boost::shared_ptr<ARDOUR::AudioRegion> out, CrossfadeModel, bool active);


	/* copy constructor to copy a crossfade with new regions. used (for example)
	   when a playlist copy is made
	*/
	Crossfade (boost::shared_ptr<Crossfade>, boost::shared_ptr<ARDOUR::AudioRegion>, boost::shared_ptr<ARDOUR::AudioRegion>);

	/* the usual XML constructor */

	Crossfade (const Playlist&, XMLNode const &);
	virtual ~Crossfade();

	static void make_property_quarks ();

	XMLNode& get_state (void);
	int set_state (const XMLNode&, int version);

	boost::shared_ptr<ARDOUR::AudioRegion> in() const { return _in; }
	boost::shared_ptr<ARDOUR::AudioRegion> out() const { return _out; }

	framecnt_t read_at (Sample *buf, Sample *mixdown_buffer,
			    float *gain_buffer, framepos_t position, framecnt_t cnt,
			    uint32_t chan_n) const;

	bool refresh ();

	uint32_t upper_layer () const {
		return std::max (_in->layer(), _out->layer());
	}

	uint32_t lower_layer () const {
		return std::min (_in->layer(), _out->layer());
	}

	bool involves (boost::shared_ptr<ARDOUR::AudioRegion> region) const {
		return _in == region || _out == region;
	}

	bool involves (boost::shared_ptr<ARDOUR::AudioRegion> a, boost::shared_ptr<ARDOUR::AudioRegion> b) const {
		return (_in == a && _out == b) || (_in == b && _out == a);
	}

	framecnt_t overlap_length() const;

	PBD::Signal1<void,boost::shared_ptr<Region> > Invalidated;

	OverlapType coverage (framepos_t start, framepos_t end) const;

	static void set_buffer_size (framecnt_t);

	bool active () const { return _active; }
	void set_active (bool yn);

	bool following_overlap() const { return _follow_overlap; }
	bool can_follow_overlap() const;
	void set_follow_overlap (bool yn);

	AutomationList& fade_in() { return _fade_in; }
	AutomationList& fade_out() { return _fade_out; }

	framecnt_t set_xfade_length (framecnt_t);

	bool is_dependent() const { return true; }
	bool depends_on (boost::shared_ptr<Region> other) const {
		return other == _in || other == _out;
	}

	static framecnt_t short_xfade_length() { return _short_xfade_length; }
	static void set_short_xfade_length (framecnt_t n);

	/** emitted when the actual fade curves change, as opposed to one of the Stateful properties */
	PBD::Signal0<void> FadesChanged;

  private:
	friend struct CrossfadeComparePtr;
	friend class AudioPlaylist;

	static framecnt_t _short_xfade_length;

	boost::shared_ptr<ARDOUR::AudioRegion> _in;
	boost::shared_ptr<ARDOUR::AudioRegion> _out;
	PBD::Property<bool>  _active;
	PBD::Property<bool>  _follow_overlap;
	bool                 _in_update;
	OverlapType           overlap_type;
	AnchorPoint          _anchor_point;
	bool                 _fixed;
	int32_t               layer_relation;


	mutable AutomationList _fade_in;
	mutable AutomationList _fade_out;

	static Sample* crossfade_buffer_out;
	static Sample* crossfade_buffer_in;

	void initialize ();
	void register_properties ();
	int  compute (boost::shared_ptr<ARDOUR::AudioRegion>, boost::shared_ptr<ARDOUR::AudioRegion>, CrossfadeModel);
	bool update ();

	bool operator== (const ARDOUR::Crossfade&);

  protected:
	framecnt_t read_raw_internal (Sample*, framepos_t, framecnt_t, int) const;
};


} // namespace ARDOUR

#endif /* __ardour_overlap_h__ */
