/*
 * Copyright (C) 2008-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2008-2011 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
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

#include "ardour/audio_buffer.h"
#include "ardour/audio_port.h"
#include "ardour/audio_track.h"
#include "ardour/audioengine.h"
#include "ardour/audioregion.h"
#include "ardour/capturing_processor.h"
#include "ardour/export_channel.h"
#include "ardour/export_failed.h"
#include "ardour/session.h"

#include "pbd/error.h"

#include "pbd/i18n.h"

using namespace ARDOUR;

PortExportChannel::PortExportChannel ()
	: _buffer_size (0)
{
}

PortExportChannel::~PortExportChannel ()
{
	_delaylines.clear ();
}

samplecnt_t PortExportChannel::common_port_playback_latency () const
{
	samplecnt_t l = 0;
	bool first = true;
	for (PortSet::const_iterator it = ports.begin(); it != ports.end(); ++it) {
		boost::shared_ptr<AudioPort> p = it->lock ();
		if (!p) { continue; }
		samplecnt_t latency = p->private_latency_range (true).max;
		if (first) {
			first = false;
			l = p->private_latency_range (true).max;
			continue;
		}
		l = std::min (l, latency);
	}
	return l;
}

void PortExportChannel::prepare_export (samplecnt_t max_samples, sampleoffset_t common_latency)
{
	_buffer_size = max_samples;
	_buffer.reset (new Sample[max_samples]);

	_delaylines.clear ();

	for (PortSet::const_iterator it = ports.begin(); it != ports.end(); ++it) {
		boost::shared_ptr<AudioPort> p = it->lock ();
		if (!p) { continue; }
		samplecnt_t latency = p->private_latency_range (true).max - common_latency;
		PBD::RingBuffer<Sample>* rb = new PBD::RingBuffer<Sample> (latency + 1 + _buffer_size);
		for (samplepos_t i = 0; i < latency; ++i) {
			Sample zero = 0;
			rb->write (&zero, 1);
		}
		_delaylines.push_back (boost::shared_ptr<PBD::RingBuffer<Sample> >(rb));
	}
}

bool
PortExportChannel::operator< (ExportChannel const & other) const
{
	PortExportChannel const * pec;
	if (!(pec = dynamic_cast<PortExportChannel const *> (&other))) {
		return this < &other;
	}
	return ports < pec->ports;
}

void
PortExportChannel::read (Sample const *& data, samplecnt_t samples) const
{
	assert(_buffer);
	assert(samples <= _buffer_size);

	if (ports.size() == 1 && _delaylines.size() ==1 && _delaylines.front()->bufsize () == _buffer_size + 1) {
		boost::shared_ptr<AudioPort> p = ports.begin()->lock ();
		AudioBuffer& ab (p->get_audio_buffer(samples)); // unsets AudioBuffer::_written
		data = ab.data();
		ab.set_written (true);
		return;
	}

	memset (_buffer.get(), 0, samples * sizeof (Sample));

	std::list <boost::shared_ptr<PBD::RingBuffer<Sample> > >::const_iterator di = _delaylines.begin ();
	for (PortSet::const_iterator it = ports.begin(); it != ports.end(); ++it) {
		boost::shared_ptr<AudioPort> p = it->lock ();
		if (!p) {
			continue;
		}
		AudioBuffer& ab (p->get_audio_buffer(samples)); // unsets AudioBuffer::_written
		Sample* port_buffer = ab.data();
		ab.set_written (true);
		(*di)->write (port_buffer, samples);

		PBD::RingBuffer<Sample>::rw_vector vec;
		(*di)->get_read_vector (&vec);
		assert (vec.len[0] + vec.len[1] >= samples);

		samplecnt_t to_write = std::min (samples, (samplecnt_t) vec.len[0]);
		mix_buffers_no_gain (&_buffer[0], vec.buf[0], to_write);

		to_write = std::min (samples - to_write, (samplecnt_t) vec.len[1]);
		if (to_write > 0) {
			mix_buffers_no_gain (&_buffer[vec.len[0]], vec.buf[1], to_write);
		}
		(*di)->increment_read_idx (samples);

		++di;
	}

	data = _buffer.get();
}

void
PortExportChannel::get_state (XMLNode * node) const
{
	XMLNode * port_node;
	for (PortSet::const_iterator it = ports.begin(); it != ports.end(); ++it) {
		boost::shared_ptr<Port> p = it->lock ();
		if (p && (port_node = node->add_child ("Port"))) {
			port_node->set_property ("name", p->name());
		}
	}
}

void
PortExportChannel::set_state (XMLNode * node, Session & session)
{
	XMLNodeList xml_ports = node->children ("Port");
	for (XMLNodeList::iterator it = xml_ports.begin(); it != xml_ports.end(); ++it) {
		std::string name;
		if ((*it)->get_property ("name", name)) {
			boost::shared_ptr<AudioPort> port = boost::dynamic_pointer_cast<AudioPort> (session.engine().get_port_by_name (name));
			if (port) {
				ports.insert (port);
			} else {
				PBD::warning << string_compose (_("Could not get port for export channel \"%1\", dropping the channel"), name) << endmsg;
			}
		}
	}
}

RegionExportChannelFactory::RegionExportChannelFactory (Session * session, AudioRegion const & region, AudioTrack&, Type type)
	: region (region)
	, type (type)
	, samples_per_cycle (session->engine().samples_per_cycle ())
	, buffers_up_to_date (false)
	, region_start (region.position_sample())
	, position (region_start)
{
	switch (type) {
	  case Raw:
		n_channels = region.n_channels();
		break;
	  case Fades:
		n_channels = region.n_channels();

		mixdown_buffer.reset (new Sample [samples_per_cycle]);
		gain_buffer.reset (new Sample [samples_per_cycle]);
		std::fill_n (gain_buffer.get(), samples_per_cycle, Sample (1.0));

		break;
	  default:
		throw ExportFailed ("Unhandled type in ExportChannelFactory constructor");
	}

	session->ProcessExport.connect_same_thread (export_connection, boost::bind (&RegionExportChannelFactory::new_cycle_started, this, _1));

	buffers.ensure_buffers (DataType::AUDIO, n_channels, samples_per_cycle);
	buffers.set_count (ChanCount (DataType::AUDIO, n_channels));
}

RegionExportChannelFactory::~RegionExportChannelFactory ()
{
}

ExportChannelPtr
RegionExportChannelFactory::create (uint32_t channel)
{
	assert (channel < n_channels);
	return ExportChannelPtr (new RegionExportChannel (*this, channel));
}

void
RegionExportChannelFactory::read (uint32_t channel, Sample const *& data, samplecnt_t samples_to_read)
{
	assert (channel < n_channels);
	assert (samples_to_read <= samples_per_cycle);

	if (!buffers_up_to_date) {
		update_buffers(samples_to_read);
		buffers_up_to_date = true;
	}

	data = buffers.get_audio (channel).data();
}

void
RegionExportChannelFactory::update_buffers (samplecnt_t samples)
{
	assert (samples <= samples_per_cycle);

	switch (type) {
	  case Raw:
		for (size_t channel = 0; channel < n_channels; ++channel) {
			region.read (buffers.get_audio (channel).data(), position - region_start, samples, channel);
		}
		break;
	  case Fades:
		assert (mixdown_buffer && gain_buffer);
		for (size_t channel = 0; channel < n_channels; ++channel) {
			memset (mixdown_buffer.get(), 0, sizeof (Sample) * samples);
			buffers.get_audio (channel).silence(samples);
			region.read_at (buffers.get_audio (channel).data(), mixdown_buffer.get(), gain_buffer.get(), position, samples, channel);
		}
		break;
	default:
		throw ExportFailed ("Unhandled type in ExportChannelFactory::update_buffers");
	}

	position += samples;
}


RouteExportChannel::RouteExportChannel(boost::shared_ptr<CapturingProcessor> processor, size_t channel,
                                       boost::shared_ptr<ProcessorRemover> remover)
  : processor (processor)
  , channel (channel)
  , remover (remover)
{
}

RouteExportChannel::~RouteExportChannel()
{
}

void
RouteExportChannel::create_from_route(std::list<ExportChannelPtr> & result, boost::shared_ptr<Route> route)
{
	boost::shared_ptr<CapturingProcessor> processor = route->add_export_point();
	uint32_t channels = processor->input_streams().n_audio();

	boost::shared_ptr<ProcessorRemover> remover (new ProcessorRemover (route, processor));
	result.clear();
	for (uint32_t i = 0; i < channels; ++i) {
		result.push_back (ExportChannelPtr (new RouteExportChannel (processor, i, remover)));
	}
}

void
RouteExportChannel::prepare_export (samplecnt_t max_samples, sampleoffset_t)
{
	if (processor) {
		processor->set_block_size (max_samples);
	}
}

void
RouteExportChannel::read (Sample const *& data, samplecnt_t samples) const
{
	assert(processor);
	AudioBuffer const & buffer = processor->get_capture_buffers().get_audio (channel);
#ifndef NDEBUG
	(void) samples;
#else
	assert (samples <= (samplecnt_t) buffer.capacity());
#endif
	data = buffer.data();
}

void
RouteExportChannel::get_state (XMLNode *) const
{
	// TODO
}

void
RouteExportChannel::set_state (XMLNode *, Session &)
{
	// TODO
}

bool
RouteExportChannel::operator< (ExportChannel const & other) const
{
	RouteExportChannel const * rec;
	if ((rec = dynamic_cast<RouteExportChannel const *>(&other)) == 0) {
		return this < &other;
	}

	if (processor.get() == rec->processor.get()) {
		return channel < rec->channel;
	}
	return processor.get() < rec->processor.get();
}

RouteExportChannel::ProcessorRemover::~ProcessorRemover()
{
	route->remove_processor (processor);
}
