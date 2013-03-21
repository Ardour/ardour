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

#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <float.h>
#include <utime.h>
#include <cerrno>
#include <ctime>
#include <cmath>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <vector>

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "pbd/xml++.h"

#include "ardour/audiosource.h"
#include "ardour/rc_configuration.h"
#include "ardour/runtime_functions.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

Glib::Threads::Mutex AudioSource::_level_buffer_lock;
vector<boost::shared_array<Sample> > AudioSource::_mixdown_buffers;
vector<boost::shared_array<gain_t> > AudioSource::_gain_buffers;
size_t AudioSource::_working_buffers_size = 0;
bool AudioSource::_build_missing_peakfiles = false;

/** true if we want peakfiles (e.g. if we are displaying a GUI) */
bool AudioSource::_build_peakfiles = false;

#define _FPP 256

AudioSource::AudioSource (Session& s, string name)
	: Source (s, DataType::AUDIO, name)
	, _length (0)
{
	_peaks_built = false;
	_peak_byte_max = 0;
	_peakfile_descriptor = 0;
	peak_leftover_cnt = 0;
	peak_leftover_size = 0;
	peak_leftovers = 0;
}

AudioSource::AudioSource (Session& s, const XMLNode& node)
	: Source (s, node)
	, _length (0)
{

	_peaks_built = false;
	_peak_byte_max = 0;
	_peakfile_descriptor = 0;
	peak_leftover_cnt = 0;
	peak_leftover_size = 0;
	peak_leftovers = 0;

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

	delete _peakfile_descriptor;
	delete [] peak_leftovers;
}

XMLNode&
AudioSource::get_state ()
{
	XMLNode& node (Source::get_state());

	if (_captured_for.length()) {
		node.add_property ("captured-for", _captured_for);
	}

	return node;
}

int
AudioSource::set_state (const XMLNode& node, int /*version*/)
{
	const XMLProperty* prop;

	if ((prop = node.property ("captured-for")) != 0) {
		_captured_for = prop->value();
	}

	return 0;
}

bool
AudioSource::empty () const
{
        return _length == 0;
}

framecnt_t
AudioSource::length (framepos_t /*pos*/) const
{
	return _length;
}

void
AudioSource::update_length (framecnt_t len)
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
	struct stat statbuf;

	if (stat (peakpath.c_str(), &statbuf) != 0 || statbuf.st_size == 0) {
		return;
	}

	struct utimbuf tbuf;

	tbuf.actime = statbuf.st_atime;
	tbuf.modtime = time ((time_t) 0);

	utime (peakpath.c_str(), &tbuf);
}

int
AudioSource::rename_peakfile (string newpath)
{
	/* caller must hold _lock */

	string oldpath = peakpath;

	if (Glib::file_test (oldpath, Glib::FILE_TEST_EXISTS)) {
		if (rename (oldpath.c_str(), newpath.c_str()) != 0) {
			error << string_compose (_("cannot rename peakfile for %1 from %2 to %3 (%4)"), _name, oldpath, newpath, strerror (errno)) << endmsg;
			return -1;
		}
	}

	peakpath = newpath;

	return 0;
}

