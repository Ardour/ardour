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

#include "ardour/export_timespan.h"

#include "ardour/export_channel_configuration.h"
#include "ardour/export_filename.h"
#include "ardour/export_file_io.h"
#include "ardour/export_failed.h"

namespace ARDOUR
{

ExportTimespan::ExportTimespan (ExportStatusPtr status, nframes_t frame_rate) :
  status (status),
  start_frame (0),
  end_frame (0),
  position (0),
  frame_rate (frame_rate)
{
}

ExportTimespan::~ExportTimespan ()
{
}

void
ExportTimespan::register_channel (ExportChannelPtr channel)
{
	TempFilePtr ptr (new ExportTempFile (1, frame_rate));
	ChannelFilePair pair (channel, ptr);
	filemap.insert (pair);
}

void
ExportTimespan::rewind ()
{
	for (TempFileMap::iterator it = filemap.begin(); it != filemap.end(); ++it) {
		it->second->reset_read ();
	}
}

nframes_t
ExportTimespan::get_data (float * data, nframes_t frames, ExportChannelPtr channel)
{
	TempFileMap::iterator it = filemap.find (channel);
	if (it == filemap.end()) {
		throw ExportFailed (X_("Trying to get data from ExportTimespan for channel that was never registered!"));
	}
	
	return it->second->read (data, frames);
}

void
ExportTimespan::set_range (nframes_t start, nframes_t end)
{
	start_frame = start;
	position = start_frame;
	end_frame = end;
}

int
ExportTimespan::process (nframes_t frames)
{
	status->stage = export_ReadTimespan;

	/* update position */

	nframes_t frames_to_read;
	
	if (position + frames <= end_frame) {
		frames_to_read = frames;
	} else {
		frames_to_read = end_frame - position;
		status->stop = true;
	}
	
	position += frames_to_read;
	status->progress = (float) (position - start_frame) / (end_frame - start_frame);

	/* Read channels from ports and save to tempfiles */

	float * data = new float[frames_to_read];
	
	for (TempFileMap::iterator it = filemap.begin(); it != filemap.end(); ++it) {
		it->first->read (data, frames_to_read);
		it->second->write (data, frames_to_read);
	}
	
	delete [] data;

	return 0;
}


} // namespace ARDOUR
