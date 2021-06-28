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

#include <boost/property_tree/json_parser.hpp>
#include <glibmm.h>

#include "pbd/compose.h"
#include "pbd/error.h"
#include "pbd/i18n.h"

#include "ardour/ffmpegfileimportable.h"
#include "ardour/filesystem_paths.h"

using namespace ARDOUR;

static void
receive_stdout (std::string* out, const std::string& data, size_t size)
{
	*out += data;
}

FFMPEGFileImportableSource::FFMPEGFileImportableSource (const std::string& path, int channel)
	: _path (path)
	, _channel (channel)
	, _buffer (32768)
	, _ffmpeg_should_terminate (0)
	, _read_pos (0)
	, _ffmpeg_exec (0)
{
	std::string ffprobe_exe, unused;
	if (!ArdourVideoToolPaths::transcoder_exe (unused, ffprobe_exe)) {
		PBD::error << "FFMPEGFileImportableSource: Can't find ffprobe and ffmpeg" << endmsg;
		throw failed_constructor ();
	}

	int    a    = 0;
	char** argp = (char**)calloc (10, sizeof (char*));

	argp[a++] = strdup (ffprobe_exe.c_str ());
	argp[a++] = strdup (_path.c_str ());
	argp[a++] = strdup ("-show_streams");
	argp[a++] = strdup ("-of");
	argp[a++] = strdup ("json");

	ARDOUR::SystemExec* exec = new ARDOUR::SystemExec (ffprobe_exe, argp);
	PBD::info << "Probe command: { " << exec->to_s () << "}" << endmsg;

	if (exec->start ()) {
		PBD::error << "FFMPEGFileImportableSource: External decoder (ffprobe) cannot be started." << endmsg;
		delete exec;
		throw failed_constructor ();
	}

	try {
		PBD::ScopedConnection c;
		std::string           ffprobe_output;
		exec->ReadStdout.connect_same_thread (c, boost::bind (&receive_stdout, &ffprobe_output, _1, _2));

		/* wait for ffprobe process to exit */
		exec->wait ();

		namespace pt = boost::property_tree;
		pt::ptree          root;
		std::istringstream is (ffprobe_output);

		pt::read_json (is, root);

		// TODO: Find the stream with the most channels, rather than whatever the first one is.
		_channels         = root.get<int> ("streams..channels");
		_length           = root.get<int64_t> ("streams..duration_ts");
		_samplerate       = root.get<int> ("streams..sample_rate");
		_natural_position = root.get<int64_t> ("streams..start_pts");
		_format_name      = root.get<std::string> ("streams..codec_long_name");
		delete exec;
	} catch (...) {
		PBD::error << "FFMPEGFileImportableSource: Failed to read file metadata" << endmsg;
		delete exec;
		throw failed_constructor ();
	}

	if (_channel != ALL_CHANNELS && (_channel < 0 || _channel > (int)channels ())) {
		PBD::error << string_compose ("FFMPEGFileImportableSource: file only contains %1 channels; %2 is invalid as a channel number", channels (), _channel) << endmsg;
		throw failed_constructor ();
	}
}

FFMPEGFileImportableSource::~FFMPEGFileImportableSource ()
{
	reset ();
}

void
FFMPEGFileImportableSource::seek (samplepos_t pos)
{
	if (pos < _read_pos) {
		reset ();
	}

	if (!_ffmpeg_exec) {
		start_ffmpeg ();
	}

	while (_read_pos < pos) {
		guint read_space = _buffer.read_space ();
		if (read_space == 0) {
			if (!_ffmpeg_exec->is_running ()) {
				// FFMPEG quit, must have reached EOF.
				PBD::warning << string_compose ("FFMPEGFileImportableSource: Reached EOF while trying to seek to %1", pos) << endmsg;
				break;
			}
			// TODO: don't just spin, but use some signalling
			Glib::usleep (1000);
			continue;
		}
		guint inc = std::min<guint> (read_space, pos - _read_pos);
		_buffer.increment_read_idx (inc);
		_read_pos += inc;
	}
}

