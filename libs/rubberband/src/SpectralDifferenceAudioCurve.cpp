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

#include "SpectralDifferenceAudioCurve.h"

namespace RubberBand
{

SpectralDifferenceAudioCurve::SpectralDifferenceAudioCurve(size_t sampleRate, size_t windowSize) :
    AudioCurve(sampleRate, windowSize)
{
    m_prevMag = new float[m_windowSize/2 + 1];

    for (size_t i = 0; i <= m_windowSize/2; ++i) {
        m_prevMag[i] = 0.f;
    }
}

SpectralDifferenceAudioCurve::~SpectralDifferenceAudioCurve()
{
    delete[] m_prevMag;
}

void
SpectralDifferenceAudioCurve::reset()
{
    for (size_t i = 0; i <= m_windowSize/2; ++i) {
        m_prevMag[i] = 0;
    }
}

void
SpectralDifferenceAudioCurve::setWindowSize(size_t newSize)
{
    delete[] m_prevMag;
    m_windowSize = newSize;
    
    m_prevMag = new float[m_windowSize/2 + 1];

    reset();
}

float
SpectralDifferenceAudioCurve::process(const float *R__ mag, size_t increment)
{
    float result = 0.0;

    for (size_t n = 0; n <= m_windowSize / 2; ++n) {
        result += sqrtf(fabsf((mag[n] * mag[n]) -
                              (m_prevMag[n] * m_prevMag[n])));
        m_prevMag[n] = mag[n];
    }

    return result;
}

}

