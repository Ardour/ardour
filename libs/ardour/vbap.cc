
/* 
   This software is being provided to you, the licensee, by Ville Pulkki,
   under the following license. By obtaining, using and/or copying this
   software, you agree that you have read, understood, and will comply
   with these terms and conditions: Permission to use, copy, modify and
   distribute, including the right to grant others rights to distribute
   at any tier, this software and its documentation for any purpose and
   without fee or royalty is hereby granted, provided that you agree to
   comply with the following copyright notice and statements, including
   the disclaimer, and that the same appear on ALL copies of the software
   and documentation, including modifications that you make for internal
   use or for distribution:
   
   Copyright 1998 by Ville Pulkki, Helsinki University of Technology.  All
   rights reserved.  
   
   The software may be used, distributed, and included to commercial
   products without any charges. When included to a commercial product,
   the method "Vector Base Amplitude Panning" and its developer Ville
   Pulkki must be referred to in documentation.
   
   This software is provided "as is", and Ville Pulkki or Helsinki
   University of Technology make no representations or warranties,
   expressed or implied. By way of example, but not limitation, Helsinki
   University of Technology or Ville Pulkki make no representations or
   warranties of merchantability or fitness for any particular purpose or
   that the use of the licensed software or documentation will not
   infringe any third party patents, copyrights, trademarks or other
   rights. The name of Ville Pulkki or Helsinki University of Technology
   may not be used in advertising or publicity pertaining to distribution
   of the software.
*/

#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <iostream>
#include <string>

#include "pbd/cartesian.h"

#include "ardour/speakers.h"
#include "ardour/vbap.h"
#include "ardour/vbap_speakers.h"
#include "ardour/audio_buffer.h"
#include "ardour/buffer_set.h"

using namespace PBD;
using namespace ARDOUR;
using namespace std;

string VBAPanner::name = X_("VBAP");

VBAPanner::VBAPanner (Panner& parent, Evoral::Parameter param, Speakers& s)
	: StreamPanner (parent, param)
	, _dirty (false)
	, _speakers (VBAPSpeakers::instance (s))
{
}

VBAPanner::~VBAPanner ()
{
}

void
VBAPanner::mark_dirty ()
{
	_dirty = true;
}

void
VBAPanner::update ()
{
	/* force 2D for now */
	_angles.ele = 0.0;
	_dirty = true;

	Changed ();
}

void 
VBAPanner::compute_gains (double gains[3], int speaker_ids[3], int azi, int ele) 
{
	/* calculates gain factors using loudspeaker setup and given direction */
	double cartdir[3];
	double power;
	int i,j,k;
	double small_g;
	double big_sm_g, gtmp[3];

	azi_ele_to_cart (azi,ele, cartdir[0], cartdir[1], cartdir[2]);  
	big_sm_g = -100000.0;

	for (i = 0; i < _speakers.n_tuples(); i++) {

		small_g = 10000000.0;

		for (j = 0; j < _speakers.dimension(); j++) {

			gtmp[j]=0.0;

			for (k = 0; k < _speakers.dimension(); k++) {
				gtmp[j] += cartdir[k] * _speakers.matrix(i)[j*_speakers.dimension()+k]; 
			}

			if (gtmp[j] < small_g) {
				small_g = gtmp[j];
			}
		}

		if (small_g > big_sm_g) {

			big_sm_g = small_g;

			gains[0] = gtmp[0]; 
			gains[1] = gtmp[1]; 

			speaker_ids[0] = _speakers.speaker_for_tuple (i, 0);
			speaker_ids[1] = _speakers.speaker_for_tuple (i, 1);
                        
			if (_speakers.dimension() == 3) {
				gains[2] = gtmp[2];
				speaker_ids[2] = _speakers.speaker_for_tuple (i, 2);
			} else {
				gains[2] = 0.0;
				speaker_ids[2] = -1;
			}
		}
	}
        
	power = sqrt (gains[0]*gains[0] + gains[1]*gains[1] + gains[2]*gains[2]);

	gains[0] /= power; 
	gains[1] /= power;
	gains[2] /= power;

	_dirty = false;
}

void
VBAPanner::do_distribute (AudioBuffer& srcbuf, BufferSet& obufs, gain_t gain_coefficient, nframes_t nframes)
{
	if (_muted) {
		return;
	}

	Sample* const src = srcbuf.data();
	Sample* dst;
	pan_t pan;
	uint32_t n_audio = obufs.count().n_audio();
	bool was_dirty;

	if ((was_dirty = _dirty)) {
		compute_gains (desired_gains, desired_outputs, _angles.azi, _angles.ele);
		cerr << " @ " << _angles.azi << " /= " << _angles.ele
		     << " Outputs: "
		     << desired_outputs[0] + 1 << ' '
		     << desired_outputs[1] + 1 << ' '
		     << " Gains "
		     << desired_gains[0] << ' '
		     << desired_gains[1] << ' '
		     << endl;
	}

	bool todo[n_audio];
        
	for (uint32_t o = 0; o < n_audio; ++o) {
		todo[o] = true;
	}

        
	/* VBAP may distribute the signal across up to 3 speakers depending on
	   the configuration of the speakers.
	*/

	for (int o = 0; o < 3; ++o) {
		if (desired_outputs[o] != -1) {

			nframes_t n = 0;

			/* XXX TODO: interpolate across changes in gain and/or outputs
			 */

			dst = obufs.get_audio(desired_outputs[o]).data();

			pan = gain_coefficient * desired_gains[o];
			mix_buffers_with_gain (dst+n,src+n,nframes-n,pan);

			todo[o] = false;
		}
	}
        
	for (uint32_t o = 0; o < n_audio; ++o) {
		if (todo[o]) {
			/* VBAP decided not to deliver any audio to this output, so we write silence */
			dst = obufs.get_audio(o).data();
			memset (dst, 0, sizeof (Sample) * nframes);
		}
	}
        
	if (was_dirty) {
		memcpy (gains, desired_gains, sizeof (gains));
		memcpy (outputs, desired_outputs, sizeof (outputs));
	}
}

void 
VBAPanner::do_distribute_automated (AudioBuffer& src, BufferSet& obufs,
                                    nframes_t start, nframes_t end, nframes_t nframes, pan_t** buffers)
{
}

XMLNode&
VBAPanner::get_state ()
{
	return state (true);
}

XMLNode&
VBAPanner::state (bool full_state)
{
        XMLNode& node (StreamPanner::get_state());
	node.add_property (X_("type"), VBAPanner::name);
	return node;
}

int
VBAPanner::set_state (const XMLNode& node, int /*version*/)
{
	return 0;
}

StreamPanner*
VBAPanner::factory (Panner& parent, Evoral::Parameter param, Speakers& s)
{
	return new VBAPanner (parent, param, s);
}

