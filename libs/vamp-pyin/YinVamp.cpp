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

#include "YinVamp.h"
#include "MonoNote.h"

#include "vamp-sdk/FFT.h"

#include <vector>
#include <algorithm>

#include <cstdio>
#include <cmath>
#include <complex>

using std::string;
using std::vector;
using Vamp::RealTime;


YinVamp::YinVamp(float inputSampleRate) :
    Plugin(inputSampleRate),
    m_channels(0),
    m_stepSize(256),
    m_blockSize(2048),
    m_fmin(40),
    m_fmax(1600),
    m_yin(2048, inputSampleRate, 0.0),
    m_outNoF0(0),
    m_outNoPeriodicity(0),
    m_outNoRms(0),
    m_outNoSalience(0),
    m_yinParameter(0.15f),
    m_outputUnvoiced(2.0f)
{
}

YinVamp::~YinVamp()
{
}

string
YinVamp::getIdentifier() const
{
    return "yin";
}

string
YinVamp::getName() const
{
    return "Yin";
}

string
YinVamp::getDescription() const
{
    return "A vamp implementation of the Yin algorithm for monophonic frequency estimation.";
}

string
YinVamp::getMaker() const
{
    return "Matthias Mauch";
}

int
YinVamp::getPluginVersion() const
{
    // Increment this each time you release a version that behaves
    // differently from the previous one
    return 2;
}

string
YinVamp::getCopyright() const
{
    return "GPL";
}

YinVamp::InputDomain
YinVamp::getInputDomain() const
{
    return TimeDomain;
}

size_t
YinVamp::getPreferredBlockSize() const
{
    return 2048;
}

size_t
YinVamp::getPreferredStepSize() const
{
    return 256;
}

size_t
YinVamp::getMinChannelCount() const
{
    return 1;
}

size_t
YinVamp::getMaxChannelCount() const
{
    return 1;
}

YinVamp::ParameterList
YinVamp::getParameterDescriptors() const
{
    ParameterList list;

    ParameterDescriptor d;
    d.identifier = "yinThreshold";
    d.name = "Yin threshold";
    d.description = "The greedy Yin search for a low value difference function is done once a dip lower than this threshold is reached.";
    d.unit = "";
    d.minValue = 0.025f;
    d.maxValue = 1.0f;
    d.defaultValue = 0.15f;
    d.isQuantized = true;
    d.quantizeStep = 0.025f;

    list.push_back(d);

    d.identifier = "outputunvoiced";
    d.valueNames.clear();
    d.name = "Output estimates classified as unvoiced?";
    d.description = ".";
    d.unit = "";
    d.minValue = 0.0f;
    d.maxValue = 2.0f;
    d.defaultValue = 2.0f;
    d.isQuantized = true;
    d.quantizeStep = 1.0f;
    d.valueNames.push_back("No");
    d.valueNames.push_back("Yes");
    d.valueNames.push_back("Yes, as negative frequencies");
    list.push_back(d);

    return list;
}

float
YinVamp::getParameter(string identifier) const
{
    if (identifier == "yinThreshold") {
        return m_yinParameter;
    }
    if (identifier == "outputunvoiced") {
        return m_outputUnvoiced;
    }
    return 0.f;
}

void
YinVamp::setParameter(string identifier, float value)
{
    if (identifier == "yinThreshold")
    {
        m_yinParameter = value;
    }
    if (identifier == "outputunvoiced")
    {
        m_outputUnvoiced = value;
    }
}

YinVamp::ProgramList
YinVamp::getPrograms() const
{
    ProgramList list;
    return list;
}

string
YinVamp::getCurrentProgram() const
{
    return ""; // no programs
}

void
YinVamp::selectProgram(string name)
{
}

