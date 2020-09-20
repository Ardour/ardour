/*
 * Copyright (C) 2000-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2008-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2015 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 Nick Mainsbridge <mainsbridge@gmail.com>
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

#ifdef COMPILER_MSVC
#include <sys/utime.h>
#else
#include <unistd.h>
#include <utime.h>
#endif
#include <sys/stat.h>
#include <fcntl.h>
#include <float.h>
#include <cerrno>
#include <ctime>
#include <cmath>
#include <iomanip>
#include <algorithm>
#include <vector>

#ifdef PLATFORM_WINDOWS
#include <windows.h>

#else
#include <sys/mman.h>

#endif

#include <glib.h>
#include "pbd/gstdio_compat.h"

#include <boost/scoped_ptr.hpp>

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "pbd/file_utils.h"
#include "pbd/playback_buffer.h"
#include "pbd/scoped_file_descriptor.h"
#include "pbd/xml++.h"

#include "ardour/audiosource.h"
#include "ardour/rc_configuration.h"
#include "ardour/runtime_functions.h"
#include "ardour/session.h"

#include "pbd/i18n.h"

#include "ardour/debug.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

bool AudioSource::_build_missing_peakfiles = false;

/** true if we want peakfiles (e.g. if we are displaying a GUI) */
bool AudioSource::_build_peakfiles = false;

#define _FPP 256

AudioSource::AudioSource (Session& s, const string& name)
	: Source (s, DataType::AUDIO, name)
	, _peak_byte_max (0)
	, _peaks_built (false)
	, _peakfile_fd (-1)
	, peak_leftover_cnt (0)
	, peak_leftover_size (0)
	, peak_leftovers (0)
	, peak_leftover_sample (0)
	, _first_run (true)
	, _last_scale (0.0)
	, _last_map_off (0)
	, _last_raw_map_length (0)
{
}

AudioSource::AudioSource (Session& s, const XMLNode& node)
	: Source (s, node)
	, _peak_byte_max (0)
	, _peaks_built (false)
	, _peakfile_fd (-1)
	, peak_leftover_cnt (0)
	, peak_leftover_size (0)
	, peak_leftovers (0)
	, peak_leftover_sample (0)
	, _first_run (true)
	, _last_scale (0.0)
	, _last_map_off (0)
	, _last_raw_map_length (0)
{
	if (set_state (node, Stateful::loading_state_version)) {
		throw failed_constructor();
	}
}

AudioSource::~AudioSource ()
{
	/* shouldn't happen but make sure we don't leak file descriptors anyway */

	if (peak_leftover_cnt) {
		cerr << "AudioSource destroyed with leftover peak data pending" << endl;
	}

	if ((-1) != _peakfile_fd) {
		close (_peakfile_fd);
		_peakfile_fd = -1;
	}

	delete [] peak_leftovers;
}

XMLNode&
AudioSource::get_state ()
{
	XMLNode& node (Source::get_state());

	if (_captured_for.length()) {
		node.set_property ("captured-for", _captured_for);
	}

	return node;
}

int
AudioSource::set_state (const XMLNode& node, int /*version*/)
{
	node.get_property ("captured-for", _captured_for);
	return 0;
}

void
AudioSource::update_length (timecnt_t const & len)
{
	if (len > _length) {
		_length = len;
	}
}


/***********************************************************************
  PEAK FILE STUFF
 ***********************************************************************/

/** Checks to see if peaks are ready.  If so, we return true.  If not, we return false, and
 *  things are set up so that doThisWhenReady is called when the peaks are ready.
 *  A new PBD::ScopedConnection is created for the associated connection and written to
 *  *connect_here_if_not.
 *
 *  @param doThisWhenReady Function to call when peaks are ready (if they are not already).
 *  @param connect_here_if_not Address to write new ScopedConnection to.
 *  @param event_loop Event loop for doThisWhenReady to be called in.
 */
bool
AudioSource::peaks_ready (boost::function<void()> doThisWhenReady, ScopedConnection** connect_here_if_not, EventLoop* event_loop) const
{
	bool ret;
	Glib::Threads::Mutex::Lock lm (_peaks_ready_lock);

	if (!(ret = _peaks_built)) {
		*connect_here_if_not = new ScopedConnection;
		PeaksReady.connect (**connect_here_if_not, MISSING_INVALIDATOR, doThisWhenReady, event_loop);
	}

	return ret;
}

void
AudioSource::touch_peakfile ()
{
	GStatBuf statbuf;

	if (g_stat (_peakpath.c_str(), &statbuf) != 0 || statbuf.st_size == 0) {
		return;
	}

	struct utimbuf tbuf;

	tbuf.actime = statbuf.st_atime;
	tbuf.modtime = time ((time_t*) 0);

	g_utime (_peakpath.c_str(), &tbuf);
}

