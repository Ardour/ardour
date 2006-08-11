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

    $Id: audio_source.h 486 2006-04-27 09:04:24Z pauld $
*/

#ifndef __ardour_audio_source_h__
#define __ardour_audio_source_h__

#include <list>
#include <vector>
#include <string>

#include <time.h>

#include <glibmm/thread.h>

#include <sigc++/signal.h>

#include <ardour/source.h>
#include <ardour/ardour.h>
#include <pbd/stateful.h> 
#include <pbd/xml++.h>

using std::list;
using std::vector;
using std::string;

namespace ARDOUR {

const jack_nframes_t frames_per_peak = 256;

class AudioSource : public Source
{
  public:
	AudioSource (string name);
	AudioSource (const XMLNode&);
	virtual ~AudioSource ();
	
	virtual jack_nframes_t available_peaks (double zoom) const;

	virtual jack_nframes_t read (Sample *dst, jack_nframes_t start, jack_nframes_t cnt) const;
	virtual jack_nframes_t write (Sample *src, jack_nframes_t cnt);

	virtual float sample_rate () const = 0;

	virtual void mark_for_remove() = 0;
	virtual void mark_streaming_write_completed () {}

	void set_captured_for (string str) { _captured_for = str; }
	string captured_for() const { return _captured_for; }

	uint32_t read_data_count() const { return _read_data_count; }
	uint32_t write_data_count() const { return _write_data_count; }

 	int  read_peaks (PeakData *peaks, jack_nframes_t npeaks, jack_nframes_t start, jack_nframes_t cnt, double samples_per_unit) const;
 	int  build_peaks ();
	bool peaks_ready (sigc::slot<void>, sigc::connection&) const;

	static sigc::signal<void,AudioSource*> AudioSourceCreated;
	       
	mutable sigc::signal<void>  PeaksReady;
	mutable sigc::signal<void,jack_nframes_t,jack_nframes_t>  PeakRangeReady;
	
	XMLNode& get_state ();
	int set_state (const XMLNode&);

	static int  start_peak_thread ();
	static void stop_peak_thread ();

	int rename_peakfile (std::string newpath);

	static void set_build_missing_peakfiles (bool yn) {
		_build_missing_peakfiles = yn;
	}

	static void set_build_peakfiles (bool yn) {
		_build_peakfiles = yn;
	}

  protected:
	static bool _build_missing_peakfiles;
	static bool _build_peakfiles;

	bool                _peaks_built;
	mutable Glib::Mutex _lock;
	bool                 next_peak_clear_should_notify;
	string               peakpath;
	string              _captured_for;

	mutable uint32_t _read_data_count;  // modified in read()
	mutable uint32_t _write_data_count; // modified in write()

	int initialize_peakfile (bool newfile, string path);
	void build_peaks_from_scratch ();

	int  do_build_peak (jack_nframes_t, jack_nframes_t);

	virtual jack_nframes_t read_unlocked (Sample *dst, jack_nframes_t start, jack_nframes_t cnt) const = 0;
	virtual jack_nframes_t write_unlocked (Sample *dst, jack_nframes_t cnt) = 0;
	virtual string peak_path(string audio_path) = 0;
	virtual string old_peak_path(string audio_path) = 0;
	
	static pthread_t peak_thread;
	static bool      have_peak_thread;
	static void*     peak_thread_work(void*);

	static int peak_request_pipe[2];

	struct PeakRequest {
	    enum Type {
		    Build,
		    Quit
	    };
	};

	static vector<AudioSource*> pending_peak_sources;
	static Glib::Mutex* pending_peak_sources_lock;

	static void queue_for_peaks (AudioSource&);
	static void clear_queue_for_peaks ();
	
	struct PeakBuildRecord {
	    jack_nframes_t frame;
	    jack_nframes_t cnt;

	    PeakBuildRecord (jack_nframes_t f, jack_nframes_t c) 
		    : frame (f), cnt (c) {}
	    PeakBuildRecord (const PeakBuildRecord& other) {
		    frame = other.frame;
		    cnt = other.cnt;
	    }
	};

	list<AudioSource::PeakBuildRecord *> pending_peak_builds;

  private:
	bool file_changed (string path);
};

}

#endif /* __ardour_audio_source_h__ */
