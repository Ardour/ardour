/*
    Copyright (C) 2009-2016 Paul Davis

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

#include "pbd/error.h"
#include "pbd/i18n.h"

#include "ardour/butler.h"
#include "ardour/disk_io.h"
#include "ardour/disk_reader.h"
#include "ardour/disk_writer.h"
#include "ardour/location.h"
#include "ardour/rc_configuration.h"
#include "ardour/session.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

const string DiskIOProcessor::state_node_name = X_("DiskIOProcessor");

// PBD::Signal0<void> DiskIOProcessor::DiskOverrun;
// PBD::Signal0<void>  DiskIOProcessor::DiskUnderrun;

DiskIOProcessor::DiskIOProcessor (Session& s, string const & str, Flag f)
	: Processor (s, str)
	, _flags (f)
	, i_am_the_modifier (false)
	, _visible_speed (0.0)
	, _actual_speed (0.0)
	, _speed (0.0)
	, _target_speed (0.0)
	, _buffer_reallocation_required (false)
	, _seek_required (false)
	, _slaved (false)
	, loop_location (0)
	, in_set_state (false)
        , wrap_buffer_size (0)
        , speed_buffer_size (0)
	, channels (new ChannelList)
{
}

void
DiskIOProcessor::set_buffering_parameters (BufferingPreset bp)
{
	framecnt_t read_chunk_size;
	framecnt_t read_buffer_size;
	framecnt_t write_chunk_size;
	framecnt_t write_buffer_size;

	if (!get_buffering_presets (bp, read_chunk_size, read_buffer_size, write_chunk_size, write_buffer_size)) {
		return;
	}

	DiskReader::set_chunk_frames (read_chunk_size);
	DiskWriter::set_chunk_frames (write_chunk_size);

	Config->set_audio_capture_buffer_seconds (write_buffer_size);
	Config->set_audio_playback_buffer_seconds (read_buffer_size);
}

bool
DiskIOProcessor::get_buffering_presets (BufferingPreset bp,
                                        framecnt_t& read_chunk_size,
                                        framecnt_t& read_buffer_size,
                                        framecnt_t& write_chunk_size,
                                        framecnt_t& write_buffer_size)
{
	switch (bp) {
	case Small:
		read_chunk_size = 65536;  /* samples */
		write_chunk_size = 65536; /* samples */
		read_buffer_size = 5;  /* seconds */
		write_buffer_size = 5; /* seconds */
		break;

	case Medium:
		read_chunk_size = 262144;  /* samples */
		write_chunk_size = 131072; /* samples */
		read_buffer_size = 10;  /* seconds */
		write_buffer_size = 10; /* seconds */
		break;

	case Large:
		read_chunk_size = 524288; /* samples */
		write_chunk_size = 131072; /* samples */
		read_buffer_size = 20; /* seconds */
		write_buffer_size = 20; /* seconds */
		break;

	default:
		return false;
	}

	return true;
}


int
DiskIOProcessor::set_loop (Location *location)
{
	if (location) {
		if (location->start() >= location->end()) {
			error << string_compose(_("Location \"%1\" not valid for track loop (start >= end)"), location->name()) << endl;
			return -1;
		}
	}

	loop_location = location;

	LoopSet (location); /* EMIT SIGNAL */
	return 0;
}

void
DiskIOProcessor::non_realtime_set_speed ()
{
	if (_buffer_reallocation_required)
	{
		Glib::Threads::Mutex::Lock lm (state_lock);
		allocate_temporary_buffers ();

		_buffer_reallocation_required = false;
	}

	if (_seek_required) {
		if (speed() != 1.0f || speed() != -1.0f) {
			seek ((framepos_t) (_session.transport_frame() * (double) speed()), true);
		}
		else {
			seek (_session.transport_frame(), true);
		}

		_seek_required = false;
	}
}

bool
DiskIOProcessor::realtime_set_speed (double sp, bool global)
{
	bool changed = false;
	double new_speed = sp * _session.transport_speed();

	if (_visible_speed != sp) {
		_visible_speed = sp;
		changed = true;
	}

	if (new_speed != _actual_speed) {

		framecnt_t required_wrap_size = (framecnt_t) ceil (_session.get_block_size() *
                                                                  fabs (new_speed)) + 2;

		if (required_wrap_size > wrap_buffer_size) {
			_buffer_reallocation_required = true;
		}

		_actual_speed = new_speed;
		_target_speed = fabs(_actual_speed);
	}

	if (changed) {
		if (!global) {
			_seek_required = true;
		}
		SpeedChanged (); /* EMIT SIGNAL */
	}

	return _buffer_reallocation_required || _seek_required;
}

int
DiskIOProcessor::set_state (const XMLNode& node, int version)
{
	XMLProperty const * prop;

	Processor::set_state (node, version);

	if ((prop = node.property ("flags")) != 0) {
		_flags = Flag (string_2_enum (prop->value(), _flags));
	}

	if ((prop = node.property ("speed")) != 0) {
		double sp = atof (prop->value().c_str());

		if (realtime_set_speed (sp, false)) {
			non_realtime_set_speed ();
		}
	}
	return 0;
}

int
DiskIOProcessor::add_channel_to (boost::shared_ptr<ChannelList> c, uint32_t how_many)
{
	while (how_many--) {
		c->push_back (new ChannelInfo(
			              _session.butler()->audio_diskstream_playback_buffer_size(),
			              speed_buffer_size, wrap_buffer_size));
		interpolation.add_channel_to (
			_session.butler()->audio_diskstream_playback_buffer_size(),
			speed_buffer_size);
	}

	_n_channels.set (DataType::AUDIO, c->size());

	return 0;
}

int
DiskIOProcessor::add_channel (uint32_t how_many)
{
	RCUWriter<ChannelList> writer (channels);
	boost::shared_ptr<ChannelList> c = writer.get_copy();

	return add_channel_to (c, how_many);
}

int
DiskIOProcessor::remove_channel_from (boost::shared_ptr<ChannelList> c, uint32_t how_many)
{
	while (how_many-- && !c->empty()) {
		delete c->back();
		c->pop_back();
		interpolation.remove_channel_from ();
	}

	_n_channels.set(DataType::AUDIO, c->size());

	return 0;
}

int
DiskIOProcessor::remove_channel (uint32_t how_many)
{
	RCUWriter<ChannelList> writer (channels);
	boost::shared_ptr<ChannelList> c = writer.get_copy();

	return remove_channel_from (c, how_many);
}

