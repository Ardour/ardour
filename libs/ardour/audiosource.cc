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
#include <algorithm>
#include <vector>

#include <pbd/xml++.h>
#include <pbd/pthread_utils.h>

#include <ardour/audiosource.h>
#include <ardour/cycle_timer.h>
#include <ardour/session.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

bool AudioSource::_build_missing_peakfiles = false;
bool AudioSource::_build_peakfiles = false;

AudioSource::AudioSource (Session& s, ustring name)
	: Source (s, name)
{
	_peaks_built = false;
	_peak_byte_max = 0;
	peakfile = -1;
	_read_data_count = 0;
	_write_data_count = 0;
	peak_leftover_cnt = 0;
	peak_leftover_size = 0;
	peak_leftovers = 0;
}

AudioSource::AudioSource (Session& s, const XMLNode& node) 
	: Source (s, node)
{
	_peaks_built = false;
	_peak_byte_max = 0;
	peakfile = -1;
	_read_data_count = 0;
	_write_data_count = 0;
	peak_leftover_cnt = 0;
	peak_leftover_size = 0;
	peak_leftovers = 0;

	if (set_state (node)) {
		throw failed_constructor();
	}
}

AudioSource::~AudioSource ()
{
	/* shouldn't happen but make sure we don't leak file descriptors anyway */
	
	if (peak_leftover_cnt) {
		cerr << "AudioSource destroyed with leftover peak data pending" << endl;
	}

	if (peakfile >= 0) {
		::close (peakfile);
	}

	if (peak_leftovers) {
		delete [] peak_leftovers;
	}
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
AudioSource::set_state (const XMLNode& node)
{
	const XMLProperty* prop;

	Source::set_state (node);

	if ((prop = node.property ("captured-for")) != 0) {
		_captured_for = prop->value();
	}

	return 0;
}

/***********************************************************************
  PEAK FILE STUFF
 ***********************************************************************/

bool
AudioSource::peaks_ready (sigc::slot<void> the_slot, sigc::connection& conn) const
{
	bool ret;
	Glib::Mutex::Lock lm (_lock);

	/* check to see if the peak data is ready. if not
	   connect the slot while still holding the lock.
	*/

	if (!(ret = _peaks_built)) {
		conn = PeaksReady.connect (the_slot);
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
AudioSource::rename_peakfile (ustring newpath)
{
	/* caller must hold _lock */

	ustring oldpath = peakpath;

	if (access (oldpath.c_str(), F_OK) == 0) {
		if (rename (oldpath.c_str(), newpath.c_str()) != 0) {
			error << string_compose (_("cannot rename peakfile for %1 from %2 to %3 (%4)"), _name, oldpath, newpath, strerror (errno)) << endmsg;
			return -1;
		}
	}

	peakpath = newpath;

	return 0;
}

int
AudioSource::initialize_peakfile (bool newfile, ustring audio_path)
{
	struct stat statbuf;

	peakpath = peak_path (audio_path);

	/* Nasty band-aid for older sessions that were created before we
	   used libsndfile for all audio files.
	*/
	
	if (!newfile && access (peakpath.c_str(), R_OK) != 0) {
		ustring str = old_peak_path (audio_path);
		if (access (str.c_str(), R_OK) == 0) {
			peakpath = str;
		}
	}

	if (newfile) {

		if (!_build_peakfiles) {
			return 0;
		}

		_peaks_built = false;

	} else {

		if (stat (peakpath.c_str(), &statbuf)) {
			if (errno != ENOENT) {
				/* it exists in the peaks dir, but there is some kind of error */
				
				error << string_compose(_("AudioSource: cannot stat peakfile \"%1\""), peakpath) << endmsg;
				return -1;
			}
			
			_peaks_built = false;

		} else {
			
			/* we found it in the peaks dir, so check it out */

			if (statbuf.st_size == 0) {
				_peaks_built = false;
			} else {
				// Check if the audio file has changed since the peakfile was built.
				struct stat stat_file;
				int err = stat (audio_path.c_str(), &stat_file);
				
				if (!err && stat_file.st_mtime > statbuf.st_mtime){
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

nframes_t
AudioSource::read (Sample *dst, nframes_t start, nframes_t cnt) const
{
	Glib::Mutex::Lock lm (_lock);
	return read_unlocked (dst, start, cnt);
}

nframes_t
AudioSource::write (Sample *dst, nframes_t cnt)
{
	Glib::Mutex::Lock lm (_lock);
	return write_unlocked (dst, cnt);
}

int 
AudioSource::read_peaks (PeakData *peaks, nframes_t npeaks, nframes_t start, nframes_t cnt, double samples_per_visual_peak) const
{
	Glib::Mutex::Lock lm (_lock);
	double scale;
	double expected_peaks;
	PeakData::PeakDatum xmax;
	PeakData::PeakDatum xmin;
	int32_t to_read;
	uint32_t nread;
	nframes_t zero_fill = 0;
	int ret = -1;
	PeakData* staging = 0;
	Sample* raw_staging = 0;
	int _peakfile = -1;

	expected_peaks = (cnt / (double) frames_per_peak);
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
		nframes_t old = npeaks;
		npeaks = min ((nframes_t) floor (cnt / samples_per_visual_peak), npeaks);
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

		for (nframes_t i = 0; i < npeaks; ++i) {
			peaks[i].max = raw_staging[i];
			peaks[i].min = raw_staging[i];
		}

		delete [] raw_staging;
		return 0;
	}

	if (scale == 1.0) {

		off_t first_peak_byte = (start / frames_per_peak) * sizeof (PeakData);

		/* open, read, close */

		if ((_peakfile = ::open (peakpath.c_str(), O_RDONLY, 0664)) < 0) {
			error << string_compose(_("AudioSource: cannot open peakpath \"%1\" (%2)"), peakpath, strerror (errno)) << endmsg;
			return -1;
		}

#ifdef DEBUG_READ_PEAKS
		cerr << "DIRECT PEAKS\n";
#endif
		
		nread = ::pread (_peakfile, peaks, sizeof (PeakData)* npeaks, first_peak_byte);
		close (_peakfile);

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
			return -1;
		}

		if (zero_fill) {
			memset (&peaks[npeaks], 0, sizeof (PeakData) * zero_fill);
		}

		return 0;
	}


	nframes_t tnp;

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

		const uint32_t chunksize = (uint32_t) min (expected_peaks, 65536.0);
		
		staging = new PeakData[chunksize];
		
		/* compute the rounded up frame position  */
	
		nframes_t current_frame = start;
		nframes_t current_stored_peak = (nframes_t) ceil (current_frame / (double) frames_per_peak);
		uint32_t       next_visual_peak  = (uint32_t) ceil (current_frame / samples_per_visual_peak);
		double         next_visual_peak_frame = next_visual_peak * samples_per_visual_peak;
		uint32_t       stored_peak_before_next_visual_peak = (nframes_t) next_visual_peak_frame / frames_per_peak;
		uint32_t       nvisual_peaks = 0;
		uint32_t       stored_peaks_read = 0;
		uint32_t       i = 0;

		/* handle the case where the initial visual peak is on a pixel boundary */

		current_stored_peak = min (current_stored_peak, stored_peak_before_next_visual_peak);

		/* open ... close during out: handling */

		if ((_peakfile = ::open (peakpath.c_str(), O_RDONLY, 0664)) < 0) {
			error << string_compose(_("AudioSource: cannot open peakpath \"%1\" (%2)"), peakpath, strerror (errno)) << endmsg;
			return 0;
		}

		while (nvisual_peaks < npeaks) {

			if (i == stored_peaks_read) {

				uint32_t       start_byte = current_stored_peak * sizeof(PeakData);
				tnp = min ((_length/frames_per_peak - current_stored_peak), (nframes_t) expected_peaks);
				to_read = min (chunksize, tnp);
				
#ifdef DEBUG_READ_PEAKS
				cerr << "read " << sizeof (PeakData) * to_read << " from peakfile @ " << start_byte << endl;
#endif
				
				if ((nread = ::pread (_peakfile, staging, sizeof (PeakData) * to_read, start_byte))
				    != sizeof (PeakData) * to_read) {

					off_t fend = lseek (_peakfile, 0, SEEK_END);
					
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
					     << " expected maxpeaks = " << (_length - current_frame)/frames_per_peak
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
			stored_peak_before_next_visual_peak = (uint32_t) next_visual_peak_frame / frames_per_peak; 
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

		nframes_t frames_read = 0;
		nframes_t current_frame = start;
		nframes_t i = 0;
		nframes_t nvisual_peaks = 0;
		nframes_t chunksize = (nframes_t) min (cnt, (nframes_t) 4096);
		raw_staging = new Sample[chunksize];
		
		nframes_t frame_pos = start;
		double pixel_pos = floor (frame_pos / samples_per_visual_peak);
		double next_pixel_pos = ceil (frame_pos / samples_per_visual_peak);
		double pixels_per_frame = 1.0 / samples_per_visual_peak;

		xmin = 1.0;
		xmax = -1.0;

		while (nvisual_peaks < npeaks) {

			if (i == frames_read) {
				
				to_read = min (chunksize, (_length - current_frame));

				if (to_read == 0) {
					/* XXX ARGH .. out by one error ... need to figure out why this happens
					   and fix it rather than do this band-aid move.
					*/
					zero_fill = npeaks - nvisual_peaks;
					break;
				}

				if ((frames_read = read_unlocked (raw_staging, current_frame, to_read)) == 0) {
					error << string_compose(_("AudioSource[%1]: peak read - cannot read %2 samples at offset %3 of %4 (%5)"),
								_name, to_read, current_frame, _length, strerror (errno)) 
					      << endmsg;
					goto out;
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
	if (_peakfile >= 0) {
		close (_peakfile);
	}

	if (staging) {
		delete [] staging;
	} 

	if (raw_staging) {
		delete [] raw_staging;
	}

#ifdef DEBUG_READ_PEAKS
	cerr << "RP DONE\n";
#endif

	return ret;
}

#undef DEBUG_PEAK_BUILD

int
AudioSource::build_peaks_from_scratch ()
{
	nframes_t current_frame;
	nframes_t cnt;
	Sample buf[frames_per_peak];
	nframes_t frames_read;
	nframes_t frames_to_read;
	int ret = -1;

	{
		/* hold lock while building peaks */

		Glib::Mutex::Lock lp (_lock);
		
		if (prepare_for_peakfile_writes ()) {
			goto out;
		}
		
		current_frame = 0;
		cnt = _length;
		_peaks_built = false;
		
		while (cnt) {
			
			frames_to_read = min (frames_per_peak, cnt);

			if ((frames_read = read_unlocked (buf, current_frame, frames_to_read)) != frames_to_read) {
				error << string_compose(_("%1: could not write read raw data for peak computation (%2)"), _name, strerror (errno)) << endmsg;
				done_with_peakfile_writes ();
				goto out;
			}

			if (compute_and_write_peaks (buf, current_frame, frames_read, true)) {
				break;
			}
			
			current_frame += frames_read;
			cnt -= frames_read;
		}
		
		if (cnt == 0) {
			/* success */
			truncate_peakfile();
			_peaks_built = true;
		} 

		done_with_peakfile_writes ();
	}

	/* lock no longer held, safe to signal */

	if (_peaks_built) {
		PeaksReady (); /* EMIT SIGNAL */
		ret = 0;
	}

  out:
	if (ret) {
		unlink (peakpath.c_str());
	}

	return ret;
}

int
AudioSource::prepare_for_peakfile_writes ()
{
	if ((peakfile = ::open (peakpath.c_str(), O_RDWR|O_CREAT, 0664)) < 0) {
		error << string_compose(_("AudioSource: cannot open peakpath \"%1\" (%2)"), peakpath, strerror (errno)) << endmsg;
		return -1;
	}
	return 0;
}

void
AudioSource::done_with_peakfile_writes ()
{
	if (peak_leftover_cnt) {
		compute_and_write_peaks (0, 0, 0, true);
	}

	if (peakfile >= 0) {
		close (peakfile);
		peakfile = -1;
	}
}

int
AudioSource::compute_and_write_peaks (Sample* buf, nframes_t first_frame, nframes_t cnt, bool force)
{
	Sample* buf2 = 0;
	nframes_t to_do;
	uint32_t  peaks_computed;
	PeakData* peakbuf;
	int ret = -1;
	nframes_t current_frame;
	nframes_t frames_done;
	const size_t blocksize = (128 * 1024);
	off_t first_peak_byte;

	if (peakfile < 0) {
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
			Session::find_peaks (peak_leftovers + 1, peak_leftover_cnt - 1, &x.min, &x.max);

			off_t byte = (peak_leftover_frame / frames_per_peak) * sizeof (PeakData);

			if (::pwrite (peakfile, &x, sizeof (PeakData), byte) != sizeof (PeakData)) {
				error << string_compose(_("%1: could not write peak file data (%2)"), _name, strerror (errno)) << endmsg;
				goto out;
			}

			_peak_byte_max = max (_peak_byte_max, (off_t) (byte + sizeof(PeakData)));

			PeakRangeReady (peak_leftover_frame, peak_leftover_cnt); /* EMIT SIGNAL */
			PeaksReady (); /* EMIT SIGNAL */

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

	peakbuf = new PeakData[(to_do/frames_per_peak)+1];
	peaks_computed = 0;
	current_frame = first_frame;
	frames_done = 0;

	while (to_do) {

		/* if some frames were passed in (i.e. we're not flushing leftovers)
		   and there are less than frames_per_peak to do, save them till
		   next time
		*/

		if (force && (to_do < frames_per_peak)) {
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
			
		nframes_t this_time = min (frames_per_peak, to_do);

		peakbuf[peaks_computed].max = buf[0];
		peakbuf[peaks_computed].min = buf[0];

		Session::find_peaks (buf+1, this_time-1, &peakbuf[peaks_computed].min, &peakbuf[peaks_computed].max);

		peaks_computed++;
		buf += this_time;
		to_do -= this_time;
		frames_done += this_time;
		current_frame += this_time;
	}
		
	first_peak_byte = (first_frame / frames_per_peak) * sizeof (PeakData);

	if (can_truncate_peaks()) {

		/* on some filesystems (ext3, at least) this helps to reduce fragmentation of
		   the peakfiles. its not guaranteed to do so, and even on ext3 (as of december 2006)
		   it does not cause single-extent allocation even for peakfiles of 
		   less than BLOCKSIZE bytes.  only call ftruncate if we'll make the file larger.
		*/
		
		off_t endpos = lseek (peakfile, 0, SEEK_END);
		off_t target_length = blocksize * ((first_peak_byte + blocksize + 1) / blocksize);
		
		if (endpos < target_length) {
			ftruncate (peakfile, target_length);
			/* error doesn't actually matter though, so continue on without testing */
		}
	}

	if (::pwrite (peakfile, peakbuf, sizeof (PeakData) * peaks_computed, first_peak_byte) != (ssize_t) (sizeof (PeakData) * peaks_computed)) {
		error << string_compose(_("%1: could not write peak file data (%2)"), _name, strerror (errno)) << endmsg;
		goto out;
	}

	_peak_byte_max = max (_peak_byte_max, (off_t) (first_peak_byte + sizeof(PeakData)*peaks_computed));	

	if (frames_done) {
		PeakRangeReady (first_frame, frames_done); /* EMIT SIGNAL */
		PeaksReady (); /* EMIT SIGNAL */
	}

	ret = 0;

  out:
	delete [] peakbuf;
	if (buf2) {
		delete [] buf2;
	}
	return ret;
}

void
AudioSource::truncate_peakfile ()
{
	if (peakfile < 0) {
		error << string_compose (_("programming error: %1"), "AudioSource::truncate_peakfile() called without open peakfile descriptor")
		      << endmsg;
		return;
	}

	/* truncate the peakfile down to its natural length if necessary */

	off_t end = lseek (peakfile, 0, SEEK_END);
	
	if (end > _peak_byte_max) {
		ftruncate (peakfile, _peak_byte_max);
	}
}

bool
AudioSource::file_changed (ustring path)
{
	struct stat stat_file;
	struct stat stat_peak;

	int e1 = stat (path.c_str(), &stat_file);
	int e2 = stat (peak_path(path).c_str(), &stat_peak);
	
	if (!e1 && !e2 && stat_file.st_mtime > stat_peak.st_mtime){
		return true;
	} else {
		return false;
	}
}

nframes_t
AudioSource::available_peaks (double zoom_factor) const
{
	off_t end;

	if (zoom_factor < frames_per_peak) {
		return length(); // peak data will come from the audio file
	} 
	
	/* peak data comes from peakfile, but the filesize might not represent
	   the valid data due to ftruncate optimizations, so use _peak_byte_max state.
	   XXX - there might be some atomicity issues here, we should probably add a lock,
	   but _peak_byte_max only monotonically increases after initialization.
	*/

	end = _peak_byte_max;

	return (end/sizeof(PeakData)) * frames_per_peak;
}

void
AudioSource::update_length (nframes_t pos, nframes_t cnt)
{
	if (pos + cnt > _length) {
		_length = pos+cnt;
	}
}

