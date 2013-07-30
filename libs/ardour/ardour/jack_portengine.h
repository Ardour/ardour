/*
    Copyright (C) 2013 Paul Davis

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

#ifndef __libardour_jack_portengine_h__
#define __libardour_jack_portengine_h__

#include <string>
#include <vector>

#include <stdint.h>

#include "ardour/port_engine.h"
#include "ardour/types.h"

namespace ARDOUR {

class JackConnection;

class JACKPortEngine : public PortEngine 
{
  public:
    JACKPortEngine (void* arg); // argument is a JackConnection

    bool  connected() const;
    void* private_handle() const;

    int         set_port_name (PortHandle, const std::string&);
    std::string get_port_name (PortHandle) const;
    PortHandle* get_port_by_name (const std::string&) const;

    std::string make_port_name_relative (const std::string& name) const;
    std::string make_port_name_non_relative (const std::string& name) const;
    bool        port_is_mine (const std::string& fullname) const;

    PortHandle register_port (const std::string& shortname, ARDOUR::DataType, ARDOUR::PortFlags);
    void  unregister_port (PortHandle);

    bool  connected (PortHandle);
    bool  connected_to (PortHandle, const std::string&);
    bool  physically_connected (PortHandle);

    int   get_connections (PortHandle, std::vector<std::string>&);

    int   connect (PortHandle, const std::string&);
    int   disconnect (PortHandle, const std::string&);
    int   disconnect_all (PortHandle);

    int   connect (const std::string& src, const std::string& dst);
    int   disconnect (const std::string& src, const std::string& dst);
    
    /* MIDI */

    void     midi_event_get (pframes_t& timestamp, size_t& size, uint8_t** buf, void* port_buffer, uint32_t event_index);
    int      midi_event_put (void* port_buffer, pframes_t timestamp, const uint8_t* buffer, size_t size);
    uint32_t get_midi_event_count (void* port_buffer);
    void     midi_clear (void* port_buffer);

    /* Monitoring */

    bool  can_monitor_input() const;
    int   request_input_monitoring (PortHandle, bool);
    int   ensure_input_monitoring (PortHandle, bool);
    bool  monitoring_input (PortHandle);

    /* Latency management
     */
    
    void          set_latency_range (PortHandle, bool for_playback, LatencyRange);
    LatencyRange  get_latency_range (PortHandle, bool for_playback);
    LatencyRange  get_connected_latency_range (PortHandle, int dir);

    void* get_buffer (PortHandle, pframes_t);

    pframes_t last_frame_time () const;

  private:
    JackConnection* _jack_connection;

    static int  _graph_order_callback (void *arg);
    static void _registration_callback (jack_port_id_t, int, void *);
    static void _connect_callback (jack_port_id_t, jack_port_id_t, int, void *);

    void connect_callback (jack_port_id_t, jack_port_id_t, int);

};

} // namespace 

#endif /* __libardour_jack_portengine_h__ */
