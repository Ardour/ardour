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

#include "SparseHMM.h"
#include <vector>
#include <cstdio>
#include <iostream>

using std::vector;
using std::pair;

const vector<double>
SparseHMM::calculateObsProb(const vector<pair<double, double> > data)
{
    // dummy (virtual?) implementation to be overloaded
    return(vector<double>());
}

const std::vector<int> 
SparseHMM::decodeViterbi(std::vector<vector<double> > obsProb,
                         vector<double> *scale) 
{
    if (obsProb.size() < 1) {
        return vector<int>();
    }

    size_t nState = init.size();
    size_t nFrame = obsProb.size();
    
    // check for consistency    
    size_t nTrans = transProb.size();
    
    // declaring variables
    std::vector<double> delta = std::vector<double>(nState);
    std::vector<double> oldDelta = std::vector<double>(nState);
    vector<vector<int> > psi; //  "matrix" of remembered indices of the best transitions
    vector<int> path = vector<int>(nFrame, nState-1); // the final output path (current assignment arbitrary, makes sense only for Chordino, where nChord-1 is the "no chord" label)

    double deltasum = 0;

    // initialise first frame
    for (size_t iState = 0; iState < nState; ++iState)
    {
        oldDelta[iState] = init[iState] * obsProb[0][iState];
        // std::cerr << iState << " ----- " << init[iState] << std::endl;
        deltasum += oldDelta[iState];
    }

    for (size_t iState = 0; iState < nState; ++iState)
    {
        oldDelta[iState] /= deltasum; // normalise (scale)
        // std::cerr << oldDelta[iState] << std::endl;
    }

    scale->push_back(1.0/deltasum);
    psi.push_back(vector<int>(nState,0));

    // rest of forward step
    for (size_t iFrame = 1; iFrame < nFrame; ++iFrame)
    {
        deltasum = 0;
        psi.push_back(vector<int>(nState,0));

        // calculate best previous state for every current state
        size_t fromState;
        size_t toState;
        double currentTransProb;
        double currentValue;
        
        // this is the "sparse" loop
        for (size_t iTrans = 0; iTrans < nTrans; ++iTrans)
        {
            fromState = from[iTrans];
            toState = to[iTrans];
            currentTransProb = transProb[iTrans];
            
            currentValue = oldDelta[fromState] * currentTransProb;
            if (currentValue > delta[toState])
            {
                delta[toState] = currentValue; // will be multiplied by the right obs later!
                psi[iFrame][toState] = fromState;
            }            
        }
        
        for (size_t jState = 0; jState < nState; ++jState)
        {
            delta[jState] *= obsProb[iFrame][jState];
            deltasum += delta[jState];
        }

        if (deltasum > 0)
        {
            for (size_t iState = 0; iState < nState; ++iState)
            {
                oldDelta[iState] = delta[iState] / deltasum; // normalise (scale)
                delta[iState] = 0;
            }
            scale->push_back(1.0/deltasum);
        } else
        {
            std::cerr << "WARNING: Viterbi has been fed some zero probabilities, at least they become zero at frame " <<  iFrame << " in combination with the model." << std::endl;
            for (size_t iState = 0; iState < nState; ++iState)
            {
                oldDelta[iState] = 1.0/nState;
                delta[iState] = 0;
            }
            scale->push_back(1.0);
        }
    }

    // initialise backward step
    double bestValue = 0;
    for (size_t iState = 0; iState < nState; ++iState)
    {
        double currentValue = oldDelta[iState];
        if (currentValue > bestValue)
        {
            bestValue = currentValue;            
            path[nFrame-1] = iState;
        }
    }

    // rest of backward step
    for (int iFrame = nFrame-2; iFrame != -1; --iFrame)
    {
        path[iFrame] = psi[iFrame+1][path[iFrame+1]];
    }
    
    // for (size_t iState = 0; iState < nState; ++iState)
    // {
    //     // std::cerr << psi[2][iState] << std::endl;
    // }
    
    return path;
}
