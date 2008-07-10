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

#include "RubberBandPitchShifter.h"

#include "RubberBandStretcher.h"

#include <iostream>
#include <cmath>

using namespace RubberBand;

using std::cout;
using std::cerr;
using std::endl;
using std::min;

const char *const
RubberBandPitchShifter::portNamesMono[PortCountMono] =
{
    "latency",
    "Cents",
    "Semitones",
    "Octaves",
    "Crispness",
    "Formant Preserving",
    "Faster",
    "Input",
    "Output"
};

const char *const
RubberBandPitchShifter::portNamesStereo[PortCountStereo] =
{
    "latency",
    "Cents",
    "Semitones",
    "Octaves",
    "Crispness",
    "Formant Preserving",
    "Faster",
    "Input L",
    "Output L",
    "Input R",
    "Output R"
};

const LADSPA_PortDescriptor 
RubberBandPitchShifter::portsMono[PortCountMono] =
{
    LADSPA_PORT_OUTPUT | LADSPA_PORT_CONTROL,
    LADSPA_PORT_INPUT  | LADSPA_PORT_CONTROL,
    LADSPA_PORT_INPUT  | LADSPA_PORT_CONTROL,
    LADSPA_PORT_INPUT  | LADSPA_PORT_CONTROL,
    LADSPA_PORT_INPUT  | LADSPA_PORT_CONTROL,
    LADSPA_PORT_INPUT  | LADSPA_PORT_CONTROL,
    LADSPA_PORT_INPUT  | LADSPA_PORT_CONTROL,
    LADSPA_PORT_INPUT  | LADSPA_PORT_AUDIO,
    LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO
};

const LADSPA_PortDescriptor 
RubberBandPitchShifter::portsStereo[PortCountStereo] =
{
    LADSPA_PORT_OUTPUT | LADSPA_PORT_CONTROL,
    LADSPA_PORT_INPUT  | LADSPA_PORT_CONTROL,
    LADSPA_PORT_INPUT  | LADSPA_PORT_CONTROL,
    LADSPA_PORT_INPUT  | LADSPA_PORT_CONTROL,
    LADSPA_PORT_INPUT  | LADSPA_PORT_CONTROL,
    LADSPA_PORT_INPUT  | LADSPA_PORT_CONTROL,
    LADSPA_PORT_INPUT  | LADSPA_PORT_CONTROL,
    LADSPA_PORT_INPUT  | LADSPA_PORT_AUDIO,
    LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO,
    LADSPA_PORT_INPUT  | LADSPA_PORT_AUDIO,
    LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO
};

const LADSPA_PortRangeHint 
RubberBandPitchShifter::hintsMono[PortCountMono] =
{
    { 0, 0, 0 },                        // latency
    { LADSPA_HINT_DEFAULT_0 |           // cents
      LADSPA_HINT_BOUNDED_BELOW |
      LADSPA_HINT_BOUNDED_ABOVE,
      -100.0, 100.0 },
    { LADSPA_HINT_DEFAULT_0 |           // semitones
      LADSPA_HINT_BOUNDED_BELOW |
      LADSPA_HINT_BOUNDED_ABOVE |
      LADSPA_HINT_INTEGER,
      -12.0, 12.0 },
    { LADSPA_HINT_DEFAULT_0 |           // octaves
      LADSPA_HINT_BOUNDED_BELOW |
      LADSPA_HINT_BOUNDED_ABOVE |
      LADSPA_HINT_INTEGER,
      -3.0, 3.0 },
    { LADSPA_HINT_DEFAULT_MAXIMUM |     // crispness
      LADSPA_HINT_BOUNDED_BELOW |
      LADSPA_HINT_BOUNDED_ABOVE |
      LADSPA_HINT_INTEGER,
       0.0, 3.0 },
    { LADSPA_HINT_DEFAULT_0 |           // formant preserving
      LADSPA_HINT_BOUNDED_BELOW |
      LADSPA_HINT_BOUNDED_ABOVE |
      LADSPA_HINT_TOGGLED,
       0.0, 1.0 },
    { LADSPA_HINT_DEFAULT_0 |           // fast
      LADSPA_HINT_BOUNDED_BELOW |
      LADSPA_HINT_BOUNDED_ABOVE |
      LADSPA_HINT_TOGGLED,
       0.0, 1.0 },
    { 0, 0, 0 },
    { 0, 0, 0 }
};

