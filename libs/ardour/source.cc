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

    $Id$
*/

#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <float.h>
#include <cerrno>
#include <ctime>
#include <cmath>
#include <iomanip>
#include <algorithm>

#include <pbd/lockmonitor.h>
#include <pbd/xml++.h>
#include <pbd/pthread_utils.h>

#include <ardour/source.h>

#include "i18n.h"

using std::min;
using std::max;

using namespace ARDOUR;
using namespace PBD;

sigc::signal<void,Source *> Source::SourceCreated;
pthread_t                    Source::peak_thread;
bool                         Source::have_peak_thread = false;
vector<Source*>              Source::pending_peak_sources;
PBD::Lock                    Source::pending_peak_sources_lock;
int                          Source::peak_request_pipe[2];

bool Source::_build_missing_peakfiles = false;
bool Source::_build_peakfiles = false;

Source::Source (bool announce)
{
	_id = ARDOUR::new_id();
	_use_cnt = 0;
	_peaks_built = false;
	next_peak_clear_should_notify = true;
	peakfile = -1;
	_timestamp = 0;
	_read_data_count = 0;
	_write_data_count = 0;
}

Source::Source (const XMLNode& node) 
{
	_use_cnt = 0;
	_peaks_built = false;
	next_peak_clear_should_notify = true;
	peakfile = -1;
	_timestamp = 0;
	_read_data_count = 0;
	_write_data_count = 0;

	if (set_state (node)) {
		throw failed_constructor();
	}
}

Source::~Source ()
{
	if (peakfile >= 0) {
		close (peakfile);
	}
}

XMLNode&
Source::get_state ()
{
	XMLNode *node = new XMLNode ("Source");
	char buf[64];

	node->add_property ("name", _name);
	snprintf (buf, sizeof(buf)-1, "%" PRIu64, _id);
	node->add_property ("id", buf);

	if (_timestamp != 0) {
		snprintf (buf, sizeof (buf), "%ld", _timestamp);
		node->add_property ("timestamp", buf);
	}

	if (_captured_for.length()) {
		node->add_property ("captured-for", _captured_for);
	}

	return *node;
}

int
Source::set_state (const XMLNode& node)
{
	const XMLProperty* prop;

	if ((prop = node.property ("name")) != 0) {
		_name = prop->value();
	} else {
		return -1;
	}
	
	if ((prop = node.property ("id")) != 0) {
		sscanf (prop->value().c_str(), "%" PRIu64, &_id);
	} else {
		return -1;
	}

	if ((prop = node.property ("timestamp")) != 0) {
		sscanf (prop->value().c_str(), "%ld", &_timestamp);
	}

	if ((prop = node.property ("captured-for")) != 0) {
		_captured_for = prop->value();
	}

	return 0;
}

/***********************************************************************
  PEAK FILE STUFF
 ***********************************************************************/

void*
Source::peak_thread_work (void* arg)
{
	PBD::ThreadCreated (pthread_self(), X_("Peak"));
	struct pollfd pfd[1];

	LockMonitor lm (pending_peak_sources_lock, __LINE__, __FILE__);

	while (true) {

		pfd[0].fd = peak_request_pipe[0];
		pfd[0].events = POLLIN|POLLERR|POLLHUP;

		pthread_mutex_unlock (pending_peak_sources_lock.mutex());

		if (poll (pfd, 1, -1) < 0) {

			if (errno == EINTR) {
				pthread_mutex_lock (pending_peak_sources_lock.mutex());
				continue;
			}
			
			error << string_compose (_("poll on peak request pipe failed (%1)"),
					  strerror (errno))
			      << endmsg;
			break;
		}

		if (pfd[0].revents & ~POLLIN) {
			error << _("Error on peak thread request pipe") << endmsg;
			break;
		}

		if (pfd[0].revents & POLLIN) {

			char req;
			
			/* empty the pipe of all current requests */

			while (1) {
				size_t nread = ::read (peak_request_pipe[0], &req, sizeof (req));

				if (nread == 1) {
					switch ((PeakRequest::Type) req) {
					
					case PeakRequest::Build:
						break;
						
					case PeakRequest::Quit:
						pthread_exit_pbd (0);
						/*NOTREACHED*/
						break;
						
					default:
						break;
					}

				} else if (nread == 0) {
					break;
				} else if (errno == EAGAIN) {
					break;
				} else {
					fatal << _("Error reading from peak request pipe") << endmsg;
					/*NOTREACHED*/
				}
			}
		}

		pthread_mutex_lock (pending_peak_sources_lock.mutex());

		while (!pending_peak_sources.empty()) {

			Source* s = pending_peak_sources.front();
			pending_peak_sources.erase (pending_peak_sources.begin());

			pthread_mutex_unlock (pending_peak_sources_lock.mutex());
			s->build_peaks();
			pthread_mutex_lock (pending_peak_sources_lock.mutex());
		}
	}

	pthread_exit_pbd (0);
	/*NOTREACHED*/
	return 0;
}

