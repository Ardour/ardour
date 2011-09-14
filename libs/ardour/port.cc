/*
    Copyright (C) 2002 Paul Davis 

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

#include <jack/weakjack.h>
#include "ardour/port.h"
#include "ardour/audioengine.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace std;

AudioEngine* Port::_engine; 
nframes_t Port::_short_over_length = 2;
nframes_t Port::_long_over_length = 10;
nframes_t Port::_port_offset = 0;
nframes_t Port::_buffer_size = 0;

Port::Port (jack_port_t *p) 
	: _port (p)
{
	if (_port == 0) {
		throw failed_constructor();
	}
	
	_private_playback_latency.min = 0;
	_private_playback_latency.max = 0;
	_private_capture_latency.min = 0;
	_private_capture_latency.max = 0;

	_flags = JackPortFlags (jack_port_flags (_port));
	_type  = jack_port_type (_port); 
	_name = jack_port_name (_port);

	reset ();
}

void
Port::reset ()
{
	_last_monitor = false;
	_silent = false;
	_metering = 0;
	
	reset_meters ();
}

int 
Port::set_name (string str)
{
	int ret;

	if ((ret = jack_port_set_name (_port, str.c_str())) == 0) {
		_name = str;
	}
	
	return ret;
}

void
Port::set_public_latency_range (jack_latency_range_t& range, bool playback) const
{
	/* this sets the visible latency that the rest of JACK sees. because we do latency
	   compensation, all (most) of our visible port latency values are identical.
	*/

	if (!jack_port_set_latency_range) {
		return;
	}

	jack_port_set_latency_range (_port,
	                             (playback ? JackPlaybackLatency : JackCaptureLatency),
	                             &range);
}

void
Port::set_private_latency_range (jack_latency_range_t& range, bool playback)
{
        // cerr << name() << " set private " << (playback ? "playback" : "capture") << " latency to " << range.min << " - " 
        // << range.max << endl;

	if (playback) {
		_private_playback_latency = range;
	} else {
		_private_capture_latency = range;
	}

	/* push to public (JACK) location so that everyone else can see it */

	set_public_latency_range (range, playback);
}

const jack_latency_range_t&
Port::private_latency_range (bool playback) const
{
	if (playback) {
		return _private_playback_latency;
	} else {
		return _private_capture_latency;
	}
}

jack_latency_range_t
Port::public_latency_range (bool playback) const
{
	jack_latency_range_t r;

	jack_port_get_latency_range (_port,
	                             sends_output() ? JackPlaybackLatency : JackCaptureLatency,
	                             &r);

	return r;
}

/** @param o Filled in with port full names of ports that we are connected to */
int
Port::get_connections (std::vector<std::string> & c) const
{
	int n = 0;

	if (_engine->connected()) {
		const char** jc = jack_port_get_connections (_port);
		if (jc) {
			for (int i = 0; jc[i]; ++i) {
				c.push_back (jc[i]);
				++n;
			}
                        if (jack_free) {
                                jack_free (jc);
                        } else {
                                free (jc);
                        }
		}
	}

	return n;
}

void
Port::get_connected_latency_range (jack_latency_range_t& range, bool playback) const
{
	if (!jack_port_get_latency_range) {
		return;
	}

	vector<string> connections;
	jack_client_t* jack = _engine->jack();

	if (!jack) {
		range.min = 0;
		range.max = 0;
		PBD::warning << _("get_connected_latency_range() called while disconnected from JACK") << endmsg;
		return;
	}

	get_connections (connections);

	if (!connections.empty()) {

		range.min = ~((jack_nframes_t) 0);
		range.max = 0;

                // cerr << const_cast<Port*>(this)->name() << " ========= " << connections.size() << " connections\n";

		for (vector<string>::const_iterator c = connections.begin();
		     c != connections.end(); ++c) {

                        jack_latency_range_t lr;
                        
                        // cerr << "\tConnected to " << (*c) << endl;

                        if (!AudioEngine::instance()->port_is_mine (*c)) {

                                /* port belongs to some other JACK client, use
                                 * JACK to lookup its latency information.
                                 */

                                jack_port_t* remote_port = jack_port_by_name (_engine->jack(), (*c).c_str());

                                if (remote_port) {
                                        jack_port_get_latency_range (
                                                remote_port,
                                                (playback ? JackPlaybackLatency : JackCaptureLatency),
                                                &lr);

                                        range.min = min (range.min, lr.min);
                                        range.max = max (range.max, lr.max);
                                        
                                        // cerr << "\t\tRemote port, range = " << range.min << " - " << range.max << endl;
                                }

			} else {

                                /* port belongs to this instance of ardour,
                                   so look up its latency information
                                   internally, because our published/public
                                   values already contain our plugin
                                   latency compensation.
                                */


                                Port* remote_port = AudioEngine::instance()->get_ardour_port_by_name_unlocked (*c);
                                if (remote_port) {
                                        lr = remote_port->private_latency_range ((playback ? JackPlaybackLatency : JackCaptureLatency));
                                        range.min = min (range.min, lr.min);
                                        range.max = max (range.max, lr.max);
                                        // cerr << "\t\tArdour port, range = " << range.min << " - " << range.max << endl;
                                }
                        }
		}

	} else {
		range.min = 0;
		range.max = 0;
	}
}