int
AudioSource::rename_peakfile (string newpath)
{
	/* caller must hold _lock */

	string oldpath = _peakpath;

	if (Glib::file_test (oldpath, Glib::FILE_TEST_EXISTS)) {
		if (g_rename (oldpath.c_str(), newpath.c_str()) != 0) {
			error << string_compose (_("cannot rename peakfile for %1 from %2 to %3 (%4)"), _name, oldpath, newpath, strerror (errno)) << endmsg;
			return -1;
		}
	}

	_peakpath = newpath;

	return 0;
}

int
AudioSource::initialize_peakfile (const string& audio_path, const bool in_session)
{
	Glib::Threads::Mutex::Lock lm (_initialize_peaks_lock);
	GStatBuf statbuf;

	_peakpath = construct_peak_filepath (audio_path, in_session);

	if (!empty() && !Glib::file_test (_peakpath.c_str(), Glib::FILE_TEST_EXISTS)) {
		string oldpeak = construct_peak_filepath (audio_path, in_session, true);
		DEBUG_TRACE(DEBUG::Peaks, string_compose ("Looking for old peak file %1 for Audio file %2\n", oldpeak, audio_path));
		if (Glib::file_test (oldpeak.c_str(), Glib::FILE_TEST_EXISTS)) {
			// TODO use hard-link if possible
			DEBUG_TRACE(DEBUG::Peaks, string_compose ("Copy old peakfile %1 to %2\n", oldpeak, _peakpath));
			PBD::copy_file (oldpeak, _peakpath);
		}
	}

	DEBUG_TRACE(DEBUG::Peaks, string_compose ("Initialize Peakfile %1 for Audio file %2\n", _peakpath, audio_path));

	if (g_stat (_peakpath.c_str(), &statbuf)) {
		if (errno != ENOENT) {
			/* it exists in the peaks dir, but there is some kind of error */

			error << string_compose(_("AudioSource: cannot stat peakfile \"%1\""), _peakpath) << endmsg;
			return -1;
		}

		DEBUG_TRACE(DEBUG::Peaks, string_compose("Peakfile %1 does not exist\n", _peakpath));

		_peaks_built = false;

	} else {

		/* we found it in the peaks dir, so check it out */

		if (statbuf.st_size == 0 || (statbuf.st_size < (off_t) ((length().samples() / _FPP) * sizeof (PeakData)))) {
			DEBUG_TRACE(DEBUG::Peaks, string_compose("Peakfile %1 is empty\n", _peakpath));
			_peaks_built = false;
		} else {
			// Check if the audio file has changed since the peakfile was built.
			GStatBuf stat_file;
			int err = g_stat (audio_path.c_str(), &stat_file);

			if (err) {

				/* no audio path - nested source or we can't
				   read it or ... whatever, use the peakfile as-is.
				*/
				DEBUG_TRACE(DEBUG::Peaks, string_compose("Error when calling stat on Peakfile %1\n", _peakpath));

				_peaks_built = true;
				_peak_byte_max = statbuf.st_size;

			} else {

				/* allow 6 seconds slop on checking peak vs. file times because of various
				   disk action "races"
				*/

				if (stat_file.st_mtime > statbuf.st_mtime && (stat_file.st_mtime - statbuf.st_mtime > 6)) {
					_peaks_built = false;
					_peak_byte_max = 0;
				} else {
					_peaks_built = true;
					_peak_byte_max = statbuf.st_size;
				}
			}
		}
	}

	if (!empty() && !_peaks_built && _build_missing_peakfiles && _build_peakfiles) {
		build_peaks_from_scratch ();
	}

	return 0;
}

samplecnt_t
AudioSource::read (Sample *dst, samplepos_t start, samplecnt_t cnt, int /*channel*/) const
{
	assert (cnt >= 0);

	Glib::Threads::Mutex::Lock lm (_lock);
	return read_unlocked (dst, start, cnt);
}

samplecnt_t
AudioSource::write (Sample *dst, samplecnt_t cnt)
{
	Glib::Threads::Mutex::Lock lm (_lock);
	/* any write makes the file not removable */
	_flags = Flag (_flags & ~Removable);
	return write_unlocked (dst, cnt);
}

int
AudioSource::read_peaks (PeakData *peaks, samplecnt_t npeaks, samplepos_t start, samplecnt_t cnt, double samples_per_visual_peak) const
{
	return read_peaks_with_fpp (peaks, npeaks, start, cnt, samples_per_visual_peak, _FPP);
}

/** @param peaks Buffer to write peak data.
 *  @param npeaks Number of peaks to write.
 */

