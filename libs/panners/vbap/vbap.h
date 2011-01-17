/*
    Copyright (C) 2010 Paul Davis

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

#ifndef __libardour_vbap_h__
#define __libardour_vbap_h__

#include <string>
#include <map>

#include "pbd/cartesian.h"

#include "ardour/panner.h"
#include "ardour/panner_shell.h"
#include "ardour/vbap_speakers.h"

namespace ARDOUR {

class Speakers;
class Pannable;

class VBAPanner : public Panner 
{ 
public:
	VBAPanner (boost::shared_ptr<Pannable>, Speakers& s);
	~VBAPanner ();

        void configure_io (const ChanCount& in, const ChanCount& /* ignored - we use Speakers */);
        ChanCount in() const;
        ChanCount out() const;

	static Panner* factory (boost::shared_ptr<Pannable>, Speakers& s);

	void do_distribute (BufferSet& ibufs, BufferSet& obufs, gain_t gain_coeff, pframes_t nframes);
	void do_distribute_automated (BufferSet& ibufs, BufferSet& obufs,
	                              framepos_t start, framepos_t end, pframes_t nframes, pan_t** buffers);

	void set_azimuth_elevation (double azimuth, double elevation);

	XMLNode& state (bool full_state);
	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

        boost::shared_ptr<AutomationControl> azimuth_control (uint32_t signal);
        boost::shared_ptr<AutomationControl> evelation_control (uint32_t signal);

        std::string describe_parameter (Evoral::Parameter param);

private:
        struct Signal {
            PBD::AngularVector direction;
            double gains[3];
            double desired_gains[3];
            int    outputs[3];
            int    desired_outputs[3];
            boost::shared_ptr<AutomationControl> azimuth_control;
            boost::shared_ptr<AutomationControl> elevation_control;

            Signal (Session&, VBAPanner&, uint32_t which);
        };

        std::vector<Signal*> _signals;
	bool                _dirty;
        VBAPSpeakers&       _speakers;
        
	void compute_gains (double g[3], int ls[3], int azi, int ele);

	void do_distribute_one (AudioBuffer& src, BufferSet& obufs, gain_t gain_coeff, pframes_t nframes, uint32_t which);
	void do_distribute_one_automated (AudioBuffer& src, BufferSet& obufs,
                                          framepos_t start, framepos_t end, pframes_t nframes, 
                                          pan_t** buffers, uint32_t which);
};

} /* namespace */

#endif /* __libardour_vbap_h__ */
