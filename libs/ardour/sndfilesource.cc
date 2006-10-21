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

#include <cerrno>
#include <climits>

#include <pwd.h>
#include <sys/utsname.h>
#include <sys/stat.h>

#include <glibmm/miscutils.h>

#include <ardour/sndfilesource.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

SndFileSource::SndFileSource (Session& s, const XMLNode& node)
	: AudioFileSource (s, node)
{
	init (_name);

	if (open()) {
		throw failed_constructor ();
	}
}

SndFileSource::SndFileSource (Session& s, string idstr, Flag flags)
	                                /* files created this way are never writable or removable */
	: AudioFileSource (s, idstr, Flag (flags & ~(Writable|Removable|RemovableIfEmpty|RemoveAtDestroy)))
{
	init (idstr);

	if (open()) {
		throw failed_constructor ();
	}
}

SndFileSource::SndFileSource (Session& s, string idstr, SampleFormat sfmt, HeaderFormat hf, nframes_t rate, Flag flags)
	: AudioFileSource (s, idstr, flags, sfmt, hf)
{
	int fmt = 0;

	init (idstr);

	/* this constructor is used to construct new files, not open
	   existing ones.
	*/

	file_is_new = true;
	
	switch (hf) {
	case CAF:
		fmt = SF_FORMAT_CAF;
		_flags = Flag (_flags & ~Broadcast);
		break;

	case AIFF:
		fmt = SF_FORMAT_AIFF;
		_flags = Flag (_flags & ~Broadcast);
		break;

	case BWF:
		fmt = SF_FORMAT_WAV;
		_flags = Flag (_flags | Broadcast);
		break;

	case WAVE:
		fmt = SF_FORMAT_WAV;
		_flags = Flag (_flags & ~Broadcast);
		break;

	case WAVE64:
		fmt = SF_FORMAT_W64;
		_flags = Flag (_flags & ~Broadcast);
		break;

	default:
		fatal << string_compose (_("programming error: %1"), X_("unsupported audio header format requested")) << endmsg;
		/*NOTREACHED*/
		break;

	}

	switch (sfmt) {
	case FormatFloat:
		fmt |= SF_FORMAT_FLOAT;
		break;

	case FormatInt24:
		fmt |= SF_FORMAT_PCM_24;
		break;
	}
	
	_info.channels = 1;
	_info.samplerate = rate;
	_info.format = fmt;

	if (open()) {
		throw failed_constructor();
	}

	if (writable() && (_flags & Broadcast)) {

		_broadcast_info = new SF_BROADCAST_INFO;
		memset (_broadcast_info, 0, sizeof (*_broadcast_info));
		
		snprintf (_broadcast_info->description, sizeof (_broadcast_info->description), "BWF %s", _name.c_str());
		
		struct utsname utsinfo;

		if (uname (&utsinfo)) {
			error << string_compose(_("FileSource: cannot get host information for BWF header (%1)"), strerror(errno)) << endmsg;
			return;
		}
		
		snprintf (_broadcast_info->originator, sizeof (_broadcast_info->originator), "ardour:%s:%s:%s:%s:%s)", 
			  Glib::get_real_name().c_str(),
			  utsinfo.nodename,
			  utsinfo.sysname,
			  utsinfo.release,
			  utsinfo.version);
		
		_broadcast_info->version = 1;  
		_broadcast_info->time_reference_low = 0;  
		_broadcast_info->time_reference_high = 0;  
		
		/* XXX do something about this field */
		
		snprintf (_broadcast_info->umid, sizeof (_broadcast_info->umid), "%s", "fnord");
		
		/* coding history is added by libsndfile */

		if (sf_command (sf, SFC_SET_BROADCAST_INFO, _broadcast_info, sizeof (_broadcast_info)) != SF_TRUE) {
			char errbuf[256];
			sf_error_str (0, errbuf, sizeof (errbuf) - 1);
			error << string_compose (_("cannot set broadcast info for audio file %1 (%2); dropping broadcast info for this file"), _path, errbuf) << endmsg;
			_flags = Flag (_flags & ~Broadcast);
			delete _broadcast_info;
			_broadcast_info = 0;
		}
		
	}
}

