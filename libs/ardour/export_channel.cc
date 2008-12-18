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

#include <ardour/export_channel.h>

#include <ardour/export_failed.h>
#include <ardour/audioengine.h>

using namespace ARDOUR;

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
PortExportChannel::read (Sample * data, nframes_t frames) const
{
	memset (data, 0, frames * sizeof (float));

	for (PortSet::const_iterator it = ports.begin(); it != ports.end(); ++it) {
		if (*it != 0) {
			Sample* port_buffer = (*it)->get_audio_buffer(frames, 0).data();
			
			for (uint32_t i = 0; i < frames; ++i) {
				data[i] += (float) port_buffer[i];
			}
		}
	}
}

void
PortExportChannel::get_state (XMLNode * node) const
{
	XMLNode * port_node;
	for (PortSet::const_iterator it = ports.begin(); it != ports.end(); ++it) {
		if ((port_node = node->add_child ("Port"))) {
			port_node->add_property ("name", (*it)->name());
		}
	}
}

void
PortExportChannel::set_state (XMLNode * node, Session & session)
{
	XMLProperty * prop;
	XMLNodeList xml_ports = node->children ("Port");
	for (XMLNodeList::iterator it = xml_ports.begin(); it != xml_ports.end(); ++it) {
		if ((prop = (*it)->property ("name"))) {
			ports.insert (dynamic_cast<AudioPort *> (session.engine().get_port_by_name (prop->value())));
		}
	}
}

RegionExportChannelFactory::RegionExportChannelFactory (Session * session, AudioRegion const & region, AudioTrack & track, Type type) :
  region (region),
  track (track),
  type (type),
  frames_per_cycle (session->engine().frames_per_cycle ()),
  buffers_up_to_date (false),
  region_start (region.position()),
  position (region_start),

  mixdown_buffer (0),
  gain_buffer (0)
{
	switch (type) {
	  case Raw:
		n_channels = region.n_channels();
		break;
	  case Fades:
		n_channels = region.n_channels();
		
		mixdown_buffer = new Sample [frames_per_cycle];
		gain_buffer = new Sample [frames_per_cycle];
		memset (gain_buffer, 1.0, sizeof (Sample) * frames_per_cycle);
		
		break;
	  case Processed:
		n_channels = track.n_outputs().n_audio();
		break;
	  default:
		throw ExportFailed ("Unhandled type in ExportChannelFactory constructor");
	}
	
	session->ProcessExport.connect (sigc::hide (sigc::mem_fun (*this, &RegionExportChannelFactory::new_cycle_started)));
	
	buffers.set_count (ChanCount (DataType::AUDIO, n_channels));
	buffers.ensure_buffers (DataType::AUDIO, n_channels, frames_per_cycle);
}

RegionExportChannelFactory::~RegionExportChannelFactory ()
{
	delete[] mixdown_buffer;
	delete[] gain_buffer;
}

ExportChannelPtr
RegionExportChannelFactory::create (uint32_t channel)
{
	assert (channel < n_channels);
	return ExportChannelPtr (new RegionExportChannel (*this, channel));
}

void
RegionExportChannelFactory::read (uint32_t channel, Sample * data, nframes_t frames_to_read)
{
	assert (channel < n_channels);
	assert (frames_to_read <= frames_per_cycle);
	
	if (!buffers_up_to_date) {
		update_buffers(frames_to_read);
		buffers_up_to_date = true;
	}
	
	memcpy (data, buffers.get_audio (channel).data(), frames_to_read * sizeof (Sample));
}

void
RegionExportChannelFactory::update_buffers (nframes_t frames)
{
	assert (frames <= frames_per_cycle);

	switch (type) {
	  case Raw:
		for (size_t channel = 0; channel < n_channels; ++channel) {
			region.read (buffers.get_audio (channel).data(), position - region_start, frames, channel);
		}
		break;
	  case Fades:
		assert (mixdown_buffer && gain_buffer);
		for (size_t channel = 0; channel < n_channels; ++channel) {
			memset (mixdown_buffer, 0, sizeof (Sample) * frames);
			region.read_at (buffers.get_audio (channel).data(), mixdown_buffer, gain_buffer, position, frames, channel);
		}
		break;
	  case Processed:
		track.export_stuff (buffers, position, frames);
		break;
	  default:
		throw ExportFailed ("Unhandled type in ExportChannelFactory::update_buffers");
	}
	
	position += frames;
}
