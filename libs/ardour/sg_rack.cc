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
#include "ardour/track.h"
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

        if (r.is_hidden()) {
                return;
        }

        if (dynamic_cast<Track*> (&r) != 0) {
                _cluster_type = eClusterType_Input;
        } else {
                /* bus */
                if (r.is_master()) {
                        _cluster_type = eClusterType_Input;
                } else if (r.is_monitor()) {
                        _cluster_type = eClusterType_Input;
                } else {
                        _cluster_type = eClusterType_Input;
                }
        }

        if (SoundGrid::instance().add_rack_synchronous (_cluster_type, _rack_id)) { 
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
        (void) SoundGrid::instance().remove_rack_synchronous (_cluster_type, _rack_id);
}

int
SoundGridRack::make_connections ()
{
        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("Mapping input for %1\n", _route.name()));

        /* we need to deliver out output (essentially at the fader) to the SG server, which will
           happen via the native OS audio driver (and thus via JACK). the output needs to get to 
           our chainer, so we map its input(s) to one or more unused JACK ports. we then connect
           our output JACK ports to these JACK ports, thus establishing signal flow into the chainer.
        */

#if 0
        string portname;
        
        portname = SoundGrid::instance().sg_port_as_jack_port (SoundGrid::TrackInputPort (_rack_id, eChainerSub_NoSub, (uint32_t) -1));
        
        if (portname.empty()) {
                return -1;
        }
#endif        

        _route.input()->disconnect (this);
        _route.output()->disconnect (this);

        /* wire up the driver input to the chainer input, thus allowing us to pick up data from the native OS driver
           (where JACK will deliver it)
        */

        SoundGrid::instance().connect (SoundGrid::PhysicalInputPort (0),
                                       SoundGrid::DriverOutputPort (0));

        SoundGrid::instance().connect (SoundGrid::DriverInputPort (0),
                                       SoundGrid::TrackInputPort (_rack_id, eChainerSub_NoSub, (uint32_t) -1));

        SoundGrid::instance().connect (SoundGrid::TrackOutputPort (_rack_id, eChainerSub_Left, eMixMatrixSub_PostFader),
                                       SoundGrid::PhysicalOutputPort (0));

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
SoundGridRack::set_fader (gain_t v)
{
        /* convert to SoundGrid value range */

        v *= 1000.0;

        if (SoundGrid::instance().set_gain (_cluster_type, _rack_id, v) != 0) {
                return;
        }

        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("fader level for %1:%2 set to %3\n",
                                                       _cluster_type, _rack_id, v));
}

void
SoundGridRack::set_input_gain (gain_t)
{
}

double
SoundGridRack::get_fader() const
{
        double v;

        if (!SoundGrid::instance().get_gain (_cluster_type, _rack_id, v)) {
                /* failure, return 0dB gain coefficient */
                return 1.0;
        }

        /* convert soundgrid value to our range (0..2.0)
         */

        return v / 1000.0;
}