int
AudioSource::read_peaks_with_fpp (PeakData *peaks, samplecnt_t npeaks, samplepos_t start, samplecnt_t cnt,
				  double samples_per_visual_peak, samplecnt_t samples_per_file_peak) const
{
	Glib::Threads::Mutex::Lock lm (_lock);

#if 0 // DEBUG ONLY
	/* Bypass peak-file cache, compute peaks using raw data from source */
	DEBUG_TRACE (DEBUG::Peaks, string_compose ("RP: npeaks = %1 start = %2 cnt = %3 spp = %4 pf = %5\n", npeaks, start, cnt, samples_per_visual_peak, _peakpath));
	{
		samplecnt_t scm = ceil (samples_per_visual_peak);
		samplecnt_t peak = 0;

#if 1 // direct read
		boost::scoped_array<Sample> buf(new Sample[scm]);
		while (peak < npeaks && cnt > 0) {
			samplecnt_t samples_read = read_unlocked (buf.get(), start, scm);
			if (samples_read == 0) {
				break;
			}
			peaks[peak].min = peaks[peak].max = buf[0];
			find_peaks (buf.get(), samples_read, &peaks[peak].min, &peaks[peak].max);

			start += samples_read;
			cnt -= samples_read;
			++peak;
		}
#else // generate square wave / ramp
		while (peak < npeaks && cnt > 0) {
			samplecnt_t samples_read = std::min (cnt, scm);
			samplecnt_t val = (start + samples_read / 2) % 24000;

			peaks[peak].min = peaks[peak].max = .5 - val / 24000.0;

			start += samples_read;
			cnt -= samples_read;
			++peak;
		}
#endif
		while (peak < npeaks) {
			peaks[peak].min = peaks[peak].max = 0;
			++peak;
		}
		return 0;
	}
#endif

	double scale;
	double expected_peaks;
	PeakData::PeakDatum xmax;
	PeakData::PeakDatum xmin;
	int32_t to_read;
#ifdef PLATFORM_WINDOWS
	SYSTEM_INFO system_info;
        GetSystemInfo (&system_info);
	const int bufsize = system_info.dwAllocationGranularity;;
#else
	const int bufsize = sysconf(_SC_PAGESIZE);
#endif
	samplecnt_t read_npeaks = npeaks;
	samplecnt_t zero_fill = 0;

	GStatBuf statbuf;

	expected_peaks = (cnt / (double) samples_per_file_peak);
	if (g_stat (_peakpath.c_str(), &statbuf) != 0) {
		error << string_compose (_("Cannot open peakfile @ %1 for size check (%2)"), _peakpath, strerror (errno)) << endmsg;
		return -1;
	}

	if (!_captured_for.empty()) {

		/* _captured_for is only set after a capture pass is
		 * complete. so we know that capturing is finished for this
		 * file, and now we can check actual size of the peakfile is at
		 * least large enough for all the data in the audio file. if it
		 * is too short, assume that a crash or other error truncated
		 * it, and rebuild it from scratch.
		 *
		 * XXX this may not work for destructive recording, but we
		 * might decided to get rid of that anyway.
		 *
		 */

		const off_t expected_file_size = (_length.samples() / (double) samples_per_file_peak) * sizeof (PeakData);

		if (statbuf.st_size < expected_file_size) {
			warning << string_compose (_("peak file %1 is truncated from %2 to %3"), _peakpath, expected_file_size, statbuf.st_size) << endmsg;
			lm.release(); // build_peaks_from_scratch() takes _lock
			const_cast<AudioSource*>(this)->build_peaks_from_scratch ();
			lm.acquire ();
			if (g_stat (_peakpath.c_str(), &statbuf) != 0) {
				error << string_compose (_("Cannot open peakfile @ %1 for size check (%2) after rebuild"), _peakpath, strerror (errno)) << endmsg;
			}
			if (statbuf.st_size < expected_file_size) {
				fatal << "peak file is still truncated after rebuild" << endmsg;
				abort (); /*NOTREACHED*/
			}
		}
	}

	ScopedFileDescriptor sfd (g_open (_peakpath.c_str(), O_RDONLY, 0444));

	if (sfd < 0) {
		error << string_compose (_("Cannot open peakfile @ %1 for reading (%2)"), _peakpath, strerror (errno)) << endmsg;
		return -1;
	}

	scale = npeaks/expected_peaks;


	DEBUG_TRACE (DEBUG::Peaks, string_compose (" ======>RP: npeaks = %1 start = %2 cnt = %3 len = %4 samples_per_visual_peak = %5 expected was %6 ... scale =  %7 PD ptr = %8 pf = %9\n"
			, npeaks, start, cnt, _length, samples_per_visual_peak, expected_peaks, scale, peaks, _peakpath));

	/* fix for near-end-of-file conditions */

	if (cnt + start > _length.samples()) {
		// cerr << "too close to end @ " << _length << " given " << start << " + " << cnt << " (" << _length - start << ")" << endl;
		cnt = std::max ((samplecnt_t)0, _length.samples() - start);
		read_npeaks = min ((samplecnt_t) floor (cnt / samples_per_visual_peak), npeaks);
		zero_fill = npeaks - read_npeaks;
		expected_peaks = (cnt / (double) samples_per_file_peak);
		scale = npeaks/expected_peaks;
	}

	assert (cnt >= 0);

	// cerr << "actual npeaks = " << read_npeaks << " zf = " << zero_fill << endl;

	if (npeaks == cnt) {

		DEBUG_TRACE (DEBUG::Peaks, "RAW DATA\n");

		/* no scaling at all, just get the sample data and duplicate it for
		   both max and min peak values.
		*/

		boost::scoped_array<Sample> raw_staging(new Sample[cnt]);

		if (read_unlocked (raw_staging.get(), start, cnt) != cnt) {
			error << _("cannot read sample data for unscaled peak computation") << endmsg;
			return -1;
		}

		for (samplecnt_t i = 0; i < npeaks; ++i) {
			peaks[i].max = raw_staging[i];
			peaks[i].min = raw_staging[i];
		}

		return 0;
	}

	if (scale == 1.0) {
		off_t first_peak_byte = (start / samples_per_file_peak) * sizeof (PeakData);
		size_t bytes_to_read = sizeof (PeakData) * read_npeaks;
		/* open, read, close */

		DEBUG_TRACE (DEBUG::Peaks, "DIRECT PEAKS\n");

		off_t  map_off =  first_peak_byte;
		off_t  read_map_off = map_off & ~(bufsize - 1);
		off_t  map_delta = map_off - read_map_off;
		size_t map_length = bytes_to_read + map_delta;

		if (_first_run  || (_last_scale != samples_per_visual_peak) || (_last_map_off != map_off) || (_last_raw_map_length  < bytes_to_read)) {
			peak_cache.reset (new PeakData[npeaks]);
			char* addr;
#ifdef PLATFORM_WINDOWS
			HANDLE file_handle = (HANDLE) _get_osfhandle(int(sfd));
			HANDLE map_handle;
			LPVOID view_handle;
			bool err_flag;

			map_handle = CreateFileMapping(file_handle, NULL, PAGE_READONLY, 0, 0, NULL);
			if (map_handle == NULL) {
				error << string_compose (_("map failed - could not create file mapping for peakfile %1."), _peakpath) << endmsg;
				return -1;
			}

			view_handle = MapViewOfFile(map_handle, FILE_MAP_READ, 0, read_map_off, map_length);
			if (view_handle == NULL) {
				error << string_compose (_("map failed - could not map peakfile %1."), _peakpath) << endmsg;
				return -1;
			}

			addr = (char*) view_handle;

			memcpy ((void*)peak_cache.get(), (void*)(addr + map_delta), bytes_to_read);

			err_flag = UnmapViewOfFile (view_handle);
			err_flag = CloseHandle(map_handle);
			if(!err_flag) {
				error << string_compose (_("unmap failed - could not unmap peakfile %1."), _peakpath) << endmsg;
				return -1;
			}
#else
			addr = (char*) mmap (0, map_length, PROT_READ, MAP_PRIVATE, sfd, read_map_off);
			if (addr ==  MAP_FAILED) {
				error << string_compose (_("map failed - could not mmap peakfile %1."), _peakpath) << endmsg;
				return -1;
			}

			memcpy ((void*)peak_cache.get(), (void*)(addr + map_delta), bytes_to_read);
			munmap (addr, map_length);
#endif
			if (zero_fill) {
				memset (&peak_cache[read_npeaks], 0, sizeof (PeakData) * zero_fill);
			}

			_first_run = false;
			_last_scale = samples_per_visual_peak;
			_last_map_off = map_off;
			_last_raw_map_length = bytes_to_read;
		}

		memcpy ((void*)peaks, (void*)peak_cache.get(), npeaks * sizeof(PeakData));

		return 0;
	}

	if (scale < 1.0) {

		DEBUG_TRACE (DEBUG::Peaks, "DOWNSAMPLE\n");

		/* the caller wants:
		 *
		 * - more samples-per-peak (lower resolution) than the peakfile, or to put it another way,
		 * - less peaks than the peakfile holds for the same range
		 *
		 * So, read a block into a staging area, and then downsample from there.
		 *
		 * to avoid confusion, I'll refer to the requested peaks as visual_peaks and the peakfile peaks as stored_peaks
		 */

		const samplecnt_t chunksize = (samplecnt_t) expected_peaks; // we read all the peaks we need in one hit.

		/* compute the rounded up sample position  */

		samplepos_t current_stored_peak = (samplepos_t) ceil (start / (double) samples_per_file_peak);
		samplepos_t next_visual_peak  = (samplepos_t) ceil (start / samples_per_visual_peak);
		double     next_visual_peak_sample = next_visual_peak * samples_per_visual_peak;
		samplepos_t stored_peak_before_next_visual_peak = (samplepos_t) next_visual_peak_sample / samples_per_file_peak;
		samplecnt_t nvisual_peaks = 0;
		uint32_t i = 0;

		/* handle the case where the initial visual peak is on a pixel boundary */

		current_stored_peak = min (current_stored_peak, stored_peak_before_next_visual_peak);

		/* open ... close during out: handling */

		off_t  map_off =  (uint32_t) (ceil (start / (double) samples_per_file_peak)) * sizeof(PeakData);
		off_t  read_map_off = map_off & ~(bufsize - 1);
		off_t  map_delta = map_off - read_map_off;
		size_t raw_map_length = chunksize * sizeof(PeakData);
		size_t map_length = (chunksize * sizeof(PeakData)) + map_delta;

		if (_first_run || (_last_scale != samples_per_visual_peak) || (_last_map_off != map_off) || (_last_raw_map_length < raw_map_length)) {
			peak_cache.reset (new PeakData[npeaks]);
			boost::scoped_array<PeakData> staging (new PeakData[chunksize]);

			char* addr;
#ifdef PLATFORM_WINDOWS
			HANDLE file_handle =  (HANDLE) _get_osfhandle(int(sfd));
			HANDLE map_handle;
			LPVOID view_handle;
			bool err_flag;

			map_handle = CreateFileMapping(file_handle, NULL, PAGE_READONLY, 0, 0, NULL);
			if (map_handle == NULL) {
				error << string_compose (_("map failed - could not create file mapping for peakfile %1."), _peakpath) << endmsg;
				return -1;
			}

			view_handle = MapViewOfFile(map_handle, FILE_MAP_READ, 0, read_map_off, map_length);
			if (view_handle == NULL) {
				error << string_compose (_("map failed - could not map peakfile %1."), _peakpath) << endmsg;
				return -1;
			}

			addr = (char *) view_handle;

			memcpy ((void*)staging.get(), (void*)(addr + map_delta), raw_map_length);

			err_flag = UnmapViewOfFile (view_handle);
			err_flag = CloseHandle(map_handle);
			if(!err_flag) {
				error << string_compose (_("unmap failed - could not unmap peakfile %1."), _peakpath) << endmsg;
				return -1;
			}
#else
			addr = (char*) mmap (0, map_length, PROT_READ, MAP_PRIVATE, sfd, read_map_off);
			if (addr ==  MAP_FAILED) {
				error << string_compose (_("map failed - could not mmap peakfile %1."), _peakpath) << endmsg;
				return -1;
			}

			memcpy ((void*)staging.get(), (void*)(addr + map_delta), raw_map_length);
			munmap (addr, map_length);
#endif
			while (nvisual_peaks < read_npeaks) {

				xmax = -1.0;
				xmin = 1.0;

				while ((current_stored_peak <= stored_peak_before_next_visual_peak) && (i < chunksize)) {

					xmax = max (xmax, staging[i].max);
					xmin = min (xmin, staging[i].min);
					++i;
					++current_stored_peak;
				}

				peak_cache[nvisual_peaks].max = xmax;
				peak_cache[nvisual_peaks].min = xmin;
				++nvisual_peaks;
				next_visual_peak_sample = min ((double) start + cnt, (next_visual_peak_sample + samples_per_visual_peak));
				stored_peak_before_next_visual_peak = (uint32_t) next_visual_peak_sample / samples_per_file_peak;
			}

			if (zero_fill) {
				cerr << "Zero fill end of peaks (@ " << read_npeaks << " with " << zero_fill << ")" << endl;
				memset (&peak_cache[read_npeaks], 0, sizeof (PeakData) * zero_fill);
			}

			_first_run = false;
			_last_scale = samples_per_visual_peak;
			_last_map_off = map_off;
			_last_raw_map_length = raw_map_length;
		}

		memcpy ((void*)peaks, (void*)peak_cache.get(), npeaks * sizeof(PeakData));

	} else {
		DEBUG_TRACE (DEBUG::Peaks, "UPSAMPLE\n");

		/* the caller wants
		 *
		 * - less samples-per-peak (more resolution)
		 * - more peaks than stored in the Peakfile
		 *
		 * So, fetch data from the raw source, and generate peak
		 * data on the fly.
		*/

		samplecnt_t samples_read = 0;
		samplepos_t current_sample = start;
		samplecnt_t i = 0;
		samplecnt_t nvisual_peaks = 0;
		samplecnt_t chunksize = (samplecnt_t) min (cnt, (samplecnt_t) 4096);
		boost::scoped_array<Sample> raw_staging(new Sample[chunksize]);

		double pixel_pos         = start / samples_per_visual_peak;
		double next_pixel_pos    = 1.0 + floor (pixel_pos);
		double pixels_per_sample = 1.0 / samples_per_visual_peak;

		xmin = 1.0;
		xmax = -1.0;

		while (nvisual_peaks < read_npeaks) {

			if (i == samples_read) {

				to_read = min (chunksize, (samplecnt_t)(_length.samples() - current_sample));

				if (current_sample >= _length.samples()) {

					/* hmm, error condition - we've reached the end of the file
					 * without generating all the peak data. cook up a zero-filled
					 * data buffer and then use it. this is simpler than
					 * adjusting zero_fill and read_npeaks and then breaking out of
					 * this loop early
					 */

					memset (raw_staging.get(), 0, sizeof (Sample) * chunksize);

				} else {

					to_read = min (chunksize, (_length.samples() - current_sample));


					if ((samples_read = read_unlocked (raw_staging.get(), current_sample, to_read)) == 0) {
						error << string_compose(_("AudioSource[%1]: peak read - cannot read %2 samples at offset %3 of %4 (%5)"),
						                        _name, to_read, current_sample, _length, strerror (errno))
						     << endmsg;
						return -1;
					}
				}

				i = 0;
			}

			xmax = max (xmax, raw_staging[i]);
			xmin = min (xmin, raw_staging[i]);
			++i;
			++current_sample;
			pixel_pos += pixels_per_sample;

			if (pixel_pos >= next_pixel_pos) {

				peaks[nvisual_peaks].max = xmax;
				peaks[nvisual_peaks].min = xmin;
				++nvisual_peaks;
				xmin = 1.0;
				xmax = -1.0;

				next_pixel_pos = ceil (pixel_pos + 0.5);
			}
		}

		if (zero_fill) {
			memset (&peaks[read_npeaks], 0, sizeof (PeakData) * zero_fill);
		}
	}

	DEBUG_TRACE (DEBUG::Peaks, "READPEAKS DONE\n");
	return 0;
}

