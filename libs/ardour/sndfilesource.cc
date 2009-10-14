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

*/

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include <cstring>
#include <cerrno>
#include <climits>
#include <cstdarg>

#include <pwd.h>
#include <sys/utsname.h>
#include <sys/stat.h>

#include <glibmm/miscutils.h>

#include "ardour/sndfilesource.h"
#include "ardour/sndfile_helpers.h"
#include "ardour/utils.h"
#include "ardour/version.h"
#include "ardour/rc_configuration.h"
#include "ardour/session.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using Glib::ustring;

gain_t* SndFileSource::out_coefficient = 0;
gain_t* SndFileSource::in_coefficient = 0;
nframes_t SndFileSource::xfade_frames = 64;
const Source::Flag SndFileSource::default_writable_flags = Source::Flag (
		Source::Writable |
		Source::Removable |
		Source::RemovableIfEmpty |
		Source::CanRename );

SndFileSource::SndFileSource (Session& s, const XMLNode& node)
	: Source(s, node)
	, AudioFileSource (s, node)
{
	init_sndfile ();

	if (open()) {
		throw failed_constructor ();
	}
}

/** Files created this way are never writable or removable */
SndFileSource::SndFileSource (Session& s, const ustring& path, bool embedded, int chn, Flag flags)
	: Source(s, DataType::AUDIO, path, flags)
	, AudioFileSource (s, path, embedded,
			Flag (flags & ~(Writable|Removable|RemovableIfEmpty|RemoveAtDestroy)))
{
	_channel = chn;

	init_sndfile ();

	if (open()) {
		throw failed_constructor ();
	}
}

/** This constructor is used to construct new files, not open existing ones. */
SndFileSource::SndFileSource (Session& s, const ustring& path, bool embedded,
		SampleFormat sfmt, HeaderFormat hf, nframes_t rate, Flag flags)
	: Source(s, DataType::AUDIO, path, flags)
	, AudioFileSource (s, path, embedded, flags, sfmt, hf)
{
	int fmt = 0;

	init_sndfile ();

	_file_is_new = true;

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

	case FormatInt16:
		fmt |= SF_FORMAT_PCM_16;
		break;
	}

	_info.channels = 1;
	_info.samplerate = rate;
	_info.format = fmt;

	if (open()) {
		throw failed_constructor();
	}

	if (writable() && (_flags & Broadcast)) {

		if (!_broadcast_info) {
			_broadcast_info = new BroadcastInfo;
		}

		_broadcast_info->set_from_session (s, header_position_offset);
		_broadcast_info->set_description (string_compose ("BWF %1", _name));

		if (!_broadcast_info->write_to_file (sf)) {
			error << string_compose (_("cannot set broadcast info for audio file %1 (%2); dropping broadcast info for this file"),
			                           _path, _broadcast_info->get_error())
			      << endmsg;
			_flags = Flag (_flags & ~Broadcast);
			delete _broadcast_info;
			_broadcast_info = 0;
		}
	}
}

void
SndFileSource::init_sndfile ()
{
	ustring file;

	// lets try to keep the object initalizations here at the top
	xfade_buf = 0;
	sf = 0;
	_broadcast_info = 0;

	if (is_embedded()) {
		_name = _path;
	} else {
		_name = Glib::path_get_basename (_path);
	}

	/* although libsndfile says we don't need to set this,
	   valgrind and source code shows us that we do.
	*/

	memset (&_info, 0, sizeof(_info));

	_capture_start = false;
	_capture_end = false;
	file_pos = 0;

	if (destructive()) {
		xfade_buf = new Sample[xfade_frames];
		_timeline_position = header_position_offset;
	}

	AudioFileSource::HeaderPositionOffsetChanged.connect (
			mem_fun (*this, &SndFileSource::handle_header_position_change));
}