YinVamp::OutputList
YinVamp::getOutputDescriptors() const
{
    OutputList outputs;

    OutputDescriptor d;

    int outputNumber = 0;

    d.identifier = "f0";
    d.name = "Estimated f0";
    d.description = "Estimated fundamental frequency";
    d.unit = "Hz";
    d.hasFixedBinCount = true;
    d.binCount = 1;
    d.hasKnownExtents = true;
    d.minValue = m_fmin;
    d.maxValue = 500;
    d.isQuantized = false;
    d.sampleType = OutputDescriptor::FixedSampleRate;
    d.sampleRate = (m_inputSampleRate / m_stepSize);
    d.hasDuration = false;
    outputs.push_back(d);
    m_outNoF0 = outputNumber++;

    d.identifier = "periodicity";
    d.name = "Periodicity";
    d.description = "by-product of Yin f0 estimation";
    d.unit = "";
    d.hasFixedBinCount = true;
    d.binCount = 1;
    d.hasKnownExtents = true;
    d.minValue = 0;
    d.maxValue = 1;
    d.isQuantized = false;
    d.sampleType = OutputDescriptor::FixedSampleRate;
    d.sampleRate = (m_inputSampleRate / m_stepSize);
    d.hasDuration = false;
    outputs.push_back(d);
    m_outNoPeriodicity = outputNumber++;

    d.identifier = "rms";
    d.name = "Root mean square";
    d.description = "Root mean square of the waveform.";
    d.unit = "";
    d.hasFixedBinCount = true;
    d.binCount = 1;
    d.hasKnownExtents = true;
    d.minValue = 0;
    d.maxValue = 1;
    d.isQuantized = false;
    d.sampleType = OutputDescriptor::FixedSampleRate;
    d.sampleRate = (m_inputSampleRate / m_stepSize);
    d.hasDuration = false;
    outputs.push_back(d);
    m_outNoRms = outputNumber++;

    d.identifier = "salience";
    d.name = "Salience";
    d.description = "Yin Salience";
    d.hasFixedBinCount = true;
    d.binCount = m_blockSize / 2;
    d.hasKnownExtents = true;
    d.minValue = 0;
    d.maxValue = 1;
    d.isQuantized = false;
    d.sampleType = OutputDescriptor::FixedSampleRate;
    d.sampleRate = (m_inputSampleRate / m_stepSize);
    d.hasDuration = false;
    outputs.push_back(d);
    m_outNoSalience = outputNumber++;

    return outputs;
}

bool
YinVamp::initialise(size_t channels, size_t stepSize, size_t blockSize)
{
    if (channels < getMinChannelCount() ||
	channels > getMaxChannelCount()) return false;

/*
    std::cerr << "YinVamp::initialise: channels = " << channels
          << ", stepSize = " << stepSize << ", blockSize = " << blockSize
          << std::endl;
*/
    m_channels = channels;
    m_stepSize = stepSize;
    m_blockSize = blockSize;

    reset();

    return true;
}

void
YinVamp::reset()
{
    m_yin.setThreshold(m_yinParameter);
    m_yin.setFrameSize(m_blockSize);
/*
    std::cerr << "YinVamp::reset: yin threshold set to " << (m_yinParameter)
          << ", blockSize = " << m_blockSize
          << std::endl;
*/
}

YinVamp::FeatureSet
YinVamp::process(const float *const *inputBuffers, RealTime timestamp)
{
    timestamp = timestamp + Vamp::RealTime::frame2RealTime(m_blockSize/2, lrintf(m_inputSampleRate));
    FeatureSet fs;

    double *dInputBuffers = new double[m_blockSize];
    for (size_t i = 0; i < m_blockSize; ++i) dInputBuffers[i] = inputBuffers[0][i];

    Yin::YinOutput yo = m_yin.process(dInputBuffers);
    // std::cerr << "f0 in YinVamp: " << yo.f0 << std::endl;
    Feature f;
    f.hasTimestamp = true;
    f.timestamp = timestamp;
    if (m_outputUnvoiced == 0.0f)
    {
        // std::cerr << "f0 in YinVamp: " << yo.f0 << std::endl;
        if (yo.f0 > 0 && yo.f0 < m_fmax && yo.f0 > m_fmin) {
            f.values.push_back(yo.f0);
            fs[m_outNoF0].push_back(f);
        }
    } else if (m_outputUnvoiced == 1.0f)
    {
        if (fabs(yo.f0) < m_fmax && fabs(yo.f0) > m_fmin) {
            f.values.push_back(fabs(yo.f0));
            fs[m_outNoF0].push_back(f);
        }
    } else
    {
        if (fabs(yo.f0) < m_fmax && fabs(yo.f0) > m_fmin) {
            f.values.push_back(yo.f0);
            fs[m_outNoF0].push_back(f);
        }
    }

    f.values.clear();
    f.values.push_back(yo.rms);
    fs[m_outNoRms].push_back(f);

    f.values.clear();
    for (size_t iBin = 0; iBin < yo.salience.size(); ++iBin)
    {
        f.values.push_back(yo.salience[iBin]);
    }
    fs[m_outNoSalience].push_back(f);

    f.values.clear();
    // f.values[0] = yo.periodicity;
    f.values.push_back(yo.periodicity);
    fs[m_outNoPeriodicity].push_back(f);

    delete [] dInputBuffers;

    return fs;
}

YinVamp::FeatureSet
YinVamp::getRemainingFeatures()
{
    FeatureSet fs;
    return fs;
}
