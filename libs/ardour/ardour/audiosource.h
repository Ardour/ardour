/*
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_audio_source_h__
#define __ardour_audio_source_h__

#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <time.h>

#include <glibmm/threads.h>
#include <boost/function.hpp>
#include <boost/scoped_array.hpp>

#include "ardour/source.h"
#include "ardour/ardour.h"
#include "ardour/readable.h"
#include "pbd/stateful.h"
#include "pbd/xml++.h"

namespace ARDOUR {

class LIBARDOUR_API AudioSource : virtual public Source, public ARDOUR::AudioReadable
{
  public:
	AudioSource (Session&, const std::string& name);
	AudioSource (Session&, const XMLNode&);
	virtual ~AudioSource ();

	samplecnt_t readable_length_samples() const { return _length.samples(); }
	virtual uint32_t n_channels()      const { return 1; }

	void       update_length (timecnt_t const & cnt);

	virtual samplecnt_t available_peaks (double zoom) const;

	virtual samplecnt_t read (Sample *dst, samplepos_t start, samplecnt_t cnt, int channel=0) const;
	virtual samplecnt_t write (Sample *src, samplecnt_t cnt);

	virtual float sample_rate () const = 0;

	virtual void mark_streaming_write_completed (const Lock& lock);

	virtual bool can_truncate_peaks() const { return true; }

	int read_peaks (PeakData *peaks, samplecnt_t npeaks,
			samplepos_t start, samplecnt_t cnt, double samples_per_visual_peak) const;

	int  build_peaks ();
	bool peaks_ready (boost::function<void()> callWhenReady, PBD::ScopedConnection** connection_created_if_not_ready, PBD::EventLoop* event_loop) const;

	mutable PBD::Signal0<void>  PeaksReady;
	mutable PBD::Signal2<void,samplepos_t,samplepos_t>  PeakRangeReady;

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

	int rename_peakfile (std::string newpath);
	void touch_peakfile ();

	static void set_build_missing_peakfiles (bool yn) {
		_build_missing_peakfiles = yn;
	}

	static void set_build_peakfiles (bool yn) {
		_build_peakfiles = yn;
	}

	static bool get_build_peakfiles () {
		return _build_peakfiles;
	}

	virtual int setup_peakfile () { return 0; }
	int close_peakfile ();

	int prepare_for_peakfile_writes ();
	void done_with_peakfile_writes (bool done = true);

	/** @return true if the each source sample s must be clamped to -1 < s < 1 */
	virtual bool clamped_at_unity () const = 0;

  protected:
	static bool _build_missing_peakfiles;
	static bool _build_peakfiles;

	/* these collections of working buffers for supporting
	   playlist's reading from potentially nested/recursive
	   sources assume SINGLE THREADED reads by the butler
	   thread, or a lock around calls that use them.
	*/

	static std::vector<boost::shared_array<Sample> > _mixdown_buffers;
	static std::vector<boost::shared_array<gain_t> > _gain_buffers;
	static Glib::Threads::Mutex    _level_buffer_lock;

	static void ensure_buffers_for_level (uint32_t, samplecnt_t);
	static void ensure_buffers_for_level_locked (uint32_t, samplecnt_t);

	std::string         _peakpath;

	int initialize_peakfile (const std::string& path, const bool in_session = false);
	int build_peaks_from_scratch ();
	int compute_and_write_peaks (Sample* buf, samplecnt_t first_sample, samplecnt_t cnt,
	bool force, bool intermediate_peaks_ready_signal);
	void truncate_peakfile();

	mutable off_t _peak_byte_max; // modified in compute_and_write_peak()

	virtual samplecnt_t read_unlocked (Sample *dst, samplepos_t start, samplecnt_t cnt) const = 0;
	virtual samplecnt_t write_unlocked (Sample *dst, samplecnt_t cnt) = 0;
	virtual std::string construct_peak_filepath (const std::string& audio_path, const bool in_session = false, const bool old_peak_name = false) const = 0;

	virtual int read_peaks_with_fpp (PeakData *peaks,
					 samplecnt_t npeaks, samplepos_t start, samplecnt_t cnt,
					 double samples_per_visual_peak, samplecnt_t fpp) const;

	int compute_and_write_peaks (Sample* buf, samplecnt_t first_sample, samplecnt_t cnt,
				     bool force, bool intermediate_peaks_ready_signal,
				     samplecnt_t samples_per_peak);

  private:
	bool _peaks_built;
	/** This mutex is used to protect both the _peaks_built
	 *  variable and also the emission (and handling) of the
	 *  PeaksReady signal.  Holding the lock when emitting
	 *  PeaksReady means that _peaks_built cannot be changed
	 *  during the handling of the signal.
	 */
        mutable Glib::Threads::Mutex _peaks_ready_lock;
        Glib::Threads::Mutex _initialize_peaks_lock;

	int        _peakfile_fd;
	samplecnt_t peak_leftover_cnt;
	samplecnt_t peak_leftover_size;
	Sample*    peak_leftovers;
	samplepos_t peak_leftover_sample;

	mutable bool _first_run;
	mutable double _last_scale;
	mutable off_t _last_map_off;
	mutable size_t  _last_raw_map_length;
	mutable boost::scoped_array<PeakData> peak_cache;
};

}

#endif /* __ardour_audio_source_h__ */
