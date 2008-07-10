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

#include "StretcherChannelData.h"

#include "Resampler.h"


namespace RubberBand 
{
      
RubberBandStretcher::Impl::ChannelData::ChannelData(size_t windowSize,
                                                    int overSample,
                                                    size_t outbufSize) :
    oversample(overSample)
{
    std::set<size_t> s;
    construct(s, windowSize, outbufSize);
}

RubberBandStretcher::Impl::ChannelData::ChannelData(const std::set<size_t> &windowSizes,
                                                    int overSample,
                                                    size_t initialWindowSize,
                                                    size_t outbufSize) :
    oversample(overSample)
{
    construct(windowSizes, initialWindowSize, outbufSize);
}

void
RubberBandStretcher::Impl::ChannelData::construct(const std::set<size_t> &windowSizes,
                                                  size_t initialWindowSize,
                                                  size_t outbufSize)
{
    size_t maxSize = initialWindowSize;

    if (!windowSizes.empty()) {
        // std::set is ordered by value
        std::set<size_t>::const_iterator i = windowSizes.end();
        maxSize = *--i;
    }
    if (windowSizes.find(initialWindowSize) == windowSizes.end()) {
        if (initialWindowSize > maxSize) maxSize = initialWindowSize;
    }

    // max size of the real "half" of freq data
    size_t realSize = (maxSize * oversample)/2 + 1;

//    std::cerr << "ChannelData::construct([" << windowSizes.size() << "], " << maxSize << ", " << outbufSize << ")" << std::endl;
    
    if (outbufSize < maxSize) outbufSize = maxSize;

    inbuf = new RingBuffer<float>(maxSize);
    outbuf = new RingBuffer<float>(outbufSize);

    mag = allocDouble(realSize);
    phase = allocDouble(realSize);
    prevPhase = allocDouble(realSize);
    prevError = allocDouble(realSize);
    unwrappedPhase = allocDouble(realSize);
    envelope = allocDouble(realSize);

    freqPeak = new size_t[realSize];

    fltbuf = allocFloat(maxSize);

    accumulator = allocFloat(maxSize);
    windowAccumulator = allocFloat(maxSize);

    for (std::set<size_t>::const_iterator i = windowSizes.begin();
         i != windowSizes.end(); ++i) {
        ffts[*i] = new FFT(*i * oversample);
        ffts[*i]->initDouble();
    }
    if (windowSizes.find(initialWindowSize) == windowSizes.end()) {
        ffts[initialWindowSize] = new FFT(initialWindowSize * oversample);
        ffts[initialWindowSize]->initDouble();
    }
    fft = ffts[initialWindowSize];

    dblbuf = fft->getDoubleTimeBuffer();

    resampler = 0;
    resamplebuf = 0;
    resamplebufSize = 0;

    reset();

    for (size_t i = 0; i < realSize; ++i) {
        freqPeak[i] = 0;
    }

    for (size_t i = 0; i < initialWindowSize * oversample; ++i) {
        dblbuf[i] = 0.0;
    }
}

void
RubberBandStretcher::Impl::ChannelData::setWindowSize(size_t windowSize)
{
    size_t oldSize = inbuf->getSize();
    size_t realSize = (windowSize * oversample) / 2 + 1;

//    std::cerr << "ChannelData::setWindowSize(" << windowSize << ") [from " << oldSize << "]" << std::endl;

    if (oldSize >= windowSize) {

        // no need to reallocate buffers, just reselect fft

        //!!! we can't actually do this without locking against the
        //process thread, can we?  we need to zero the mag/phase
        //buffers without interference

        if (ffts.find(windowSize) == ffts.end()) {
            //!!! this also requires a lock, but it shouldn't occur in
            //RT mode with proper initialisation
            ffts[windowSize] = new FFT(windowSize * oversample);
            ffts[windowSize]->initDouble();
        }
        
        fft = ffts[windowSize];

        dblbuf = fft->getDoubleTimeBuffer();

        for (size_t i = 0; i < windowSize * oversample; ++i) {
            dblbuf[i] = 0.0;
        }

        for (size_t i = 0; i < realSize; ++i) {
            mag[i] = 0.0;
            phase[i] = 0.0;
            prevPhase[i] = 0.0;
            prevError[i] = 0.0;
            unwrappedPhase[i] = 0.0;
            freqPeak[i] = 0;
        }

        return;
    }

    //!!! at this point we need a lock in case a different client
    //thread is calling process() -- we need this lock even if we
    //aren't running in threaded mode ourselves -- if we're in RT
    //mode, then the process call should trylock and fail if the lock
    //is unavailable (since this should never normally be the case in
    //general use in RT mode)

    RingBuffer<float> *newbuf = inbuf->resized(windowSize);
    delete inbuf;
    inbuf = newbuf;

    // We don't want to preserve data in these arrays

    mag = allocDouble(mag, realSize);
    phase = allocDouble(phase, realSize);
    prevPhase = allocDouble(prevPhase, realSize);
    prevError = allocDouble(prevError, realSize);
    unwrappedPhase = allocDouble(unwrappedPhase, realSize);
    envelope = allocDouble(envelope, realSize);

    delete[] freqPeak;
    freqPeak = new size_t[realSize];

    fltbuf = allocFloat(fltbuf, windowSize);

    // But we do want to preserve data in these

    float *newAcc = allocFloat(windowSize);

    for (size_t i = 0; i < oldSize; ++i) newAcc[i] = accumulator[i];

    freeFloat(accumulator);
    accumulator = newAcc;

    newAcc = allocFloat(windowSize);

    for (size_t i = 0; i < oldSize; ++i) newAcc[i] = windowAccumulator[i];

    freeFloat(windowAccumulator);
    windowAccumulator = newAcc;
    
    //!!! and resampler?

    for (size_t i = 0; i < realSize; ++i) {
        freqPeak[i] = 0;
    }

    for (size_t i = 0; i < windowSize; ++i) {
        fltbuf[i] = 0.f;
    }

    if (ffts.find(windowSize) == ffts.end()) {
        ffts[windowSize] = new FFT(windowSize * oversample);
        ffts[windowSize]->initDouble();
    }
    
    fft = ffts[windowSize];

    dblbuf = fft->getDoubleTimeBuffer();

    for (size_t i = 0; i < windowSize * oversample; ++i) {
        dblbuf[i] = 0.0;
    }
}

void
RubberBandStretcher::Impl::ChannelData::setOutbufSize(size_t outbufSize)
{
    size_t oldSize = outbuf->getSize();

//    std::cerr << "ChannelData::setOutbufSize(" << outbufSize << ") [from " << oldSize << "]" << std::endl;

    if (oldSize < outbufSize) {

        //!!! at this point we need a lock in case a different client
        //thread is calling process()

        RingBuffer<float> *newbuf = outbuf->resized(outbufSize);
        delete outbuf;
        outbuf = newbuf;
    }
}

void
RubberBandStretcher::Impl::ChannelData::setResampleBufSize(size_t sz)
{
    resamplebuf = allocFloat(resamplebuf, sz);
    resamplebufSize = sz;
}

RubberBandStretcher::Impl::ChannelData::~ChannelData()
{
    delete resampler;

    freeFloat(resamplebuf);

    delete inbuf;
    delete outbuf;

    freeDouble(mag);
    freeDouble(phase);
    freeDouble(prevPhase);
    freeDouble(prevError);
    freeDouble(unwrappedPhase);
    freeDouble(envelope);
    delete[] freqPeak;
    freeFloat(accumulator);
    freeFloat(windowAccumulator);
    freeFloat(fltbuf);

    for (std::map<size_t, FFT *>::iterator i = ffts.begin();
         i != ffts.end(); ++i) {
        delete i->second;
    }
}

void
RubberBandStretcher::Impl::ChannelData::reset()
{
    inbuf->reset();
    outbuf->reset();

    if (resampler) resampler->reset();

    accumulatorFill = 0;
    prevIncrement = 0;
    chunkCount = 0;
    inCount = 0;
    inputSize = -1;
    outCount = 0;
    unchanged = true;
    draining = false;
    outputComplete = false;
}

}
