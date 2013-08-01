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

#include <boost/shared_ptr.hpp>

#include "ardour/port_engine.h"
#include "ardour/types.h"

namespace ARDOUR {

class JackConnection;

class JACKPortEngine : public PortEngine 
{
  public:
    JACKPortEngine (PortManager&, boost::shared_ptr<JackConnection>);
    ~JACKPortEngine();

    void* private_handle() const;

    const std::string& my_name() const;

    int         set_port_name (PortHandle, const std::string&);
    std::string get_port_name (PortHandle) const;
    PortHandle* get_port_by_name (const std::string&) const;

    int get_ports (const std::string& port_name_pattern, DataType type, PortFlags flags, std::vector<std::string>&) const;

    DataType port_data_type (PortHandle) const;

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

    int      midi_event_get (pframes_t& timestamp, size_t& size, uint8_t** buf, void* port_buffer, uint32_t event_index);
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

    /* Physical ports */

    bool      port_is_physical (PortHandle) const;
    void      get_physical_outputs (DataType type, std::vector<std::string>&);
    void      get_physical_inputs (DataType type, std::vector<std::string>&);
    ChanCount n_physical_outputs () const;
    ChanCount n_physical_inputs () const;

    /* Getting access to the data buffer for a port */

    void* get_buffer (PortHandle, pframes_t);

    /* Miscellany */

    pframes_t sample_time_at_cycle_start ();
    
  private:
    boost::shared_ptr<JackConnection> _jack_connection;

    static int  _graph_order_callback (void *arg);
    static void _registration_callback (jack_port_id_t, int, void *);
    static void _connect_callback (jack_port_id_t, jack_port_id_t, int, void *);

    void connect_callback (jack_port_id_t, jack_port_id_t, int);

    ChanCount n_physical (unsigned long flags) const;
    void get_physical (DataType type, unsigned long flags, std::vector<std::string>& phy) const;

};

} // namespace 

#endif /* __libardour_jack_portengine_h__ */
