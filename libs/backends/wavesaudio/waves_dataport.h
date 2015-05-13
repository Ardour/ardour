/*
    Copyright (C) 2013 Waves Audio Ltd.

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

#ifndef __libardour_waves_dataport_h__
#define __libardour_waves_dataport_h__

#include "ardour/types.h"
#include "memory.h"
        
namespace ARDOUR {

class WavesDataPort {
public:

    virtual ~WavesDataPort ();

    inline const std::string& name () const
    {
        return _name;
    }
    
    int set_name (const std::string &name)
    {
        _name = name;
        return 0;
    }

    virtual DataType type () const = 0;

    inline PortFlags flags () const
    {
        return _flags;
    }

    inline bool is_input () { return flags () & IsInput; }
    inline bool is_output () { return flags () & IsOutput; }
    inline bool is_physical () { return flags () & IsPhysical; }
    inline bool is_terminal () { return flags () & IsTerminal; }
    inline operator void* () { return (void*)this; }

    inline const LatencyRange& latency_range (bool for_playback) const
    {
        return for_playback ? _playback_latency_range : _capture_latency_range;
    }

    inline void set_latency_range (const LatencyRange &latency_range, bool for_playback)
    {
        if (for_playback)
        {
            _playback_latency_range = latency_range;
        }
        else
        {
            _capture_latency_range = latency_range;
        }
    }

    int connect (WavesDataPort *port);
    
    int disconnect (WavesDataPort *port);
    
    void disconnect_all ();

    bool inline is_connected (const WavesDataPort *port) const
    {
        return std::find (_connections.begin (), _connections.end (), port) != _connections.end ();
    }

    bool inline is_connected () const
    {
        return _connections.size () != 0;
    }

    bool is_physically_connected () const;

    inline const std::vector<WavesDataPort *>& get_connections () const { return _connections; }

    virtual void* get_buffer (pframes_t nframes) = 0;

protected:
    WavesDataPort (const std::string& inport_name, PortFlags inflags);
	virtual void _wipe_buffer() = 0;

private:

    std::string _name;
    const PortFlags _flags;
    LatencyRange _capture_latency_range;
    LatencyRange  _playback_latency_range;
    std::vector<WavesDataPort*> _connections;

    void _connect (WavesDataPort* port, bool api_call);
    void _disconnect_all ();
    void _disconnect (WavesDataPort* port, bool api_call);
};

} // namespace

#endif /* __libardour_waves_dataport_h__ */
    
