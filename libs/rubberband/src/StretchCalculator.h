/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Rubber Band
    An audio time-stretching and pitch-shifting library.
    Copyright 2007-2008 Chris Cannam.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef _RUBBERBAND_STRETCH_CALCULATOR_H_
#define _RUBBERBAND_STRETCH_CALCULATOR_H_

#include <sys/types.h>

#include <vector>

namespace RubberBand
{

class StretchCalculator
{
public:
    StretchCalculator(size_t sampleRate, size_t inputIncrement, bool useHardPeaks);
    virtual ~StretchCalculator();

    /**
     * Calculate phase increments for a region of audio, given the
     * overall target stretch ratio, input duration in audio samples,
     * and the audio curves to use for identifying phase lock points
     * (lockAudioCurve) and for allocating stretches to relatively
     * less prominent points (stretchAudioCurve).
     */
    virtual std::vector<int> calculate(double ratio, size_t inputDuration,
                                       const std::vector<float> &lockAudioCurve,
                                       const std::vector<float> &stretchAudioCurve);

    /**
     * Calculate the phase increment for a single audio block, given
     * the overall target stretch ratio and the block's value on the
     * phase-lock audio curve.  State is retained between calls in the
     * StretchCalculator object; call reset() to reset it.  This uses
     * a less sophisticated method than the offline calculate().
     *
     * If increment is non-zero, use it for the input increment for
     * this block in preference to m_increment.
     */
    virtual int calculateSingle(double ratio, float curveValue,
                                size_t increment = 0);

    void setUseHardPeaks(bool use) { m_useHardPeaks = use; }

    void reset();
  
    void setDebugLevel(int level) { m_debugLevel = level; }

    struct Peak {
        size_t chunk;
        bool hard;
    };
    std::vector<Peak> getLastCalculatedPeaks() const { return m_lastPeaks; }

    std::vector<float> smoothDF(const std::vector<float> &df);

protected:
    std::vector<Peak> findPeaks(const std::vector<float> &audioCurve);

    std::vector<int> distributeRegion(const std::vector<float> &regionCurve,
                                      size_t outputDuration, float ratio,
                                      bool phaseReset);

    void calculateDisplacements(const std::vector<float> &df,
                                float &maxDf,
                                double &totalDisplacement,
                                double &maxDisplacement,
                                float adj) const;

    size_t m_sampleRate;
    size_t m_blockSize;
    size_t m_increment;
    float m_prevDf;
    double m_divergence;
    float m_recovery;
    float m_prevRatio;
    int m_transientAmnesty; // only in RT mode; handled differently offline
    int m_debugLevel;
    bool m_useHardPeaks;
    
    std::vector<Peak> m_lastPeaks;
};

}

#endif