const LADSPA_PortRangeHint 
RubberBandPitchShifter::hintsStereo[PortCountStereo] =
{
    { 0, 0, 0 },                        // latency
    { LADSPA_HINT_DEFAULT_0 |           // cents
      LADSPA_HINT_BOUNDED_BELOW |
      LADSPA_HINT_BOUNDED_ABOVE,
      -100.0, 100.0 },
    { LADSPA_HINT_DEFAULT_0 |           // semitones
      LADSPA_HINT_BOUNDED_BELOW |
      LADSPA_HINT_BOUNDED_ABOVE |
      LADSPA_HINT_INTEGER,
      -12.0, 12.0 },
    { LADSPA_HINT_DEFAULT_0 |           // octaves
      LADSPA_HINT_BOUNDED_BELOW |
      LADSPA_HINT_BOUNDED_ABOVE |
      LADSPA_HINT_INTEGER,
      -3.0, 3.0 },
    { LADSPA_HINT_DEFAULT_MAXIMUM |     // crispness
      LADSPA_HINT_BOUNDED_BELOW |
      LADSPA_HINT_BOUNDED_ABOVE |
      LADSPA_HINT_INTEGER,
       0.0, 3.0 },
    { LADSPA_HINT_DEFAULT_0 |           // formant preserving
      LADSPA_HINT_BOUNDED_BELOW |
      LADSPA_HINT_BOUNDED_ABOVE |
      LADSPA_HINT_TOGGLED,
       0.0, 1.0 },
    { LADSPA_HINT_DEFAULT_0 |           // fast
      LADSPA_HINT_BOUNDED_BELOW |
      LADSPA_HINT_BOUNDED_ABOVE |
      LADSPA_HINT_TOGGLED,
       0.0, 1.0 },
    { 0, 0, 0 },
    { 0, 0, 0 },
    { 0, 0, 0 },
    { 0, 0, 0 }
};

const LADSPA_Properties
RubberBandPitchShifter::properties = LADSPA_PROPERTY_HARD_RT_CAPABLE;

const LADSPA_Descriptor 
RubberBandPitchShifter::ladspaDescriptorMono =
{
    2979, // "Unique" ID
    "rubberband-pitchshifter-mono", // Label
    properties,
    "Rubber Band Mono Pitch Shifter", // Name
    "Breakfast Quay",
    "GPL",
    PortCountMono,
    portsMono,
    portNamesMono,
    hintsMono,
    0, // Implementation data
    instantiate,
    connectPort,
    activate,
    run,
    0, // Run adding
    0, // Set run adding gain
    deactivate,
    cleanup
};

const LADSPA_Descriptor 
RubberBandPitchShifter::ladspaDescriptorStereo =
{
    9792, // "Unique" ID
    "rubberband-pitchshifter-stereo", // Label
    properties,
    "Rubber Band Stereo Pitch Shifter", // Name
    "Breakfast Quay",
    "GPL",
    PortCountStereo,
    portsStereo,
    portNamesStereo,
    hintsStereo,
    0, // Implementation data
    instantiate,
    connectPort,
    activate,
    run,
    0, // Run adding
    0, // Set run adding gain
    deactivate,
    cleanup
};

const LADSPA_Descriptor *
RubberBandPitchShifter::getDescriptor(unsigned long index)
{
    if (index == 0) return &ladspaDescriptorMono;
    if (index == 1) return &ladspaDescriptorStereo;
    else return 0;
}

RubberBandPitchShifter::RubberBandPitchShifter(int sampleRate, size_t channels) :
    m_latency(0),
    m_cents(0),
    m_semitones(0),
    m_octaves(0),
    m_crispness(0),
    m_formant(0),
    m_fast(0),
    m_ratio(1.0),
    m_prevRatio(1.0),
    m_currentCrispness(-1),
    m_currentFormant(false),
    m_currentFast(false),
    m_blockSize(1024),
    m_reserve(1024),
    m_minfill(0),
    m_stretcher(new RubberBandStretcher
                (sampleRate, channels,
                 RubberBandStretcher::OptionProcessRealTime |
                 RubberBandStretcher::OptionPitchHighConsistency)),
    m_sampleRate(sampleRate),
    m_channels(channels)
{
    for (size_t c = 0; c < m_channels; ++c) {

        m_input[c] = 0;
        m_output[c] = 0;

        int bufsize = m_blockSize + m_reserve + 8192;

        m_outputBuffer[c] = new RingBuffer<float>(bufsize);

        m_scratch[c] = new float[bufsize];
        for (int i = 0; i < bufsize; ++i) m_scratch[c][i] = 0.f;
    }

    activateImpl();
}

