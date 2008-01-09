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

#include "Resampler.h"

#include <cstdlib>
#include <cmath>

#include <iostream>


#include <samplerate.h>

namespace RubberBand {

class Resampler::D
{
public:
    D(Quality quality, size_t channels, size_t maxBufferSize);
    ~D();

    size_t resample(float **in, float **out,
                    size_t incount, float ratio, bool final);

    void reset();

protected:
    SRC_STATE *m_src;
    float *m_iin;
    float *m_iout;
    size_t m_channels;
    size_t m_iinsize;
    size_t m_ioutsize;
};

Resampler::D::D(Quality quality, size_t channels, size_t maxBufferSize) :
    m_src(0),
    m_iin(0),
    m_iout(0),
    m_channels(channels),
    m_iinsize(0),
    m_ioutsize(0)
{
//    std::cerr << "Resampler::Resampler: using libsamplerate implementation"
//              << std::endl;

    int err = 0;
    m_src = src_new(quality == Best ? SRC_SINC_BEST_QUALITY :
                    quality == Fastest ? SRC_LINEAR :
                    SRC_SINC_FASTEST,
                    channels, &err);

    //!!! check err, throw

    if (maxBufferSize > 0 && m_channels > 1) {
        //!!! alignment?
        m_iinsize = maxBufferSize * m_channels;
        m_ioutsize = maxBufferSize * m_channels * 2;
        m_iin = (float *)malloc(m_iinsize * sizeof(float));
        m_iout = (float *)malloc(m_ioutsize * sizeof(float));
    }
}

Resampler::D::~D()
{
    src_delete(m_src);
    if (m_iinsize > 0) {
        free(m_iin);
    }
    if (m_ioutsize > 0) {
        free(m_iout);
    }
}

size_t
Resampler::D::resample(float **in, float **out, size_t incount, float ratio,
                       bool final)
{
    SRC_DATA data;

    size_t outcount = lrintf(ceilf(incount * ratio));

    if (m_channels == 1) {
        data.data_in = *in;
        data.data_out = *out;
    } else {
        if (incount * m_channels > m_iinsize) {
            m_iinsize = incount * m_channels;
            m_iin = (float *)realloc(m_iin, m_iinsize * sizeof(float));
        }
        if (outcount * m_channels > m_ioutsize) {
            m_ioutsize = outcount * m_channels;
            m_iout = (float *)realloc(m_iout, m_ioutsize * sizeof(float));
        }
        for (size_t i = 0; i < incount; ++i) {
            for (size_t c = 0; c < m_channels; ++c) {
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

    //!!! check err, respond appropriately

    if (m_channels > 1) {
        for (int i = 0; i < data.output_frames_gen; ++i) {
            for (size_t c = 0; c < m_channels; ++c) {
                out[c][i] = m_iout[i * m_channels + c];
            }
        }
    }

    return data.output_frames_gen;
}

void
Resampler::D::reset()
{
    src_reset(m_src);
}

} // end namespace


namespace RubberBand {

Resampler::Resampler(Quality quality, size_t channels, size_t maxBufferSize)
{
    m_d = new D(quality, channels, maxBufferSize);
}

Resampler::~Resampler()
{
    delete m_d;
}

size_t 
Resampler::resample(float **in, float **out,
                    size_t incount, float ratio, bool final)
{
    return m_d->resample(in, out, incount, ratio, final);
}

void
Resampler::reset()
{
    m_d->reset();
}

}
