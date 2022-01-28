/*
 * Copyright (C) 2006-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2006-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016-2017 Nick Mainsbridge <mainsbridge@gmail.com>
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

#ifndef __ardour_audio_region_h__
#define __ardour_audio_region_h__

#include <vector>
#include <list>

#include "pbd/fastlog.h"
#include "pbd/undo.h"

#include "ardour/ardour.h"
#include "ardour/automatable.h"
#include "ardour/automation_list.h"
#include "ardour/interthread_info.h"
#include "ardour/logcurve.h"
#include "ardour/region.h"

class XMLNode;
class AudioRegionReadTest;
class PlaylistReadTest;

namespace ARDOUR {

namespace Properties {
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool> envelope_active;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool> default_fade_in;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool> default_fade_out;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool> fade_in_active;
	LIBARDOUR_API extern PBD::PropertyDescriptor<bool> fade_out_active;
	LIBARDOUR_API extern PBD::PropertyDescriptor<float> scale_amplitude;
	LIBARDOUR_API extern PBD::PropertyDescriptor<boost::shared_ptr<AutomationList> > fade_in;
	LIBARDOUR_API extern PBD::PropertyDescriptor<boost::shared_ptr<AutomationList> > inverse_fade_in;
	LIBARDOUR_API extern PBD::PropertyDescriptor<boost::shared_ptr<AutomationList> > fade_out;
	LIBARDOUR_API extern PBD::PropertyDescriptor<boost::shared_ptr<AutomationList> > inverse_fade_out;
	LIBARDOUR_API extern PBD::PropertyDescriptor<boost::shared_ptr<AutomationList> > envelope;
}

class Playlist;
class Session;
class Filter;
class AudioSource;


class LIBARDOUR_API AudioRegion : public Region, public AudioReadable
{
  public:
	static void make_property_quarks ();

	~AudioRegion();

	void copy_settings (boost::shared_ptr<const AudioRegion>);

	bool source_equivalent (boost::shared_ptr<const Region>) const;

	bool speed_mismatch (float) const;

	boost::shared_ptr<AudioSource> audio_source (uint32_t n=0) const;

	void   set_scale_amplitude (gain_t);
	gain_t scale_amplitude() const { return _scale_amplitude; }

	void normalize (float, float target_in_dB = 0.0f);

	/** @return the maximum (linear) amplitude of the region, or a -ve
	 *  number if the Progress object reports that the process was cancelled.
	 */
	double maximum_amplitude (Progress* p = 0) const;

	/** @return the maximum (rms) signal power of the region, or a -1
	 *  if the Progress object reports that the process was cancelled.
	 */
	double rms (Progress* p = 0) const;

	bool loudness (float& tp, float& i, float& s, float& m, Progress* p = 0) const;

	bool envelope_active () const { return _envelope_active; }
	bool fade_in_active ()  const { return _fade_in_active; }
	bool fade_out_active () const { return _fade_out_active; }

	boost::shared_ptr<AutomationList> fade_in()  { return _fade_in.val (); }
	boost::shared_ptr<AutomationList> inverse_fade_in()  { return _inverse_fade_in.val (); }
	boost::shared_ptr<AutomationList> fade_out() { return _fade_out.val (); }
	boost::shared_ptr<AutomationList> inverse_fade_out()  { return _inverse_fade_out.val (); }
	boost::shared_ptr<AutomationList> envelope() { return _envelope.val (); }

	Temporal::Range body_range () const;

	virtual samplecnt_t read_peaks (PeakData *buf, samplecnt_t npeaks,
	                                samplecnt_t offset, samplecnt_t cnt,
	                                uint32_t chan_n=0, double samples_per_pixel = 1.0) const;

	/* AudioReadable interface */

	samplecnt_t read (Sample*, samplepos_t pos, samplecnt_t cnt, int channel) const;
	samplecnt_t readable_length_samples() const { return length_samples(); }
	uint32_t    n_channels() const { return _sources.size(); }

	samplecnt_t read_at (Sample *buf, Sample *mixdown_buf, float *gain_buf,
	                             samplepos_t position,
	                             samplecnt_t cnt,
	                             uint32_t   chan_n = 0) const;

	samplecnt_t master_read_at (Sample *buf, Sample *mixdown_buf, float *gain_buf,
	                                    samplepos_t position, samplecnt_t cnt,
	                                    uint32_t chan_n=0) const;

	samplecnt_t read_raw_internal (Sample*, samplepos_t, samplecnt_t, int channel) const;

	XMLNode& state ();
	XMLNode& get_basic_state ();
	int set_state (const XMLNode&, int version);

	void fade_range (samplepos_t, samplepos_t);

	bool fade_in_is_default () const;
	bool fade_out_is_default () const;

	void set_fade_in_active (bool yn);
	void set_fade_in_shape (FadeShape);
	void set_fade_in_length (samplecnt_t);
	void set_fade_in (FadeShape, samplecnt_t);
	void set_fade_in (boost::shared_ptr<AutomationList>);

	void set_fade_out_active (bool yn);
	void set_fade_out_shape (FadeShape);
	void set_fade_out_length (samplecnt_t);
	void set_fade_out (FadeShape, samplecnt_t);
	void set_fade_out (boost::shared_ptr<AutomationList>);

	void set_default_fade_in ();
	void set_default_fade_out ();

	samplecnt_t verify_xfade_bounds (samplecnt_t, bool start);

	void set_envelope_active (bool yn);
	void set_default_envelope ();

	int separate_by_channel (std::vector<boost::shared_ptr<Region> >&) const;

	/* automation */

	boost::shared_ptr<Evoral::Control>
	control(const Evoral::Parameter& id, bool create=false) {
		return _automatable.control(id, create);
	}

	virtual boost::shared_ptr<const Evoral::Control>
	control(const Evoral::Parameter& id) const {
		return _automatable.control(id);
	}

	/* export */

	bool do_export (std::string const&) const;

	/* xfade/fade interactions */

	void suspend_fade_in ();
	void suspend_fade_out ();
	void resume_fade_in ();
	void resume_fade_out ();

	void add_transient (samplepos_t where);
	void remove_transient (samplepos_t where);
	void clear_transients ();
	void set_onsets (AnalysisFeatureList&);
	void get_transients (AnalysisFeatureList&);
	void update_transient (samplepos_t old_position, samplepos_t new_position);

	AudioIntervalResult find_silence (Sample, samplecnt_t, samplecnt_t, InterThreadInfo&) const;

  private:
	friend class RegionFactory;

	AudioRegion (boost::shared_ptr<AudioSource>);
	AudioRegion (const SourceList &);
	AudioRegion (boost::shared_ptr<const AudioRegion>);
	AudioRegion (boost::shared_ptr<const AudioRegion>, timecnt_t const & offset);
	AudioRegion (boost::shared_ptr<const AudioRegion>, const SourceList&);
	AudioRegion (SourceList &);

  private:
	friend class ::AudioRegionReadTest;
	friend class ::PlaylistReadTest;

	void build_transients ();

	PBD::Property<bool>     _envelope_active;
	PBD::Property<bool>     _default_fade_in;
	PBD::Property<bool>     _default_fade_out;
	PBD::Property<bool>     _fade_in_active;
	PBD::Property<bool>     _fade_out_active;
	/** linear gain to apply to the whole region */
	PBD::Property<gain_t>   _scale_amplitude;

	void register_properties ();
	void post_set (const PBD::PropertyChange&);

	void init ();
	void set_default_fades ();

	void recompute_gain_at_end ();
	void recompute_gain_at_start ();

	samplecnt_t read_from_sources (SourceList const &, samplecnt_t, Sample *, samplepos_t, samplecnt_t, uint32_t) const;

	void recompute_at_start ();
	void recompute_at_end ();

	void envelope_changed ();
	void fade_in_changed ();
	void fade_out_changed ();
	void source_offset_changed ();
	void listen_to_my_curves ();
	void connect_to_analysis_changed ();
	void connect_to_header_position_offset_changed ();


	AutomationListProperty _fade_in;
	AutomationListProperty _inverse_fade_in;
	AutomationListProperty _fade_out;
	AutomationListProperty _inverse_fade_out;
	AutomationListProperty _envelope;
	Automatable            _automatable;
	uint32_t               _fade_in_suspended;
	uint32_t               _fade_out_suspended;

	boost::shared_ptr<ARDOUR::Region> get_single_other_xfade_region (bool start) const;

  protected:
	/* default constructor for derived (compound) types */

	AudioRegion (Session& s, timepos_t const &, timecnt_t const &, std::string name);

	int _set_state (const XMLNode&, int version, PBD::PropertyChange& what_changed, bool send_signal);
};

} /* namespace ARDOUR */

/* access from C objects */

extern "C" {
	LIBARDOUR_API int    region_read_peaks_from_c   (void *arg, uint32_t npeaks, uint32_t start, uint32_t length, intptr_t data, uint32_t n_chan, double samples_per_unit);
	LIBARDOUR_API uint32_t region_length_from_c (void *arg);
	LIBARDOUR_API uint32_t sourcefile_length_from_c (void *arg, double);
}

#endif /* __ardour_audio_region_h__ */
