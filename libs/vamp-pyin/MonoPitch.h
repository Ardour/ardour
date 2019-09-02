/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    pYIN - A fundamental frequency estimator for monophonic audio
    Centre for Digital Music, Queen Mary, University of London.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef _MONOPITCH_H_
#define _MONOPITCH_H_

#include "MonoPitchHMM.h"

#include <iostream>
#include <vector>
#include <exception>

using std::vector;
using std::pair;

class MonoPitch {
public:
    MonoPitch();
    virtual ~MonoPitch();
    
    // pitchProb is a frame-wise vector carrying a vector of pitch-probability pairs
    const vector<float> process(const vector<vector<pair<double, double> > > pitchProb);
private:
    MonoPitchHMM hmm;
};

#endif
