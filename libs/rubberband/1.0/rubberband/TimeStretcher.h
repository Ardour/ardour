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

#ifndef _RUBBERBAND_TIMESTRETCHER_H_
#define _RUBBERBAND_TIMESTRETCHER_H_

#include <sys/types.h>

namespace RubberBand
{

/**
 * Base class for time stretchers.  RubberBand currently provides only
 * a single subclass implementation.
 *
 * @see RubberBandStretcher
 */
class TimeStretcher
{
public:
    TimeStretcher(size_t sampleRate, size_t channels) :
        m_sampleRate(sampleRate),
        m_channels(channels)
    { }
    virtual ~TimeStretcher()
    { }

    virtual void reset() = 0;
    virtual void setTimeRatio(double ratio) = 0;
    virtual void setPitchScale(double scale) = 0;
    virtual size_t getLatency() const = 0;

    virtual void study(const float *const *input, size_t samples, bool final) = 0;
    virtual size_t getSamplesRequired() const = 0;
    virtual void process(const float *const *input, size_t samples, bool final) = 0;
    virtual int available() const = 0;
    virtual size_t retrieve(float *const *output, size_t samples) const = 0;

protected:
    size_t m_sampleRate;
    size_t m_channels;
};

}

#endif
    