int
AudioSource::build_peaks_from_scratch ()
{
	const samplecnt_t bufsize = 65536; // 256kB per disk read for mono data is about ideal

	DEBUG_TRACE (DEBUG::Peaks, "Building peaks from scratch\n");

	int ret = -1;

	{
		/* hold lock while building peaks */

		Glib::Threads::Mutex::Lock lp (_lock);

		if (prepare_for_peakfile_writes ()) {
			goto out;
		}

		samplecnt_t current_sample = 0;
		samplecnt_t cnt = _length.samples();

		_peaks_built = false;
		boost::scoped_array<Sample> buf(new Sample[bufsize]);

		while (cnt) {

			samplecnt_t samples_to_read = min (bufsize, cnt);
			samplecnt_t samples_read;

			if ((samples_read = read_unlocked (buf.get(), current_sample, samples_to_read)) != samples_to_read) {
				error << string_compose(_("%1: could not write read raw data for peak computation (%2)"), _name, strerror (errno)) << endmsg;
				done_with_peakfile_writes (false);
				goto out;
			}

			lp.release(); // allow butler to refill buffers

			if (_session.deletion_in_progress() || _session.peaks_cleanup_in_progres()) {
				cerr << "peak file creation interrupted: " << _name << endmsg;
				lp.acquire();
				done_with_peakfile_writes (false);
				goto out;
			}

			if (compute_and_write_peaks (buf.get(), current_sample, samples_read, true, false, _FPP)) {
				break;
			}

			current_sample += samples_read;
			cnt -= samples_read;

			lp.acquire();
		}

		if (cnt == 0) {
			/* success */
			truncate_peakfile();
		}

		done_with_peakfile_writes ((cnt == 0));
		if (cnt == 0) {
			ret = 0;
		}
	}

  out:
	if (ret) {
		DEBUG_TRACE (DEBUG::Peaks, string_compose("Could not write peak data, attempting to remove peakfile %1\n", _peakpath));
		::g_unlink (_peakpath.c_str());
	}

	return ret;
}

