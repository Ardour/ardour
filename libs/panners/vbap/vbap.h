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

#include "vbap_speakers.h"

namespace ARDOUR {

class Speakers;
class Pannable;

class VBAPanner : public Panner 
{ 
public:
	VBAPanner (boost::shared_ptr<Pannable>, boost::shared_ptr<Speakers>);
	~VBAPanner ();

        void configure_io (ChanCount in, ChanCount /* ignored - we use Speakers */);
        ChanCount in() const;
        ChanCount out() const;

        void set_position (double);
        void set_width (double);
        void set_elevation (double);

        std::set<Evoral::Parameter> what_can_be_automated() const;

	static Panner* factory (boost::shared_ptr<Pannable>, boost::shared_ptr<Speakers>);

	void distribute (BufferSet& ibufs, BufferSet& obufs, gain_t gain_coeff, pframes_t nframes);

	void set_azimuth_elevation (double azimuth, double elevation);

        std::string describe_parameter (Evoral::Parameter);
        std::string value_as_string (boost::shared_ptr<AutomationControl>) const;

	XMLNode& get_state ();

        PBD::AngularVector signal_position (uint32_t n) const;
        boost::shared_ptr<Speakers> get_speakers() const;

	void reset ();

private:
        struct Signal {
            PBD::AngularVector direction;
            std::vector<double> gains; /* most recently used gain for all speakers */

            int outputs[3];  /* most recent set of outputs used (2 or 3, depending on dimension) */
            int desired_outputs[3]; /* outputs to use the next time we distribute */
            double desired_gains[3]; /* target gains for desired_outputs */

            Signal (Session&, VBAPanner&, uint32_t which, uint32_t n_speakers);
            void resize_gains (uint32_t n_speakers);
        };

        std::vector<Signal*> _signals;
        boost::shared_ptr<VBAPSpeakers>  _speakers;
        
	void compute_gains (double g[3], int ls[3], int azi, int ele);
        void update ();
        void clear_signals ();

	void distribute_one (AudioBuffer& src, BufferSet& obufs, gain_t gain_coeff, pframes_t nframes, uint32_t which);
	void distribute_one_automated (AudioBuffer& src, BufferSet& obufs,
                                          framepos_t start, framepos_t end, pframes_t nframes, 
                                          pan_t** buffers, uint32_t which);
};

} /* namespace */

#endif /* __libardour_vbap_h__ */
