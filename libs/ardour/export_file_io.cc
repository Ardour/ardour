/*
    Copyright (C) 2008 Paul Davis
    Author: Sakari Bergen

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
#include <cstring>

#include <string.h>

#include <ardour/export_file_io.h>
#include <ardour/export_failed.h>
#include <pbd/failed_constructor.h>

#include "i18n.h"

using namespace PBD;

namespace ARDOUR
{

/* SndfileWriterBase */

SndfileWriterBase::SndfileWriterBase (int channels, nframes_t samplerate, int format, string const & path) :
  ExportFileWriter (path)
{
	char errbuf[256];
	
	sf_info.channels = channels;
	sf_info.samplerate = samplerate;
	sf_info.format = format;
	
	if (!sf_format_check (&sf_info)) {
		throw ExportFailed (_("Export failed due to a programming error"), "Invalid format given for SndfileWriter!");
	}
	
	if (path.length() == 0) {
		throw ExportFailed (_("Export failed due to a programming error"), "No output file specified for SndFileWriter");
	}

	/* TODO add checks that the directory path exists, and also 
	   check if we are overwriting an existing file...
	*/
	
	// Open file TODO make sure we have enough disk space for the output 
	if (path.compare ("temp")) {
		if ((sndfile = sf_open (path.c_str(), SFM_WRITE, &sf_info)) == 0) {
			sf_error_str (0, errbuf, sizeof (errbuf) - 1);
			throw ExportFailed (string_compose(_("Export: cannot open output file \"%1\""), path),
			                    string_compose(_("Export: cannot open output file \"%1\" for SndFileWriter (%2)"), path, errbuf));
		}
	} else {
		FILE * file;
		if (!(file = tmpfile ())) {
			throw ExportFailed (_("Export failed due to a programming error"), "Cannot open tempfile");
		}
		sndfile = sf_open_fd (fileno(file), SFM_RDWR, &sf_info, true);
	}
}

SndfileWriterBase::~SndfileWriterBase ()
{
	sf_close (sndfile);
}

/* SndfileWriter */

template <typename T>
SndfileWriter<T>::SndfileWriter (int channels, nframes_t samplerate, int format, string const & path) :
  SndfileWriterBase (channels, samplerate, format, path)
{
	// init write function
	init ();
}

template <>
void
SndfileWriter<float>::init ()
{
	write_func = &sf_writef_float;
}

template <>
void
SndfileWriter<int>::init ()
{
	write_func = &sf_writef_int;
}

template <>
void
SndfileWriter<short>::init ()
{
	write_func = &sf_writef_short;
}

template <typename T>
nframes_t
SndfileWriter<T>::write (T * data, nframes_t frames)
{
	char errbuf[256];
	nframes_t written = (*write_func) (sndfile, data, frames);
	if (written != frames) {
		sf_error_str (sndfile, errbuf, sizeof (errbuf) - 1);
		throw ExportFailed (_("Writing export file failed"), string_compose(_("Could not write data to output file (%1)"), errbuf));
	}
	
	if (GraphSink<T>::end_of_input) {
		sf_write_sync (sndfile);
	}
	
	return frames;
}

template class SndfileWriter<short>;
template class SndfileWriter<int>;
template class SndfileWriter<float>;

/* ExportTempFile */

ExportTempFile::ExportTempFile (uint32_t channels, nframes_t samplerate) :
  SndfileWriter<float> (channels, samplerate, SF_FORMAT_RAW | SF_FORMAT_FLOAT | SF_ENDIAN_FILE, "temp"),
  channels (channels),
  reading (false),
  start (0),
  end (0),
  beginning_processed (false),
  end_processed (false),
  silence_beginning (0),
  silence_end (0),
  end_set (false)
{
}

