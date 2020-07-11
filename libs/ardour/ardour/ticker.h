/*
 * Copyright (C) 2008-2009 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2009-2010 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013 Michael Fisher <mfisher31@gmail.com>
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

#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>

#include "pbd/signals.h"

#include "ardour/libardour_visibility.h"
#include "ardour/session_handle.h"
#include "ardour/types.h"

#ifndef __libardour_ticker_h__
#define __libardour_ticker_h__

namespace ARDOUR
{
class Session;
class MidiPort;

class LIBARDOUR_API MidiClockTicker : boost::noncopyable
{
public:
	MidiClockTicker (Session*);
	virtual ~MidiClockTicker ();

	void tick (samplepos_t, samplepos_t, pframes_t, samplecnt_t);

private:
	boost::shared_ptr<MidiPort> _midi_port;

	void   reset ();
	void   resync_latency (bool);
	double one_ppqn_in_samples (samplepos_t transport_position) const;

	void send_midi_clock_event (pframes_t offset, pframes_t nframes);
	void send_start_event (pframes_t offset, pframes_t nframes);
	void send_continue_event (pframes_t offset, pframes_t nframes);
	void send_stop_event (pframes_t offset, pframes_t nframes);
	void send_position_event (uint32_t midi_clocks, pframes_t offset, pframes_t nframes);

	bool        _rolling;
	double      _next_tick;
	uint32_t    _beat_pos;
	uint32_t    _clock_cnt;
	samplepos_t _transport_pos;

	ARDOUR::Session* _session;

	LatencyRange          _mclk_out_latency;
	PBD::ScopedConnection _latency_connection;
};

} // namespace ARDOUR

#endif /* __libardour_ticker_h__ */
