/*
    Copyright (C) 2012 Paul Davis

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

#include "pbd/compose.h"
#include "pbd/failed_constructor.h"

#include "ardour/ardour.h"
#include "ardour/debug.h"
#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/sg_rack.h"
#include "ardour/soundgrid.h"

using namespace ARDOUR;
using namespace PBD;
using std::string;

SoundGridRack::SoundGridRack (Session& s, Route& r, const std::string& name)
        : SessionObject (s, name)
        , _route (r)
{
        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("Creating SG Chainer for %1\n", r.name()));

        if (SoundGrid::instance().add_rack_synchronous (eClusterType_Input, _rack_id)) { 
                throw failed_constructor();
        }

        if (IO::connecting_legal) {
                make_connections ();
        } else {
                IO::ConnectingLegal.connect_same_thread (*this, boost::bind (&SoundGridRack::make_connections, this));
        }
}

SoundGridRack::~SoundGridRack ()
{
        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("Destroying SG Chainer for %1\n", _route.name()));
        (void) SoundGrid::instance().remove_rack_synchronous (eClusterType_Input, _rack_id);
}

int
SoundGridRack::make_connections ()
{
        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("Mapping input for %1\n", _route.name()));

        /* input */

        string portname;

        portname = SoundGrid::instance().sg_port_as_jack_port (SoundGrid::TrackInputPort (_rack_id, 0, eMixMatrixSub_Input));
        
        if (portname.empty()) {
                return -1;
        }

        /* output */

        portname = SoundGrid::instance().sg_port_as_jack_port (SoundGrid::TrackOutputPort (_rack_id, 0, eMixMatrixSub_PostPan));

        if (portname.empty()) {
                return -1;
        }

        return 0;
}

void 
SoundGridRack::add_plugin (boost::shared_ptr<SoundGridPlugin>)
{
}

void 
SoundGridRack::remove_plugin (boost::shared_ptr<SoundGridPlugin>)
{
}

void
SoundGridRack::set_gain (gain_t)
{
}

void
SoundGridRack::set_input_gain (gain_t)
{
}

int32_t
SoundGridRack::jack_port_as_input (const std::string& port_name)
{
        JackChannelMap::iterator x;
        Glib::Threads::Mutex::Lock lm (map_lock);
        
        if ((x = jack_channel_map.find (port_name)) != jack_channel_map.end()) {
                return x->second;
        }

        return -1;
}

string
SoundGridRack::input_as_jack_port (uint32_t channel)
{
        ChannelJackMap::iterator x;
        Glib::Threads::Mutex::Lock lm (map_lock);
        
        if ((x = channel_jack_map.find (channel)) != channel_jack_map.end()) {
                return x->second;
        }

        return string ();
}
