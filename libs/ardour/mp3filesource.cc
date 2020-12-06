/*
 * Copyright (C) 2019 Robin Gareus <robin@gareus.org>
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

#include "pbd/error.h"
#include "pbd/compose.h"
#include "ardour/mp3filesource.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;

/** Constructor to be called for existing external-to-session files
 * Sources created with this method are never writable or removable.
 */

Mp3FileSource::Mp3FileSource (Session& s, const string& path, int chn, Flag flags)
	: Source (s, DataType::AUDIO, path,
			Source::Flag (flags & ~(Writable|Removable|RemovableIfEmpty|RemoveAtDestroy)))
	, AudioFileSource (s, path,
			Source::Flag (flags & ~(Writable|Removable|RemovableIfEmpty|RemoveAtDestroy)))
	, _mp3 (path)
	, _channel (chn)
{
	_length = timecnt_t (_mp3.length ());

	if (_channel >= (int) _mp3.channels ()) {
		error << string_compose("Mp3FileSource: file only contains %1 channels; %2 is invalid as a channel number (%3)", _mp3.channels (), _channel, name()) << endmsg;
		throw failed_constructor();
	}
}

Mp3FileSource::~Mp3FileSource ()
{
}

void
Mp3FileSource::close ()
{
}

samplecnt_t
Mp3FileSource::read_unlocked (Sample* dst, samplepos_t start, samplecnt_t cnt) const
{
	return _mp3.read_unlocked (dst, start, cnt, _channel);
}

int
Mp3FileSource::get_soundfile_info (string path, SoundFileInfo& _info, string& error_msg)
{
	try {
		Mp3FileImportableSource mp3 (path);
		_info.samplerate  = mp3.samplerate ();
		_info.channels    = mp3.channels ();
		_info.length      = mp3.length ();
		_info.format_name = string_compose (_("MPEG Layer %1 (%2 kbps)"), mp3.layer (), mp3.bitrate ());
		_info.timecode    = 0;
		_info.seekable    = false;
		return 0;
	} catch (...) {}
	return -1;
}
