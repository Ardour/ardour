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

#ifndef _HIGHFREQUENCY_AUDIO_CURVE_H_
#define _HIGHFREQUENCY_AUDIO_CURVE_H_

#include "AudioCurve.h"
#include "Window.h"

namespace RubberBand
{

class HighFrequencyAudioCurve : public AudioCurve
{
public:
    HighFrequencyAudioCurve(size_t sampleRate, size_t windowSize);

    virtual ~HighFrequencyAudioCurve();

    virtual void setWindowSize(size_t newSize);

    virtual float process(const float *R__ mag, size_t increment);
    virtual void reset();
};

}

#endif