int
SndFileSource::open ()
{
	if ((sf = sf_open (_path.c_str(), (writable() ? SFM_RDWR : SFM_READ), &_info)) == 0) {
		char errbuf[256];
		sf_error_str (0, errbuf, sizeof (errbuf) - 1);
#ifndef HAVE_COREAUDIO
		/* if we have CoreAudio, we will be falling back to that if libsndfile fails,
		   so we don't want to see this message.
		*/

		error << string_compose(_("SndFileSource: cannot open file \"%1\" for %2 (%3)"),
					_path, (writable() ? "read+write" : "reading"), errbuf) << endmsg;
#endif
		return -1;
	}

	if (_channel >= _info.channels) {
#ifndef HAVE_COREAUDIO
		error << string_compose(_("SndFileSource: file only contains %1 channels; %2 is invalid as a channel number"), _info.channels, _channel) << endmsg;
#endif
		sf_close (sf);
		sf = 0;
		return -1;
	}

	_length = _info.frames;

	if (!_broadcast_info) {
		_broadcast_info = new BroadcastInfo;
	}

	bool bwf_info_exists = _broadcast_info->load_from_file (sf);

	set_timeline_position (bwf_info_exists ? _broadcast_info->get_time_reference() : header_position_offset);

	if (_length != 0 && !bwf_info_exists) {
		delete _broadcast_info;
		_broadcast_info = 0;
		_flags = Flag (_flags & ~Broadcast);
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

	delete _broadcast_info;
	delete [] xfade_buf;
}

float
SndFileSource::sample_rate () const
{
	return _info.samplerate;
}

nframes_t
SndFileSource::read_unlocked (Sample *dst, sframes_t start, nframes_t cnt) const
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

	if (file_cnt != cnt) {
		nframes_t delta = cnt - file_cnt;
		memset (dst+file_cnt, 0, sizeof (Sample) * delta);
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
			_read_data_count = ret * sizeof(float);
			if (ret != file_cnt) {
				char errbuf[256];
				sf_error_str (0, errbuf, sizeof (errbuf) - 1);
				cerr << string_compose(_("SndFileSource: @ %1 could not read %2 within %3 (%4) (len = %5)"), start, file_cnt, _name.substr (1), errbuf, _length) << endl;
			}
			return ret;
		}
	}

	real_cnt = cnt * _info.channels;

	Sample* interleave_buf = get_interleave_buffer (real_cnt);

	nread = sf_read_float (sf, interleave_buf, real_cnt);
	ptr = interleave_buf + _channel;
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
	if (destructive()) {
		return destructive_write_unlocked (data, cnt);
	} else {
		return nondestructive_write_unlocked (data, cnt);
	}
}

nframes_t
SndFileSource::nondestructive_write_unlocked (Sample *data, nframes_t cnt)
{
	if (!writable()) {
		warning << string_compose (_("attempt to write a non-writable audio file source (%1)"), _path) << endmsg;
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
		compute_and_write_peaks (data, frame_pos, cnt, false, true);
	}

	_write_data_count = cnt;

	return cnt;
}

nframes_t
SndFileSource::destructive_write_unlocked (Sample* data, nframes_t cnt)
{
	nframes_t old_file_pos;

	if (!writable()) {
		warning << string_compose (_("attempt to write a non-writable audio file source (%1)"), _path) << endmsg;
		return 0;
	}

	if (_capture_start && _capture_end) {

		/* start and end of capture both occur within the data we are writing,
		   so do both crossfades.
		*/

		_capture_start = false;
		_capture_end = false;

		/* move to the correct location place */
		file_pos = capture_start_frame - _timeline_position;

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
		file_pos = capture_start_frame - _timeline_position;

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

	if (_build_peakfiles) {
		compute_and_write_peaks (data, file_pos, cnt, false, true);
	}

	file_pos += cnt;

	return cnt;
}

int
SndFileSource::update_header (sframes_t when, struct tm& now, time_t tnow)
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
		warning << string_compose (_("attempt to flush a non-writable audio file source (%1)"), _path) << endmsg;
		return -1;
	}
	return (sf_command (sf, SFC_UPDATE_HEADER_NOW, 0, 0) != SF_TRUE);
}

int
SndFileSource::setup_broadcast_info (sframes_t /*when*/, struct tm& now, time_t /*tnow*/)
{
	if (!writable()) {
		warning << string_compose (_("attempt to store broadcast info in a non-writable audio file source (%1)"), _path) << endmsg;
		return -1;
	}

	if (!(_flags & Broadcast)) {
		return 0;
	}

	_broadcast_info->set_originator_ref (_session);
	_broadcast_info->set_origination_time (&now);

	/* now update header position taking header offset into account */

	set_header_timeline_position ();

	if (!_broadcast_info->write_to_file (sf)) {
		error << string_compose (_("cannot set broadcast info for audio file %1 (%2); dropping broadcast info for this file"),
		                           _path, _broadcast_info->get_error())
		      << endmsg;
		_flags = Flag (_flags & ~Broadcast);
		delete _broadcast_info;
		_broadcast_info = 0;
	}

	return 0;
}

void
SndFileSource::set_header_timeline_position ()
{
	if (!(_flags & Broadcast)) {
		return;
	}

	_broadcast_info->set_time_reference (_timeline_position);

	if (!_broadcast_info->write_to_file (sf)) {
		error << string_compose (_("cannot set broadcast info for audio file %1 (%2); dropping broadcast info for this file"),
		                           _path, _broadcast_info->get_error())
		      << endmsg;
		_flags = Flag (_flags & ~Broadcast);
		delete _broadcast_info;
		_broadcast_info = 0;
	}
}

nframes_t
SndFileSource::write_float (Sample* data, sframes_t frame_pos, nframes_t cnt)
{
	if (sf_seek (sf, frame_pos, SEEK_SET|SFM_WRITE) < 0) {
		char errbuf[256];
		sf_error_str (0, errbuf, sizeof (errbuf) - 1);
		error << string_compose (_("%1: cannot seek to %2 (libsndfile error: %3"), _path, frame_pos, errbuf) << endmsg;
		return 0;
	}

	if (sf_writef_float (sf, data, cnt) != (ssize_t) cnt) {
		return 0;
	}

	return cnt;
}

