/*
 * Copyright (C) 2008-2012 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017-2022 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_export_channel_h__
#define __ardour_export_channel_h__

#include <list>
#include <set>

#include <boost/scoped_array.hpp>
#include <boost/shared_ptr.hpp>

#include "pbd/ringbuffer.h"
#include "pbd/signals.h"

#include "ardour/audio_buffer.h"
#include "ardour/buffer_set.h"
#include "ardour/export_pointers.h"
#include "ardour/fixed_delay.h"
#include "ardour/midi_buffer.h"

namespace ARDOUR {

class Session;
class AudioTrack;
class AudioPort;
class AudioRegion;
class CapturingProcessor;
class MidiPort;

/// Export channel base class interface for different source types
class LIBARDOUR_API ExportChannel : public boost::less_than_comparable<ExportChannel>
{
public:
	virtual ~ExportChannel () {}

	virtual samplecnt_t common_port_playback_latency () const { return 0; }
	virtual void prepare_export (samplecnt_t max_samples, sampleoffset_t common_latency) {}

	virtual void read (Buffer const*&, samplecnt_t samples) const = 0;

	virtual bool empty () const = 0;

	virtual bool audio () const { return true; }
	virtual bool midi () const { return false; }

	virtual std::string state_node_name () const = 0;

	/// Adds state to node passed
	virtual void get_state (XMLNode* node) const = 0;

	/// Sets state from node passed
	virtual void set_state (XMLNode* node, Session& session) = 0;

	// Operator< must be defined for usage in e.g. std::map or std::set to disallow duplicates when necessary
	virtual bool operator< (ExportChannel const& other) const = 0;
};

/// Basic export channel that reads from AudioPorts
class LIBARDOUR_API PortExportChannel : public ExportChannel
{
public:
	typedef std::set<boost::weak_ptr<AudioPort>> PortSet;

	PortExportChannel ();
	~PortExportChannel ();

	samplecnt_t common_port_playback_latency () const;
	void        prepare_export (samplecnt_t max_samples, sampleoffset_t common_latency);

	void read (Buffer const*&, samplecnt_t samples) const;

	bool empty () const { return ports.empty (); }

	std::string state_node_name () const { return "PortExportChannel"; }

	void get_state (XMLNode* node) const;
	void set_state (XMLNode* node, Session& session);

	bool operator< (ExportChannel const& other) const;

	void add_port (boost::weak_ptr<AudioPort> port) { ports.insert (port); }
	PortSet const& get_ports () const { return ports; }

private:
	PortSet                                               ports;
	samplecnt_t                                           _buffer_size;
	boost::scoped_array<Sample>                           _buffer;
	mutable AudioBuffer                                   _buf;
	std::list<boost::shared_ptr<PBD::RingBuffer<Sample>>> _delaylines;
};

/// Basic export channel that reads from MIDIPorts
class LIBARDOUR_API PortExportMIDI : public ExportChannel
{
public:
	PortExportMIDI ();
	~PortExportMIDI ();

	/* ExportChannel interface */
	samplecnt_t common_port_playback_latency () const;
	void        prepare_export (samplecnt_t max_samples, sampleoffset_t common_latency);

	void read (Buffer const*&, samplecnt_t samples) const;

	bool empty () const { return _port.expired (); }

	bool audio () const { return false; }
	bool midi () const { return true; }

	std::string state_node_name () const { return "PortExportMIDI"; }

	void get_state (XMLNode* node) const;
	void set_state (XMLNode* node, Session& session);

	bool operator< (ExportChannel const& other) const;

	boost::shared_ptr<MidiPort> port () const { return _port.lock (); }

	void set_port (boost::weak_ptr<MidiPort> port)
	{
		_port = port;
	}

private:
	boost::weak_ptr<MidiPort> _port;
	mutable FixedDelay        _delayline;
	mutable MidiBuffer        _buf;
};

