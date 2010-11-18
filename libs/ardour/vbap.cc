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

#include <string>

#include "ardour/vbap.h"

using namespace ARDOUR;

VBAPanner::VBAPanner (Panner& parent, Evoral::Parameter param, VBAPSpeakers& s)
        : StreamPanner (parent, param)
        , _speakers (s)
{
}

VBAPanner::~VBAPanner ()
{
}

void
VBAPanner::azi_ele_to_cart (int azi, int ele, double* c)
{
        static const double atorad = (2.0 * M_PI / 360.0) ;
        c[0] = cos (azi * atorad) * cos (ele * atorad);
        c[1] = sin (azi * atorad) * cos (ele * atorad);
        c[2] = sin (ele * atorad);
}

void
VBAPanner::update ()
{
        double g[3];
        int    ls[3];

        compute_gains (g, ls, _azimuth, _elevation);
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

        azi_ele_to_cart (azi,ele, cartdir);  
        big_sm_g = -100000.0;

        for (i = 0; i < _speakers.n_tuples(); i++){
                small_g = 10000000.0;
                for (j = 0; j < _speakers.dimension(); j++) {
                        gtmp[j]=0.0;
                        for (k = 0; k < _speakers.dimension(); k++)
                                gtmp[j]+=cartdir[k]*_speakers.matrix(i)[j*_speakers.dimension()+k]; 
                        if (gtmp[j] < small_g)
                                small_g = gtmp[j];
                }

                if (small_g > big_sm_g) {
                        big_sm_g = small_g;
                        gains[0]=gtmp[0]; 
                        gains[1]=gtmp[1]; 
                        speaker_ids[0]= _speakers.speaker_for_tuple (i, 0);
                        speaker_ids[1]= _speakers.speaker_for_tuple (i, 1);

                        if (_speakers.dimension() == 3) {
                                gains[2] = gtmp[2];
                                speaker_ids[2] = _speakers.speaker_for_tuple (i, 2);
                        } else {
                                gains[2] = 0.0;
                                speaker_ids[2] = 0;
                        }
                }
        }
        
        power = sqrt (gains[0]*gains[0] + gains[1]*gains[1] + gains[2]*gains[2]);

        gains[0] /= power; 
        gains[1] /= power;
        gains[2] /= power;
}

void
VBAPanner::do_distribute (AudioBuffer& bufs, BufferSet& obufs, gain_t gain_coefficient, nframes_t nframes)
{
}
