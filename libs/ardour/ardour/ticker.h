/*
    Copyright (C) 2008 Hans Baier

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

    $Id$
*/

#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>

#include "pbd/signals.h"

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"
#include "ardour/session_handle.h"


#ifndef __libardour_ticker_h__
#define __libardour_ticker_h__

namespace ARDOUR {

class Session;
class MidiPort;

class LIBARDOUR_API MidiClockTicker : public SessionHandlePtr, boost::noncopyable
{
public:
	MidiClockTicker ();
	virtual ~MidiClockTicker();

        void tick (const framepos_t& transport_frames, pframes_t nframes);

	bool has_midi_port() const { return _midi_port != 0; }

	void set_session (Session* s);
	void session_going_away();

	/// slot for the signal session::MIDIClock_PortChanged
	void update_midi_clock_port();

	/// slot for the signal session::TransportStateChange
	void transport_state_changed();

	/// slot for the signal session::TransportLooped
	void transport_looped();

	/// slot for the signal session::Located
	void session_located();

	/// pulses per quarter note (default 24)
	void set_ppqn(int ppqn) { _ppqn = ppqn; }

  private:
    boost::shared_ptr<MidiPort> _midi_port;
    int        _ppqn;
    double     _last_tick;
    bool       _send_pos;
    bool       _send_state;

    class Position;
    boost::scoped_ptr<Position> _pos;
    
    double one_ppqn_in_frames (framepos_t transport_position);

    void send_midi_clock_event (pframes_t offset, pframes_t nframes);
    void send_start_event (pframes_t offset, pframes_t nframes);
    void send_continue_event (pframes_t offset, pframes_t nframes);
    void send_stop_event (pframes_t offset, pframes_t nframes);
    void send_position_event (uint32_t midi_clocks, pframes_t offset, pframes_t nframes);
};
}
 // namespace 

#endif /* __libardour_ticker_h__ */