void 
SndFileSource::init (const string& idstr)
{
	string::size_type pos;
	string file;

	interleave_buf = 0;
	interleave_bufsize = 0;
	sf = 0;
	_broadcast_info = 0;

	if ((pos = idstr.find_last_of (':')) == string::npos) {
		channel = 0;
		_name = Glib::path_get_basename (idstr);
	} else {
		channel = atoi (idstr.substr (pos+1).c_str());
		_name = Glib::path_get_basename (idstr.substr (0, pos));
	}

	/* although libsndfile says we don't need to set this,
	   valgrind and source code shows us that we do.
	*/

	memset (&_info, 0, sizeof(_info));
}

int
SndFileSource::open ()
{
	if ((sf = sf_open (_path.c_str(), (writable() ? SFM_RDWR : SFM_READ), &_info)) == 0) {
		char errbuf[256];
		sf_error_str (0, errbuf, sizeof (errbuf) - 1);
		error << string_compose(_("SndFileSource: cannot open file \"%1\" for %2 (%3)"), 
					_path, (writable() ? "read+write" : "reading"), errbuf) << endmsg;
		return -1;
	}

	if (channel >= _info.channels) {
		error << string_compose(_("SndFileSource: file only contains %1 channels; %2 is invalid as a channel number"), _info.channels, channel) << endmsg;
		sf_close (sf);
		sf = 0;
		return -1;
	}

	_length = _info.frames;

	_broadcast_info = new SF_BROADCAST_INFO;
	memset (_broadcast_info, 0, sizeof (*_broadcast_info));
	
	/* lookup broadcast info */
	
	if (sf_command (sf, SFC_GET_BROADCAST_INFO, _broadcast_info, sizeof (*_broadcast_info)) != SF_TRUE) {

		/* if the file has data but no broadcast info, then clearly, there is no broadcast info */

		if (_length) {
			delete _broadcast_info;
			_broadcast_info = 0;
			_flags = Flag (_flags & ~Broadcast);
		}

		set_timeline_position (header_position_offset);

	} else {
	
		/* XXX 64 bit alert: when JACK switches to a 64 bit frame count, this needs to use the high bits
		   of the time reference.
		*/

		set_timeline_position ( _broadcast_info->time_reference_low );
	}

	if (writable()) {
		sf_command (sf, SFC_SET_UPDATE_HEADER_AUTO, 0, SF_FALSE);
	}

	return 0;
}

SndFileSource::~SndFileSource ()
{
	GoingAway (); /* EMIT SIGNAL */

	if (sf) {
		sf_close (sf);
		sf = 0;

		/* stupid libsndfile updated the headers on close,
		   so touch the peakfile if it exists and has data
		   to make sure its time is as new as the audio
		   file.
		*/

		touch_peakfile ();
	}

	if (interleave_buf) {
		delete [] interleave_buf;
	}

	if (_broadcast_info) {
		delete _broadcast_info;
	}
}

float
SndFileSource::sample_rate () const 
{
	return _info.samplerate;
}

nframes_t
SndFileSource::read_unlocked (Sample *dst, nframes_t start, nframes_t cnt) const
{
	int32_t nread;
	float *ptr;
	uint32_t real_cnt;
	nframes_t file_cnt;

	if (start > _length) {

		/* read starts beyond end of data, just memset to zero */
		
		file_cnt = 0;

	} else if (start + cnt > _length) {
		
		/* read ends beyond end of data, read some, memset the rest */
		
		file_cnt = _length - start;

	} else {
		
		/* read is entirely within data */

		file_cnt = cnt;
	}
	
	if (file_cnt) {

		if (sf_seek (sf, (sf_count_t) start, SEEK_SET|SFM_READ) != (sf_count_t) start) {
			char errbuf[256];
			sf_error_str (0, errbuf, sizeof (errbuf) - 1);
			error << string_compose(_("SndFileSource: could not seek to frame %1 within %2 (%3)"), start, _name.substr (1), errbuf) << endmsg;
			return 0;
		}
		
		if (_info.channels == 1) {
			nframes_t ret = sf_read_float (sf, dst, file_cnt);
			_read_data_count = cnt * sizeof(float);
			return ret;
		}
	}

	if (file_cnt != cnt) {
		nframes_t delta = cnt - file_cnt;
		memset (dst+file_cnt, 0, sizeof (Sample) * delta);
	}

	real_cnt = cnt * _info.channels;

	if (interleave_bufsize < real_cnt) {
		
		if (interleave_buf) {
			delete [] interleave_buf;
		}
		interleave_bufsize = real_cnt;
		interleave_buf = new float[interleave_bufsize];
	}
	
	nread = sf_read_float (sf, interleave_buf, real_cnt);
	ptr = interleave_buf + channel;
	nread /= _info.channels;
	
	/* stride through the interleaved data */
	
	for (int32_t n = 0; n < nread; ++n) {
		dst[n] = *ptr;
		ptr += _info.channels;
	}

	_read_data_count = cnt * sizeof(float);
		
	return nread;
}

