/*
 * Copyright (C) 2021 Marijn Kruisselbrink <mek@google.com>
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

#ifndef _ardour_ffmpegfile_importable_source_h_
#define _ardour_ffmpegfile_importable_source_h_

#include "pbd/g_atomic_compat.h"
#include "pbd/ringbuffer.h"

#include "ardour/importable_source.h"
#include "ardour/libardour_visibility.h"
#include "ardour/system_exec.h"
#include "ardour/types.h"

namespace ARDOUR {

class LIBARDOUR_API FFMPEGFileImportableSource : public ImportableSource
{
public:
	enum {
		ALL_CHANNELS = -1,
	};

	FFMPEGFileImportableSource (const std::string& path, int channel = ALL_CHANNELS);

	virtual ~FFMPEGFileImportableSource ();

	/* ImportableSource API */
	samplecnt_t read (Sample*, samplecnt_t nframes);
	uint32_t    channels () const { return _channels; }
	samplecnt_t length () const { return _length; }
	samplecnt_t samplerate () const { return _samplerate; }
	void        seek (samplepos_t pos);
	samplepos_t natural_position () const { return _natural_position; }
	bool        clamped_at_unity () const { return false; }

	std::string format_name () const { return _format_name; }

private:
	void start_ffmpeg ();
	void reset ();

	void did_read_data (std::string data, size_t size);

	std::string _path;
	int         _channel;

	uint32_t    _channels;
	samplecnt_t _length;
	samplecnt_t _samplerate;
	samplepos_t _natural_position;
	std::string _format_name;

	PBD::RingBuffer<Sample> _buffer;
	/* Set to 1 to indicate that ffmpeg should be terminating. */
	GATOMIC_QUAL gint _ffmpeg_should_terminate;

	/* To make sure we don't try to parse partial floats, we might have a couple of bytes
	 * of leftover unparsable data after any `did_read_data` call. Those couple of bytes are
	 * stored here until the next `did_read_data` call.
	 */
	std::string _leftover_data;

	samplecnt_t _read_pos;

	ARDOUR::SystemExec*   _ffmpeg_exec;
	PBD::ScopedConnection _ffmpeg_conn;
};

} // namespace ARDOUR

#endif /* _ardour_ffmpegfile_importable_source_h_ */