RubberBandPitchShifter::~RubberBandPitchShifter()
{
    delete m_stretcher;
    for (size_t c = 0; c < m_channels; ++c) {
        delete m_outputBuffer[c];
        delete[] m_scratch[c];
    }
}
    
LADSPA_Handle
RubberBandPitchShifter::instantiate(const LADSPA_Descriptor *desc, unsigned long rate)
{
    if (desc->PortCount == ladspaDescriptorMono.PortCount) {
        return new RubberBandPitchShifter(rate, 1);
    } else if (desc->PortCount == ladspaDescriptorStereo.PortCount) {
        return new RubberBandPitchShifter(rate, 2);
    }
    return 0;
}

void
RubberBandPitchShifter::connectPort(LADSPA_Handle handle,
				    unsigned long port, LADSPA_Data *location)
{
    RubberBandPitchShifter *shifter = (RubberBandPitchShifter *)handle;

    float **ports[PortCountStereo] = {
        &shifter->m_latency,
	&shifter->m_cents,
	&shifter->m_semitones,
	&shifter->m_octaves,
        &shifter->m_crispness,
	&shifter->m_formant,
	&shifter->m_fast,
    	&shifter->m_input[0],
	&shifter->m_output[0],
	&shifter->m_input[1],
	&shifter->m_output[1]
    };

    if (shifter->m_channels == 1) {
        if (port >= PortCountMono) return;
    } else {
        if (port >= PortCountStereo) return;
    }

    *ports[port] = (float *)location;

    if (shifter->m_latency) {
        *(shifter->m_latency) =
            float(shifter->m_stretcher->getLatency() + shifter->m_reserve);
    }
}

void
RubberBandPitchShifter::activate(LADSPA_Handle handle)
{
    RubberBandPitchShifter *shifter = (RubberBandPitchShifter *)handle;
    shifter->activateImpl();
}

void
RubberBandPitchShifter::activateImpl()
{
    updateRatio();
    m_prevRatio = m_ratio;
    m_stretcher->reset();
    m_stretcher->setPitchScale(m_ratio);

    for (size_t c = 0; c < m_channels; ++c) {
        m_outputBuffer[c]->reset();
        m_outputBuffer[c]->zero(m_reserve);
    }

    m_minfill = 0;

    // prime stretcher
//    for (int i = 0; i < 8; ++i) {
//        int reqd = m_stretcher->getSamplesRequired();
//        m_stretcher->process(m_scratch, reqd, false);
//        int avail = m_stretcher->available();
//        if (avail > 0) {
//            m_stretcher->retrieve(m_scratch, avail);
//        }
//    }
}

void
RubberBandPitchShifter::run(LADSPA_Handle handle, unsigned long samples)
{
    RubberBandPitchShifter *shifter = (RubberBandPitchShifter *)handle;
    shifter->runImpl(samples);
}

void
RubberBandPitchShifter::updateRatio()
{
    double oct = (m_octaves ? *m_octaves : 0.0);
    oct += (m_semitones ? *m_semitones : 0.0) / 12;
    oct += (m_cents ? *m_cents : 0.0) / 1200;
    m_ratio = pow(2.0, oct);
}

void
RubberBandPitchShifter::updateCrispness()
{
    if (!m_crispness) return;
    
    int c = lrintf(*m_crispness);
    if (c == m_currentCrispness) return;
    if (c < 0 || c > 3) return;
    RubberBandStretcher *s = m_stretcher;

    switch (c) {
    case 0:
        s->setPhaseOption(RubberBandStretcher::OptionPhaseIndependent);
        s->setTransientsOption(RubberBandStretcher::OptionTransientsSmooth);
        break;
    case 1:
        s->setPhaseOption(RubberBandStretcher::OptionPhaseLaminar);
        s->setTransientsOption(RubberBandStretcher::OptionTransientsSmooth);
        break;
    case 2:
        s->setPhaseOption(RubberBandStretcher::OptionPhaseLaminar);
        s->setTransientsOption(RubberBandStretcher::OptionTransientsMixed);
        break;
    case 3:
        s->setPhaseOption(RubberBandStretcher::OptionPhaseLaminar);
        s->setTransientsOption(RubberBandStretcher::OptionTransientsCrisp);
        break;
    }

    m_currentCrispness = c;
}

void
RubberBandPitchShifter::updateFormant()
{
    if (!m_formant) return;

    bool f = (*m_formant > 0.5f);
    if (f == m_currentFormant) return;
    
    RubberBandStretcher *s = m_stretcher;
    
    s->setFormantOption(f ?
                        RubberBandStretcher::OptionFormantPreserved :
                        RubberBandStretcher::OptionFormantShifted);

    m_currentFormant = f;
}

