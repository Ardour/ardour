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

#include "ConstantAudioCurve.h"

namespace RubberBand
{

ConstantAudioCurve::ConstantAudioCurve(size_t sampleRate, size_t windowSize) :
    AudioCurve(sampleRate, windowSize)
{
}

ConstantAudioCurve::~ConstantAudioCurve()
{
}

void
ConstantAudioCurve::reset()
{
}

void
ConstantAudioCurve::setWindowSize(size_t newSize)
{
    m_windowSize = newSize;
}

float
ConstantAudioCurve::process(const float *R__, size_t)
{
    return 1.f;
}

}

