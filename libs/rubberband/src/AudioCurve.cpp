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

#include "AudioCurve.h"

#include <iostream>
using namespace std;

namespace RubberBand
{

AudioCurve::AudioCurve(size_t sampleRate, size_t windowSize) :
    m_sampleRate(sampleRate),
    m_windowSize(windowSize)
{
}

AudioCurve::~AudioCurve()
{
}

float
AudioCurve::process(const double *R__ mag, size_t increment)
{
    cerr << "WARNING: Using inefficient AudioCurve::process(double)" << endl;
    float *tmp = new float[m_windowSize];
    for (int i = 0; i < int(m_windowSize); ++i) tmp[i] = float(mag[i]);
    float df = process(tmp, increment);
    delete[] tmp;
    return df;
}

}
