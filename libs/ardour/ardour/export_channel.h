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

#ifndef __ardour_export_channel_h__
#define __ardour_export_channel_h__

#include <ardour/audioregion.h>
#include <ardour/audio_track.h>
#include <ardour/buffer_set.h>

#include <set>

#include <boost/shared_ptr.hpp>
#include <sigc++/signal.h>

namespace ARDOUR {

class Session;

/// Export channel base class interface for different source types
class ExportChannel
{
  public:

	virtual ~ExportChannel () {}

	virtual void read (Sample * data, nframes_t frames) const = 0;
	virtual bool empty () const = 0;
	
	/// Adds state to node passed
	virtual void get_state (XMLNode * node) const = 0;
	
	/// Sets state from node passed
	virtual void set_state (XMLNode * node, Session & session) = 0;
	
	// Operator< must be defined for usage in e.g. std::map or std::set to disallow duplicates when necessary
	virtual bool operator< (ExportChannel const & other) const = 0;
};

/// Safe pointer for storing ExportChannels in ordered STL containers
class ExportChannelPtr : public boost::shared_ptr<ExportChannel>
{
  public:
	ExportChannelPtr () {}
	template<typename Y> explicit ExportChannelPtr (Y * ptr) : boost::shared_ptr<ExportChannel> (ptr) {}

	bool operator< (ExportChannelPtr const & other) const { return **this < *other; }
};

/// Basic export channel that reads from AudioPorts
class PortExportChannel : public ExportChannel
{
  public:
	typedef std::set<AudioPort *> PortSet;

	PortExportChannel () {}
	
	void read (Sample * data, nframes_t frames) const;
	bool empty () const { return ports.empty(); }
	
	void get_state (XMLNode * node) const;
	void set_state (XMLNode * node, Session & session);
	
	bool operator< (ExportChannel const & other) const;

	void add_port (AudioPort * port) { ports.insert (port); }
	PortSet const & get_ports () { return ports; }

  private:
	PortSet ports;
};

/// Handles RegionExportChannels and does actual reading from region
class RegionExportChannelFactory : public sigc::trackable
{
  public:
	enum Type {
		Raw,
		Fades,
		Processed
	};
	
	RegionExportChannelFactory (Session * session, AudioRegion const & region, AudioTrack & track, Type type);
	~RegionExportChannelFactory ();

	ExportChannelPtr create (uint32_t channel);
	void read (uint32_t channel, Sample * data, nframes_t frames_to_read);
	
  private:

	int new_cycle_started () { buffers_up_to_date = false; return 0; }
	void update_buffers (nframes_t frames);

	AudioRegion const & region;
	AudioTrack & track;
	Type type;

	nframes_t frames_per_cycle;
	size_t n_channels;
	BufferSet buffers;
	bool buffers_up_to_date;
	nframes_t region_start;
	nframes_t position;
	
	Sample * mixdown_buffer;
	Sample * gain_buffer;
};

/// Export channel that reads from region channel
class RegionExportChannel : public ExportChannel
{
	friend class RegionExportChannelFactory;

  public:
	void read (Sample * data, nframes_t frames_to_read) const { factory.read (channel, data, frames_to_read); }
	void get_state (XMLNode * node) const {};
	void set_state (XMLNode * node, Session & session) {};
	bool empty () const { return false; }
	// Region export should never have duplicate channels, so there need not be any semantics here
	bool operator< (ExportChannel const & other) const { return this < &other; }

  private:

	RegionExportChannel (RegionExportChannelFactory & factory, uint32_t channel) :
	  factory (factory),
	  channel (channel)
	{}
	
	RegionExportChannelFactory & factory;
	uint32_t channel;
};

} // namespace ARDOUR

#endif