samplecnt_t
FFMPEGFileImportableSource::read (Sample* dst, samplecnt_t nframes)
{
	if (!_ffmpeg_exec) {
		start_ffmpeg ();
	}

	samplecnt_t total_read = 0;
	while (nframes > 0) {
		guint read = _buffer.read (dst + total_read, nframes);
		if (read == 0) {
			if (!_ffmpeg_exec->is_running ()) {
				// FFMPEG quit, must have reached EOF.
				break;
			}
			// TODO: don't just spin, but use some signalling
			Glib::usleep (1000);
			continue;
		}
		nframes -= read;
		total_read += read;
		_read_pos += read;
	}

	return total_read;
}

void
FFMPEGFileImportableSource::start_ffmpeg ()
{
	std::string ffmpeg_exe, unused;
	ArdourVideoToolPaths::transcoder_exe (ffmpeg_exe, unused);

	int    a    = 0;
	char** argp = (char**)calloc (16, sizeof (char*));
	char   tmp[32];
	argp[a++] = strdup (ffmpeg_exe.c_str ());
	argp[a++] = strdup ("-nostdin");
	argp[a++] = strdup ("-i");
	argp[a++] = strdup (_path.c_str ());
	if (_channel != ALL_CHANNELS) {
		argp[a++] = strdup ("-map_channel");
		snprintf (tmp, sizeof (tmp), "0.0.%d", _channel);
		argp[a++] = strdup (tmp);
	}
	argp[a++] = strdup ("-f");
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
	argp[a++] = strdup ("f32le");
#else
	argp[a++] = strdup ("f32be");
#endif
	argp[a++] = strdup ("-");

	_ffmpeg_exec = new ARDOUR::SystemExec (ffmpeg_exe, argp);
	PBD::info << "Decode command: { " << _ffmpeg_exec->to_s () << "}" << endmsg;
	if (_ffmpeg_exec->start ()) {
		PBD::error << "FFMPEGFileImportableSource: External decoder (ffmpeg) cannot be started." << endmsg;
		throw std::runtime_error ("Failed to start ffmpeg");
	}

	_ffmpeg_exec->ReadStdout.connect_same_thread (_ffmpeg_conn, boost::bind (&FFMPEGFileImportableSource::did_read_data, this, _1, _2));
}

void
FFMPEGFileImportableSource::reset ()
{
	// TODO: actually signal did_read_data to unblock
	g_atomic_int_set (&_ffmpeg_should_terminate, 1);
	delete _ffmpeg_exec;
	_ffmpeg_exec = 0;
	_ffmpeg_conn.disconnect ();
	_buffer.reset ();
	_read_pos = 0;
	g_atomic_int_set (&_ffmpeg_should_terminate, 0);
}

void
FFMPEGFileImportableSource::did_read_data (std::string data, size_t size)
{
	// Prepend the left-over data from a previous chunk of received data to this chunk.
	data                  = _leftover_data + data;
	samplecnt_t n_samples = data.length () / sizeof (float);

	// Stash leftover data.
	_leftover_data = data.substr (n_samples * sizeof (float));

	const char* cur = data.data ();
	while (n_samples > 0) {
		if (g_atomic_int_get (&_ffmpeg_should_terminate)) {
			break;
		}

		PBD::RingBuffer<float>::rw_vector wv;
		_buffer.get_write_vector (&wv);
		if (wv.len[0] == 0) {
			// TODO: don't just spin, but use some signalling
			Glib::usleep (1000);
			continue;
		}

		samplecnt_t written = 0;
		for (int i = 0; i < 2; ++i) {
			samplecnt_t cnt = std::min<samplecnt_t> (n_samples, wv.len[i]);
			if (!cnt || !wv.buf[i]) {
				break;
			}
			memcpy (wv.buf[i], cur, cnt * sizeof (float));
			written += cnt;
			n_samples -= cnt;
			cur += cnt * sizeof (float);
		}
		_buffer.increment_write_idx (written);
	}
}