nframes_t 
SndFileSource::write_unlocked (Sample *data, nframes_t cnt)
{
	if (!writable()) {
		return 0;
	}

	if (_info.channels != 1) {
		fatal << string_compose (_("programming error: %1 %2"), X_("SndFileSource::write called on non-mono file"), _path) << endmsg;
		/*NOTREACHED*/
		return 0;
	}
	
	nframes_t oldlen;
	int32_t frame_pos = _length;
	
	if (write_float (data, frame_pos, cnt) != cnt) {
		return 0;
	}

	oldlen = _length;
	update_length (oldlen, cnt);

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
	
	
	if (_build_peakfiles) {
		queue_for_peaks (shared_from_this ());
	}

	_write_data_count = cnt;
	
	return cnt;
}

int
SndFileSource::update_header (nframes_t when, struct tm& now, time_t tnow)
{	
	set_timeline_position (when);

	if (_flags & Broadcast) {
		if (setup_broadcast_info (when, now, tnow)) {
			return -1;
		}
	} 

	return flush_header ();
}

int
SndFileSource::flush_header ()
{
	if (!writable() || (sf == 0)) {
		return -1;
	}
	return (sf_command (sf, SFC_UPDATE_HEADER_NOW, 0, 0) != SF_TRUE);
}

int
SndFileSource::setup_broadcast_info (nframes_t when, struct tm& now, time_t tnow)
{
	if (!writable()) {
		return -1;
	}

	if (!(_flags & Broadcast)) {
		return 0;
	}

	/* random code is 9 digits */
	
	int random_code = random() % 999999999;
	
	snprintf (_broadcast_info->originator_reference, sizeof (_broadcast_info->originator_reference), "%2s%3s%12s%02d%02d%02d%9d",
		  Config->get_bwf_country_code().c_str(),
		  Config->get_bwf_organization_code().c_str(),
		  bwf_serial_number,
		  now.tm_hour,
		  now.tm_min,
		  now.tm_sec,
		  random_code);
	
	snprintf (_broadcast_info->origination_date, sizeof (_broadcast_info->origination_date), "%4d-%02d-%02d",
		  1900 + now.tm_year,
		  now.tm_mon,
		  now.tm_mday);
	
	snprintf (_broadcast_info->origination_time, sizeof (_broadcast_info->origination_time), "%02d:%02d:%02d",
		  now.tm_hour,
		  now.tm_min,
		  now.tm_sec);

	/* now update header position taking header offset into account */
	
	set_header_timeline_position ();

	if (sf_command (sf, SFC_SET_BROADCAST_INFO, _broadcast_info, sizeof (*_broadcast_info)) != SF_TRUE) {
		error << string_compose (_("cannot set broadcast info for audio file %1; Dropping broadcast info for this file"), _path) << endmsg;
		_flags = Flag (_flags & ~Broadcast);
		delete _broadcast_info;
		_broadcast_info = 0;
		return -1;
	}

	return 0;
}

void
SndFileSource::set_header_timeline_position ()
{
	if (!(_flags & Broadcast)) {
		return;
	}

	_broadcast_info->time_reference_high = (timeline_position >> 32);
	_broadcast_info->time_reference_low = (timeline_position & 0xffffffff);

	if (sf_command (sf, SFC_SET_BROADCAST_INFO, _broadcast_info, sizeof (*_broadcast_info)) != SF_TRUE) {
		error << string_compose (_("cannot set broadcast info for audio file %1; Dropping broadcast info for this file"), _path) << endmsg;
		_flags = Flag (_flags & ~Broadcast);
		delete _broadcast_info;
		_broadcast_info = 0;
	}

	

}

nframes_t
SndFileSource::write_float (Sample* data, nframes_t frame_pos, nframes_t cnt)
{
	if (sf_seek (sf, frame_pos, SEEK_SET|SFM_WRITE) != frame_pos) {
		error << string_compose (_("%1: cannot seek to %2"), _path, frame_pos) << endmsg;
		return 0;
	}
	
	if (sf_writef_float (sf, data, cnt) != (ssize_t) cnt) {
		return 0;
	}
	
	return cnt;
}

nframes_t
SndFileSource::natural_position() const
{
	return timeline_position;
}