int
AudioSource::close_peakfile ()
{
	Glib::Threads::Mutex::Lock lp (_lock);
	if (_peakfile_fd >= 0) {
		close (_peakfile_fd);
		_peakfile_fd = -1;
	}
	if (!_peakpath.empty()) {
		::g_unlink (_peakpath.c_str());
	}
	_peaks_built = false;
	return 0;
}

int
AudioSource::prepare_for_peakfile_writes ()
{
	if (_session.deletion_in_progress() || _session.peaks_cleanup_in_progres()) {
		return -1;
	}

	if ((_peakfile_fd = g_open (_peakpath.c_str(), O_CREAT|O_RDWR, 0664)) < 0) {
		error << string_compose(_("AudioSource: cannot open _peakpath (c) \"%1\" (%2)"), _peakpath, strerror (errno)) << endmsg;
		return -1;
	}
	return 0;
}

void
AudioSource::done_with_peakfile_writes (bool done)
{
	if (_session.deletion_in_progress() || _session.peaks_cleanup_in_progres()) {
		if (_peakfile_fd) {
			close (_peakfile_fd);
			_peakfile_fd = -1;
		}
		return;
	}

	if (peak_leftover_cnt) {
		compute_and_write_peaks (0, 0, 0, true, false, _FPP);
	}

	close (_peakfile_fd);
	_peakfile_fd = -1;

	if (done) {
		Glib::Threads::Mutex::Lock lm (_peaks_ready_lock);
		_peaks_built = true;
		PeaksReady (); /* EMIT SIGNAL */
	}
}

