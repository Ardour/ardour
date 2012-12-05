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

#include <climits>

#include "pbd/compose.h"
#include "pbd/failed_constructor.h"

#include "ardour/ardour.h"
#include "ardour/debug.h"
#include "ardour/port.h"
#include "ardour/port_set.h"
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
        , _rack_id (UINT32_MAX)
{
        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("Creating SG Chainer for %1\n", r.name()));

        if (dynamic_cast<Track*> (&r) != 0 && !_route.is_hidden()) {
                /* only real tracks use an InputTrack. the auditioner is a track, but
                   it doesn't need any input
                */
                _cluster_type = eClusterType_InputTrack;
        } else {
                /* bus */
                _cluster_type = eClusterType_GroupTrack;
        }

        int32_t process_group = 1;

        /* XXX eventually these need to be discovered from the route which sets them during a graph sort 
         */

        if (_route.is_monitor()) {
                /* monitor runs last */
                process_group = 6;
        } else if (_route.is_master()) {
                /* master runs before monitor */
                process_group = 5;
        } else if (dynamic_cast<Track*>(&_route) == 0) {
                /* this is a bus, and busses run after tracks */
                process_group = 2;
        }

        if (SoundGrid::instance().add_rack (_cluster_type, process_group, r.n_outputs().n_audio(), _rack_id)) { 
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
        if (_rack_id == UINT32_MAX) {
                return;
        }

        PortSet& ports (_route.output()->ports());

        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("Removing SG/JACK mapping for outputs of %1 with %2 outputs\n", 
                                                       _route.name(), ports.num_ports()));

        for (PortSet::iterator p = ports.begin(); p != ports.end(); ++p) {
                SoundGrid::instance().drop_sg_jack_mapping (p->name());
        }

        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("Destroying SG Chainer for %1\n", _route.name()));
        (void) SoundGrid::instance().remove_rack (_cluster_type, _rack_id);
}

int
SoundGridRack::reconfigure (uint32_t channels)
{
        return SoundGrid::instance().configure_io (_cluster_type, _rack_id, channels);
}

int 
SoundGridRack::set_process_group (uint32_t /*pg*/)
{
        return 0;
}

int
SoundGridRack::make_connections ()
{
        /* we need to deliver out output (essentially at the fader) to the SG server, which will
           happen via the native OS audio driver (and thus via JACK). the output needs to get to 
           our chainer, so we map its input(s) to one or more unused JACK ports. we then connect
           our output JACK ports to these JACK ports, thus establishing signal flow into the chainer.
        */

        _route.output()->disconnect (this);
        
        PortSet& ports (_route.output()->ports());
        uint32_t channel = 0;
        boost::shared_ptr<Route> master_out = _route.session().master_out();
        bool is_track = (dynamic_cast<Track*>(&_route) != 0) && !_route.is_hidden();

        assert (master_out);

        DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("Mapping input for %1 (track ? %2) with %3 outputs\n", 
                                                       _route.name(), is_track, ports.num_ports()));

        for (PortSet::iterator p = ports.begin(); p != ports.end(); ++p, ++channel) {

                DEBUG_TRACE (DEBUG::SoundGrid, string_compose ("Looking at output %1\n", p->name()));

                if (channel > 0) {
                        continue;
                }

                if (p->type() != DataType::AUDIO) {
                        continue;
                }

                /* find a JACK port that will be used to deliver data to the track's chainer's input */
                
                string portname;
                
                if (is_track) {
                        portname = SoundGrid::instance().sg_port_as_jack_port (SoundGrid::TrackInputPort (_rack_id, channel));
                } else {
                        /* bus or auditioner */
                        portname = SoundGrid::instance().sg_port_as_jack_port (SoundGrid::BusInputPort (_rack_id, channel));
                }
                
                if (portname.empty()) {
                        DEBUG_TRACE (DEBUG::SoundGrid, "no JACK port found to route track audio to SG\n");
                        continue;
                }
                
                /* connect this port to it */
                
                p->connect (portname);

                /* Now wire up the output of our SG chainer to ... yes, to what precisely ? 
                   
                   For now:

                    - if its the master or monitor bus, wire it up to physical outputs 1 ( + 2, etc)
                    - otherwise, wire it up to the master bus.
                */


                if (_route.is_master()) {
                        
                        SoundGrid::instance().connect (SoundGrid::BusOutputPort (_rack_id, channel), 
                                                       SoundGrid::PseudoPhysicalOutputPort (channel));

                        /* how to wire to the monitor bus ? */

                } else if (_route.is_monitor()) {

                        /* XXX force different physical wiring for the monitor bus just so that it shows up
                           differently in any wiring graphs.
                        */

                        SoundGrid::instance().connect (SoundGrid::BusOutputPort (_rack_id, channel), 
                                                       SoundGrid::PseudoPhysicalOutputPort (channel+4));
                        

                } else if (_route.is_hidden()) {

                        /* auditioner - wire it directly to the "outputs" */
                        
                        SoundGrid::instance().connect (SoundGrid::BusOutputPort (_rack_id, channel), 
                                                       SoundGrid::PseudoPhysicalOutputPort (channel));
                        
                } else {

                        /* wire normal tracks and busses to the master bus */

                        SoundGrid::instance().connect (SoundGrid::TrackOutputPort (_rack_id, channel), 
                                                       SoundGrid::PseudoPhysicalOutputPort (channel));
                                                       //SoundGrid::BusInputPort (master_out->rack_id(), channel));
                }
                
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
