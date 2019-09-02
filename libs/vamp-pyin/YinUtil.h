/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    pYIN - A fundamental frequency estimator for monophonic audio
    Centre for Digital Music, Queen Mary, University of London.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef _YINUTIL_H_
#define _YINUTIL_H_

#include "vamp-sdk/FFT.h"
#include "MeanFilter.h"

#include <cmath>

#include <iostream>
#include <vector>
#include <exception>

using std::vector;

class YinUtil
{
public:
    static double sumSquare(const double *in, const size_t startInd, const size_t endInd);
    static void difference(const double *in, double *yinBuffer, const size_t yinBufferSize);
    static void fastDifference(const double *in, double *yinBuffer, const size_t yinBufferSize);
    static void slowDifference(const double *in, double *yinBuffer, const size_t yinBufferSize);
    static void cumulativeDifference(double *yinBuffer, const size_t yinBufferSize);
    static int absoluteThreshold(const double *yinBuffer, const size_t yinBufferSize, const double thresh);
    static vector<double> yinProb(const double *yinBuffer, const size_t prior, const size_t yinBufferSize, size_t minTau = 0, size_t maxTau = 0);
    static double parabolicInterpolation(const double *yinBuffer, const size_t tau,
                                         const size_t yinBufferSize);
};

#endif
