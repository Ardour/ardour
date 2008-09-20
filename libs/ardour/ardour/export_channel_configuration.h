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

#ifndef __ardour_export_channel_configuration_h__
#define __ardour_export_channel_configuration_h__

#include <set>
#include <list>

#include <glibmm/ustring.h>
#include <sigc++/signal.h>

#include <ardour/export_status.h>
#include <ardour/ardour.h>

#include <pbd/xml++.h>

using Glib::ustring;

namespace ARDOUR
{

class ExportHandler;
class AudioPort;
class ExportChannel;
class ExportFormatSpecification;
class ExportFilename;
class ExportProcessor;
class ExportTimespan;
class Session;

class ExportChannel : public std::set<AudioPort *>
{
  public:
	void add_port (AudioPort * port) { if (port) { insert (port); } }
	void read_ports (float * data, nframes_t frames) const;
};

class ExportChannelConfiguration
{
  private:
	typedef boost::shared_ptr<ExportProcessor> ProcessorPtr;
	typedef boost::shared_ptr<ExportTimespan> TimespanPtr;
	typedef boost::shared_ptr<ExportFormatSpecification const> FormatPtr;
	typedef boost::shared_ptr<ExportFilename> FilenamePtr;
	
	typedef std::pair<FormatPtr, FilenamePtr> FileConfig;
	typedef std::list<FileConfig> FileConfigList;
	
	/// Struct for threading, acts like a pointer to a ExportChannelConfiguration
	struct WriterThread {
		WriterThread (ExportChannelConfiguration & channel_config) :
		  channel_config (channel_config), running (false) {}
		
		ExportChannelConfiguration * operator-> () { return &channel_config; }
		ExportChannelConfiguration & operator* () { return channel_config; }
		
		ExportChannelConfiguration & channel_config;
		
		pthread_t thread;
		bool      running;
	};

  private:
	friend class ExportElementFactory;
	ExportChannelConfiguration (ExportStatus & status, Session & session);
	
  public:
	XMLNode & get_state ();
	int set_state (const XMLNode &);
	
	typedef boost::shared_ptr<ExportChannel const> ChannelPtr;
	typedef std::list<ChannelPtr> ChannelList;
	
	ChannelList const & get_channels () { return channels; }
	bool all_channels_have_ports ();
	
	ustring name () const { return _name; }
	void set_name (ustring name) { _name = name; }
	void set_split (bool value) { split = value; }
	
	bool get_split () { return split; }
	uint32_t get_n_chans () { return channels.size(); }
	
	void register_channel (ChannelPtr channel) { channels.push_back (channel); }
	void register_file_config (FormatPtr format, FilenamePtr filename) { file_configs.push_back (FileConfig (format, filename)); }
	
	void clear_channels () { channels.clear (); }
	
	/// Writes all files for this channel config @return true if a new thread was spawned
	bool write_files (boost::shared_ptr<ExportProcessor> new_processor);
	sigc::signal<void> FilesWritten;
	
	// Tells the handler the necessary information for it to handle tempfiles
	void register_with_timespan (TimespanPtr timespan);
	
	void unregister_all ();
	
  private:

	 Session & session;

	// processor has to be prepared before doing this.
	void write_file ();
	
	/// The actual write files, needed for threading
	static void *  _write_files (void *arg);
	WriterThread    writer_thread;
	ProcessorPtr    processor;
	ExportStatus &  status;

	bool            files_written;

	TimespanPtr     timespan;
	ChannelList     channels;
	FileConfigList  file_configs;
	
	bool            split; // Split to mono files
	ustring        _name;
};

} // namespace ARDOUR

#endif /* __ardour_export_channel_configuration_h__ */