int
Source::start_peak_thread ()
{
	if (!_build_peakfiles) {
		return 0;
	}

	if (pipe (peak_request_pipe)) {
		error << string_compose(_("Cannot create transport request signal pipe (%1)"), strerror (errno)) << endmsg;
		return -1;
	}

	if (fcntl (peak_request_pipe[0], F_SETFL, O_NONBLOCK)) {
		error << string_compose(_("UI: cannot set O_NONBLOCK on peak request pipe (%1)"), strerror (errno)) << endmsg;
		return -1;
	}

	if (fcntl (peak_request_pipe[1], F_SETFL, O_NONBLOCK)) {
		error << string_compose(_("UI: cannot set O_NONBLOCK on peak request pipe (%1)"), strerror (errno)) << endmsg;
		return -1;
	}

	if (pthread_create_and_store ("peak file builder", &peak_thread, 0, peak_thread_work, 0)) {
		error << _("Source: could not create peak thread") << endmsg;
		return -1;
	}

	have_peak_thread = true;
	return 0;
}

void
Source::stop_peak_thread ()
{
	if (!have_peak_thread) {
		return;
	}

	void* status;

	char c = (char) PeakRequest::Quit;
	::write (peak_request_pipe[1], &c, 1);
	pthread_join (peak_thread, &status);
}

void 
Source::queue_for_peaks (Source& source)
{
	if (have_peak_thread) {

		LockMonitor lm (pending_peak_sources_lock, __LINE__, __FILE__);

		source.next_peak_clear_should_notify = true;

		if (find (pending_peak_sources.begin(),
			  pending_peak_sources.end(),
			  &source) == pending_peak_sources.end()) {
			pending_peak_sources.push_back (&source);
		}

		char c = (char) PeakRequest::Build;
		::write (peak_request_pipe[1], &c, 1);
	}
}

void Source::clear_queue_for_peaks ()
{
	/* this is done to cancel a group of running peak builds */
	if (have_peak_thread) {
		LockMonitor lm (pending_peak_sources_lock, __LINE__, __FILE__);
		pending_peak_sources.clear ();
	}
}


bool
Source::peaks_ready (sigc::slot<void> the_slot, sigc::connection& conn) const
{
	bool ret;
	LockMonitor lm (_lock, __LINE__, __FILE__);

	/* check to see if the peak data is ready. if not
	   connect the slot while still holding the lock.
	*/

	if (!(ret = _peaks_built)) {
		conn = PeaksReady.connect (the_slot);
	}

	return ret;
}