/** @param first_sample Offset from the source start of the first sample to
 * process. _lock MUST be held by caller.
*/
int
AudioSource::compute_and_write_peaks (Sample* buf, samplecnt_t first_sample, samplecnt_t cnt,
				      bool force, bool intermediate_peaks_ready)
{
	return compute_and_write_peaks (buf, first_sample, cnt, force, intermediate_peaks_ready, _FPP);
}

int
AudioSource::compute_and_write_peaks (Sample* buf, samplecnt_t first_sample, samplecnt_t cnt,
				      bool force, bool intermediate_peaks_ready, samplecnt_t fpp)
{
	samplecnt_t to_do;
	uint32_t  peaks_computed;
	samplepos_t current_sample;
	samplecnt_t samples_done;
	const size_t blocksize = (128 * 1024);
	off_t first_peak_byte;
	boost::scoped_array<Sample> buf2;

	if (_peakfile_fd < 0) {
		if (prepare_for_peakfile_writes ()) {
			return -1;
		}
	}

  restart:
	if (peak_leftover_cnt) {

		if (first_sample != peak_leftover_sample + peak_leftover_cnt) {

			/* uh-oh, ::seek() since the last ::compute_and_write_peaks(),
			   and we have leftovers. flush a single peak (since the leftovers
			   never represent more than that, and restart.
			*/

			PeakData x;

			x.min = peak_leftovers[0];
			x.max = peak_leftovers[0];

			off_t byte = (peak_leftover_sample / fpp) * sizeof (PeakData);

			off_t offset = lseek (_peakfile_fd, byte, SEEK_SET);

			if (offset != byte) {
				error << string_compose(_("%1: could not seek in peak file data (%2)"), _name, strerror (errno)) << endmsg;
				return -1;
			}

			if (::write (_peakfile_fd, &x, sizeof (PeakData)) != sizeof (PeakData)) {
				error << string_compose(_("%1: could not write peak file data (%2)"), _name, strerror (errno)) << endmsg;
				return -1;
			}

			_peak_byte_max = max (_peak_byte_max, (off_t) (byte + sizeof(PeakData)));

			{
				Glib::Threads::Mutex::Lock lm (_peaks_ready_lock);
				PeakRangeReady (peak_leftover_sample, peak_leftover_cnt); /* EMIT SIGNAL */
				if (intermediate_peaks_ready) {
					PeaksReady (); /* EMIT SIGNAL */
				}
			}

			/* left overs are done */

			peak_leftover_cnt = 0;
			goto restart;
		}

		/* else ... had leftovers, but they immediately preceed the new data, so just
		   merge them and compute.
		*/

		/* make a new contiguous buffer containing leftovers and the new stuff */

		to_do = cnt + peak_leftover_cnt;
		buf2.reset(new Sample[to_do]);

		/* the remnants */
		memcpy (buf2.get(), peak_leftovers, peak_leftover_cnt * sizeof (Sample));

		/* the new stuff */
		if (buf && cnt > 0) {
			memcpy (buf2.get()+peak_leftover_cnt, buf, cnt * sizeof (Sample));
		}

		/* no more leftovers */
		peak_leftover_cnt = 0;

		/* use the temporary buffer */
		buf = buf2.get();

		/* make sure that when we write into the peakfile, we startup where we left off */

		first_sample = peak_leftover_sample;

	} else {
		to_do = cnt;
	}

	boost::scoped_array<PeakData> peakbuf(new PeakData[(to_do/fpp)+1]);
	peaks_computed = 0;
	current_sample = first_sample;
	samples_done = 0;

	while (to_do) {

		/* if some samples were passed in (i.e. we're not flushing leftovers)
		   and there are less than fpp to do, save them till
		   next time
		*/

		if (force && (to_do < fpp)) {
			/* keep the left overs around for next time */

			if (peak_leftover_size < to_do) {
				delete [] peak_leftovers;
				peak_leftovers = new Sample[to_do];
				peak_leftover_size = to_do;
			}
			memcpy (peak_leftovers, buf, to_do * sizeof (Sample));
			peak_leftover_cnt = to_do;
			peak_leftover_sample = current_sample;

			/* done for now */

			break;
		}

		samplecnt_t this_time = min (fpp, to_do);

		peakbuf[peaks_computed].max = buf[0];
		peakbuf[peaks_computed].min = buf[0];

		ARDOUR::find_peaks (buf+1, this_time-1, &peakbuf[peaks_computed].min, &peakbuf[peaks_computed].max);

		peaks_computed++;
		buf += this_time;
		to_do -= this_time;
		samples_done += this_time;
		current_sample += this_time;
	}

	first_peak_byte = (first_sample / fpp) * sizeof (PeakData);

	if (can_truncate_peaks()) {

		/* on some filesystems (ext3, at least) this helps to reduce fragmentation of
		   the peakfiles. its not guaranteed to do so, and even on ext3 (as of december 2006)
		   it does not cause single-extent allocation even for peakfiles of
		   less than BLOCKSIZE bytes.  only call ftruncate if we'll make the file larger.
		*/

		off_t endpos = lseek (_peakfile_fd, 0, SEEK_END);
		off_t target_length = blocksize * ((first_peak_byte + blocksize + 1) / blocksize);

		if (endpos < target_length) {
			DEBUG_TRACE(DEBUG::Peaks, string_compose ("Truncating Peakfile %1\n", _peakpath));
			if (ftruncate (_peakfile_fd, target_length)) {
				/* error doesn't actually matter so continue on without testing */
			}
		}
	}


	off_t offset = lseek(_peakfile_fd, first_peak_byte, SEEK_SET);

	if (offset != first_peak_byte) {
		error << string_compose(_("%1: could not seek in peak file data (%2)"), _name, strerror (errno)) << endmsg;
		return -1;
	}

	ssize_t bytes_to_write = sizeof (PeakData) * peaks_computed;

	ssize_t bytes_written = ::write (_peakfile_fd, peakbuf.get(), bytes_to_write);

	if (bytes_written != bytes_to_write) {
		error << string_compose(_("%1: could not write peak file data (%2)"), _name, strerror (errno)) << endmsg;
		return -1;
	}

	_peak_byte_max = max (_peak_byte_max, (off_t) (first_peak_byte + bytes_to_write));

	if (samples_done) {
		Glib::Threads::Mutex::Lock lm (_peaks_ready_lock);
		PeakRangeReady (first_sample, samples_done); /* EMIT SIGNAL */
		if (intermediate_peaks_ready) {
			PeaksReady (); /* EMIT SIGNAL */
		}
	}

	return 0;
}

