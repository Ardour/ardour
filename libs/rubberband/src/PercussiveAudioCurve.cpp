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

#include "PercussiveAudioCurve.h"

#include "Profiler.h"

#include <cmath>


namespace RubberBand
{

PercussiveAudioCurve::PercussiveAudioCurve(size_t sampleRate, size_t windowSize) :
    AudioCurve(sampleRate, windowSize)
{
    m_prevMag = new float[m_windowSize/2 + 1];

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
    m_windowSize = newSize;

    delete[] m_prevMag;
    m_prevMag = new float[m_windowSize/2 + 1];

    reset();
}

float
PercussiveAudioCurve::process(const float *R__ mag, size_t increment)
{
    static float threshold = powf(10.f, 0.15f); // 3dB rise in square of magnitude
    static float zeroThresh = powf(10.f, -8);

    size_t count = 0;
    size_t nonZeroCount = 0;

    const int sz = m_windowSize / 2;

    for (int n = 1; n <= sz; ++n) {
        bool above = ((mag[n] / m_prevMag[n]) >= threshold);
        if (above) ++count;
        if (mag[n] > zeroThresh) ++nonZeroCount;
    }

    for (int n = 1; n <= sz; ++n) {
	m_prevMag[n] = mag[n];
    }

    if (nonZeroCount == 0) return 0;
    else return float(count) / float(nonZeroCount);
}

float
PercussiveAudioCurve::process(const double *R__ mag, size_t increment)
{
    Profiler profiler("PercussiveAudioCurve::process");

    static double threshold = pow(10.0, 0.15); // 3dB rise in square of magnitude
    static double zeroThresh = pow(10.0, -8);

    size_t count = 0;
    size_t nonZeroCount = 0;

    const int sz = m_windowSize / 2;

    for (int n = 1; n <= sz; ++n) {
        bool above = ((mag[n] / m_prevMag[n]) >= threshold);
        if (above) ++count;
        if (mag[n] > zeroThresh) ++nonZeroCount;
    }

    for (int n = 1; n <= sz; ++n) {
	m_prevMag[n] = mag[n];
    }

    if (nonZeroCount == 0) return 0;
    else return float(count) / float(nonZeroCount);
}

}

