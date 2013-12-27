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

#include <set>

#include <boost/scoped_array.hpp>
#include <boost/shared_ptr.hpp>

#include "pbd/signals.h"

#include "ardour/buffer_set.h"
#include "ardour/export_pointers.h"

namespace ARDOUR {

class Session;
class AudioTrack;
class AudioPort;
class AudioRegion;
class CapturingProcessor;

/// Export channel base class interface for different source types
class LIBARDOUR_API ExportChannel : public boost::less_than_comparable<ExportChannel>
{
  public:

	virtual ~ExportChannel () {}

	virtual void set_max_buffer_size(framecnt_t) { }

	virtual void read (Sample const *& data, framecnt_t frames) const = 0;
	virtual bool empty () const = 0;

	/// Adds state to node passed
	virtual void get_state (XMLNode * node) const = 0;

	/// Sets state from node passed
	virtual void set_state (XMLNode * node, Session & session) = 0;

	// Operator< must be defined for usage in e.g. std::map or std::set to disallow duplicates when necessary
	virtual bool operator< (ExportChannel const & other) const = 0;
};

/// Basic export channel that reads from AudioPorts
class LIBARDOUR_API PortExportChannel : public ExportChannel
{
  public:
	typedef std::set<boost::weak_ptr<AudioPort> > PortSet;

	PortExportChannel ();
	void set_max_buffer_size(framecnt_t frames);

	void read (Sample const *& data, framecnt_t frames) const;
	bool empty () const { return ports.empty(); }

	void get_state (XMLNode * node) const;
	void set_state (XMLNode * node, Session & session);

	bool operator< (ExportChannel const & other) const;

	void add_port (boost::weak_ptr<AudioPort> port) { ports.insert (port); }
	PortSet const & get_ports () { return ports; }

  private:
	PortSet ports;
	boost::scoped_array<Sample> buffer;
	framecnt_t buffer_size;
};


/// Handles RegionExportChannels and does actual reading from region
class LIBARDOUR_API RegionExportChannelFactory
{
  public:
	enum Type {
		None,
		Raw,
		Fades,
		Processed
	};

	RegionExportChannelFactory (Session * session, AudioRegion const & region, AudioTrack & track, Type type);
	~RegionExportChannelFactory ();

	ExportChannelPtr create (uint32_t channel);
	void read (uint32_t channel, Sample const *& data, framecnt_t frames_to_read);

  private:

	int new_cycle_started (framecnt_t) { buffers_up_to_date = false; return 0; }
	void update_buffers (framecnt_t frames);

	AudioRegion const & region;
	AudioTrack & track;
	Type type;

	framecnt_t frames_per_cycle;
	size_t n_channels;
	BufferSet buffers;
	bool buffers_up_to_date;
	framecnt_t region_start;
	framecnt_t position;

	boost::scoped_array<Sample> mixdown_buffer;
	boost::scoped_array<Sample> gain_buffer;

	PBD::ScopedConnection export_connection;
};

/// Export channel that reads from region channel
class LIBARDOUR_API RegionExportChannel : public ExportChannel
{
	friend class RegionExportChannelFactory;

  public:
	void read (Sample const *& data, framecnt_t frames_to_read) const { factory.read (channel, data, frames_to_read); }
	void get_state (XMLNode * /*node*/) const {};
	void set_state (XMLNode * /*node*/, Session & /*session*/) {};
	bool empty () const { return false; }
	// Region export should never have duplicate channels, so there need not be any semantics here
	bool operator< (ExportChannel const & other) const { return this < &other; }

  private:

	RegionExportChannel (RegionExportChannelFactory & factory, uint32_t channel)
		: factory (factory)
		, channel (channel)
	{}

	RegionExportChannelFactory & factory;
	uint32_t channel;
};

/// Export channel for exporting from different positions in a route
class LIBARDOUR_API RouteExportChannel : public ExportChannel
{
	class ProcessorRemover; // fwd declaration

  public:
	RouteExportChannel(boost::shared_ptr<CapturingProcessor> processor, size_t channel,
	                   boost::shared_ptr<ProcessorRemover> remover);
	~RouteExportChannel();

        static void create_from_route(std::list<ExportChannelPtr> & result, boost::shared_ptr<Route> route);

  public: // ExportChannel interface
	void set_max_buffer_size(framecnt_t frames);

	void read (Sample const *& data, framecnt_t frames) const;
	bool empty () const { return false; }

	void get_state (XMLNode * node) const;
	void set_state (XMLNode * node, Session & session);

	bool operator< (ExportChannel const & other) const;

  private:

	// Removes the processor from the track when deleted
	class ProcessorRemover {
	  public:
   	         ProcessorRemover (boost::shared_ptr<Route> route, boost::shared_ptr<CapturingProcessor> processor)
			: route (route), processor (processor) {}
		~ProcessorRemover();
	  private:
                boost::shared_ptr<Route> route;
		boost::shared_ptr<CapturingProcessor> processor;
	};

	boost::shared_ptr<CapturingProcessor> processor;
	size_t channel;
	// Each channel keeps a ref to the remover. Last one alive
	// will cause the processor to be removed on deletion.
	boost::shared_ptr<ProcessorRemover> remover;
};

} // namespace ARDOUR

#endif
