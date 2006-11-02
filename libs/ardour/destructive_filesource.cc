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
#include <ardour/utils.h>
#include <ardour/session.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

gain_t* DestructiveFileSource::out_coefficient = 0;
gain_t* DestructiveFileSource::in_coefficient = 0;
nframes_t DestructiveFileSource::xfade_frames = 64;

DestructiveFileSource::DestructiveFileSource (Session& s, string path, SampleFormat samp_format, HeaderFormat hdr_format, nframes_t rate, Flag flags)
	: SndFileSource (s, path, samp_format, hdr_format, rate, flags)
{
	init ();
}


DestructiveFileSource::DestructiveFileSource (Session& s, string path, Flag flags)
	: SndFileSource (s, path, flags)
{
	init ();
}

DestructiveFileSource::DestructiveFileSource (Session& s, const XMLNode& node)
	: SndFileSource (s, node)
{
	init ();
}

void
DestructiveFileSource::init ()
{
	xfade_buf = new Sample[xfade_frames];

	_capture_start = false;
	_capture_end = false;
	file_pos = 0;

	timeline_position = header_position_offset;
	AudioFileSource::HeaderPositionOffsetChanged.connect (mem_fun (*this, &DestructiveFileSource::handle_header_position_change));
}

DestructiveFileSource::~DestructiveFileSource()
{
	delete xfade_buf;
}

void
DestructiveFileSource::setup_standard_crossfades (nframes_t rate)
{
	/* This static method is assumed to have been called by the Session
	   before any DFS's are created.
	*/

	xfade_frames = (nframes_t) floor ((Config->get_destructive_xfade_msecs () / 1000.0) * rate);

	if (out_coefficient) {
		delete [] out_coefficient;
	}

	if (in_coefficient) {
		delete [] in_coefficient;
	}

	out_coefficient = new gain_t[xfade_frames];
	in_coefficient = new gain_t[xfade_frames];

	compute_equal_power_fades (xfade_frames, in_coefficient, out_coefficient);
}

