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

#include "MonoPitchHMM.h"

#include <boost/math/distributions.hpp>

#include <cstdio>
#include <cmath>

using std::vector;
using std::pair;

MonoPitchHMM::MonoPitchHMM() :
m_minFreq(61.735),
m_nBPS(5),
m_nPitch(0),
m_transitionWidth(0),
m_selfTrans(0.99),
m_yinTrust(.5),
m_freqs(0)
{
    m_transitionWidth = 5*(m_nBPS/2) + 1;
    m_nPitch = 69 * m_nBPS;
    m_freqs = vector<double>(2*m_nPitch);
    for (size_t iPitch = 0; iPitch < m_nPitch; ++iPitch)
    {
        m_freqs[iPitch] = m_minFreq * std::pow(2, iPitch * 1.0 / (12 * m_nBPS));
        m_freqs[iPitch+m_nPitch] = -m_freqs[iPitch];
    }
    build();
}

const vector<double>
MonoPitchHMM::calculateObsProb(const vector<pair<double, double> > pitchProb)
{
    vector<double> out = vector<double>(2*m_nPitch+1);
    double probYinPitched = 0;
    // BIN THE PITCHES
    for (size_t iPair = 0; iPair < pitchProb.size(); ++iPair)
    {
        double freq = 440. * std::pow(2, (pitchProb[iPair].first - 69)/12);
        if (freq <= m_minFreq) continue;
        double d = 0;
        double oldd = 1000;
        for (size_t iPitch = 0; iPitch < m_nPitch; ++iPitch)
        {
            d = std::abs(freq-m_freqs[iPitch]);
            if (oldd < d && iPitch > 0)
            {
                // previous bin must have been the closest
                out[iPitch-1] = pitchProb[iPair].second;
                probYinPitched += out[iPitch-1];
                break;
            }
            oldd = d;
        }
    }

    double probReallyPitched = m_yinTrust * probYinPitched;
    // std::cerr << probReallyPitched << " " << probYinPitched << std::endl;
    // damn, I forget what this is all about...
    for (size_t iPitch = 0; iPitch < m_nPitch; ++iPitch)
    {
        if (probYinPitched > 0) out[iPitch] *= (probReallyPitched/probYinPitched) ;
        out[iPitch+m_nPitch] = (1 - probReallyPitched) / m_nPitch;
    }
    // out[2*m_nPitch] = m_yinTrust * (1 - probYinPitched);
    return(out);
}

void
MonoPitchHMM::build()
{
    // INITIAL VECTOR
    init = vector<double>(2*m_nPitch, 1.0 / 2*m_nPitch);

    // TRANSITIONS
    for (size_t iPitch = 0; iPitch < m_nPitch; ++iPitch)
    {
        int theoreticalMinNextPitch = static_cast<int>(iPitch)-static_cast<int>(m_transitionWidth/2);
        size_t minNextPitch = iPitch>m_transitionWidth/2 ? iPitch-m_transitionWidth/2 : 0;
        size_t maxNextPitch = iPitch<m_nPitch-m_transitionWidth/2 ? iPitch+m_transitionWidth/2 : m_nPitch-1;

        // WEIGHT VECTOR
        double weightSum = 0;
        vector<double> weights;
        for (size_t i = minNextPitch; i <= maxNextPitch; ++i)
        {
            if (i <= iPitch)
            {
                weights.push_back(i-theoreticalMinNextPitch+1);
                // weights.push_back(i-theoreticalMinNextPitch+1+m_transitionWidth/2);
            } else {
                weights.push_back(iPitch-theoreticalMinNextPitch+1-(i-iPitch));
                // weights.push_back(iPitch-theoreticalMinNextPitch+1-(i-iPitch)+m_transitionWidth/2);
            }
            weightSum += weights[weights.size()-1];
        }

        // std::cerr << minNextPitch << "  " << maxNextPitch << std::endl;
        // TRANSITIONS TO CLOSE PITCH
        for (size_t i = minNextPitch; i <= maxNextPitch; ++i)
        {
            from.push_back(iPitch);
            to.push_back(i);
            transProb.push_back(weights[i-minNextPitch] / weightSum * m_selfTrans);

            from.push_back(iPitch);
            to.push_back(i+m_nPitch);
            transProb.push_back(weights[i-minNextPitch] / weightSum * (1-m_selfTrans));

            from.push_back(iPitch+m_nPitch);
            to.push_back(i+m_nPitch);
            transProb.push_back(weights[i-minNextPitch] / weightSum * m_selfTrans);
            // transProb.push_back(weights[i-minNextPitch] / weightSum * 0.5);

            from.push_back(iPitch+m_nPitch);
            to.push_back(i);
            transProb.push_back(weights[i-minNextPitch] / weightSum * (1-m_selfTrans));
            // transProb.push_back(weights[i-minNextPitch] / weightSum * 0.5);
        }

        // TRANSITION TO UNVOICED
        // from.push_back(iPitch+m_nPitch);
        // to.push_back(2*m_nPitch);
        // transProb.push_back(1-m_selfTrans);

        // TRANSITION FROM UNVOICED TO PITCH
        // from.push_back(2*m_nPitch);
        // to.push_back(iPitch+m_nPitch);
        // transProb.push_back(1.0/m_nPitch);
    }
    // UNVOICED SELFTRANSITION
    // from.push_back(2*m_nPitch);
    // to.push_back(2*m_nPitch);
    // transProb.push_back(m_selfTrans);

    // for (size_t i = 0; i < from.size(); ++i) {
    //     std::cerr << "P(["<< from[i] << " --> " << to[i] << "]) = " << transProb[i] << std::endl;
    // }

}
