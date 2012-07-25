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

#ifndef __ardour_audio_source_h__
#define __ardour_audio_source_h__

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <time.h>

#include <glibmm/threads.h>
#include <boost/function.hpp>

#include "ardour/source.h"
#include "ardour/ardour.h"
#include "ardour/readable.h"
#include "pbd/file_manager.h"
#include "pbd/stateful.h"
#include "pbd/xml++.h"

namespace ARDOUR {

class AudioSource : virtual public Source,
		public ARDOUR::Readable,
		public boost::enable_shared_from_this<ARDOUR::AudioSource>
{
  public:
	AudioSource (Session&, std::string name);
	AudioSource (Session&, const XMLNode&);
	virtual ~AudioSource ();

	framecnt_t readable_length() const { return _length; }
	virtual uint32_t n_channels()      const { return 1; }

	virtual bool       empty() const;
	framecnt_t length (framepos_t pos) const;
	void       update_length (framecnt_t cnt);

	virtual framecnt_t available_peaks (double zoom) const;

	virtual framecnt_t read (Sample *dst, framepos_t start, framecnt_t cnt, int channel=0) const;
	virtual framecnt_t write (Sample *src, framecnt_t cnt);

	virtual float sample_rate () const = 0;

	virtual void mark_streaming_write_completed ();

	virtual bool can_truncate_peaks() const { return true; }

	void set_captured_for (std::string str) { _captured_for = str; }
	std::string captured_for() const { return _captured_for; }

	int read_peaks (PeakData *peaks, framecnt_t npeaks,
			framepos_t start, framecnt_t cnt, double samples_per_visual_peak) const;

	int  build_peaks ();
	bool peaks_ready (boost::function<void()> callWhenReady, PBD::ScopedConnection** connection_created_if_not_ready, PBD::EventLoop* event_loop) const;

	mutable PBD::Signal0<void>  PeaksReady;
	mutable PBD::Signal2<void,framepos_t,framepos_t>  PeakRangeReady;

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

	int prepare_for_peakfile_writes ();
	void done_with_peakfile_writes (bool done = true);

	/** @return true if the each source sample s must be clamped to -1 < s < 1 */
	virtual bool clamped_at_unity () const = 0;

	static void allocate_working_buffers (framecnt_t framerate);

  protected:
	static bool _build_missing_peakfiles;
	static bool _build_peakfiles;

	static size_t _working_buffers_size;

	/* these collections of working buffers for supporting
	   playlist's reading from potentially nested/recursive
	   sources assume SINGLE THREADED reads by the butler
	   thread, or a lock around calls that use them.
	*/

	static std::vector<boost::shared_ptr<Sample> > _mixdown_buffers;
	static std::vector<boost::shared_ptr<gain_t> > _gain_buffers;
        static Glib::Threads::Mutex    _level_buffer_lock;

	static void ensure_buffers_for_level (uint32_t, framecnt_t);
	static void ensure_buffers_for_level_locked (uint32_t, framecnt_t);

	framecnt_t           _length;
	std::string         peakpath;
	std::string        _captured_for;

	int initialize_peakfile (bool newfile, std::string path);
	int build_peaks_from_scratch ();
	int compute_and_write_peaks (Sample* buf, framecnt_t first_frame, framecnt_t cnt,
	bool force, bool intermediate_peaks_ready_signal);
	void truncate_peakfile();

	mutable off_t _peak_byte_max; // modified in compute_and_write_peak()

	virtual framecnt_t read_unlocked (Sample *dst, framepos_t start, framecnt_t cnt) const = 0;
	virtual framecnt_t write_unlocked (Sample *dst, framecnt_t cnt) = 0;
	virtual std::string peak_path(std::string audio_path) = 0;
	virtual std::string find_broken_peakfile (std::string /* missing_peak_path */,
	                                          std::string audio_path) { return peak_path (audio_path); }

	virtual int read_peaks_with_fpp (PeakData *peaks,
					 framecnt_t npeaks, framepos_t start, framecnt_t cnt,
					 double samples_per_visual_peak, framecnt_t fpp) const;

	int compute_and_write_peaks (Sample* buf, framecnt_t first_frame, framecnt_t cnt,
				     bool force, bool intermediate_peaks_ready_signal,
				     framecnt_t frames_per_peak);

  private:
	bool _peaks_built;
	/** This mutex is used to protect both the _peaks_built
	 *  variable and also the emission (and handling) of the
	 *  PeaksReady signal.  Holding the lock when emitting
	 *  PeaksReady means that _peaks_built cannot be changed
	 *  during the handling of the signal.
	 */
        mutable Glib::Threads::Mutex _peaks_ready_lock;

	PBD::FdFileDescriptor* _peakfile_descriptor;
	int        _peakfile_fd;
	framecnt_t peak_leftover_cnt;
	framecnt_t peak_leftover_size;
	Sample*    peak_leftovers;
	framepos_t peak_leftover_frame;
};

}

#endif /* __ardour_audio_source_h__ */
