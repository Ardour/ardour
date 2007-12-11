/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*- vi:set ts=8 sts=4 sw=4: */

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

#ifndef _RUBBERBAND_RESAMPLER_H_
#define _RUBBERBAND_RESAMPLER_H_

#include <sys/types.h>

namespace RubberBand {

class Resampler
{
public:
    enum Quality { Best, FastestTolerable, Fastest };

    /**
     * Construct a resampler with the given quality level and channel
     * count.  maxBufferSize gives a bound on the maximum incount size
     * that may be passed to the resample function before the
     * resampler needs to reallocate its internal buffers.
     */
    Resampler(Quality quality, size_t channels, size_t maxBufferSize = 0);
    ~Resampler();

    size_t resample(float **in, float **out,
                    size_t incount, float ratio, bool final = false);

    void reset();

protected:
    class D;
    D *m_d;
};

}

#endif