void
RubberBandPitchShifter::updateFast()
{
    if (!m_fast) return;

    bool f = (*m_fast > 0.5f);
    if (f == m_currentFast) return;
    
    RubberBandStretcher *s = m_stretcher;
    
    s->setPitchOption(f ?
                      RubberBandStretcher::OptionPitchHighSpeed :
                      RubberBandStretcher::OptionPitchHighConsistency);

    m_currentFast = f;
}

void
RubberBandPitchShifter::runImpl(unsigned long insamples)
{
    unsigned long offset = 0;

    // We have to break up the input into chunks like this because
    // insamples could be arbitrarily large and our output buffer is
    // of limited size

    while (offset < insamples) {

        unsigned long block = (unsigned long)m_blockSize;
        if (block + offset > insamples) block = insamples - offset;

        runImpl(block, offset);

        offset += block;
    }
}

void
RubberBandPitchShifter::runImpl(unsigned long insamples, unsigned long offset)
{
//    cerr << "RubberBandPitchShifter::runImpl(" << insamples << ")" << endl;

//    static int incount = 0, outcount = 0;

    updateRatio();
    if (m_ratio != m_prevRatio) {
        m_stretcher->setPitchScale(m_ratio);
        m_prevRatio = m_ratio;
    }

    if (m_latency) {
        *m_latency = float(m_stretcher->getLatency() + m_reserve);
//        cerr << "latency = " << *m_latency << endl;
    }

    updateCrispness();
    updateFormant();
    updateFast();

    const int samples = insamples;
    int processed = 0;
    size_t outTotal = 0;

    float *ptrs[2];

    int rs = m_outputBuffer[0]->getReadSpace();
    if (rs < int(m_minfill)) {
//        cerr << "temporary expansion (have " << rs << ", want " << m_reserve << ")" << endl;
        m_stretcher->setTimeRatio(1.1); // fill up temporarily
    } else if (rs > 8192) {
//        cerr << "temporary reduction (have " << rs << ", want " << m_reserve << ")" << endl;
        m_stretcher->setTimeRatio(0.9); // reduce temporarily
    } else {
        m_stretcher->setTimeRatio(1.0);
    }

    while (processed < samples) {

        // never feed more than the minimum necessary number of
        // samples at a time; ensures nothing will overflow internally
        // and we don't need to call setMaxProcessSize

        int toCauseProcessing = m_stretcher->getSamplesRequired();
        int inchunk = min(samples - processed, toCauseProcessing);
        for (size_t c = 0; c < m_channels; ++c) {
            ptrs[c] = &(m_input[c][offset + processed]);
        }
        m_stretcher->process(ptrs, inchunk, false);
        processed += inchunk;

        int avail = m_stretcher->available();
        int writable = m_outputBuffer[0]->getWriteSpace();
        int outchunk = min(avail, writable);
        size_t actual = m_stretcher->retrieve(m_scratch, outchunk);
        outTotal += actual;

//        incount += inchunk;
//        outcount += actual;

//        cout << "avail: " << avail << ", outchunk = " << outchunk;
//        if (actual != outchunk) cout << " (" << actual << ")";
//        cout << endl;

        outchunk = actual;

        for (size_t c = 0; c < m_channels; ++c) {
            if (int(m_outputBuffer[c]->getWriteSpace()) < outchunk) {
                cerr << "RubberBandPitchShifter::runImpl: buffer overrun: chunk = " << outchunk << ", space = " << m_outputBuffer[c]->getWriteSpace() << endl;
            }                
            m_outputBuffer[c]->write(m_scratch[c], outchunk);
        }
    }
    
    for (size_t c = 0; c < m_channels; ++c) {
        int toRead = m_outputBuffer[c]->getReadSpace();
        if (toRead < samples && c == 0) {
            cerr << "RubberBandPitchShifter::runImpl: buffer underrun: required = " << samples << ", available = " << toRead << endl;
        }
        int chunk = min(toRead, samples);
        m_outputBuffer[c]->read(&(m_output[c][offset]), chunk);
    }

    if (m_minfill == 0) {
        m_minfill = m_outputBuffer[0]->getReadSpace();
//        cerr << "minfill = " << m_minfill << endl;
    }
}

void
RubberBandPitchShifter::deactivate(LADSPA_Handle handle)
{
    activate(handle); // both functions just reset the plugin
}

void
RubberBandPitchShifter::cleanup(LADSPA_Handle handle)
{
    delete (RubberBandPitchShifter *)handle;
}

