#ifndef __libardour_slice_h__
#define __libardour_slice_h__

#include "pbd/properties.h"
#include "pbd/stateful.h"

#include "temporal/timeline.h"
#include "temporal/range.h"

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"
#include "ardour/types_convert.h"

namespace ARDOUR {

namespace Properties {
	LIBARDOUR_API extern PBD::PropertyDescriptor<timepos_t>         start;
	LIBARDOUR_API extern PBD::PropertyDescriptor<timecnt_t>         length;
}

class LIBARDOUR_API Slice : virtual public PBD::Stateful
{
  public:
  	Slice (Temporal::timepos_t const &, Temporal::timecnt_t const &);
	Slice (Slice const &);
	virtual ~Slice() {}

	Slice& operator= (Slice const &);

	timepos_t position ()  const { return _length.val().position(); }
	timepos_t start ()     const { return _start.val(); }
	timecnt_t length ()    const { return _length.val(); }
	timepos_t end()        const;
	timepos_t nt_last()    const { return end().decrement(); }

	/* these two are valid ONLY during a StateChanged signal handler */

	timepos_t last_position () const { return _last_length.position(); }
	timecnt_t last_length ()   const { return _last_length; }

	timepos_t source_position () const;
	timecnt_t source_relative_position (Temporal::timepos_t const &) const;
	timecnt_t region_relative_position (Temporal::timepos_t const &) const;

	samplepos_t position_sample ()  const { return position().samples(); }
	samplecnt_t start_sample ()     const { return _start.val().samples(); }
	samplecnt_t length_samples ()   const { return _length.val().samples(); }


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

	Temporal::TimeDomain position_time_domain () const;

  protected:
	PBD::Property<timepos_t> _start;
	PBD::Property<timecnt_t> _length;
	timecnt_t                _last_length;

	virtual void set_length_internal (timecnt_t const &);
	virtual void set_start_internal (timepos_t const &);
	virtual void set_position_internal (timepos_t const &);

  private:
  	void register_properties ();
};

} /* namespace */

#endif /* __libardour_slice_h__ */
