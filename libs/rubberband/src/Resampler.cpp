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

#include "Resampler.h"

#include "Profiler.h"

#include <cstdlib>
#include <cmath>

#include <iostream>


#include <samplerate.h>



namespace RubberBand {

class ResamplerImpl
{
public:
    virtual ~ResamplerImpl() { }
    
    virtual int resample(const float *const R__ *const R__ in, 
                         float *const R__ *const R__ out,
                         int incount,
                         float ratio,
                         bool final) = 0;

    virtual void reset() = 0;
};

namespace Resamplers {



class D_SRC : public ResamplerImpl
{
public:
    D_SRC(Resampler::Quality quality, int channels, int maxBufferSize,
          int m_debugLevel);
    ~D_SRC();

    int resample(const float *const R__ *const R__ in,
                 float *const R__ *const R__ out,
                 int incount,
                 float ratio,
                 bool final);

    void reset();

protected:
    SRC_STATE *m_src;
    float *m_iin;
    float *m_iout;
    float m_lastRatio;
    int m_channels;
    int m_iinsize;
    int m_ioutsize;
    int m_debugLevel;
};

D_SRC::D_SRC(Resampler::Quality quality, int channels, int maxBufferSize,
             int debugLevel) :
    m_src(0),
    m_iin(0),
    m_iout(0),
    m_lastRatio(1.f),
    m_channels(channels),
    m_iinsize(0),
    m_ioutsize(0),
    m_debugLevel(debugLevel)
{
    if (m_debugLevel > 0) {
        std::cerr << "Resampler::Resampler: using libsamplerate implementation"
                  << std::endl;
    }

    int err = 0;
    m_src = src_new(quality == Resampler::Best ? SRC_SINC_BEST_QUALITY :
                    quality == Resampler::Fastest ? SRC_LINEAR :
                    SRC_SINC_FASTEST,
                    channels, &err);

    if (err) {
        std::cerr << "Resampler::Resampler: failed to create libsamplerate resampler: " 
                  << src_strerror(err) << std::endl;
        throw Resampler::ImplementationError; //!!! of course, need to catch this!
    }

    if (maxBufferSize > 0 && m_channels > 1) {
        m_iinsize = maxBufferSize * m_channels;
        m_ioutsize = maxBufferSize * m_channels * 2;
        m_iin = allocFloat(m_iinsize);
        m_iout = allocFloat(m_ioutsize);
    }

    reset();
}

D_SRC::~D_SRC()
{
    src_delete(m_src);
    if (m_iinsize > 0) {
        free(m_iin);
    }
    if (m_ioutsize > 0) {
        free(m_iout);
    }
}

int
D_SRC::resample(const float *const R__ *const R__ in,
                float *const R__ *const R__ out,
                int incount,
                float ratio,
                bool final)
{
    SRC_DATA data;

    int outcount = lrintf(ceilf(incount * ratio));

    if (m_channels == 1) {
        data.data_in = const_cast<float *>(*in); //!!!???
        data.data_out = *out;
    } else {
        if (incount * m_channels > m_iinsize) {
            m_iin = allocFloat(m_iin, m_iinsize);
        }
        if (outcount * m_channels > m_ioutsize) {
            m_iout = allocFloat(m_iout, m_ioutsize);
        }
        for (int i = 0; i < incount; ++i) {
            for (int c = 0; c < m_channels; ++c) {
                m_iin[i * m_channels + c] = in[c][i];
            }
        }
        data.data_in = m_iin;
        data.data_out = m_iout;
    }

    data.input_frames = incount;
    data.output_frames = outcount;
    data.src_ratio = ratio;
    data.end_of_input = (final ? 1 : 0);

    int err = 0;
    err = src_process(m_src, &data);

    if (err) {
        std::cerr << "Resampler::process: libsamplerate error: "
                  << src_strerror(err) << std::endl;
        throw Resampler::ImplementationError; //!!! of course, need to catch this!
    }

    if (m_channels > 1) {
        for (int i = 0; i < data.output_frames_gen; ++i) {
            for (int c = 0; c < m_channels; ++c) {
                out[c][i] = m_iout[i * m_channels + c];
            }
        }
    }

    m_lastRatio = ratio;

    return data.output_frames_gen;
}

void
D_SRC::reset()
{
    src_reset(m_src);
}



} /* end namespace Resamplers */

Resampler::Resampler(Resampler::Quality quality, int channels,
                     int maxBufferSize, int debugLevel)
{
    m_method = -1;
    
    switch (quality) {

    case Resampler::Best:
        m_method = 1;
        break;

    case Resampler::FastestTolerable:
        m_method = 1;
        break;

    case Resampler::Fastest:
        m_method = 1;
        break;
    }

    if (m_method == -1) {
        std::cerr << "Resampler::Resampler(" << quality << ", " << channels
                  << ", " << maxBufferSize << "): No implementation available!"
                  << std::endl;
        abort();
    }

    switch (m_method) {
    case 0:
        std::cerr << "Resampler::Resampler(" << quality << ", " << channels
                  << ", " << maxBufferSize << "): No implementation available!"
                  << std::endl;
        abort();
        break;

    case 1:
        d = new Resamplers::D_SRC(quality, channels, maxBufferSize, debugLevel);
        break;

    case 2:
        std::cerr << "Resampler::Resampler(" << quality << ", " << channels
                  << ", " << maxBufferSize << "): No implementation available!"
                  << std::endl;
        abort();
        break;
    }
}

Resampler::~Resampler()
{
    delete d;
}

int 
Resampler::resample(const float *const R__ *const R__ in,
                    float *const R__ *const R__ out,
                    int incount, float ratio, bool final)
{
    Profiler profiler("Resampler::resample");
    return d->resample(in, out, incount, ratio, final);
}

void
Resampler::reset()
{
    d->reset();
}

}
