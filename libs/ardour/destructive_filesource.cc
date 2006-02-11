/*
    Copyright (C) 2006 Paul Davis 

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

/* This is is very hacky way to get pread and pwrite declarations.
   First, include <features.h> so that we can avoid its #undef __USE_UNIX98.
   Then define __USE_UNIX98, include <unistd.h>, and then undef it
   again. If #define _XOPEN_SOURCE actually worked, I'd use that, but
   despite claims in the header that it does, it doesn't.

   features.h isn't available on osx and it compiles fine without it.
*/

#ifdef HAVE_FEATURES_H
#include <features.h>
#endif

#if __GNUC__ >= 3
// #define _XOPEN_SOURCE 500
#include <unistd.h>
#else
#define __USE_UNIX98
#include <unistd.h>
#undef  __USE_UNIX98
#endif

// darwin supports 64 by default and doesn't provide wrapper functions.
#if defined (__APPLE__)
typedef off_t off64_t;
#define open64 open
#define close64 close
#define lseek64 lseek
#define pread64 pread
#define pwrite64 pwrite
#endif

#include <errno.h>
#include <cmath>
#include <fcntl.h>

#include <pbd/error.h>
#include <ardour/destructive_filesource.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;

gain_t* DestructiveFileSource::out_coefficient = 0;
gain_t* DestructiveFileSource::in_coefficient = 0;
jack_nframes_t DestructiveFileSource::xfade_frames = 64;

DestructiveFileSource::DestructiveFileSource (string path, jack_nframes_t rate, bool repair_first, SampleFormat samp_format)
	: FileSource (path, rate, repair_first, samp_format)
{
	if (out_coefficient == 0) {
		setup_standard_crossfades (rate);
	}

	xfade_buf = new Sample[xfade_frames];

	_capture_start = false;
	_capture_end = false;
}

DestructiveFileSource::DestructiveFileSource (const XMLNode& node, jack_nframes_t rate)
	: FileSource (node, rate)
{
	if (out_coefficient == 0) {
		setup_standard_crossfades (rate);
	}

	xfade_buf = new Sample[xfade_frames];

	_capture_start = false;
	_capture_end = false;
}

DestructiveFileSource::~DestructiveFileSource()
{
	delete xfade_buf;
}

void
DestructiveFileSource::setup_standard_crossfades (jack_nframes_t rate)
{
	xfade_frames = (jack_nframes_t) floor ((/*Config->get_destructive_crossfade_msecs()*/ 64 / 1000.0) * rate);

	out_coefficient = new gain_t[xfade_frames];
	in_coefficient = new gain_t[xfade_frames];

	for (jack_nframes_t n = 0; n < xfade_frames; ++n) {

		/* XXXX THIS IS NOT THE RIGHT XFADE CURVE: USE A PROPER VOLUMETRIC EQUAL POWER CURVE */

		in_coefficient[n] = n/(gain_t) (xfade_frames-1); /* 0 .. 1 */
		out_coefficient[n] = 1.0 - in_coefficient[n];    /* 1 .. 0 */
	}
}

int
DestructiveFileSource::seek (jack_nframes_t frame)
{
	return 0;
}

void
DestructiveFileSource::mark_capture_start (jack_nframes_t pos)
{
	_capture_start = true;
	capture_start_frame = pos;
}

void
DestructiveFileSource::mark_capture_end()
{
	_capture_end = true;
}

void
DestructiveFileSource::clear_capture_marks ()
{
	_capture_start = false;
	_capture_end = false;
}	

