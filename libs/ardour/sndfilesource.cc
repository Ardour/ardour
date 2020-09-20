/*
 * Copyright (C) 2005-2006 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2005-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2012-2018 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013-2015 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2015 Tim Mayberry <mojofunk@gmail.com>
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

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include <cstring>
#include <cerrno>
#include <climits>
#include <cstdarg>
#include <fcntl.h>

#include <sys/stat.h>

#include <glib.h>
#include "pbd/gstdio_compat.h"

#include <glibmm/convert.h>
#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "ardour/runtime_functions.h"
#include "ardour/sndfilesource.h"
#include "ardour/sndfile_helpers.h"
#include "ardour/utils.h"
#include "ardour/session.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using std::string;

const Source::Flag SndFileSource::default_writable_flags = Source::Flag (
		Source::Writable |
		Source::Removable |
		Source::RemovableIfEmpty |
		Source::CanRename );

SndFileSource::SndFileSource (Session& s, const XMLNode& node)
	: Source(s, node)
	, AudioFileSource (s, node)
	, _sndfile (0)
	, _broadcast_info (0)
{
	init_sndfile ();

        assert (Glib::file_test (_path, Glib::FILE_TEST_EXISTS));
	existence_check ();

	if (open()) {
		throw failed_constructor ();
	}
}

/** Constructor for existing external-to-session files.
    Files created this way are never writable or removable
*/
SndFileSource::SndFileSource (Session& s, const string& path, int chn, Flag flags)
	: Source(s, DataType::AUDIO, path, flags)
          /* note that the origin of an external file is itself */
	, AudioFileSource (s, path, Flag (flags & ~(Writable|Removable|RemovableIfEmpty|RemoveAtDestroy)))
	, _sndfile (0)
	, _broadcast_info (0)
{
	_channel = chn;

	init_sndfile ();

        assert (Glib::file_test (_path, Glib::FILE_TEST_EXISTS));
	existence_check ();

	if (open()) {
		throw failed_constructor ();
	}
}

