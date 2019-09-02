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

#include "MonoNoteHMM.h"

#include <boost/math/distributions.hpp>

#include <cstdio>
#include <cmath>

using std::vector;
using std::pair;

MonoNoteHMM::MonoNoteHMM() :
    par()
{
    build();
}

const vector<double>
MonoNoteHMM::calculateObsProb(const vector<pair<double, double> > pitchProb)
{
    // pitchProb is a list of pairs (pitches and their probabilities)
    
    size_t nCandidate = pitchProb.size();
    
    // what is the probability of pitched
    double pIsPitched = 0;
    for (size_t iCandidate = 0; iCandidate < nCandidate; ++iCandidate)
    {
        // pIsPitched = pitchProb[iCandidate].second > pIsPitched ? pitchProb[iCandidate].second : pIsPitched;
        pIsPitched += pitchProb[iCandidate].second;
    }

    // pIsPitched = std::pow(pIsPitched, (1-par.priorWeight)) * std::pow(par.priorPitchedProb, par.priorWeight);
    pIsPitched = pIsPitched * (1-par.priorWeight) + par.priorPitchedProb * par.priorWeight;

    vector<double> out = vector<double>(par.n);
    double tempProbSum = 0;
    for (size_t i = 0; i < par.n; ++i)
    {
        if (i % par.nSPP != 2)
        {
            // std::cerr << getMidiPitch(i) << std::endl;
            double tempProb = 0;
            if (nCandidate > 0)
            {
                double minDist = 10000.0;
                double minDistProb = 0;
                size_t minDistCandidate = 0;
                for (size_t iCandidate = 0; iCandidate < nCandidate; ++iCandidate)
                {
                    double currDist = std::abs(getMidiPitch(i)-pitchProb[iCandidate].first);
                    if (currDist < minDist)
                    {
                        minDist = currDist;
                        minDistProb = pitchProb[iCandidate].second;
                        minDistCandidate = iCandidate;
                    }
                }
                tempProb = std::pow(minDistProb, par.yinTrust) * 
                           boost::math::pdf(pitchDistr[i], 
                                            pitchProb[minDistCandidate].first);
            } else {
                tempProb = 1;
            }
            tempProbSum += tempProb;
            out[i] = tempProb;
        }
    }
    
    for (size_t i = 0; i < par.n; ++i)
    {
        if (i % par.nSPP != 2)
        {
            if (tempProbSum > 0) 
            {
                out[i] = out[i] / tempProbSum * pIsPitched;
            }
        } else {
            out[i] = (1-pIsPitched) / (par.nPPS * par.nS);
        }
    }

    return(out);
}

void
MonoNoteHMM::build()
{
    // the states are organised as follows:
    // 0-2. lowest pitch
    //    0. attack state
    //    1. stable state
    //    2. silent state
    // 3-5. second-lowest pitch
    //    3. attack state
    //    ...
    
    // observation distributions
    for (size_t iState = 0; iState < par.n; ++iState)
    {
        pitchDistr.push_back(boost::math::normal(0,1));
        if (iState % par.nSPP == 2)
        {
            // silent state starts tracking
            init.push_back(1.0/(par.nS * par.nPPS));
        } else {
            init.push_back(0.0);            
        }
    }

    for (size_t iPitch = 0; iPitch < (par.nS * par.nPPS); ++iPitch)
    {
        size_t index = iPitch * par.nSPP;
        double mu = par.minPitch + iPitch * 1.0/par.nPPS;
        pitchDistr[index] = boost::math::normal(mu, par.sigmaYinPitchAttack);
        pitchDistr[index+1] = boost::math::normal(mu, par.sigmaYinPitchStable);
        pitchDistr[index+2] = boost::math::normal(mu, 1.0); // dummy
    }
    
    boost::math::normal noteDistanceDistr(0, par.sigma2Note);

    for (size_t iPitch = 0; iPitch < (par.nS * par.nPPS); ++iPitch)
    {
        // loop through all notes and set sparse transition probabilities
        size_t index = iPitch * par.nSPP;

        // transitions from attack state
        from.push_back(index);
        to.push_back(index);
        transProb.push_back(par.pAttackSelftrans);

        from.push_back(index);
        to.push_back(index+1);
        transProb.push_back(1-par.pAttackSelftrans);

        // transitions from stable state
        from.push_back(index+1);
        to.push_back(index+1); // to itself
        transProb.push_back(par.pStableSelftrans);
        
        from.push_back(index+1);
        to.push_back(index+2); // to silent
        transProb.push_back(par.pStable2Silent);

        // the "easy" transitions from silent state
        from.push_back(index+2);
        to.push_back(index+2);
        transProb.push_back(par.pSilentSelftrans);
        
        
        // the more complicated transitions from the silent
        double probSumSilent = 0;

        vector<double> tempTransProbSilent;
        for (size_t jPitch = 0; jPitch < (par.nS * par.nPPS); ++jPitch)
        {
            int fromPitch = iPitch;
            int toPitch = jPitch;
            double semitoneDistance = 
                std::abs(fromPitch - toPitch) * 1.0 / par.nPPS;
            
            // if (std::fmod(semitoneDistance, 1) == 0 && semitoneDistance > par.minSemitoneDistance)
            if (semitoneDistance == 0 || 
                (semitoneDistance > par.minSemitoneDistance 
                 && semitoneDistance < par.maxJump))
            {
                size_t toIndex = jPitch * par.nSPP; // note attack index

                double tempWeightSilent = boost::math::pdf(noteDistanceDistr, 
                                                           semitoneDistance);
                probSumSilent += tempWeightSilent;

                tempTransProbSilent.push_back(tempWeightSilent);

                from.push_back(index+2);
                to.push_back(toIndex);
            }
        }
        for (size_t i = 0; i < tempTransProbSilent.size(); ++i)
        {
            transProb.push_back((1-par.pSilentSelftrans) * tempTransProbSilent[i]/probSumSilent);
        }
    }
}

double
MonoNoteHMM::getMidiPitch(size_t index)
{
    return pitchDistr[index].mean();
}

double
MonoNoteHMM::getFrequency(size_t index)
{
    return 440 * pow(2, (pitchDistr[index].mean()-69)/12);
}
