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

#include <pbd/error.h>
#include <ardour/destructive_filesource.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;

DestructiveFileSource::DestructiveFileSource (string path, jack_nframes_t rate, bool repair_first)
	: FileSource (path, rate, repair_first)
{
	_capture_start = false;
	_capture_end = false;
}

DestructiveFileSource::DestructiveFileSource (const XMLNode& node, jack_nframes_t rate)
	: FileSource (node, rate)
{
	_capture_start = false;
	_capture_end = false;
}

DestructiveFileSource::~DestructiveFileSource()
{
}

int
DestructiveFileSource::seek (jack_nframes_t frame)
{
	return 0;
}

void
DestructiveFileSource::mark_capture_start ()
{
	_capture_start = true;
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
DestructiveFileSource::write (Sample* data, jack_nframes_t cnt)
{
	{
		LockMonitor lm (_lock, __LINE__, __FILE__);
		
		int32_t byte_cnt = cnt * sizeof (Sample);
		int32_t byte_pos = data_offset + (_length * sizeof (Sample));
		jack_nframes_t oldlen;

		if (::pwrite64 (fd, (char *) data, byte_cnt, byte_pos) != (off64_t) byte_cnt) {
			error << string_compose(_("FileSource: \"%1\" bad write (%2)"), _path, strerror (errno)) << endmsg;
			return 0;
		}

		oldlen = _length;
		_length += cnt;
		_write_data_count = byte_cnt;

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