void
DestructiveFileSource::mark_capture_start (nframes_t pos)
{
	if (pos < timeline_position) {
		_capture_start = false;
	} else {
		_capture_start = true;
		capture_start_frame = pos;
	}
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

nframes_t
DestructiveFileSource::crossfade (Sample* data, nframes_t cnt, int fade_in)
{
	nframes_t xfade = min (xfade_frames, cnt);
	nframes_t nofade = cnt - xfade;
	Sample* fade_data = 0;
	nframes_t fade_position = 0; // in frames
	ssize_t retval;
	nframes_t file_cnt;

	if (fade_in) {
		fade_position = file_pos;
		fade_data = data;
	} else {
		fade_position = file_pos + nofade;
		fade_data = data + nofade;
	}

	if (fade_position > _length) {
		
		/* read starts beyond end of data, just memset to zero */
		
		file_cnt = 0;

	} else if (fade_position + xfade > _length) {
		
		/* read ends beyond end of data, read some, memset the rest */
		
		file_cnt = _length - fade_position;

	} else {
		
		/* read is entirely within data */

		file_cnt = xfade;
	}

	if (file_cnt) {
		
		if ((retval = read_unlocked (xfade_buf, fade_position, file_cnt)) != (ssize_t) file_cnt) {
			if (retval >= 0 && errno == EAGAIN) {
				/* XXX - can we really trust that errno is meaningful here?  yes POSIX, i'm talking to you.
				 * short or no data there */
				memset (xfade_buf, 0, xfade * sizeof(Sample));
			} else {
				error << string_compose(_("DestructiveFileSource: \"%1\" bad read retval: %2 of %5 (%3: %4)"), _path, retval, errno, strerror (errno), xfade) << endmsg;
				return 0;
			}
		}
	} 

	if (file_cnt != xfade) {
		nframes_t delta = xfade - file_cnt;
		memset (xfade_buf+file_cnt, 0, sizeof (Sample) * delta);
	}
	
	if (nofade && !fade_in) {
		if (write_float (data, file_pos, nofade) != nofade) {
			error << string_compose(_("DestructiveFileSource: \"%1\" bad write (%2)"), _path, strerror (errno)) << endmsg;
			return 0;
		}
	}

	if (xfade == xfade_frames) {

		nframes_t n;

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

		gain_t in[xfade];
		gain_t out[xfade];

		/* short xfade, compute custom curve */

		compute_equal_power_fades (xfade, in, out);

		for (nframes_t n = 0; n < xfade; ++n) {
			xfade_buf[n] = (xfade_buf[n] * out[n]) + (fade_data[n] * in[n]);		
		}
	}

	if (xfade) {
		if (write_float (xfade_buf, fade_position, xfade) != xfade) {
			error << string_compose(_("DestructiveFileSource: \"%1\" bad write (%2)"), _path, strerror (errno)) << endmsg;
			return 0;
		}
	}
	
	if (fade_in && nofade) {
		if (write_float (data + xfade, file_pos + xfade, nofade) != nofade) {
			error << string_compose(_("DestructiveFileSource: \"%1\" bad write (%2)"), _path, strerror (errno)) << endmsg;
			return 0;
		}
	}

	return cnt;
}

nframes_t
DestructiveFileSource::write_unlocked (Sample* data, nframes_t cnt)
{
	nframes_t old_file_pos;

	if (!writable()) {
		return 0;
	}

	if (_capture_start && _capture_end) {

		/* start and end of capture both occur within the data we are writing,
		   so do both crossfades.
		*/

		_capture_start = false;
		_capture_end = false;
		
		/* move to the correct location place */
		file_pos = capture_start_frame;
		
		// split cnt in half
		nframes_t subcnt = cnt / 2;
		nframes_t ofilepos = file_pos;
		
		// fade in
		if (crossfade (data, subcnt, 1) != subcnt) {
			return 0;
		}
		
		file_pos += subcnt;
		Sample * tmpdata = data + subcnt;
		
		// fade out
		subcnt = cnt - subcnt;
		if (crossfade (tmpdata, subcnt, 0) != subcnt) {
			return 0;
		}
		
		file_pos = ofilepos; // adjusted below

	} else if (_capture_start) {

		/* start of capture both occur within the data we are writing,
		   so do the fade in
		*/

		_capture_start = false;
		_capture_end = false;
		
		/* move to the correct location place */
		file_pos = capture_start_frame - timeline_position;

		if (crossfade (data, cnt, 1) != cnt) {
			return 0;
		}
		
	} else if (_capture_end) {

		/* end of capture both occur within the data we are writing,
		   so do the fade out
		*/

		_capture_start = false;
		_capture_end = false;
		
		if (crossfade (data, cnt, 0) != cnt) {
			return 0;
		}

	} else {

		/* in the middle of recording */
		

		if (write_float (data, file_pos, cnt) != cnt) {
			return 0;
		}
	}
	
	old_file_pos = file_pos;
	update_length (file_pos, cnt);
	file_pos += cnt;

	if (_build_peakfiles) {
		PeakBuildRecord *pbr = 0;
		
		if (pending_peak_builds.size()) {
			pbr = pending_peak_builds.back();
		}
		
		if (pbr && pbr->frame + pbr->cnt == old_file_pos) {
			
			/* the last PBR extended to the start of the current write,
			   so just extend it again.
			*/
			
			pbr->cnt += cnt;
		} else {
			pending_peak_builds.push_back (new PeakBuildRecord (old_file_pos, cnt));
		}
		
		_peaks_built = false;
	}

	if (_build_peakfiles) {
		queue_for_peaks (shared_from_this ());
	}
	
	return cnt;
}

nframes_t
DestructiveFileSource::last_capture_start_frame () const
{
	return capture_start_frame;
}

XMLNode& 
DestructiveFileSource::get_state ()
{
	XMLNode& node = AudioFileSource::get_state ();
	node.add_property (X_("destructive"), "true");
	return node;
}

void
DestructiveFileSource::handle_header_position_change ()
{
	if ( _length != 0 ) {
		error << string_compose(_("Filesource: start time is already set for existing file (%1): Cannot change start time."), _path ) << endmsg;
		//in the future, pop up a dialog here that allows user to regenerate file with new start offset
	} else if (writable()) {
		timeline_position = header_position_offset;
		set_header_timeline_position ();  //this will get flushed if/when the file is recorded to
	}
}

void
DestructiveFileSource::set_timeline_position (nframes_t pos)
{
	//destructive track timeline postion does not change except at instantion or when header_position_offset (session start) changes
}

int
DestructiveFileSource::read_peaks (PeakData *peaks, nframes_t npeaks, nframes_t start, nframes_t cnt, double samples_per_unit) const
{
	return AudioFileSource::read_peaks (peaks, npeaks, start, cnt, samples_per_unit);
}
	
