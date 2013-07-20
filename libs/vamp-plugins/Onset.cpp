/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Vamp feature extraction plugins using Paul Brossier's Aubio library.

    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.

*/

#ifdef COMPILER_MSVC
#include <ardourext/float_cast.h>
#endif
#include <math.h>
#include "Onset.h"

using std::string;
using std::vector;
using std::cerr;
using std::endl;

Onset::Onset(float inputSampleRate) :
    Plugin(inputSampleRate),
    m_ibuf(0),
    m_fftgrain(0),
    m_onset(0),
    m_pv(0),
    m_peakpick(0),
    m_onsetdet(0),
    m_onsettype(aubio_onset_complex),
    m_threshold(0.3),
    m_silence(-90),
    m_channelCount(1)
{
}

Onset::~Onset()
{
    if (m_onsetdet) aubio_onsetdetection_free(m_onsetdet);
    if (m_ibuf) del_fvec(m_ibuf);
    if (m_onset) del_fvec(m_onset);
    if (m_fftgrain) del_cvec(m_fftgrain);
    if (m_pv) del_aubio_pvoc(m_pv);
    if (m_peakpick) del_aubio_peakpicker(m_peakpick);
}

string
Onset::getIdentifier() const
{
    return "aubioonset";
}

string
Onset::getName() const
{
    return "Aubio Onset Detector";
}

string
Onset::getDescription() const
{
    return "Estimate note onset times";
}

string
Onset::getMaker() const
{
    return "Paul Brossier (plugin by Chris Cannam)";
}

int
Onset::getPluginVersion() const
{
    return 1;
}

string
Onset::getCopyright() const
{
    return "GPL";
}

bool
Onset::initialise(size_t channels, size_t stepSize, size_t blockSize)
{
    m_channelCount = channels;
    m_stepSize = stepSize;
    m_blockSize = blockSize;

    m_ibuf = new_fvec(stepSize, channels);
    m_onset = new_fvec(1, channels);
    m_fftgrain = new_cvec(blockSize, channels);
    m_pv = new_aubio_pvoc(blockSize, stepSize, channels);
    m_peakpick = new_aubio_peakpicker(m_threshold);

    m_onsetdet = new_aubio_onsetdetection(m_onsettype, blockSize, channels);
    
    m_delay = Vamp::RealTime::frame2RealTime(4 * stepSize,
                                             lrintf(m_inputSampleRate));

    m_lastOnset = Vamp::RealTime::zeroTime - m_delay - m_delay;

    return true;
}

void
Onset::reset()
{
}

size_t
Onset::getPreferredStepSize() const
{
    return 512;
}

size_t
Onset::getPreferredBlockSize() const
{
    return 2 * getPreferredStepSize();
}

Onset::ParameterList
Onset::getParameterDescriptors() const
{
    ParameterList list;
    
    ParameterDescriptor desc;
    desc.identifier = "onsettype";
    desc.name = "Onset Detection Function Type";
    desc.minValue = 0;
    desc.maxValue = 6;
    desc.defaultValue = (int)aubio_onset_complex;
    desc.isQuantized = true;
    desc.quantizeStep = 1;
    desc.valueNames.push_back("Energy Based");
    desc.valueNames.push_back("Spectral Difference");
    desc.valueNames.push_back("High-Frequency Content");
    desc.valueNames.push_back("Complex Domain");
    desc.valueNames.push_back("Phase Deviation");
    desc.valueNames.push_back("Kullback-Liebler");
    desc.valueNames.push_back("Modified Kullback-Liebler");
    list.push_back(desc);

    desc = ParameterDescriptor();
    desc.identifier = "peakpickthreshold";
    desc.name = "Peak Picker Threshold";
    desc.minValue = 0;
    desc.maxValue = 1;
    desc.defaultValue = 0.3;
    desc.isQuantized = false;
    list.push_back(desc);

    desc = ParameterDescriptor();
    desc.identifier = "silencethreshold";
    desc.name = "Silence Threshold";
    desc.minValue = -120;
    desc.maxValue = 0;
    desc.defaultValue = -90;
    desc.unit = "dB";
    desc.isQuantized = false;
    list.push_back(desc);

    return list;
}

float
Onset::getParameter(std::string param) const
{
    if (param == "onsettype") {
        return m_onsettype;
    } else if (param == "peakpickthreshold") {
        return m_threshold;
    } else if (param == "silencethreshold") {
        return m_silence;
    } else {
        return 0.0;
    }
}

void
Onset::setParameter(std::string param, float value)
{
    if (param == "onsettype") {
        switch (lrintf(value)) {
        case 0: m_onsettype = aubio_onset_energy; break;
        case 1: m_onsettype = aubio_onset_specdiff; break;
        case 2: m_onsettype = aubio_onset_hfc; break;
        case 3: m_onsettype = aubio_onset_complex; break;
        case 4: m_onsettype = aubio_onset_phase; break;
        case 5: m_onsettype = aubio_onset_kl; break;
        case 6: m_onsettype = aubio_onset_mkl; break;
        }
    } else if (param == "peakpickthreshold") {
        m_threshold = value;
    } else if (param == "silencethreshold") {
        m_silence = value;
    }
}

Onset::OutputList
Onset::getOutputDescriptors() const
{
    OutputList list;

    OutputDescriptor d;
    d.identifier = "onsets";
    d.name = "Onsets";
    d.unit = "";
    d.hasFixedBinCount = true;
    d.binCount = 0;
    d.sampleType = OutputDescriptor::VariableSampleRate;
    d.sampleRate = 0;
    list.push_back(d);

    d = OutputDescriptor();
    d.identifier = "detectionfunction";
    d.name = "Onset Detection Function";
    d.unit = "";
    d.hasFixedBinCount = true;
    d.binCount = m_channelCount;
    d.hasKnownExtents = false;
    d.isQuantized = false;
    d.sampleType = OutputDescriptor::OneSamplePerStep;
    list.push_back(d);

    return list;
}

Onset::FeatureSet
Onset::process(const float *const *inputBuffers,
               Vamp::RealTime timestamp)
{
    for (size_t i = 0; i < m_stepSize; ++i) {
        for (size_t j = 0; j < m_channelCount; ++j) {
            fvec_write_sample(m_ibuf, inputBuffers[j][i], j, i);
        }
    }

    aubio_pvoc_do(m_pv, m_ibuf, m_fftgrain);
    aubio_onsetdetection(m_onsetdet, m_fftgrain, m_onset);

    bool isonset = aubio_peakpick_pimrt(m_onset, m_peakpick);

    if (isonset) {
        if (aubio_silence_detection(m_ibuf, m_silence)) {
            isonset = false;
        }
    }

    FeatureSet returnFeatures;

    if (isonset) {
        if (timestamp - m_lastOnset >= m_delay) {
            Feature onsettime;
            onsettime.hasTimestamp = true;
            if (timestamp < m_delay) timestamp = m_delay;
            onsettime.timestamp = timestamp - m_delay;
            returnFeatures[0].push_back(onsettime);
            m_lastOnset = timestamp;
        }
    }
    Feature feature;
    for (size_t j = 0; j < m_channelCount; ++j) {
        feature.values.push_back(m_onset->data[j][0]);
    }
    returnFeatures[1].push_back(feature);

    return returnFeatures;
}

Onset::FeatureSet
Onset::getRemainingFeatures()
{
    return FeatureSet();
}