/// Handles RegionExportChannels and does actual reading from region
class LIBARDOUR_API RegionExportChannelFactory
{
public:
	enum Type {
		None,
		Raw,
		Fades,
	};

	RegionExportChannelFactory (Session* session, AudioRegion const& region, AudioTrack& track, Type type);
	~RegionExportChannelFactory ();

	ExportChannelPtr create (uint32_t channel);

	void read (uint32_t channel, Buffer const*&, samplecnt_t samples_to_read);

private:
	int new_cycle_started (samplecnt_t)
	{
		buffers_up_to_date = false;
		return 0;
	}

	void update_buffers (samplecnt_t samples);

	AudioRegion const& region;

	Type        type;
	samplecnt_t samples_per_cycle;
	size_t      n_channels;
	BufferSet   buffers;
	bool        buffers_up_to_date;
	samplepos_t region_start;
	samplepos_t position;

	boost::scoped_array<Sample> mixdown_buffer;
	boost::scoped_array<Sample> gain_buffer;

	PBD::ScopedConnection export_connection;
};

/// Export channel that reads from region channel
class LIBARDOUR_API RegionExportChannel : public ExportChannel
{
	friend class RegionExportChannelFactory;

public:
	void read (Buffer const*& buf, samplecnt_t samples_to_read) const
	{
		factory.read (channel, buf, samples_to_read);
	}

	std::string state_node_name () const { return "RegionExportChannel"; }

	void get_state (XMLNode* /*node*/) const {};
	void set_state (XMLNode* /*node*/, Session& /*session*/){};

	bool empty () const { return false; }

	// Region export should never have duplicate channels, so there need not be any semantics here
	bool operator< (ExportChannel const& other) const
	{
		return this < &other;
	}

private:
	RegionExportChannel (RegionExportChannelFactory& factory, uint32_t channel)
		: factory (factory)
		, channel (channel)
	{
	}

	RegionExportChannelFactory& factory;
	uint32_t                    channel;
};

/// Export channel for exporting from different positions in a route
class LIBARDOUR_API RouteExportChannel : public ExportChannel
{
	class ProcessorRemover; // fwd declaration

public:
	RouteExportChannel (boost::shared_ptr<CapturingProcessor> processor,
	                    DataType                              type,
	                    size_t                                channel,
	                    boost::shared_ptr<ProcessorRemover>   remover);

	~RouteExportChannel ();

	static void create_from_route (std::list<ExportChannelPtr>& result, boost::shared_ptr<Route> route);
	static void create_from_state (std::list<ExportChannelPtr>& result, Session&, XMLNode*);

public: // ExportChannel interface
	void prepare_export (samplecnt_t max_samples, sampleoffset_t common_latency);

	void read (Buffer const*&, samplecnt_t samples) const;

	bool empty () const { return false; }

	bool audio () const;
	bool midi () const;

	boost::shared_ptr<Route> route () const { return _remover->route (); }

	std::string state_node_name () const { return "RouteExportChannel"; }

	void get_state (XMLNode* node) const;
	void set_state (XMLNode* node, Session& session);

	bool operator< (ExportChannel const& other) const;

private:
	// Removes the processor from the track when deleted
	class ProcessorRemover
	{
	public:
		ProcessorRemover (boost::shared_ptr<Route> route, boost::shared_ptr<CapturingProcessor> processor)
			: _route (route)
			, _processor (processor)
		{
		}
		~ProcessorRemover ();

		boost::shared_ptr<Route> route () const { return _route; }

	private:
		boost::shared_ptr<Route>              _route;
		boost::shared_ptr<CapturingProcessor> _processor;
	};

	boost::shared_ptr<CapturingProcessor> _processor;

	DataType _type;
	size_t   _channel;

	// Each channel keeps a ref to the remover. Last one alive
	// will cause the processor to be removed on deletion.
	boost::shared_ptr<ProcessorRemover> _remover;
};

} // namespace ARDOUR

#endif