/** This constructor is used to construct new internal-to-session files,
    not open existing ones.
*/
SndFileSource::SndFileSource (Session& s, const string& path, const string& origin,
                              SampleFormat sfmt, HeaderFormat hf, samplecnt_t rate, Flag flags)
	: Source(s, DataType::AUDIO, path, flags)
	, AudioFileSource (s, path, origin, flags, sfmt, hf)
	, _sndfile (0)
	, _broadcast_info (0)
{
	int fmt = 0;

	init_sndfile ();

	assert (!Glib::file_test (_path, Glib::FILE_TEST_EXISTS));
	existence_check ();

	_file_is_new = true;

	switch (hf) {
	case CAF:
		fmt = SF_FORMAT_CAF;
		_flags = Flag (_flags & ~Broadcast);
		break;

	case FLAC:
		fmt = SF_FORMAT_FLAC;
		if (sfmt == FormatFloat) {
			sfmt = FormatInt24;
		}
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

	case RF64_WAV:
		fmt = SF_FORMAT_RF64;
		_flags = Flag (_flags & ~Broadcast);
		_flags = Flag (_flags | RF64_RIFF);
		break;

	case MBWF:
		fmt = SF_FORMAT_RF64;
		_flags = Flag (_flags | Broadcast);
		_flags = Flag (_flags | RF64_RIFF);
		break;

	case RF64:
		fmt = SF_FORMAT_RF64;
		_flags = Flag (_flags & ~Broadcast);
		break;

	default:
		fatal << string_compose (_("programming error: %1"), X_("unsupported audio header format requested")) << endmsg;
		abort(); /*NOTREACHED*/
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

	/* normal mode: do not open the file here - do that in {read,write}_unlocked() as needed
	 */
}

/** Constructor to be called for recovering files being used for
 * capture. They are in-session, they already exist, they should not
 * be writable. They are an odd hybrid (from a constructor point of
 * view) of the previous two constructors.
 */
SndFileSource::SndFileSource (Session& s, const string& path, int chn)
	: Source (s, DataType::AUDIO, path, Flag (0))
	  /* the final boolean argument is not used, its value is irrelevant. see audiofilesource.h for explanation */
	, AudioFileSource (s, path, Flag (0))
	, _sndfile (0)
	, _broadcast_info (0)
{
	_channel = chn;

	init_sndfile ();

        assert (Glib::file_test (_path, Glib::FILE_TEST_EXISTS));
	existence_check ();

	if (open()) {
		throw failed_constructor ();
	}
}

/** Constructor to losslessly compress existing source to flac */
SndFileSource::SndFileSource (Session& s, const AudioFileSource& other, const string& path, bool use16bits, Progress* progress)
	: Source(s, DataType::AUDIO, path, Flag ((other.flags () | default_writable_flags | NoPeakFile) & ~RF64_RIFF))
	, AudioFileSource (s, path, "", Flag ((other.flags () | default_writable_flags | NoPeakFile) & ~RF64_RIFF), /*unused*/ FormatFloat, /*unused*/ WAVE64)
	, _sndfile (0)
	, _broadcast_info (0)
{
	if (other.readable_length_samples () == 0) {
		throw failed_constructor();
	}

	assert (!Glib::file_test (_path, Glib::FILE_TEST_EXISTS));

	_channel = 0;
	init_sndfile ();

	_file_is_new = true;

	_info.channels = 1;
	_info.samplerate = other.sample_rate ();
	_info.format = SF_FORMAT_FLAC | (use16bits ? SF_FORMAT_PCM_16 : SF_FORMAT_PCM_24);

	/* flac is either read or write -- never both,
	 * so we need to special-case ::open () */
#ifdef PLATFORM_WINDOWS
	int fd = g_open (_path.c_str(), O_CREAT | O_RDWR, 0644);
#else
	int fd = ::open (_path.c_str(), O_CREAT | O_RDWR, 0644);
#endif
	if (fd == -1) {
		throw failed_constructor();
	}

	_sndfile = sf_open_fd (fd, SFM_WRITE, &_info, true);

	if (_sndfile == 0) {
		throw failed_constructor();
	}

#if 0
	/* setting flac compression quality above the default does not produce a significant size
	 * improvement (not for large raw recordings anyway, the_CLA tests 2017-10-02, >> 250MB files,
	 * ~1% smaller), but does have a significant encoding speed penalty.
	 *
	 * We still may expose this as option someday though, perhaps for opposite reason: "fast encoding"
	 */
	double flac_quality = 1; // libsndfile uses range 0..1 (mapped to flac 0..8), default is (5/8)
	if (sf_command (_sndfile, SFC_SET_COMPRESSION_LEVEL, &flac_quality, sizeof (double)) != SF_TRUE) {
		char errbuf[256];
		sf_error_str (_sndfile, errbuf, sizeof (errbuf) - 1);
		error << string_compose (_("Cannot set flac compression level: %1"), errbuf) << endmsg;
	}
#endif

	Sample buf[8192];
	samplecnt_t off = 0;
	float peak = 0;
	float norm = 1.f;

	/* normalize before converting to fixed point, calc gain factor */
	samplecnt_t len = other.read (buf, off, 8192, other.channel ());
	while (len > 0) {
		peak = compute_peak (buf, len, peak);
		off += len;
		len = other.read (buf, off, 8192, other.channel ());
		if (progress) {
			progress->set_progress (0.5f * (float) off / other.readable_length_samples ());
		}
	}

	if (peak > 0) {
		_gain *= peak;
		norm = 1.f / peak;
	}

	/* copy file */
	off = 0;
	len = other.read (buf, off, 8192, other.channel ());
	while (len > 0) {
		if (norm != 1.f) {
			for (samplecnt_t i = 0; i < len; ++i) {
				buf[i] *= norm;
			}
		}
		write (buf, len);
		off += len;
		len = other.read (buf, off, 8192, other.channel ());
		if (progress) {
			progress->set_progress (0.5f + 0.5f * (float) off / other.readable_length_samples ());
		}
	}
}

void
SndFileSource::init_sndfile ()
{
	/* although libsndfile says we don't need to set this,
	   valgrind and source code shows us that we do.
	*/

	memset (&_info, 0, sizeof(_info));

	AudioFileSource::HeaderPositionOffsetChanged.connect_same_thread (header_position_connection, boost::bind (&SndFileSource::handle_header_position_change, this));
}

void
SndFileSource::close ()
{
	if (_sndfile) {
		sf_close (_sndfile);
		_sndfile = 0;
		file_closed ();
	}
}

int
SndFileSource::open ()
{
	if (_sndfile) {
		return 0;
	}

// We really only want to use g_open for all platforms but because of this
// method(SndfileSource::open), the compiler(or at least GCC) is confused
// because g_open will expand to "open" on non-POSIX systems and needs the
// global namespace qualifer. The problem is since since C99 ::g_open will
// apparently expand to ":: open"
#ifdef PLATFORM_WINDOWS
	int fd = g_open (_path.c_str(), writable() ? O_CREAT | O_RDWR : O_RDONLY, writable() ? 0644 : 0444);
#else
	int fd = ::open (_path.c_str(), writable() ? O_CREAT | O_RDWR : O_RDONLY, writable() ? 0644 : 0444);
#endif

	if (fd == -1) {
		error << string_compose (
		             _ ("SndFileSource: cannot open file \"%1\" for %2"),
		             _path,
		             (writable () ? "read+write" : "reading")) << endmsg;
		return -1;
	}

	if ((_info.format & SF_FORMAT_TYPEMASK ) == SF_FORMAT_FLAC) {
		_sndfile = sf_open_fd (fd, writable () ? SFM_WRITE : SFM_READ, &_info, true);
	} else {
		_sndfile = sf_open_fd (fd, writable() ? SFM_RDWR : SFM_READ, &_info, true);
	}

	if (_sndfile == 0) {
		return -1;
	}

	if (_channel >= _info.channels) {
#ifndef HAVE_COREAUDIO
		error << string_compose(_("SndFileSource: file only contains %1 channels; %2 is invalid as a channel number"), _info.channels, _channel) << endmsg;
#endif
		sf_close (_sndfile);
		_sndfile = 0;
		return -1;
	}

	_length = _info.frames;

#ifdef HAVE_RF64_RIFF
	if (_file_is_new && _length == 0 && writable()) {
		if (_flags & RF64_RIFF) {
			if (sf_command (_sndfile, SFC_RF64_AUTO_DOWNGRADE, 0, 0) != SF_TRUE) {
				char errbuf[256];
				sf_error_str (_sndfile, errbuf, sizeof (errbuf) - 1);
				error << string_compose (_("Cannot mark RF64 audio file for automatic downgrade to WAV: %1"), errbuf)
				      << endmsg;
			}
		}
	}
#endif

#ifdef LIBSNDFILE_SUPPORTS_CUES_AND_LABELS
	if (!writable()) {
		cerr << "Check for cues with " << _sndfile << endl;
		uint32_t cc;
		if (sf_command (_sndfile, SFC_GET_CUE_COUNT, &cc, sizeof (cc)) == SF_TRUE) {
			cerr << "cc is " << cc << endl;
		}

		SF_CUES cues;
		if (sf_command (_sndfile, SFC_GET_CUE, &cues, sizeof (SF_CUES)) == SF_TRUE) {
			cerr << "Found " << cues.cue_count << " cues !\n";
			for (size_t n = 0; n < cues.cue_count; ++n) {
				_cue_markers.insert (CueMarker (string_compose (X_("cue %1"), n+1), cues.cue_points[n].sample_offset));
			}
		}
	}
#endif
	if (!_broadcast_info) {
		_broadcast_info = new BroadcastInfo;
	}

	bool bwf_info_exists = _broadcast_info->load_from_file (_sndfile);

	if (_file_is_new && _length == 0 && writable() && !bwf_info_exists) {
		/* newly created files will not have a BWF header at this point in time.
		 * Import will have called Source::set_natural_position() if one exists
		 * in the original. */
		header_position_offset = _natural_position.samples();
	}

	/* If a BWF header exists, set our _natural_position from it */
	if (bwf_info_exists) {
		set_natural_position (timepos_t (_broadcast_info->get_time_reference()));
	}

	if (_length != 0 && !bwf_info_exists) {
		delete _broadcast_info;
		_broadcast_info = 0;
		_flags = Flag (_flags & ~Broadcast);
	}

	/* Set the broadcast flag if the BWF info is already there. We need
	 * this when recovering or using existing files.
	 */

	if (bwf_info_exists) {
		_flags = Flag (_flags | Broadcast);
	}

	if (writable()) {
		sf_command (_sndfile, SFC_SET_UPDATE_HEADER_AUTO, 0, SF_FALSE);

                if (_flags & Broadcast) {

                        if (!_broadcast_info) {
                                _broadcast_info = new BroadcastInfo;
                        }

                        _broadcast_info->set_from_session (_session, header_position_offset);
                        _broadcast_info->set_description (string_compose ("BWF %1", _name));

                        if (!_broadcast_info->write_to_file (_sndfile)) {
                                error << string_compose (_("cannot set broadcast info for audio file %1 (%2); dropping broadcast info for this file"),
                                                         _path, _broadcast_info->get_error())
                                      << endmsg;
                                _flags = Flag (_flags & ~Broadcast);
                                delete _broadcast_info;
                                _broadcast_info = 0;
                        }
                }
        }

	return 0;
}

SndFileSource::~SndFileSource ()
{
	close ();
	delete _broadcast_info;
}

float
SndFileSource::sample_rate () const
{
	return _info.samplerate;
}

samplecnt_t
SndFileSource::read_unlocked (Sample *dst, samplepos_t start, samplecnt_t cnt) const
{
	assert (cnt >= 0);

	samplecnt_t nread;
	float *ptr;
	samplecnt_t real_cnt;
	samplepos_t file_cnt;

        if (writable() && !_sndfile) {
                /* file has not been opened yet - nothing written to it */
                memset (dst, 0, sizeof (Sample) * cnt);
                return cnt;
        }

        if (const_cast<SndFileSource*>(this)->open()) {
		error << string_compose (_("could not open file %1 for reading."), _path) << endmsg;
		return 0;
        }

        if (start > _length.samples()) {

		/* read starts beyond end of data, just memset to zero */

		file_cnt = 0;

        } else if (start + cnt > _length.samples()) {

		/* read ends beyond end of data, read some, memset the rest */

	        file_cnt = _length.samples() - start;

	} else {

		/* read is entirely within data */

		file_cnt = cnt;
	}

	assert (file_cnt >= 0);

	if (file_cnt != cnt) {
		samplepos_t delta = cnt - file_cnt;
		memset (dst+file_cnt, 0, sizeof (Sample) * delta);
	}

	if (file_cnt) {

		if (sf_seek (_sndfile, (sf_count_t) start, SEEK_SET|SFM_READ) != (sf_count_t) start) {
			char errbuf[256];
			sf_error_str (0, errbuf, sizeof (errbuf) - 1);
			error << string_compose(_("SndFileSource: could not seek to sample %1 within %2 (%3)"), start, _name.val().substr (1), errbuf) << endmsg;
			return 0;
		}

		if (_info.channels == 1) {
			samplecnt_t ret = sf_read_float (_sndfile, dst, file_cnt);
			if (ret != file_cnt) {
				char errbuf[256];
				sf_error_str (0, errbuf, sizeof (errbuf) - 1);
				error << string_compose(_("SndFileSource: @ %1 could not read %2 within %3 (%4) (len = %5, ret was %6)"), start, file_cnt, _name.val().substr (1), errbuf, _length, ret) << endl;
			}
			if (_gain != 1.f) {
				for (samplecnt_t i = 0; i < ret; ++i) {
					dst[i] *= _gain;
				}
			}
			return ret;
		}
	}

	real_cnt = cnt * _info.channels;

	Sample* interleave_buf = get_interleave_buffer (real_cnt);

	nread = sf_read_float (_sndfile, interleave_buf, real_cnt);
	ptr = interleave_buf + _channel;
	nread /= _info.channels;

	/* stride through the interleaved data */

	if (_gain != 1.f) {
		for (samplecnt_t n = 0; n < nread; ++n) {
			dst[n] = *ptr * _gain;
			ptr += _info.channels;
		}
	} else {
		for (samplecnt_t n = 0; n < nread; ++n) {
			dst[n] = *ptr;
			ptr += _info.channels;
		}
	}

	return nread;
}

samplecnt_t
SndFileSource::write_unlocked (Sample *data, samplecnt_t cnt)
{
        if (open()) {
                return 0; // failure
        }

        return nondestructive_write_unlocked (data, cnt);
}

samplecnt_t
SndFileSource::nondestructive_write_unlocked (Sample *data, samplecnt_t cnt)
{
	if (!writable()) {
		warning << string_compose (_("attempt to write a non-writable audio file source (%1)"), _path) << endmsg;
		return 0;
	}

	if (_info.channels != 1) {
		fatal << string_compose (_("programming error: %1 %2"), X_("SndFileSource::write called on non-mono file"), _path) << endmsg;
		abort(); /*NOTREACHED*/
		return 0;
	}

	samplepos_t sample_pos = _length.samples();

	if (write_float (data, sample_pos, cnt) != cnt) {
		return 0;
	}

	update_length (_length + timecnt_t (cnt, timepos_t (Temporal::AudioTime)));

	if (_build_peakfiles) {
		compute_and_write_peaks (data, sample_pos, cnt, true, true);
	}

	return cnt;
}

int
SndFileSource::update_header (samplepos_t when, struct tm& now, time_t tnow)
{
	set_natural_position (timepos_t (when));

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
	if (!writable()) {
		warning << string_compose (_("attempt to flush a non-writable audio file source (%1)"), _path) << endmsg;
		return -1;
	}

	if (_sndfile == 0) {
		error << string_compose (_("could not allocate file %1 to write header"), _path) << endmsg;
		return -1;
	}

	int const r = sf_command (_sndfile, SFC_UPDATE_HEADER_NOW, 0, 0) != SF_TRUE;

	return r;
}

void
SndFileSource::flush ()
{
	if (!writable()) {
		warning << string_compose (_("attempt to flush a non-writable audio file source (%1)"), _path) << endmsg;
		return;
	}

	if (_sndfile == 0) {
		error << string_compose (_("could not allocate file %1 to flush contents"), _path) << endmsg;
		return;
	}

	// Hopefully everything OK
	sf_write_sync (_sndfile);
}

int
SndFileSource::setup_broadcast_info (samplepos_t /*when*/, struct tm& now, time_t /*tnow*/)
{
	if (!writable()) {
		warning << string_compose (_("attempt to store broadcast info in a non-writable audio file source (%1)"), _path) << endmsg;
		return -1;
	}

        if (!_sndfile) {
		warning << string_compose (_("attempt to set BWF info for an un-opened audio file source (%1)"), _path) << endmsg;
		return -1;
        }

	if (!(_flags & Broadcast) || !_broadcast_info) {
		return 0;
	}

	_broadcast_info->set_originator_ref_from_session (_session);
	_broadcast_info->set_origination_time (&now);

	/* now update header position taking header offset into account */

	set_header_natural_position ();

	return 0;
}

void
SndFileSource::set_header_natural_position ()
{
	if (!(_flags & Broadcast)) {
		return;
	}
	assert (_broadcast_info);

	_broadcast_info->set_time_reference (_natural_position.samples());

	if (_sndfile == 0 || !_broadcast_info->write_to_file (_sndfile)) {
		error << string_compose (_("cannot set broadcast info for audio file %1 (%2); dropping broadcast info for this file"),
		                           _path, _broadcast_info->get_error())
		      << endmsg;
		_flags = Flag (_flags & ~Broadcast);
		delete _broadcast_info;
		_broadcast_info = 0;
	}
}

samplecnt_t
SndFileSource::write_float (Sample* data, samplepos_t sample_pos, samplecnt_t cnt)
{
	if ((_info.format & SF_FORMAT_TYPEMASK ) == SF_FORMAT_FLAC) {
		assert (_length == sample_pos);
	}
	else if (_sndfile == 0 || sf_seek (_sndfile, sample_pos, SEEK_SET|SFM_WRITE) < 0) {
		char errbuf[256];
		sf_error_str (0, errbuf, sizeof (errbuf) - 1);
		error << string_compose (_("%1: cannot seek to %2 (libsndfile error: %3)"), _path, sample_pos, errbuf) << endmsg;
		return 0;
	}

	if (sf_writef_float (_sndfile, data, cnt) != (ssize_t) cnt) {
		return 0;
	}

	return cnt;
}

void
SndFileSource::set_natural_position (timepos_t const & pos)
{
	AudioFileSource::set_natural_position (pos);
}

int
SndFileSource::get_soundfile_info (const string& path, SoundFileInfo& info, string& error_msg)
{
	SNDFILE *sf;
	SF_INFO sf_info;
	BroadcastInfo binfo;

	sf_info.format = 0; // libsndfile says to clear this before sf_open().

	if (path.empty() || Glib::file_test(path, Glib::FILE_TEST_IS_DIR)) {
		return false;
	}

#ifdef PLATFORM_WINDOWS
	int fd = g_open (path.c_str(), O_RDONLY, 0444);
#else
	int fd = ::open (path.c_str(), O_RDONLY, 0444);
#endif

	if (fd == -1) {
		error << string_compose ( _("SndFileSource: cannot open file \"%1\" for reading"), path)
		      << endmsg;
		return false;
	}
	if ((sf = sf_open_fd (fd, SFM_READ, &sf_info, true)) == 0) {
		char errbuf[1024];
		error_msg = sf_error_str (0, errbuf, sizeof (errbuf) - 1);
		return false;
	}

	info.samplerate  = sf_info.samplerate;
	info.channels    = sf_info.channels;
	info.length      = sf_info.frames;

        string major = sndfile_major_format(sf_info.format);
        string minor = sndfile_minor_format(sf_info.format);

        if (major.length() + minor.length() < 16) { /* arbitrary */
                info.format_name = string_compose("%1/%2", major, minor);
        } else {
                info.format_name = string_compose("%1\n%2", major, minor);
        }

	info.timecode = binfo.load_from_file (sf) ? binfo.get_time_reference() : 0;

	/* Mark Ogg/Vorbis files as not seekable, require conversion on import */
	int const type = sf_info.format & SF_FORMAT_TYPEMASK;
	int const sub = sf_info.format & SF_FORMAT_SUBMASK;
	info.seekable = (type != SF_FORMAT_OGG && sub != SF_FORMAT_VORBIS);

	sf_close (sf);

	return true;
}

bool
SndFileSource::one_of_several_channels () const
{
	return _info.channels > 1;
}

bool
SndFileSource::clamped_at_unity () const
{
	int const type = _info.format & SF_FORMAT_TYPEMASK;
	int const sub = _info.format & SF_FORMAT_SUBMASK;
	/* XXX: this may not be the full list of formats that are unclamped */
	return (sub != SF_FORMAT_FLOAT && sub != SF_FORMAT_DOUBLE && type != SF_FORMAT_OGG);
}

void
SndFileSource::file_closed ()
{
	/* stupid libsndfile updated the headers on close,
	   so touch the peakfile if it exists and has data
	   to make sure its time is as new as the audio
	   file.
	*/

	touch_peakfile ();
}

void
SndFileSource::set_path (const string& p)
{
        FileSource::set_path (p);
}
