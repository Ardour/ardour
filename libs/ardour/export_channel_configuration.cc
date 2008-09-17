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

#include <ardour/export_channel_configuration.h>

#include <ardour/export_handler.h>
#include <ardour/export_filename.h>
#include <ardour/export_processor.h>
#include <ardour/export_timespan.h>

#include <ardour/audio_port.h>
#include <ardour/export_failed.h>
#include <ardour/midi_port.h>
#include <pbd/pthread_utils.h>

namespace ARDOUR
{

/* ExportChannel */

ExportChannel::ExportChannel ()
{

}

ExportChannel::~ExportChannel ()
{

}

void
ExportChannel::read_ports (float * data, nframes_t frames) const
{
	memset (data, 0, frames * sizeof (float));

	for (iterator it = begin(); it != end(); ++it) {
		if (*it != 0) {
			Sample* port_buffer = (*it)->get_audio_buffer().data();
			
			for (uint32_t i = 0; i < frames; ++i) {
				data[i] += (float) port_buffer[i];
			}
		}
	}
}

/* ExportChannelConfiguration */

ExportChannelConfiguration::ExportChannelConfiguration (ExportStatus & status) :
  writer_thread (*this),
  status (status),
  files_written (false),
  split (false)
{

}

ExportChannelConfiguration::~ExportChannelConfiguration ()
{

}

bool
ExportChannelConfiguration::all_channels_have_ports ()
{
	for (ChannelList::iterator it = channels.begin(); it != channels.end(); ++it) {
		if ((*it)->empty ()) { return false; }
	}
	
	return true;
}

bool
ExportChannelConfiguration::write_files (boost::shared_ptr<ExportProcessor> new_processor)
{
	if (files_written || writer_thread.running) {
		return false;
	}
	
	files_written = true;

	if (!timespan) {
		throw ExportFailed (_("Export failed due to a programming error"), _("No timespan registered to channel configuration when requesting files to be written"));
	}
	
	/* Take a local copy of the processor to be used in the thread that is created below */
	
	processor.reset (new_processor->copy());
	
	/* Create new thread for post processing */
	
	pthread_create (&writer_thread.thread, 0, _write_files, &writer_thread);
	writer_thread.running = true;
	pthread_detach (writer_thread.thread);
	
	return true;
}

void
ExportChannelConfiguration::write_file ()
{
	timespan->rewind ();
	nframes_t progress = 0;
	nframes_t timespan_length = timespan->get_length();

	nframes_t frames = 2048; // TODO good block size ?
	nframes_t frames_read = 0;
	
	float * channel_buffer = new float [frames];
	float * file_buffer = new float [channels.size() * frames];
	uint32_t channel_count = channels.size();
	uint32_t channel;
	
	do {
		if (status.aborted()) { break; }
	
		channel = 0;
		for (ChannelList::iterator it = channels.begin(); it != channels.end(); ++it) {
			
			/* Get channel data */
			
			frames_read = timespan->get_data (channel_buffer, frames, **it);
			
			/* Interleave into file buffer */
			
			for (uint32_t i = 0; i < frames_read; ++i) {
				file_buffer[channel + (channel_count * i)] = channel_buffer[i];
			}
			
			++channel;
		}
		
		progress += frames_read;
		status.progress = (float) progress / timespan_length;
		
	} while (processor->process (file_buffer, frames_read) > 0);
	
	delete [] channel_buffer;
	delete [] file_buffer;
}

void *
ExportChannelConfiguration::_write_files (void *arg)
{

	PBD::ThreadCreated (pthread_self(), "Export post-processing");
	
	// cc can be trated like 'this'
	WriterThread & cc (*((WriterThread *) arg));
	
	for (FileConfigList::iterator it = cc->file_configs.begin(); it != cc->file_configs.end(); ++it) {
		if (cc->status.aborted()) {
			break;
		}
		cc->processor->prepare (it->first, it->second, cc->channels.size(), cc->split, cc->timespan->get_start());
		cc->write_file (); // Writes tempfile
		cc->processor->prepare_post_processors ();
		cc->processor->write_files();
	}
	
	cc.running = false;
	cc->files_written = true;
	cc->FilesWritten();
	
	return 0; // avoid compiler warnings
}

void
ExportChannelConfiguration::register_with_timespan (TimespanPtr new_timespan)
{
	timespan = new_timespan;
	
	for (ChannelList::iterator it = channels.begin(); it != channels.end(); ++it) {
		timespan->register_channel (**it);
	}
}

void
ExportChannelConfiguration::unregister_all ()
{
	timespan.reset();
	processor.reset();
	file_configs.clear();
	files_written = false;
}

} // namespace ARDOUR
