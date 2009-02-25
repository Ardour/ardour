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

#include "ardour/export_processor.h"

#include "pbd/error.h"
#include "pbd/filesystem.h"

#include "ardour/session.h"
#include "ardour/audiofile_tagger.h"
#include "ardour/broadcast_info.h"
#include "ardour/export_failed.h"
#include "ardour/export_filename.h"
#include "ardour/export_status.h"
#include "ardour/export_format_specification.h"

#include "i18n.h"

using namespace PBD;

namespace ARDOUR
{

sigc::signal<void, Glib::ustring> ExportProcessor::WritingFile;

ExportProcessor::ExportProcessor (Session & session) :
  session (session),
  status (session.get_export_status()),
  blocksize (session.get_block_size()),
  frame_rate (session.frame_rate())
{
	reset ();
}

ExportProcessor::~ExportProcessor ()
{

}

void
ExportProcessor::reset ()
{
	file_sinks.clear();
	writer_list.clear();
	filename.reset();
	normalizer.reset();
	src.reset();
	peak_reader.reset();
	temp_file.reset();
}

int
ExportProcessor::prepare (FormatPtr format, FilenamePtr fname, uint32_t chans, bool split, nframes_t start)
{
	status->format++;
	temp_file_length = 0;

	/* Reset just to be sure all references are dropped */
	
	reset();
	
	/* Get parameters needed later on */
	
	channels = chans;
	split_files = split;
	filename = fname;
	tag = format->tag();
	broadcast_info = format->has_broadcast_info();
	normalize = format->normalize();
	trim_beginning = format->trim_beginning();
	trim_end = format->trim_end();
	silence_beginning = format->silence_beginning();
	silence_end = format->silence_end();
	
	/* SRC */
	
	src.reset (new SampleRateConverter (channels, frame_rate, format->sample_rate(), format->src_quality()));
	
	/* Construct export pipe to temp file */
	
	status->stage = export_PostProcess;
	
	if (normalize) {
		/* Normalizing => we need a normalizer, peak reader and tempfile */
		
		normalizer.reset (new Normalizer (channels, format->normalize_target()));
	
		peak_reader.reset (new PeakReader (channels));
		temp_file.reset (new ExportTempFile (channels, format->sample_rate()));
		
		src->pipe_to (peak_reader);
		peak_reader->pipe_to (temp_file);
	
	} else if (trim_beginning || trim_end) {
		/* Not normalizing, but silence will be trimmed => need for a tempfile */
	
		temp_file.reset (new ExportTempFile (channels, format->sample_rate()));
		src->pipe_to (temp_file);
		
	} else {
		/* Due to complexity and time running out, a tempfile will be created for this also... */
		
		temp_file.reset (new ExportTempFile (channels, format->sample_rate()));
		src->pipe_to (temp_file);
	}

	/* Ensure directory exists */
	
	sys::path folder (filename->get_folder());
	if (!sys::exists (folder)) {
		if (!sys::create_directory (folder)) {
			throw ExportFailed (X_("sys::create_directory failed for export dir"));
		}
	}
	
	/* prep file sinks */
	
	if (split) {
		filename->include_channel = true;
		for (uint32_t chn = 1; chn <= channels; ++chn) {
			filename->set_channel (chn);
			ExportFileFactory::FilePair pair = ExportFileFactory::create (format, 1, filename->get_path (format));
			file_sinks.push_back (pair.first);
			writer_list.push_back (pair.second);
			WritingFile (filename->get_path (format));
		}

	} else {
		ExportFileFactory::FilePair pair = ExportFileFactory::create (format, channels, filename->get_path (format));
		file_sinks.push_back (pair.first);
		writer_list.push_back (pair.second);
		WritingFile (filename->get_path (format));
	}
	
	/* Set position info */
	
	nframes_t start_position = ((double) format->sample_rate() / frame_rate) * start + 0.5;
	
	for (FileWriterList::iterator it = writer_list.begin(); it != writer_list.end(); ++it) {
		(*it)->set_position (start_position);
	}
	
	/* set broadcast info if necessary */
	
	if (broadcast_info) {
		for (FileWriterList::iterator it = writer_list.begin(); it != writer_list.end(); ++it) {
		
			BroadcastInfo bci;
			bci.set_from_session (session, (*it)->position());
			
			boost::shared_ptr<SndfileWriterBase> sndfile_ptr;
			if ((sndfile_ptr = boost::dynamic_pointer_cast<SndfileWriterBase> (*it))) {
				if (!bci.write_to_file (sndfile_ptr->get_sndfile())) {
					std::cerr << bci.get_error() << std::endl;
				}
			} else {
				if (!bci.write_to_file ((*it)->filename())) {
					std::cerr << bci.get_error() << std::endl;
				}
			}
		}
	}

	return 0;
}

nframes_t
ExportProcessor::process (float * data, nframes_t frames)
{
	nframes_t frames_written = src->write (data, frames);
	temp_file_length += frames_written;
	return frames_written;
}

void
ExportProcessor::prepare_post_processors ()
{
	/* Set end of input and do last write */
	float  dummy;
	src->set_end_of_input ();
	src->write (&dummy, 0);
	
	/* Trim and add silence */
	
	temp_file->trim_beginning (trim_beginning);
	temp_file->trim_end (trim_end);
	
	temp_file->set_silence_beginning (silence_beginning);
	temp_file->set_silence_end (silence_end);
	
	/* Set up normalizer */
	
	if (normalize) {
		normalizer->set_peak (peak_reader->get_peak ());
	}
}

void
ExportProcessor::write_files ()
{
	/* Write to disk */
	
	status->stage = export_Write;
	temp_file_position = 0;
	
	uint32_t buffer_size = 4096; // TODO adjust buffer size?
	float * buf = new float[channels * buffer_size];
	int frames_read;
	
	FloatSinkPtr disk_sink;
	
	if (normalize) {
		disk_sink = boost::dynamic_pointer_cast<FloatSink> (normalizer);
		normalizer->pipe_to (file_sinks[0]);
	} else {
		disk_sink = file_sinks[0];
	}
	
	if (split_files) {
		
		/* Get buffers for each channel separately */
		
		std::vector<float *> chan_bufs;
		
		for (uint32_t i = 0; i < channels; ++i) {
			chan_bufs.push_back(new float[buffer_size]);
		}
		
		/* de-interleave and write files */
		
		while ((frames_read = temp_file->read (buf, buffer_size)) > 0) {
			for (uint32_t channel = 0; channel < channels; ++channel) {
				for (uint32_t i = 0; i < buffer_size; ++i) {
					chan_bufs[channel][i] = buf[channel + (channels * i)];
				}
				if (normalize) {
					normalizer->pipe_to (file_sinks[channel]);
				} else {
					disk_sink = file_sinks[channel];
				}
				disk_sink->write (chan_bufs[channel], frames_read);
			}
			
			if (status->aborted()) { break; }
			temp_file_position += frames_read;
			status->progress = (float) temp_file_position / temp_file_length;
		}
		
		/* Clean up */
		
		for (std::vector<float *>::iterator it = chan_bufs.begin(); it != chan_bufs.end(); ++it) {
			delete[] *it;
		}
		
	} else {
		while ((frames_read = temp_file->read (buf, buffer_size)) > 0) {
			disk_sink->write (buf, frames_read);
			
			if (status->aborted()) { break; }
			temp_file_position += frames_read;
			status->progress = (float) temp_file_position / temp_file_length;
		}
	}
	
	delete [] buf;
	
	/* Tag files if necessary and send exported signal */
	
	
	for (FileWriterList::iterator it = writer_list.begin(); it != writer_list.end(); ++it) {
		if (tag) {
			AudiofileTagger::tag_file ((*it)->filename(), session.metadata());
		}
		session.Exported ((*it)->filename(), session.name());
	}
}

}; // namespace ARDOUR
