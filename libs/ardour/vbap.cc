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
