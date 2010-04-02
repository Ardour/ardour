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

#include <glibmm/thread.h>
#include <glibmm/ustring.h>
#include <boost/function.hpp>

#include "ardour/source.h"
#include "ardour/ardour.h"
#include "pbd/stateful.h"
#include "pbd/xml++.h"

namespace ARDOUR {

class AudioSource : virtual public Source,
		public ARDOUR::Readable,
		public boost::enable_shared_from_this<ARDOUR::AudioSource>
{
  public:
	AudioSource (Session&, Glib::ustring name);
	AudioSource (Session&, const XMLNode&);
	virtual ~AudioSource ();

	framecnt_t readable_length() const { return _length; }
	uint32_t   n_channels()      const { return 1; }

	framecnt_t length (framepos_t pos) const;
	void      update_length (framepos_t pos, framecnt_t cnt);

	virtual framecnt_t available_peaks (double zoom) const;

	virtual framecnt_t read (Sample *dst, framepos_t start, framecnt_t cnt, int channel=0) const;
	virtual framecnt_t write (Sample *src, framecnt_t cnt);

	virtual float sample_rate () const = 0;

	virtual void mark_streaming_write_completed () {}

	virtual bool can_truncate_peaks() const { return true; }

	void set_captured_for (Glib::ustring str) { _captured_for = str; }
	Glib::ustring captured_for() const { return _captured_for; }

	uint32_t read_data_count() const { return _read_data_count; }
	uint32_t write_data_count() const { return _write_data_count; }

	int read_peaks (PeakData *peaks, framecnt_t npeaks,
			framepos_t start, framecnt_t cnt, double samples_per_visual_peak) const;

	int  build_peaks ();
	bool peaks_ready (boost::function<void()> callWhenReady, PBD::ScopedConnection& connection_created_if_not_ready, PBD::EventLoop* event_loop) const;

	mutable PBD::Signal0<void>  PeaksReady;
	mutable PBD::Signal2<void,framepos_t,framepos_t>  PeakRangeReady;

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

	int rename_peakfile (Glib::ustring newpath);
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

  protected:
	static bool _build_missing_peakfiles;
	static bool _build_peakfiles;

	framecnt_t           _length;
	bool                 _peaks_built;
	mutable Glib::Mutex  _peaks_ready_lock;
	Glib::ustring         peakpath;
	Glib::ustring        _captured_for;

	mutable uint32_t _read_data_count;  // modified in read()
	mutable uint32_t _write_data_count; // modified in write()

	int initialize_peakfile (bool newfile, Glib::ustring path);
	int build_peaks_from_scratch ();
	int compute_and_write_peaks (Sample* buf, framepos_t first_frame, framecnt_t cnt,
	bool force, bool intermediate_peaks_ready_signal);
	void truncate_peakfile();

	mutable off_t _peak_byte_max; // modified in compute_and_write_peak()

	virtual framecnt_t read_unlocked (Sample *dst, framepos_t start, framecnt_t cnt) const = 0;
	virtual framecnt_t write_unlocked (Sample *dst, framecnt_t cnt) = 0;
	virtual Glib::ustring peak_path(Glib::ustring audio_path) = 0;
	virtual Glib::ustring find_broken_peakfile (Glib::ustring missing_peak_path,
	                                            Glib::ustring audio_path) = 0;

	virtual int read_peaks_with_fpp (PeakData *peaks,
					 framecnt_t npeaks, framepos_t start, framecnt_t cnt,
					 double samples_per_visual_peak, framecnt_t fpp) const;
	
	int compute_and_write_peaks (Sample* buf, framepos_t first_frame, framecnt_t cnt,
				     bool force, bool intermediate_peaks_ready_signal, 
				     framecnt_t frames_per_peak);

  private:
	int        peakfile;
	framecnt_t peak_leftover_cnt;
	framecnt_t peak_leftover_size;
	Sample*    peak_leftovers;
	framepos_t peak_leftover_frame;
};

}

#endif /* __ardour_audio_source_h__ */
