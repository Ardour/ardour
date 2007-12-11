/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Rubber Band
    An audio time-stretching and pitch-shifting library.
    Copyright 2007 Chris Cannam.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "PercussiveAudioCurve.h"

#include <cmath>

namespace RubberBand
{

PercussiveAudioCurve::PercussiveAudioCurve(size_t sampleRate, size_t windowSize) :
    AudioCurve(sampleRate, windowSize)
{
    m_prevMag = new double[m_windowSize/2 + 1];

    for (size_t i = 0; i <= m_windowSize/2; ++i) {
        m_prevMag[i] = 0.f;
    }
}

PercussiveAudioCurve::~PercussiveAudioCurve()
{
    delete[] m_prevMag;
}

void
PercussiveAudioCurve::reset()
{
    for (size_t i = 0; i <= m_windowSize/2; ++i) {
        m_prevMag[i] = 0;
    }
}

void
PercussiveAudioCurve::setWindowSize(size_t newSize)
{
    delete[] m_prevMag;
    m_windowSize = newSize;
    
    m_prevMag = new double[m_windowSize/2 + 1];

    reset();
}

float
PercussiveAudioCurve::process(float *mag, size_t increment)
{
    static float threshold = pow(10, 0.3);
    static float zeroThresh = pow(10, -16);

    size_t count = 0;
    size_t nonZeroCount = 0;

    for (size_t n = 1; n <= m_windowSize / 2; ++n) {
	float sqrmag = mag[n] * mag[n];
        bool above = ((sqrmag / m_prevMag[n]) >= threshold);
        if (above) ++count;
        if (sqrmag > zeroThresh) ++nonZeroCount;
	m_prevMag[n] = sqrmag;
    }

    if (nonZeroCount == 0) return 0;
    else return float(count) / float(nonZeroCount);
}

}