void
AudioSource::truncate_peakfile ()
{
	if (_peakfile_fd < 0) {
		error << string_compose (_("programming error: %1"), "AudioSource::truncate_peakfile() called without open peakfile descriptor")
		      << endmsg;
		return;
	}

	/* truncate the peakfile down to its natural length if necessary */

	off_t end = lseek (_peakfile_fd, 0, SEEK_END);

	if (end > _peak_byte_max) {
		DEBUG_TRACE(DEBUG::Peaks, string_compose ("Truncating Peakfile  %1\n", _peakpath));
		if (ftruncate (_peakfile_fd, _peak_byte_max)) {
			error << string_compose (_("could not truncate peakfile %1 to %2 (error: %3)"),
						 _peakpath, _peak_byte_max, errno) << endmsg;
		}
	}
}

samplecnt_t
AudioSource::available_peaks (double zoom_factor) const
{
	if (zoom_factor < _FPP) {
		return _length.samples(); // peak data will come from the audio file
	}

	/* peak data comes from peakfile, but the filesize might not represent
	   the valid data due to ftruncate optimizations, so use _peak_byte_max state.
	   XXX - there might be some atomicity issues here, we should probably add a lock,
	   but _peak_byte_max only monotonically increases after initialization.
	*/

	off_t end = _peak_byte_max;

	return (end/sizeof(PeakData)) * _FPP;
}

void
AudioSource::mark_streaming_write_completed (const Lock& lock)
{
	Glib::Threads::Mutex::Lock lm (_peaks_ready_lock);

	if (_peaks_built) {
		PeaksReady (); /* EMIT SIGNAL */
	}
}
