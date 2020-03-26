/*
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2007-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_midi_port_h__
#define __ardour_midi_port_h__

#include "midi++/parser.h"

#include "ardour/port.h"
#include "ardour/midi_buffer.h"

namespace ARDOUR {

class MidiBuffer;
class MidiEngine;

class LIBARDOUR_API MidiPort : public Port {
  public:
	~MidiPort();

	DataType type () const {
		return DataType::MIDI;
	}

	void cycle_start (pframes_t nframes);
	void cycle_end (pframes_t nframes);
	void cycle_split ();

	void flush_buffers (pframes_t nframes);
	void transport_stopped ();
	void realtime_locate (bool);
	void reset ();
	void require_resolve ();

	bool input_active() const { return _input_active; }
	void set_input_active (bool yn);

	Buffer& get_buffer (pframes_t nframes) {
		return get_midi_buffer (nframes);
	}

	MidiBuffer& get_midi_buffer (pframes_t nframes);

	void set_trace (MIDI::Parser* trace_parser);

	typedef boost::function<bool(MidiBuffer&,MidiBuffer&)> MidiFilter;
	void set_inbound_filter (MidiFilter);
	int add_shadow_port (std::string const &, MidiFilter);
	boost::shared_ptr<MidiPort> shadow_port() const { return _shadow_port; }

	void read_and_parse_entire_midi_buffer_with_no_speed_adjustment (pframes_t nframes, MIDI::Parser& parser, samplepos_t now);

protected:
	friend class PortManager;

	MidiPort (const std::string& name, PortFlags);

private:
	MidiBuffer*                 _buffer;
	bool                        _resolve_required;
	bool                        _input_active;
	MidiFilter                  _inbound_midi_filter;
	boost::shared_ptr<MidiPort> _shadow_port;
	MidiFilter                  _shadow_midi_filter;
	MIDI::Parser*               _trace_parser;
	bool                        _data_fetched_for_cycle;

	void resolve_notes (void* buffer, samplepos_t when);
	void pull_input (pframes_t nframes, bool adjust_speed);
	void parse_input (pframes_t nframes, MIDI::Parser& parser);
};

} // namespace ARDOUR

#endif /* __ardour_midi_port_h__ */