int
Source::initialize_peakfile (bool newfile, string audio_path)
{
	struct stat statbuf;

	peakpath = peak_path (audio_path);

	if (newfile) {

		if (!_build_peakfiles) {
			return 0;
		}

		_peaks_built = false;

	} else {

		if (stat (peakpath.c_str(), &statbuf)) {
			if (errno != ENOENT) {
				/* it exists in the peaks dir, but there is some kind of error */
				
				error << string_compose(_("Source: cannot stat peakfile \"%1\""), peakpath) << endmsg;
				return -1;
			}

			/* older sessions stored peaks in the same directory
			   as the audio. so check there as well.
			*/
			
			string oldpeakpath = old_peak_path (audio_path);
			
			if (stat (oldpeakpath.c_str(), &statbuf)) {
				
				if (errno == ENOENT) {

					statbuf.st_size = 0;

				} else {
					
					/* it exists in the audio dir , but there is some kind of error */
					
					error << string_compose(_("Source: cannot stat peakfile \"%1\" or \"%2\""), peakpath, oldpeakpath) << endmsg;
					return -1;
				}
				
			} else {

				/* we found it in the sound dir, where they lived once upon a time, in a land ... etc. */

				peakpath = oldpeakpath;
			}

		} else {
			
			/* we found it in the peaks dir */
		}
		
		if (statbuf.st_size == 0) {
			_peaks_built = false;
		} else {
			// Check if the audio file has changed since the peakfile was built.
			struct stat stat_file;
			int err = stat (audio_path.c_str(), &stat_file);
			
			if (!err && stat_file.st_mtime > statbuf.st_mtime){
				_peaks_built = false;
			} else {
				_peaks_built = true;
			}
		}
	}

	if ((peakfile = ::open (peakpath.c_str(), O_RDWR|O_CREAT, 0664)) < 0) {
		error << string_compose(_("Source: cannot open peakpath \"%1\" (%2)"), peakpath, strerror (errno)) << endmsg;
		return -1;
	}

	if (!newfile && !_peaks_built && _build_missing_peakfiles && _build_peakfiles) {
		build_peaks_from_scratch ();
	} 

	return 0;
}

