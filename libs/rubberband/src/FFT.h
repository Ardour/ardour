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

#ifndef _RUBBERBAND_FFT_H_
#define _RUBBERBAND_FFT_H_

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

    FFT(unsigned int size); // may throw InvalidSize
    ~FFT();

    void forward(double *realIn, double *realOut, double *imagOut);
    void forwardPolar(double *realIn, double *magOut, double *phaseOut);
    void forwardMagnitude(double *realIn, double *magOut);

    void forward(float *realIn, float *realOut, float *imagOut);
    void forwardPolar(float *realIn, float *magOut, float *phaseOut);
    void forwardMagnitude(float *realIn, float *magOut);

    void inverse(double *realIn, double *imagIn, double *realOut);
    void inversePolar(double *magIn, double *phaseIn, double *realOut);

    void inverse(float *realIn, float *imagIn, float *realOut);
    void inversePolar(float *magIn, float *phaseIn, float *realOut);

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

