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

#ifndef _MONONOTEPARAMETERS_H_
#define _MONONOTEPARAMETERS_H_

#include <iostream>
#include <vector>
#include <exception>

using std::vector;

class MonoNoteParameters
{
public:
    MonoNoteParameters();
    virtual ~MonoNoteParameters();
    
    // model architecture parameters
    size_t minPitch; // lowest pitch in MIDI notes
    size_t nPPS; // number of pitches per semitone
    size_t nS; // number of semitones
    size_t nSPP; // number of states per pitch
    size_t n; // number of states (will be calcualted from other parameters)
    
    // initial state probabilities
    vector<double> initPi; 
    
    // transition parameters
    double pAttackSelftrans;
    double pStableSelftrans;
    double pStable2Silent;
    double pSilentSelftrans;
    double sigma2Note; // standard deviation of next note Gaussian distribution
    double maxJump;
    double pInterSelftrans;
    
    double priorPitchedProb;
    double priorWeight;

    double minSemitoneDistance; // minimum distance for a transition
    
    double sigmaYinPitchAttack;
    double sigmaYinPitchStable;
    double sigmaYinPitchInter;
    
    double yinTrust;
    
};

#endif
