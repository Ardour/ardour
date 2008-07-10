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

#ifndef _RUBBERBAND_FFT_H_
#define _RUBBERBAND_FFT_H_

#include "sysutils.h"

namespace RubberBand {

class FFTImpl;

/**
 * Provide the basic FFT computations we need, using one of a set of
 * candidate FFT implementations (depending on compile flags).
 *
 * Implements real->complex FFTs of power-of-two sizes only.  Note
 * that only the first half of the output signal is returned (the
 * complex conjugates half is omitted), so the "complex" arrays need
 * room for size/2+1 elements.
 *
 * Not thread safe: use a separate instance per thread.
 */

class FFT
{
public:
    enum Exception { InvalidSize };

    FFT(int size, int debugLevel = 0); // may throw InvalidSize
    ~FFT();

    void forward(const double *R__ realIn, double *R__ realOut, double *R__ imagOut);
    void forwardPolar(const double *R__ realIn, double *R__ magOut, double *R__ phaseOut);
    void forwardMagnitude(const double *R__ realIn, double *R__ magOut);

    void forward(const float *R__ realIn, float *R__ realOut, float *R__ imagOut);
    void forwardPolar(const float *R__ realIn, float *R__ magOut, float *R__ phaseOut);
    void forwardMagnitude(const float *R__ realIn, float *R__ magOut);

    void inverse(const double *R__ realIn, const double *R__ imagIn, double *R__ realOut);
    void inversePolar(const double *R__ magIn, const double *R__ phaseIn, double *R__ realOut);
    void inverseCepstral(const double *R__ magIn, double *R__ cepOut);

    void inverse(const float *R__ realIn, const float *R__ imagIn, float *R__ realOut);
    void inversePolar(const float *R__ magIn, const float *R__ phaseIn, float *R__ realOut);
    void inverseCepstral(const float *R__ magIn, float *R__ cepOut);

    // Calling one or both of these is optional -- if neither is
    // called, the first call to a forward or inverse method will call
    // init().  You only need call these if you don't want to risk
    // expensive allocations etc happening in forward or inverse.
    void initFloat();
    void initDouble();

    float *getFloatTimeBuffer();
    double *getDoubleTimeBuffer();

    static void tune();

protected:
    FFTImpl *d;
    static int m_method;
};

}

#endif