nframes_t
ExportTempFile::read (float * data, nframes_t frames)
{
	nframes_t frames_read = 0;
	nframes_t to_read = 0;
	sf_count_t read_status = 0;
	
	/* Initialize state at first read */
	
	if (!reading) {
		if (!end_set) {
			end = get_length();
			end_set = true;
		}
		locate_to (start);
		reading = true;
	}
	
	/* Add silence to beginning */
	
	if (silence_beginning > 0) {
		if (silence_beginning >= frames) {
			memset (data, 0, channels * frames * sizeof (float));
			silence_beginning -= frames;
			return frames;
		}
		
		memset (data, 0, channels * silence_beginning * sizeof (float));
		
		frames_read += silence_beginning;
		data += channels * silence_beginning;
		silence_beginning = 0;
	}
	
	/* Read file, but don't read past end */
	
	if (get_read_position() >= end) {
		// File already read, do nothing!
	} else {
		if ((get_read_position() + (frames - frames_read)) > end) {
			to_read = end - get_read_position();
		} else {
			to_read = frames - frames_read;
		}
		
		read_status = sf_readf_float (sndfile, data, to_read);
		
		frames_read += to_read;
		data += channels * to_read;
	}
	
	/* Check for errors */
	
	if (read_status != to_read) {
		throw ExportFailed (_("Reading export file failed"), _("Error reading temporary export file, export might not be complete!"));
	}
	
	/* Add silence at end */
	
	if (silence_end > 0) {
		to_read = frames - frames_read;
		if (silence_end < to_read) {
			to_read = silence_end;
		}
		
		memset (data, 0, channels * to_read * sizeof (float));
		silence_end -= to_read;
		frames_read += to_read;
	}
	
	return frames_read;
}

nframes_t
ExportTempFile::trim_beginning (bool yn)
{
	if (!yn) {
		start = 0;
		return start;
	}

	if (!beginning_processed) {
		process_beginning ();
	}
	
	start = silent_frames_beginning;
	return start;
	
}

nframes_t
ExportTempFile::trim_end (bool yn)
{
	end_set = true;

	if (!yn) {
		end = get_length();
		return end;
	}

	if (!end_processed) {
		process_end ();
	}
	
	end = silent_frames_end;
	return end;
}


void
ExportTempFile::process_beginning ()
{
	nframes_t frames = 1024;
	nframes_t frames_read;
	float * buf = new float[channels * frames];
	
	nframes_t pos = 0;
	locate_to (pos);
	
	while ((frames_read = _read (buf, frames)) > 0) {
		for (nframes_t i = 0; i < frames_read; i++) {
			for (uint32_t chn = 0; chn < channels; ++chn) {
				if (buf[chn + i * channels] != 0.0f) {
					--pos;
					goto out;
				}
			}
			++pos;
		}
	}
	
	out:
	
	silent_frames_beginning = pos;
	beginning_processed = true;
	
	delete [] buf;
}

void
ExportTempFile::process_end ()
{
	nframes_t frames = 1024;
	nframes_t frames_read;
	float * buf = new float[channels * frames];
	
	nframes_t pos = get_length() - 1;
	
	while (pos > 0) {
		if (pos > frames) {
			locate_to (pos - frames);
			frames_read = _read (buf, frames);
		} else {
			// Last time reading
			locate_to (0);
			frames_read = _read (buf, pos);
		}
		
		for (nframes_t i = frames_read; i > 0; --i) {
			for (uint32_t chn = 0; chn < channels; ++chn) {
				if (buf[chn + (i - 1) * channels] != 0.0f) {
					goto out;
				}
			}
			--pos;
		}
	}
	
	out:
	
	silent_frames_end = pos;
	end_processed = true;
	
	delete [] buf;
}

void
ExportTempFile::set_silence_beginning (nframes_t frames)
{
	silence_beginning = frames;
}

void
ExportTempFile::set_silence_end (nframes_t frames)
{
	silence_end = frames;
}

sf_count_t
ExportTempFile::get_length ()
{
	sf_count_t pos = get_position();
	sf_count_t len = sf_seek (sndfile, 0, SEEK_END);
	locate_to (pos);
	return len;
}

sf_count_t
ExportTempFile::get_position ()
{
	return sf_seek (sndfile, 0, SEEK_CUR);
}

sf_count_t
ExportTempFile::get_read_position ()
{
	return sf_seek (sndfile, 0, SEEK_CUR | SFM_READ);
}

sf_count_t
ExportTempFile::locate_to (nframes_t frames)
{
	return sf_seek (sndfile, frames, SEEK_SET);
}

sf_count_t
ExportTempFile::_read (float * data, nframes_t frames)
{
	return sf_readf_float (sndfile, data, frames);
}

};