jack_nframes_t
DestructiveFileSource::crossfade (Sample* data, jack_nframes_t cnt, int fade_in, char * workbuf)
{
	jack_nframes_t xfade = min (xfade_frames, cnt);
	jack_nframes_t nofade = cnt - xfade;
	Sample* fade_data = 0;
	off_t fade_position = 0; // in frames
	ssize_t retval;
	
	if (fade_in) {
		fade_position = file_pos;
		fade_data = data;
	} else {
		fade_position = file_pos + nofade;
		fade_data = data + nofade;
	}

	if ((retval = file_read (xfade_buf, fade_position, xfade, workbuf)) != (ssize_t) xfade) {
		if (retval < 0 && errno == EAGAIN) {
			/* no data there, so no xfade */

			xfade = 0;
			nofade = cnt;
		} else {
			error << string_compose(_("FileSource: \"%1\" bad read (%2: %3)"), _path, errno, strerror (errno)) << endmsg;
			return 0;
		}
	}
	
	if (nofade && !fade_in) {
		cerr << "write " << nofade << " frames of prefade OUT data to " << file_pos << " .. " << file_pos + nofade << endl;
		if (file_write (data, file_pos, nofade, workbuf) != (ssize_t) nofade) {
			error << string_compose(_("FileSource: \"%1\" bad write (%2)"), _path, strerror (errno)) << endmsg;
			return 0;
		}
	}

	if (xfade == xfade_frames) {

		jack_nframes_t n;

		/* use the standard xfade curve */
		
		if (fade_in) {

			/* fade new material in */
			
			for (n = 0; n < xfade; ++n) {
				xfade_buf[n] = (xfade_buf[n] * out_coefficient[n]) + (fade_data[n] * in_coefficient[n]);
			}

		} else {


			/* fade new material out */
			
			for (n = 0; n < xfade; ++n) {
				xfade_buf[n] = (xfade_buf[n] * in_coefficient[n]) + (fade_data[n] * out_coefficient[n]);
			}
		}

	} else if (xfade) {

		/* short xfade, compute custom curve */

		/* XXX COMPUTE THE CURVE, DAMMIT! */

		for (jack_nframes_t n = 0; n < xfade; ++n) {
			xfade_buf[n] = (xfade_buf[n] * out_coefficient[n]) + (fade_data[n] * in_coefficient[n]);
		}
	}

	if (xfade) {
		cerr << "write " << xfade << " frames of xfade  data to " << fade_position << " .. " << fade_position + xfade << endl;

		if (file_write (xfade_buf, fade_position, xfade, workbuf) != (ssize_t) xfade) {
			error << string_compose(_("FileSource: \"%1\" bad write (%2)"), _path, strerror (errno)) << endmsg;
			return 0;
		}
	}
	
	if (fade_in && nofade) {
		cerr << "write " << nofade << " frames of postfade IN  data to " << file_pos + xfade << " .. " 
		     << file_pos + xfade + nofade << endl;
		if (file_write (data + xfade, file_pos + xfade, nofade, workbuf) != (ssize_t) nofade) {
			error << string_compose(_("FileSource: \"%1\" bad write (%2)"), _path, strerror (errno)) << endmsg;
			return 0;
		}
	}

	return cnt;
}

jack_nframes_t
DestructiveFileSource::write (Sample* data, jack_nframes_t cnt, char * workbuf)
{
	cerr << _name << ": write " << cnt << " to " << file_pos << " start ? " << _capture_start << " end ? " << _capture_end << endl;

	{
		LockMonitor lm (_lock, __LINE__, __FILE__);
		
		jack_nframes_t oldlen;

		if (_capture_start) {
			_capture_start = false;
			_capture_end = false;

			/* move to the correct location place */
			//file_pos = data_offset + (capture_start_frame * sizeof (Sample));
			file_pos = capture_start_frame;
			
			cerr << "First frame of capture will be at " << file_pos << endl;
			
			if (crossfade (data, cnt, 1, workbuf) != cnt) {
				return 0;
			}

		} else if (_capture_end) {
			_capture_start = false;
			_capture_end = false;

			if (crossfade (data, cnt, 0, workbuf) != cnt) {
				return 0;
			}
		} else {
			if (file_write(data, file_pos, cnt, workbuf) != (ssize_t) cnt) {
				return 0;
			}
		}

		oldlen = _length;
		if (file_pos + cnt > _length) {
			_length += cnt;
		}
		file_pos += cnt;

		cerr << "at end of write, file_pos = " << file_pos << endl;

		if (_build_peakfiles) {
			PeakBuildRecord *pbr = 0;
			
			if (pending_peak_builds.size()) {
				pbr = pending_peak_builds.back();
			}
			
			if (pbr && pbr->frame + pbr->cnt == oldlen) {
				
				/* the last PBR extended to the start of the current write,
				   so just extend it again.
				*/

				pbr->cnt += cnt;
			} else {
				pending_peak_builds.push_back (new PeakBuildRecord (oldlen, cnt));
			}
			
			_peaks_built = false;
		}

	}


	if (_build_peakfiles) {
		queue_for_peaks (*this);
	}

	return cnt;
}

jack_nframes_t
DestructiveFileSource::last_capture_start_frame () const
{
	return capture_start_frame;
}
