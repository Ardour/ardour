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

#ifndef __ardour_export_timespan_h__
#define __ardour_export_timespan_h__

#include <map>
#include <list>

#include <glibmm/ustring.h>

#include "ardour/export_status.h"
#include "ardour/export_channel.h"
#include "ardour/ardour.h"

namespace ARDOUR
{

class ExportChannel;
class ExportTempFile;

class ExportTimespan : public sigc::trackable
{
  private:
	typedef boost::shared_ptr<ExportTempFile> TempFilePtr;
	typedef std::pair<ExportChannelPtr, TempFilePtr> ChannelFilePair;
	typedef std::map<ExportChannelPtr, TempFilePtr> TempFileMap;
	typedef boost::shared_ptr<ExportStatus> ExportStatusPtr;

  private:
	friend class ExportElementFactory;
	ExportTimespan (ExportStatusPtr status, nframes_t frame_rate);
	
  public:
	~ExportTimespan ();
	
	Glib::ustring name () const { return _name; }
	void set_name (Glib::ustring name) { _name = name; }
	
	Glib::ustring range_id () const { return _range_id; }
	void set_range_id (Glib::ustring range_id) { _range_id = range_id; }
	
	/// Registers a channel to be read when export starts rolling
	void register_channel (ExportChannelPtr channel);
	
	/// "Rewinds" the tempfiles to start reading the beginnings again
	void rewind ();
	
	/// Reads data from the tempfile belonging to channel into data
	nframes_t get_data (float * data, nframes_t frames, ExportChannelPtr channel);
	
	/// Reads data from each channel and writes to tempfile
	int process (nframes_t frames);
	
	sigc::connection  process_connection;
	
	void set_range (nframes_t start, nframes_t end);
	nframes_t get_length () const { return end_frame - start_frame; }
	nframes_t get_start () const { return start_frame; }
	nframes_t get_end () const { return end_frame; }

  private:

	ExportStatusPtr status;

	nframes_t      start_frame;
	nframes_t      end_frame;
	nframes_t      position;
	nframes_t      frame_rate;

	TempFileMap    filemap;
	
	Glib::ustring _name;
	Glib::ustring _range_id;

};

} // namespace ARDOUR

#endif /* __ardour_export_timespan_h__ */
