/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*- vi:set ts=8 sts=4 sw=4: */

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

#ifndef _RUBBERBAND_RESAMPLER_H_
#define _RUBBERBAND_RESAMPLER_H_

#include <sys/types.h>

#include "sysutils.h"

namespace RubberBand {

class ResamplerImpl;

class Resampler
{
public:
    enum Quality { Best, FastestTolerable, Fastest };
    enum Exception { ImplementationError };

    /**
     * Construct a resampler with the given quality level and channel
     * count.  maxBufferSize gives a bound on the maximum incount size
     * that may be passed to the resample function before the
     * resampler needs to reallocate its internal buffers.
     */
    Resampler(Quality quality, int channels, int maxBufferSize = 0,
              int debugLevel = 0);
    ~Resampler();

    int resample(const float *const R__ *const R__ in,
                 float *const R__ *const R__ out,
                 int incount,
                 float ratio,
                 bool final = false);

    void reset();

protected:
    ResamplerImpl *d;
    int m_method;
};

}

#endif
