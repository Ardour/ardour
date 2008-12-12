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
#include <ardour/session.h>
#include <ardour/audioengine.h>

#include <pbd/convert.h>
#include <pbd/pthread_utils.h>

namespace ARDOUR
{

/* ExportChannelConfiguration */

ExportChannelConfiguration::ExportChannelConfiguration (Session & session) :
  session (session),
  writer_thread (*this),
  status (session.get_export_status ()),
  files_written (false),
  split (false)
{

}


XMLNode &
ExportChannelConfiguration::get_state ()
{
	XMLNode * root = new XMLNode ("ExportChannelConfiguration");
	XMLNode * channel;
	
	root->add_property ("split", get_split() ? "true" : "false");
	root->add_property ("channels", to_string (get_n_chans(), std::dec));
	
	uint32_t i = 1;
	for (ExportChannelConfiguration::ChannelList::const_iterator c_it = channels.begin(); c_it != channels.end(); ++c_it) {
		channel = root->add_child ("Channel");
		if (!channel) { continue; }
		
		channel->add_property ("number", to_string (i, std::dec));
		(*c_it)->get_state (channel);
		
		++i;
	}
	
	return *root;
}

int
ExportChannelConfiguration::set_state (const XMLNode & root)
{
	XMLProperty const * prop;
	
	if ((prop = root.property ("split"))) {
		set_split (!prop->value().compare ("true"));
	}

	XMLNodeList channels = root.children ("Channel");
	for (XMLNodeList::iterator it = channels.begin(); it != channels.end(); ++it) {
		ExportChannelPtr channel (new PortExportChannel ());
		channel->set_state (*it, session);
		register_channel (channel);
	}

	return 0;
}

bool
ExportChannelConfiguration::all_channels_have_ports () const
{
	for (ChannelList::const_iterator it = channels.begin(); it != channels.end(); ++it) {
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
		throw ExportFailed (X_("Programming error: No timespan registered to channel configuration when requesting files to be written"));
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
		if (status->aborted()) { break; }
	
		channel = 0;
		for (ChannelList::iterator it = channels.begin(); it != channels.end(); ++it) {
			
			/* Get channel data */
			
			frames_read = timespan->get_data (channel_buffer, frames, *it);
			
			/* Interleave into file buffer */
			
			for (uint32_t i = 0; i < frames_read; ++i) {
				file_buffer[channel + (channel_count * i)] = channel_buffer[i];
			}
			
			++channel;
		}
		
		progress += frames_read;
		status->progress = (float) progress / timespan_length;
		
	} while (processor->process (file_buffer, frames_read) > 0);
	
	delete [] channel_buffer;
	delete [] file_buffer;
}

void *
ExportChannelConfiguration::_write_files (void *arg)
{
	notify_gui_about_thread_creation (pthread_self(), "Export post-processing");
	
	// cc can be trated like 'this'
	WriterThread & cc (*((WriterThread *) arg));
	
	try {
		for (FileConfigList::iterator it = cc->file_configs.begin(); it != cc->file_configs.end(); ++it) {
			if (cc->status->aborted()) {
				break;
			}
			cc->processor->prepare (it->first, it->second, cc->channels.size(), cc->split, cc->timespan->get_start());
			cc->write_file (); // Writes tempfile
			cc->processor->prepare_post_processors ();
			cc->processor->write_files();
		}
	} catch (ExportFailed & e) {
		cc->status->abort (true);
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
		timespan->register_channel (*it);
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