int
AudioSource::initialize_peakfile (bool newfile, string audio_path)
{
	struct stat statbuf;

	peakpath = peak_path (audio_path);

	/* if the peak file should be there, but isn't .... */

	if (!newfile && !Glib::file_test (peakpath.c_str(), Glib::FILE_TEST_EXISTS)) {
		peakpath = find_broken_peakfile (peakpath, audio_path);
	}

	if (stat (peakpath.c_str(), &statbuf)) {
		if (errno != ENOENT) {
			/* it exists in the peaks dir, but there is some kind of error */

			error << string_compose(_("AudioSource: cannot stat peakfile \"%1\""), peakpath) << endmsg;
			return -1;
		}

		/* peakfile does not exist */

		_peaks_built = false;

	} else {

		/* we found it in the peaks dir, so check it out */

		if (statbuf.st_size == 0 || (statbuf.st_size < (off_t) ((length(_timeline_position) / _FPP) * sizeof (PeakData)))) {
			// empty
			_peaks_built = false;
		} else {
			// Check if the audio file has changed since the peakfile was built.
			struct stat stat_file;
			int err = stat (audio_path.c_str(), &stat_file);

			if (err) {

				/* no audio path - nested source or we can't
				   read it or ... whatever, use the peakfile as-is.
				*/

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

	if (!newfile && !_peaks_built && _build_missing_peakfiles && _build_peakfiles) {
		build_peaks_from_scratch ();
	}

	return 0;
}

framecnt_t
AudioSource::read (Sample *dst, framepos_t start, framecnt_t cnt, int /*channel*/) const
{
	assert (cnt >= 0);
	
	Glib::Threads::Mutex::Lock lm (_lock);
	return read_unlocked (dst, start, cnt);
}

framecnt_t
AudioSource::write (Sample *dst, framecnt_t cnt)
{
	Glib::Threads::Mutex::Lock lm (_lock);
	/* any write makes the fill not removable */
	_flags = Flag (_flags & ~Removable);
	return write_unlocked (dst, cnt);
}

int
AudioSource::read_peaks (PeakData *peaks, framecnt_t npeaks, framepos_t start, framecnt_t cnt, double samples_per_visual_peak) const
{
	return read_peaks_with_fpp (peaks, npeaks, start, cnt, samples_per_visual_peak, _FPP);
}

/** @param peaks Buffer to write peak data.
 *  @param npeaks Number of peaks to write.
 */

int
AudioSource::read_peaks_with_fpp (PeakData *peaks, framecnt_t npeaks, framepos_t start, framecnt_t cnt,
				  double samples_per_visual_peak, framecnt_t samples_per_file_peak) const
{
	Glib::Threads::Mutex::Lock lm (_lock);
	double scale;
	double expected_peaks;
	PeakData::PeakDatum xmax;
	PeakData::PeakDatum xmin;
	int32_t to_read;
	uint32_t nread;
	framecnt_t zero_fill = 0;
	int ret = -1;
	PeakData* staging = 0;
	Sample* raw_staging = 0;

	FdFileDescriptor* peakfile_descriptor = new FdFileDescriptor (peakpath, false, 0664);
	int peakfile_fd = -1;

	expected_peaks = (cnt / (double) samples_per_file_peak);
	scale = npeaks/expected_peaks;

#undef DEBUG_READ_PEAKS
#ifdef DEBUG_READ_PEAKS
	cerr << "======>RP: npeaks = " << npeaks
	     << " start = " << start
	     << " cnt = " << cnt
	     << " len = " << _length
	     << "   samples_per_visual_peak =" << samples_per_visual_peak
	     << " expected was " << expected_peaks << " ... scale = " << scale
	     << " PD ptr = " << peaks
	     <<endl;

#endif

	/* fix for near-end-of-file conditions */

	if (cnt > _length - start) {
		// cerr << "too close to end @ " << _length << " given " << start << " + " << cnt << endl;
		cnt = _length - start;
		framecnt_t old = npeaks;
		npeaks = min ((framecnt_t) floor (cnt / samples_per_visual_peak), npeaks);
		zero_fill = old - npeaks;
	}

	// cerr << "actual npeaks = " << npeaks << " zf = " << zero_fill << endl;

	if (npeaks == cnt) {

#ifdef DEBUG_READ_PEAKS
		cerr << "RAW DATA\n";
#endif
		/* no scaling at all, just get the sample data and duplicate it for
		   both max and min peak values.
		*/

		Sample* raw_staging = new Sample[cnt];

		if (read_unlocked (raw_staging, start, cnt) != cnt) {
			error << _("cannot read sample data for unscaled peak computation") << endmsg;
			return -1;
		}

		for (framecnt_t i = 0; i < npeaks; ++i) {
			peaks[i].max = raw_staging[i];
			peaks[i].min = raw_staging[i];
		}

		delete peakfile_descriptor;
		delete [] raw_staging;
		return 0;
	}

	if (scale == 1.0) {

		off_t first_peak_byte = (start / samples_per_file_peak) * sizeof (PeakData);

		/* open, read, close */

		if ((peakfile_fd = peakfile_descriptor->allocate ()) < 0) {
			error << string_compose(_("AudioSource: cannot open peakpath (a) \"%1\" (%2)"), peakpath, strerror (errno)) << endmsg;
			delete peakfile_descriptor;
			return -1;
		}

#ifdef DEBUG_READ_PEAKS
		cerr << "DIRECT PEAKS\n";
#endif

		nread = ::pread (peakfile_fd, peaks, sizeof (PeakData)* npeaks, first_peak_byte);

		if (nread != sizeof (PeakData) * npeaks) {
			cerr << "AudioSource["
			     << _name
			     << "]: cannot read peaks from peakfile! (read only "
			     << nread
			     << " not "
			     << npeaks
			      << "at sample "
			     << start
			     << " = byte "
			     << first_peak_byte
			     << ')'
			     << endl;
			delete peakfile_descriptor;
			return -1;
		}

		if (zero_fill) {
			memset (&peaks[npeaks], 0, sizeof (PeakData) * zero_fill);
		}

		delete peakfile_descriptor;
		return 0;
	}


	framecnt_t tnp;

	if (scale < 1.0) {

#ifdef DEBUG_READ_PEAKS
		cerr << "DOWNSAMPLE\n";
#endif
		/* the caller wants:

		    - more frames-per-peak (lower resolution) than the peakfile, or to put it another way,
                    - less peaks than the peakfile holds for the same range

		    So, read a block into a staging area, and then downsample from there.

		    to avoid confusion, I'll refer to the requested peaks as visual_peaks and the peakfile peaks as stored_peaks
		*/

		const framecnt_t chunksize = (framecnt_t) min (expected_peaks, 65536.0);

		staging = new PeakData[chunksize];

		/* compute the rounded up frame position  */

		framepos_t current_frame = start;
		framepos_t current_stored_peak = (framepos_t) ceil (current_frame / (double) samples_per_file_peak);
		framepos_t next_visual_peak  = (framepos_t) ceil (current_frame / samples_per_visual_peak);
		double     next_visual_peak_frame = next_visual_peak * samples_per_visual_peak;
		framepos_t stored_peak_before_next_visual_peak = (framepos_t) next_visual_peak_frame / samples_per_file_peak;
		framecnt_t nvisual_peaks = 0;
		framecnt_t stored_peaks_read = 0;
		framecnt_t i = 0;

		/* handle the case where the initial visual peak is on a pixel boundary */

		current_stored_peak = min (current_stored_peak, stored_peak_before_next_visual_peak);

		/* open ... close during out: handling */

		if ((peakfile_fd = peakfile_descriptor->allocate ()) < 0) {
			error << string_compose(_("AudioSource: cannot open peakpath (b) \"%1\" (%2)"), peakpath, strerror (errno)) << endmsg;
			delete peakfile_descriptor;
			delete [] staging;
			return 0;
		}

		while (nvisual_peaks < npeaks) {

			if (i == stored_peaks_read) {

				uint32_t       start_byte = current_stored_peak * sizeof(PeakData);
				tnp = min ((framecnt_t)(_length/samples_per_file_peak - current_stored_peak), (framecnt_t) expected_peaks);
				to_read = min (chunksize, tnp);

#ifdef DEBUG_READ_PEAKS
				cerr << "read " << sizeof (PeakData) * to_read << " from peakfile @ " << start_byte << endl;
#endif

				if ((nread = ::pread (peakfile_fd, staging, sizeof (PeakData) * to_read, start_byte))
				    != sizeof (PeakData) * to_read) {

					off_t fend = lseek (peakfile_fd, 0, SEEK_END);

					cerr << "AudioSource["
					     << _name
					     << "]: cannot read peak data from peakfile ("
					     << (nread / sizeof(PeakData))
					     << " peaks instead of "
					     << to_read
					     << ") ("
					     << strerror (errno)
					     << ')'
					     << " at start_byte = " << start_byte
					     << " _length = " << _length << " versus len = " << fend
					     << " expected maxpeaks = " << (_length - current_frame)/samples_per_file_peak
					     << " npeaks was " << npeaks
					     << endl;
					goto out;
				}

				i = 0;
				stored_peaks_read = nread / sizeof(PeakData);
			}

			xmax = -1.0;
			xmin = 1.0;

			while ((i < stored_peaks_read) && (current_stored_peak <= stored_peak_before_next_visual_peak)) {

				xmax = max (xmax, staging[i].max);
				xmin = min (xmin, staging[i].min);
				++i;
				++current_stored_peak;
				--expected_peaks;
			}

			peaks[nvisual_peaks].max = xmax;
			peaks[nvisual_peaks].min = xmin;
			++nvisual_peaks;
			++next_visual_peak;

			//next_visual_peak_frame = min ((next_visual_peak * samples_per_visual_peak), (next_visual_peak_frame+samples_per_visual_peak) );
			next_visual_peak_frame =  min ((double) start+cnt, (next_visual_peak_frame+samples_per_visual_peak) );
			stored_peak_before_next_visual_peak = (uint32_t) next_visual_peak_frame / samples_per_file_peak;
		}

		if (zero_fill) {
			memset (&peaks[npeaks], 0, sizeof (PeakData) * zero_fill);
		}

		ret = 0;

	} else {

#ifdef DEBUG_READ_PEAKS
		cerr << "UPSAMPLE\n";
#endif
		/* the caller wants

		     - less frames-per-peak (more resolution)
		     - more peaks than stored in the Peakfile

		   So, fetch data from the raw source, and generate peak
		   data on the fly.
		*/

		framecnt_t frames_read = 0;
		framepos_t current_frame = start;
		framecnt_t i = 0;
		framecnt_t nvisual_peaks = 0;
		framecnt_t chunksize = (framecnt_t) min (cnt, (framecnt_t) 4096);
		raw_staging = new Sample[chunksize];

		framepos_t frame_pos = start;
		double pixel_pos = floor (frame_pos / samples_per_visual_peak);
		double next_pixel_pos = ceil (frame_pos / samples_per_visual_peak);
		double pixels_per_frame = 1.0 / samples_per_visual_peak;

		xmin = 1.0;
		xmax = -1.0;

		while (nvisual_peaks < npeaks) {

			if (i == frames_read) {

				to_read = min (chunksize, (framecnt_t)(_length - current_frame));

				if (current_frame >= _length) {

                                        /* hmm, error condition - we've reached the end of the file
                                           without generating all the peak data. cook up a zero-filled
                                           data buffer and then use it. this is simpler than
                                           adjusting zero_fill and npeaks and then breaking out of
                                           this loop early
					*/

                                        memset (raw_staging, 0, sizeof (Sample) * chunksize);

                                } else {

                                        to_read = min (chunksize, (_length - current_frame));


                                        if ((frames_read = read_unlocked (raw_staging, current_frame, to_read)) == 0) {
                                                error << string_compose(_("AudioSource[%1]: peak read - cannot read %2 samples at offset %3 of %4 (%5)"),
                                                                        _name, to_read, current_frame, _length, strerror (errno))
                                                      << endmsg;
                                                goto out;
                                        }
                                }

				i = 0;
			}

			xmax = max (xmax, raw_staging[i]);
			xmin = min (xmin, raw_staging[i]);
			++i;
			++current_frame;
			pixel_pos += pixels_per_frame;

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
			memset (&peaks[npeaks], 0, sizeof (PeakData) * zero_fill);
		}

		ret = 0;
	}

  out:
	delete peakfile_descriptor;

	delete [] staging;
	delete [] raw_staging;

#ifdef DEBUG_READ_PEAKS
	cerr << "RP DONE\n";
#endif

	return ret;
}

#undef DEBUG_PEAK_BUILD

int
AudioSource::build_peaks_from_scratch ()
{
	Sample* buf = 0;

	const framecnt_t bufsize = 65536; // 256kB per disk read for mono data is about ideal

	int ret = -1;

	{
		/* hold lock while building peaks */

		Glib::Threads::Mutex::Lock lp (_lock);

		if (prepare_for_peakfile_writes ()) {
			goto out;
		}

		framecnt_t current_frame = 0;
		framecnt_t cnt = _length;

		_peaks_built = false;
		buf = new Sample[bufsize];

		while (cnt) {

			framecnt_t frames_to_read = min (bufsize, cnt);
			framecnt_t frames_read;
			
			if ((frames_read = read_unlocked (buf, current_frame, frames_to_read)) != frames_to_read) {
				error << string_compose(_("%1: could not write read raw data for peak computation (%2)"), _name, strerror (errno)) << endmsg;
				done_with_peakfile_writes (false);
				goto out;
			}

			if (compute_and_write_peaks (buf, current_frame, frames_read, true, false, _FPP)) {
				break;
			}

			current_frame += frames_read;
			cnt -= frames_read;
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
		unlink (peakpath.c_str());
	}

	delete [] buf;

	return ret;
}

int
AudioSource::prepare_for_peakfile_writes ()
{
	_peakfile_descriptor = new FdFileDescriptor (peakpath, true, 0664);
	if ((_peakfile_fd = _peakfile_descriptor->allocate()) < 0) {
		error << string_compose(_("AudioSource: cannot open peakpath (c) \"%1\" (%2)"), peakpath, strerror (errno)) << endmsg;
		return -1;
	}
	return 0;
}

void
AudioSource::done_with_peakfile_writes (bool done)
{
	if (peak_leftover_cnt) {
		compute_and_write_peaks (0, 0, 0, true, false, _FPP);
	}

	if (done) {
		Glib::Threads::Mutex::Lock lm (_peaks_ready_lock);
		_peaks_built = true;
		PeaksReady (); /* EMIT SIGNAL */
	}

	delete _peakfile_descriptor;
	_peakfile_descriptor = 0;
}

/** @param first_frame Offset from the source start of the first frame to process */
int
AudioSource::compute_and_write_peaks (Sample* buf, framecnt_t first_frame, framecnt_t cnt,
				      bool force, bool intermediate_peaks_ready)
{
	return compute_and_write_peaks (buf, first_frame, cnt, force, intermediate_peaks_ready, _FPP);
}

int
AudioSource::compute_and_write_peaks (Sample* buf, framecnt_t first_frame, framecnt_t cnt,
				      bool force, bool intermediate_peaks_ready, framecnt_t fpp)
{
	Sample* buf2 = 0;
	framecnt_t to_do;
	uint32_t  peaks_computed;
	PeakData* peakbuf = 0;
	int ret = -1;
	framepos_t current_frame;
	framecnt_t frames_done;
	const size_t blocksize = (128 * 1024);
	off_t first_peak_byte;

	if (_peakfile_descriptor == 0) {
		prepare_for_peakfile_writes ();
	}

  restart:
	if (peak_leftover_cnt) {

		if (first_frame != peak_leftover_frame + peak_leftover_cnt) {

			/* uh-oh, ::seek() since the last ::compute_and_write_peaks(),
			   and we have leftovers. flush a single peak (since the leftovers
			   never represent more than that, and restart.
			*/

			PeakData x;

			x.min = peak_leftovers[0];
			x.max = peak_leftovers[0];

			off_t byte = (peak_leftover_frame / fpp) * sizeof (PeakData);

			if (::pwrite (_peakfile_fd, &x, sizeof (PeakData), byte) != sizeof (PeakData)) {
				error << string_compose(_("%1: could not write peak file data (%2)"), _name, strerror (errno)) << endmsg;
				goto out;
			}

			_peak_byte_max = max (_peak_byte_max, (off_t) (byte + sizeof(PeakData)));

			{
				Glib::Threads::Mutex::Lock lm (_peaks_ready_lock);
				PeakRangeReady (peak_leftover_frame, peak_leftover_cnt); /* EMIT SIGNAL */
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
		buf2 = new Sample[to_do];

		/* the remnants */
		memcpy (buf2, peak_leftovers, peak_leftover_cnt * sizeof (Sample));

		/* the new stuff */
		memcpy (buf2+peak_leftover_cnt, buf, cnt * sizeof (Sample));

		/* no more leftovers */
		peak_leftover_cnt = 0;

		/* use the temporary buffer */
		buf = buf2;

		/* make sure that when we write into the peakfile, we startup where we left off */

		first_frame = peak_leftover_frame;

	} else {
		to_do = cnt;
	}

	peakbuf = new PeakData[(to_do/fpp)+1];
	peaks_computed = 0;
	current_frame = first_frame;
	frames_done = 0;

	while (to_do) {

		/* if some frames were passed in (i.e. we're not flushing leftovers)
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
			peak_leftover_frame = current_frame;

			/* done for now */

			break;
		}

		framecnt_t this_time = min (fpp, to_do);

		peakbuf[peaks_computed].max = buf[0];
		peakbuf[peaks_computed].min = buf[0];

		ARDOUR::find_peaks (buf+1, this_time-1, &peakbuf[peaks_computed].min, &peakbuf[peaks_computed].max);

		peaks_computed++;
		buf += this_time;
		to_do -= this_time;
		frames_done += this_time;
		current_frame += this_time;
	}

	first_peak_byte = (first_frame / fpp) * sizeof (PeakData);

	if (can_truncate_peaks()) {

		/* on some filesystems (ext3, at least) this helps to reduce fragmentation of
		   the peakfiles. its not guaranteed to do so, and even on ext3 (as of december 2006)
		   it does not cause single-extent allocation even for peakfiles of
		   less than BLOCKSIZE bytes.  only call ftruncate if we'll make the file larger.
		*/

		off_t endpos = lseek (_peakfile_fd, 0, SEEK_END);
		off_t target_length = blocksize * ((first_peak_byte + blocksize + 1) / blocksize);

		if (endpos < target_length) {
			if (ftruncate (_peakfile_fd, target_length)) {
				/* error doesn't actually matter so continue on without testing */
			}
		}
	}

	if (::pwrite (_peakfile_fd, peakbuf, sizeof (PeakData) * peaks_computed, first_peak_byte) != (ssize_t) (sizeof (PeakData) * peaks_computed)) {
		error << string_compose(_("%1: could not write peak file data (%2)"), _name, strerror (errno)) << endmsg;
		goto out;
	}

	_peak_byte_max = max (_peak_byte_max, (off_t) (first_peak_byte + sizeof(PeakData)*peaks_computed));

	if (frames_done) {
		Glib::Threads::Mutex::Lock lm (_peaks_ready_lock);
		PeakRangeReady (first_frame, frames_done); /* EMIT SIGNAL */
		if (intermediate_peaks_ready) {
			PeaksReady (); /* EMIT SIGNAL */
		}
	}

	ret = 0;

  out:
	delete [] peakbuf;
	delete [] buf2;

	return ret;
}

void
AudioSource::truncate_peakfile ()
{
	if (_peakfile_descriptor == 0) {
		error << string_compose (_("programming error: %1"), "AudioSource::truncate_peakfile() called without open peakfile descriptor")
		      << endmsg;
		return;
	}

	/* truncate the peakfile down to its natural length if necessary */

	off_t end = lseek (_peakfile_fd, 0, SEEK_END);

	if (end > _peak_byte_max) {
		if (ftruncate (_peakfile_fd, _peak_byte_max)) {
			error << string_compose (_("could not truncate peakfile %1 to %2 (error: %3)"),
						 peakpath, _peak_byte_max, errno) << endmsg;
		}
	}
}

framecnt_t
AudioSource::available_peaks (double zoom_factor) const
{
	if (zoom_factor < _FPP) {
		return length(_timeline_position); // peak data will come from the audio file
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
AudioSource::mark_streaming_write_completed ()
{
	Glib::Threads::Mutex::Lock lm (_peaks_ready_lock);

	if (_peaks_built) {
		PeaksReady (); /* EMIT SIGNAL */
	}
}

void
AudioSource::allocate_working_buffers (framecnt_t framerate)
{
	Glib::Threads::Mutex::Lock lm (_level_buffer_lock);


	/* Note: we don't need any buffers allocated until
	   a level 1 audiosource is created, at which
	   time we'll call ::ensure_buffers_for_level()
	   with the right value and do the right thing.
	*/

	if (!_mixdown_buffers.empty()) {
		ensure_buffers_for_level_locked ( _mixdown_buffers.size(), framerate);
	}
}

void
AudioSource::ensure_buffers_for_level (uint32_t level, framecnt_t frame_rate)
{
	Glib::Threads::Mutex::Lock lm (_level_buffer_lock);
	ensure_buffers_for_level_locked (level, frame_rate);
}

void
AudioSource::ensure_buffers_for_level_locked (uint32_t level, framecnt_t frame_rate)
{
	framecnt_t nframes = (framecnt_t) floor (Config->get_audio_playback_buffer_seconds() * frame_rate);

	/* this may be called because either "level" or "frame_rate" have
	 * changed. and it may be called with "level" smaller than the current
	 * number of buffers, because a new compound region has been created at
	 * a more shallow level than the deepest one we currently have.
	 */

	uint32_t limit = max ((size_t) level, _mixdown_buffers.size());

	_mixdown_buffers.clear ();
	_gain_buffers.clear ();

	for (uint32_t n = 0; n < limit; ++n) {
		_mixdown_buffers.push_back (boost::shared_array<Sample> (new Sample[nframes]));
		_gain_buffers.push_back (boost::shared_array<gain_t> (new gain_t[nframes]));
	}
}