int 
Source::read_peaks (PeakData *peaks, jack_nframes_t npeaks, jack_nframes_t start, jack_nframes_t cnt, double samples_per_visual_peak) const
{
	LockMonitor lm (_lock, __LINE__, __FILE__);
	double scale;
	double expected_peaks;
	PeakData::PeakDatum xmax;
	PeakData::PeakDatum xmin;
	int32_t to_read;
	uint32_t nread;
	jack_nframes_t zero_fill = 0;
	int ret = -1;
	PeakData* staging = 0;
	Sample* raw_staging = 0;
	char * workbuf = 0;
	
	expected_peaks = (cnt / (double) frames_per_peak);
	scale = npeaks/expected_peaks;

#if 0
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
		jack_nframes_t old = npeaks;
		npeaks = min ((jack_nframes_t) floor (cnt / samples_per_visual_peak), npeaks);
		zero_fill = old - npeaks;
	}

	// cerr << "actual npeaks = " << npeaks << " zf = " << zero_fill << endl;
	
	if (npeaks == cnt) {

		// cerr << "RAW DATA\n";
		
		/* no scaling at all, just get the sample data and duplicate it for
		   both max and min peak values.
		*/

		Sample* raw_staging = new Sample[cnt];
		workbuf = new char[cnt*4];
		
		if (read_unlocked (raw_staging, start, cnt, workbuf) != cnt) {
			error << _("cannot read sample data for unscaled peak computation") << endmsg;
			return -1;
		}

		for (jack_nframes_t i = 0; i < npeaks; ++i) {
			peaks[i].max = raw_staging[i];
			peaks[i].min = raw_staging[i];
		}

		delete [] raw_staging;
		delete [] workbuf;
		return 0;
	}

	if (scale == 1.0) {

		// cerr << "DIRECT PEAKS\n";
		
		off_t first_peak_byte = (start / frames_per_peak) * sizeof (PeakData);

		if ((nread = ::pread (peakfile, peaks, sizeof (PeakData)* npeaks, first_peak_byte)) != sizeof (PeakData) * npeaks) {
			cerr << "Source["
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


	jack_nframes_t tnp;

	if (scale < 1.0) {

		// cerr << "DOWNSAMPLE\n";

		/* the caller wants:

		    - more frames-per-peak (lower resolution) than the peakfile, or to put it another way,
                    - less peaks than the peakfile holds for the same range

		    So, read a block into a staging area, and then downsample from there.

		    to avoid confusion, I'll refer to the requested peaks as visual_peaks and the peakfile peaks as stored_peaks  
		*/

		const uint32_t chunksize = (uint32_t) min (expected_peaks, 4096.0);
		
		staging = new PeakData[chunksize];
		
		/* compute the rounded up frame position  */
	
		jack_nframes_t current_frame = start;
		jack_nframes_t current_stored_peak = (jack_nframes_t) ceil (current_frame / (double) frames_per_peak);
		uint32_t       next_visual_peak  = (uint32_t) ceil (current_frame / samples_per_visual_peak);
		double         next_visual_peak_frame = next_visual_peak * samples_per_visual_peak;
		uint32_t       stored_peak_before_next_visual_peak = (jack_nframes_t) next_visual_peak_frame / frames_per_peak;
		uint32_t       nvisual_peaks = 0;
		uint32_t       stored_peaks_read = 0;
		uint32_t       i = 0;

		/* handle the case where the initial visual peak is on a pixel boundary */

		current_stored_peak = min (current_stored_peak, stored_peak_before_next_visual_peak);

		while (nvisual_peaks < npeaks) {

			if (i == stored_peaks_read) {

				uint32_t       start_byte = current_stored_peak * sizeof(PeakData);
				tnp = min ((_length/frames_per_peak - current_stored_peak), (jack_nframes_t) expected_peaks);
				to_read = min (chunksize, tnp);
				
				off_t fend = lseek (peakfile, 0, SEEK_END);
				
				if ((nread = ::pread (peakfile, staging, sizeof (PeakData) * to_read, start_byte))
				    != sizeof (PeakData) * to_read) {
					cerr << "Source["
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
		
		// cerr << "UPSAMPLE\n";

		/* the caller wants 

		     - less frames-per-peak (more resolution)
		     - more peaks than stored in the Peakfile

		   So, fetch data from the raw source, and generate peak
		   data on the fly.
		*/

		jack_nframes_t frames_read = 0;
		jack_nframes_t current_frame = start;
		jack_nframes_t i = 0;
		jack_nframes_t nvisual_peaks = 0;
		jack_nframes_t chunksize = (jack_nframes_t) min (cnt, (jack_nframes_t) 4096);
		raw_staging = new Sample[chunksize];
		workbuf = new char[chunksize *4];
		
		jack_nframes_t frame_pos = start;
		double pixel_pos = floor (frame_pos / samples_per_visual_peak);
		double next_pixel_pos = ceil (frame_pos / samples_per_visual_peak);
		double pixels_per_frame = 1.0 / samples_per_visual_peak;

		xmin = 1.0;
		xmax = -1.0;

		while (nvisual_peaks < npeaks) {

			if (i == frames_read) {
				
				to_read = min (chunksize, (_length - current_frame));
				
				if ((frames_read = read_unlocked (raw_staging, current_frame, to_read, workbuf)) < 0) {
					error << string_compose(_("Source[%1]: peak read - cannot read %2 samples at offset %3")
							 , _name, to_read, current_frame) 
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
	if (staging) {
		delete [] staging;
	} 

	if (raw_staging) {
		delete [] raw_staging;
	}

	if (workbuf) {
		delete [] workbuf;
	}
	
	return ret;
}

#undef DEBUG_PEAK_BUILD

int
Source::build_peaks ()
{
	vector<PeakBuildRecord*> built;
	int status = -1;
	bool pr_signal = false;
	list<PeakBuildRecord*> copy;

	{
		LockMonitor lm (_lock, __LINE__, __FILE__);
		copy = pending_peak_builds;
		pending_peak_builds.clear ();
	}
		
#ifdef DEBUG_PEAK_BUILD
	cerr << "build peaks with " << copy.size() << " requests pending\n";
#endif		

	for (list<PeakBuildRecord *>::iterator i = copy.begin(); i != copy.end(); ++i) {
		
		if ((status = do_build_peak ((*i)->frame, (*i)->cnt)) != 0) { 
			unlink (peakpath.c_str());
			break;
		}
		built.push_back (new PeakBuildRecord (*(*i)));
		delete *i;
	}

	{ 
		LockMonitor lm (_lock, __LINE__, __FILE__);

		if (status == 0) {
			_peaks_built = true;
			
			if (next_peak_clear_should_notify) {
				next_peak_clear_should_notify = false;
				pr_signal = true;
			}
		}
	}

	if (status == 0) {
		for (vector<PeakBuildRecord *>::iterator i = built.begin(); i != built.end(); ++i) {
			PeakRangeReady ((*i)->frame, (*i)->cnt); /* EMIT SIGNAL */
			delete *i;
		}

		if (pr_signal) {
			off_t fend = lseek (peakfile, 0, SEEK_END);
			PeaksReady (); /* EMIT SIGNAL */
		}
	}

	return status;
}

int
Source::do_build_peak (jack_nframes_t first_frame, jack_nframes_t cnt)
{
	jack_nframes_t current_frame;
	Sample buf[frames_per_peak];
	Sample xmin, xmax;
	uint32_t  peaki;
	PeakData* peakbuf;
	char * workbuf = 0;
	jack_nframes_t frames_read;
	jack_nframes_t frames_to_read;
	off_t first_peak_byte;
	int ret = -1;

#ifdef DEBUG_PEAK_BUILD
	cerr << pthread_self() << ": " << _name << ": building peaks for " << first_frame << " to " << first_frame + cnt - 1 << endl;
#endif

	first_peak_byte = (first_frame / frames_per_peak) * sizeof (PeakData);

#ifdef DEBUG_PEAK_BUILD
	cerr << "seeking to " << first_peak_byte << " before writing new peak data\n";
#endif

	current_frame = first_frame;
	peakbuf = new PeakData[(cnt/frames_per_peak)+1];
	peaki = 0;

	workbuf = new char[max(frames_per_peak, cnt) * 4];
	
	while (cnt) {

		frames_to_read = min (frames_per_peak, cnt);

		if ((frames_read = read_unlocked (buf, current_frame, frames_to_read, workbuf)) != frames_to_read) {
			error << string_compose(_("%1: could not write read raw data for peak computation (%2)"), _name, strerror (errno)) << endmsg;
			goto out;
		}

		xmin = buf[0];
		xmax = buf[0];

		for (jack_nframes_t n = 1; n < frames_read; ++n) {
			xmax = max (xmax, buf[n]);
			xmin = min (xmin, buf[n]);

//			if (current_frame < frames_read) {
//				cerr << "sample = " << buf[n] << " max = " << xmax << " min = " << xmin << " max of 2 = " << max (xmax, buf[n]) << endl;
//			}
		}

		peakbuf[peaki].max = xmax;
		peakbuf[peaki].min = xmin;
		peaki++;

		current_frame += frames_read;
		cnt -= frames_read;
	}

	if (::pwrite (peakfile, peakbuf, sizeof (PeakData) * peaki, first_peak_byte) != (ssize_t) (sizeof (PeakData) * peaki)) {
		error << string_compose(_("%1: could not write peak file data (%2)"), _name, strerror (errno)) << endmsg;
		goto out;
	}

	ret = 0;

  out:
	delete [] peakbuf;
	if (workbuf)
		delete [] workbuf;
	return ret;
}

void
Source::build_peaks_from_scratch ()
{
	LockMonitor lp (_lock, __LINE__, __FILE__); 

	next_peak_clear_should_notify = true;
	pending_peak_builds.push_back (new PeakBuildRecord (0, _length));
	queue_for_peaks (*this);
}

bool
Source::file_changed (string path)
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

void
Source::use ()
{
	_use_cnt++;
}

void
Source::release ()
{
	if (_use_cnt) --_use_cnt;
}

jack_nframes_t
Source::available_peaks (double zoom_factor) const
{
	if (zoom_factor < frames_per_peak) {
		return length(); // peak data will come from the audio file
	} 
	
	/* peak data comes from peakfile */

	LockMonitor lm (_lock, __LINE__, __FILE__);
	off_t end = lseek (peakfile, 0, SEEK_END);
	return (end/sizeof(PeakData)) * frames_per_peak;
}