sframes_t
SndFileSource::natural_position() const
{
	return _timeline_position;
}

bool
SndFileSource::set_destructive (bool yn)
{
	if (yn) {
		_flags = Flag (_flags | Destructive);
		if (!xfade_buf) {
			xfade_buf = new Sample[xfade_frames];
		}
		clear_capture_marks ();
		_timeline_position = header_position_offset;
	} else {
		_flags = Flag (_flags & ~Destructive);
		_timeline_position = 0;
		/* leave xfade buf alone in case we need it again later */
	}

	return true;
}

void
SndFileSource::clear_capture_marks ()
{
	_capture_start = false;
	_capture_end = false;
}

void
SndFileSource::mark_capture_start (sframes_t pos)
{
	if (destructive()) {
		if (pos < _timeline_position) {
			_capture_start = false;
		} else {
			_capture_start = true;
			capture_start_frame = pos;
		}
	}
}

void
SndFileSource::mark_capture_end()
{
	if (destructive()) {
		_capture_end = true;
	}
}

nframes_t
SndFileSource::crossfade (Sample* data, nframes_t cnt, int fade_in)
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
				error << string_compose(_("SndFileSource: \"%1\" bad read retval: %2 of %5 (%3: %4)"), _path, retval, errno, strerror (errno), xfade) << endmsg;
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
			error << string_compose(_("SndFileSource: \"%1\" bad write (%2)"), _path, strerror (errno)) << endmsg;
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

	} else if (xfade < xfade_frames) {

		gain_t in[xfade];
		gain_t out[xfade];

		/* short xfade, compute custom curve */

		compute_equal_power_fades (xfade, in, out);

		for (nframes_t n = 0; n < xfade; ++n) {
			xfade_buf[n] = (xfade_buf[n] * out[n]) + (fade_data[n] * in[n]);
		}

	} else if (xfade) {

		/* long xfade length, has to be computed across several calls */

	}

	if (xfade) {
		if (write_float (xfade_buf, fade_position, xfade) != xfade) {
			error << string_compose(_("SndFileSource: \"%1\" bad write (%2)"), _path, strerror (errno)) << endmsg;
			return 0;
		}
	}

	if (fade_in && nofade) {
		if (write_float (data + xfade, file_pos + xfade, nofade) != nofade) {
			error << string_compose(_("SndFileSource: \"%1\" bad write (%2)"), _path, strerror (errno)) << endmsg;
			return 0;
		}
	}

	return cnt;
}

sframes_t
SndFileSource::last_capture_start_frame () const
{
	if (destructive()) {
		return capture_start_frame;
	} else {
		return 0;
	}
}

void
SndFileSource::handle_header_position_change ()
{
	if (destructive()) {
		if ( _length != 0 ) {
			error << string_compose(_("Filesource: start time is already set for existing file (%1): Cannot change start time."), _path ) << endmsg;
			//in the future, pop up a dialog here that allows user to regenerate file with new start offset
		} else if (writable()) {
			_timeline_position = header_position_offset;
			set_header_timeline_position ();  //this will get flushed if/when the file is recorded to
		}
	}
}

void
SndFileSource::setup_standard_crossfades (Session const & s, nframes_t rate)
{
	/* This static method is assumed to have been called by the Session
	   before any DFS's are created.
	*/

	xfade_frames = (nframes_t) floor ((s.config.get_destructive_xfade_msecs () / 1000.0) * rate);

	delete [] out_coefficient;
	delete [] in_coefficient;

	out_coefficient = new gain_t[xfade_frames];
	in_coefficient = new gain_t[xfade_frames];

	compute_equal_power_fades (xfade_frames, in_coefficient, out_coefficient);
}

void
SndFileSource::set_timeline_position (int64_t pos)
{
	// destructive track timeline postion does not change
	// except at instantion or when header_position_offset
	// (session start) changes

	if (!destructive()) {
		AudioFileSource::set_timeline_position (pos);
	}
}

int
SndFileSource::get_soundfile_info (const ustring& path, SoundFileInfo& info, string& error_msg)
{
	SNDFILE *sf;
	SF_INFO sf_info;
	BroadcastInfo binfo;

	sf_info.format = 0; // libsndfile says to clear this before sf_open().

	if ((sf = sf_open ((char*) path.c_str(), SFM_READ, &sf_info)) == 0) {
		char errbuf[256];
		error_msg = sf_error_str (0, errbuf, sizeof (errbuf) - 1);
		return false;
	}

	info.samplerate  = sf_info.samplerate;
	info.channels    = sf_info.channels;
	info.length      = sf_info.frames;
	info.format_name = string_compose("Format: %1, %2",
					   sndfile_major_format(sf_info.format),
					   sndfile_minor_format(sf_info.format));

	info.timecode = binfo.load_from_file (sf) ? binfo.get_time_reference() : 0;

	sf_close (sf);

	return true;
}

bool
SndFileSource::one_of_several_channels () const
{
	return _info.channels > 1;
}

