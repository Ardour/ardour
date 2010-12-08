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

#include "ardour/panner.h"
#include "ardour/vbap_speakers.h"

namespace ARDOUR {

class Speakers;

class VBAPanner : public StreamPanner { 
public:
	VBAPanner (Panner& parent, Evoral::Parameter param, Speakers& s);
	~VBAPanner ();

	static StreamPanner* factory (Panner& parent, Evoral::Parameter param, Speakers& s);
	static std::string name;

	void do_distribute (AudioBuffer&, BufferSet& obufs, gain_t gain_coeff, pframes_t nframes);
	void do_distribute_automated (AudioBuffer& src, BufferSet& obufs,
	                              framepos_t start, framepos_t end, pframes_t nframes, pan_t** buffers);

	void set_azimuth_elevation (double azimuth, double elevation);

	XMLNode& state (bool full_state);
	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

	/* there never was any old-school automation */

	int load (std::istream&, std::string path, uint32_t&) { return 0; }

private:
	bool   _dirty;
	double gains[3];
	double desired_gains[3];
	int    outputs[3];
	int    desired_outputs[3];

	VBAPSpeakers& _speakers;
        
	void compute_gains (double g[3], int ls[3], int azi, int ele);

	void update ();
};

} /* namespace */

#endif /* __libardour_vbap_h__ */
