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

#ifndef _MONONOTEHMM_H_
#define _MONONOTEHMM_H_

#include "MonoNoteParameters.h"
#include "SparseHMM.h"

#include <boost/math/distributions.hpp>

#include <vector>
#include <cstdio>

using std::vector;

class MonoNoteHMM : public SparseHMM
{
public:
    MonoNoteHMM();
    const std::vector<double> calculateObsProb(const vector<pair<double, double> >);
    double getMidiPitch(size_t index);
    double getFrequency(size_t index);
    void build();
    MonoNoteParameters par;
    vector<boost::math::normal> pitchDistr;
};

#endif
