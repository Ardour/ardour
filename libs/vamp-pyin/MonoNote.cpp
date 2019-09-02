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

#include "MonoNote.h"
#include <vector>

#include <cstdio>
#include <cmath>
#include <complex>

using std::vector;
using std::pair;

MonoNote::MonoNote() :
    hmm()
{
}

MonoNote::~MonoNote()
{
}

const vector<MonoNote::FrameOutput>
MonoNote::process(const vector<vector<pair<double, double> > > pitchProb)
{
    vector<vector<double> > obsProb;
    for (size_t iFrame = 0; iFrame < pitchProb.size(); ++iFrame)
    {
        obsProb.push_back(hmm.calculateObsProb(pitchProb[iFrame]));
    }
    
    vector<double> *scale = new vector<double>(pitchProb.size());
    
    vector<MonoNote::FrameOutput> out; 
    
    vector<int> path = hmm.decodeViterbi(obsProb, scale);
    
    for (size_t iFrame = 0; iFrame < path.size(); ++iFrame)
    {
        double currPitch = -1;
        int stateKind = 0;

        currPitch = hmm.par.minPitch + (path[iFrame]/hmm.par.nSPP) * 1.0/hmm.par.nPPS;
        stateKind = (path[iFrame]) % hmm.par.nSPP + 1;

        out.push_back(FrameOutput(iFrame, currPitch, stateKind));
        // std::cerr << path[iFrame] << " -- "<< pitchProb[iFrame][0].first << " -- "<< currPitch << " -- " << stateKind << std::endl;
    }
    delete scale;
    return(out);
}
