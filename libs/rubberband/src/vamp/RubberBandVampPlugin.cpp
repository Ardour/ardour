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

#include "RubberBandVampPlugin.h"

#include "StretchCalculator.h"
#include "sysutils.h"

#include <cmath>

using std::string;
using std::vector;
using std::cerr;
using std::endl;

class RubberBandVampPlugin::Impl
{
public:
    size_t m_stepSize;
    size_t m_blockSize;
    size_t m_sampleRate;

    float m_timeRatio;
    float m_pitchRatio;

    bool m_realtime;
    bool m_elasticTiming;
    int m_transientMode;
    bool m_phaseIndependent;
    int m_windowLength;

    RubberBand::RubberBandStretcher *m_stretcher;

    int m_incrementsOutput;
    int m_aggregateIncrementsOutput;
    int m_divergenceOutput;
    int m_phaseResetDfOutput;
    int m_smoothedPhaseResetDfOutput;
    int m_phaseResetPointsOutput;
    int m_timeSyncPointsOutput;

    size_t m_counter;
    size_t m_accumulatedIncrement;

    float **m_outputDump;

    FeatureSet processOffline(const float *const *inputBuffers,
                              Vamp::RealTime timestamp);

    FeatureSet getRemainingFeaturesOffline();

    FeatureSet processRealTime(const float *const *inputBuffers,
                               Vamp::RealTime timestamp);

    FeatureSet getRemainingFeaturesRealTime();

    FeatureSet createFeatures(size_t inputIncrement,
                              std::vector<int> &outputIncrements,
                              std::vector<float> &phaseResetDf,
                              std::vector<int> &exactPoints,
                              std::vector<float> &smoothedDf,
                              size_t baseCount,
                              bool includeFinal);
};


RubberBandVampPlugin::RubberBandVampPlugin(float inputSampleRate) :
    Plugin(inputSampleRate)
{
    m_d = new Impl();
    m_d->m_stepSize = 0;
    m_d->m_timeRatio = 1.f;
    m_d->m_pitchRatio = 1.f;
    m_d->m_realtime = false;
    m_d->m_elasticTiming = true;
    m_d->m_transientMode = 0;
    m_d->m_phaseIndependent = false;
    m_d->m_windowLength = 0;
    m_d->m_stretcher = 0;
    m_d->m_sampleRate = lrintf(m_inputSampleRate);
}

RubberBandVampPlugin::~RubberBandVampPlugin()
{
    if (m_d->m_outputDump) {
        for (size_t i = 0; i < m_d->m_stretcher->getChannelCount(); ++i) {
            delete[] m_d->m_outputDump[i];
        }
        delete[] m_d->m_outputDump;
    }
    delete m_d->m_stretcher;
    delete m_d;
}

string
RubberBandVampPlugin::getIdentifier() const
{
    return "rubberband";
}

string
RubberBandVampPlugin::getName() const
{
    return "Rubber Band Timestretch Analysis";
}

string
RubberBandVampPlugin::getDescription() const
{
    return "Carry out analysis phases of time stretcher process";
}

string
RubberBandVampPlugin::getMaker() const
{
    return "Breakfast Quay";
}

int
RubberBandVampPlugin::getPluginVersion() const
{
    return 1;
}

string
RubberBandVampPlugin::getCopyright() const
{
    return "";//!!!
}

RubberBandVampPlugin::OutputList
RubberBandVampPlugin::getOutputDescriptors() const
{
    OutputList list;

    size_t rate = 0;
    if (m_d->m_stretcher) {
        rate = lrintf(m_inputSampleRate / m_d->m_stretcher->getInputIncrement());
    }

    OutputDescriptor d;
    d.identifier = "increments";
    d.name = "Output Increments";
    d.description = "Output time increment for each input step";
    d.unit = "samples";
    d.hasFixedBinCount = true;
    d.binCount = 1;
    d.hasKnownExtents = false;
    d.isQuantized = true;
    d.quantizeStep = 1.0;
    d.sampleType = OutputDescriptor::VariableSampleRate;
    d.sampleRate = float(rate);
    m_d->m_incrementsOutput = list.size();
    list.push_back(d);

    d.identifier = "aggregate_increments";
    d.name = "Accumulated Output Increments";
    d.description = "Accumulated output time increments";
    d.sampleRate = 0;
    m_d->m_aggregateIncrementsOutput = list.size();
    list.push_back(d);

    d.identifier = "divergence";
    d.name = "Divergence from Linear";
    d.description = "Difference between actual output time and the output time for a theoretical linear stretch";
    d.isQuantized = false;
    d.sampleRate = 0;
    m_d->m_divergenceOutput = list.size();
    list.push_back(d);

    d.identifier = "phaseresetdf";
    d.name = "Phase Reset Detection Function";
    d.description = "Curve whose peaks are used to identify transients for phase reset points";
    d.unit = "";
    d.sampleRate = float(rate);
    m_d->m_phaseResetDfOutput = list.size();
    list.push_back(d);

    d.identifier = "smoothedphaseresetdf";
    d.name = "Smoothed Phase Reset Detection Function";
    d.description = "Phase reset curve smoothed for peak picking";
    d.unit = "";
    m_d->m_smoothedPhaseResetDfOutput = list.size();
    list.push_back(d);

    d.identifier = "phaseresetpoints";
    d.name = "Phase Reset Points";
    d.description = "Points estimated as transients at which phase reset occurs";
    d.unit = "";
    d.hasFixedBinCount = true;
    d.binCount = 0;
    d.hasKnownExtents = false;
    d.isQuantized = false;
    d.sampleRate = 0;
    m_d->m_phaseResetPointsOutput = list.size();
    list.push_back(d);

    d.identifier = "timesyncpoints";
    d.name = "Time Sync Points";
    d.description = "Salient points which stretcher aims to place with strictly correct timing";
    d.unit = "";
    d.hasFixedBinCount = true;
    d.binCount = 0;
    d.hasKnownExtents = false;
    d.isQuantized = false;
    d.sampleRate = 0;
    m_d->m_timeSyncPointsOutput = list.size();
    list.push_back(d);

    return list;
}

RubberBandVampPlugin::ParameterList
RubberBandVampPlugin::getParameterDescriptors() const
{
    ParameterList list;

    ParameterDescriptor d;
    d.identifier = "timeratio";
    d.name = "Time Ratio";
    d.description = "Ratio to modify overall duration by";
    d.unit = "%";
    d.minValue = 1;
    d.maxValue = 500;
    d.defaultValue = 100;
    d.isQuantized = false;
    list.push_back(d);

    d.identifier = "pitchratio";
    d.name = "Pitch Scale Ratio";
    d.description = "Frequency ratio to modify pitch by";
    d.unit = "%";
    d.minValue = 1;
    d.maxValue = 500;
    d.defaultValue = 100;
    d.isQuantized = false;
    list.push_back(d);

    d.identifier = "mode";
    d.name = "Processing Mode";
    d.description = ""; //!!!
    d.unit = "";
    d.minValue = 0;
    d.maxValue = 1;
    d.defaultValue = 0;
    d.isQuantized = true;
    d.quantizeStep = 1;
    d.valueNames.clear();
    d.valueNames.push_back("Offline");
    d.valueNames.push_back("Real Time");
    list.push_back(d);

    d.identifier = "stretchtype";
    d.name = "Stretch Flexibility";
    d.description = ""; //!!!
    d.unit = "";
    d.minValue = 0;
    d.maxValue = 1;
    d.defaultValue = 0;
    d.isQuantized = true;
    d.quantizeStep = 1;
    d.valueNames.clear();
    d.valueNames.push_back("Elastic");
    d.valueNames.push_back("Precise");
    list.push_back(d);

    d.identifier = "transientmode";
    d.name = "Transient Handling";
    d.description = ""; //!!!
    d.unit = "";
    d.minValue = 0;
    d.maxValue = 2;
    d.defaultValue = 0;
    d.isQuantized = true;
    d.quantizeStep = 1;
    d.valueNames.clear();
    d.valueNames.push_back("Mixed");
    d.valueNames.push_back("Smooth");
    d.valueNames.push_back("Crisp");
    list.push_back(d);

    d.identifier = "phasemode";
    d.name = "Phase Handling";
    d.description = ""; //!!!
    d.unit = "";
    d.minValue = 0;
    d.maxValue = 1;
    d.defaultValue = 0;
    d.isQuantized = true;
    d.quantizeStep = 1;
    d.valueNames.clear();
    d.valueNames.push_back("Peak Locked");
    d.valueNames.push_back("Independent");
    list.push_back(d);

    d.identifier = "windowmode";
    d.name = "Window Length";
    d.description = ""; //!!!
    d.unit = "";
    d.minValue = 0;
    d.maxValue = 2;
    d.defaultValue = 0;
    d.isQuantized = true;
    d.quantizeStep = 1;
    d.valueNames.clear();
    d.valueNames.push_back("Standard");
    d.valueNames.push_back("Short");
    d.valueNames.push_back("Long");
    list.push_back(d);

    return list;
}

float
RubberBandVampPlugin::getParameter(std::string id) const
{
    if (id == "timeratio") return m_d->m_timeRatio * 100.f;
    if (id == "pitchratio") return m_d->m_pitchRatio * 100.f;
    if (id == "mode") return m_d->m_realtime ? 1.f : 0.f;
    if (id == "stretchtype") return m_d->m_elasticTiming ? 0.f : 1.f;
    if (id == "transientmode") return float(m_d->m_transientMode);
    if (id == "phasemode") return m_d->m_phaseIndependent ? 1.f : 0.f;
    if (id == "windowmode") return float(m_d->m_windowLength);
    return 0.f;
}

void
RubberBandVampPlugin::setParameter(std::string id, float value)
{
    if (id == "timeratio") {
        m_d->m_timeRatio = value / 100;
    } else if (id == "pitchratio") {
        m_d->m_pitchRatio = value / 100;
    } else {
        bool set = (value > 0.5);
        if (id == "mode") m_d->m_realtime = set;
        else if (id == "stretchtype") m_d->m_elasticTiming = !set;
        else if (id == "transientmode") m_d->m_transientMode = int(value + 0.5);
        else if (id == "phasemode") m_d->m_phaseIndependent = set;
        else if (id == "windowmode") m_d->m_windowLength = int(value + 0.5);
    }
}

bool
RubberBandVampPlugin::initialise(size_t channels, size_t stepSize, size_t blockSize)
{
    if (channels < getMinChannelCount() ||
	channels > getMaxChannelCount()) return false;

    m_d->m_stepSize = std::min(stepSize, blockSize);
    m_d->m_blockSize = stepSize;

    RubberBand::RubberBandStretcher::Options options = 0;

    if (m_d->m_realtime)
         options |= RubberBand::RubberBandStretcher::OptionProcessRealTime;
    else options |= RubberBand::RubberBandStretcher::OptionProcessOffline;

    if (m_d->m_elasticTiming)
         options |= RubberBand::RubberBandStretcher::OptionStretchElastic;
    else options |= RubberBand::RubberBandStretcher::OptionStretchPrecise;
 
    if (m_d->m_transientMode == 0) 
         options |= RubberBand::RubberBandStretcher::OptionTransientsMixed;
    else if (m_d->m_transientMode == 1) 
         options |= RubberBand::RubberBandStretcher::OptionTransientsSmooth;
    else options |= RubberBand::RubberBandStretcher::OptionTransientsCrisp;

    if (m_d->m_phaseIndependent) 
         options |= RubberBand::RubberBandStretcher::OptionPhaseIndependent;
    else options |= RubberBand::RubberBandStretcher::OptionPhaseLaminar;

    if (m_d->m_windowLength == 0)
         options |= RubberBand::RubberBandStretcher::OptionWindowStandard;
    else if (m_d->m_windowLength == 1)
         options |= RubberBand::RubberBandStretcher::OptionWindowShort;
    else options |= RubberBand::RubberBandStretcher::OptionWindowLong;

    delete m_d->m_stretcher;
    m_d->m_stretcher = new RubberBand::RubberBandStretcher
        (m_d->m_sampleRate, channels, options);
    m_d->m_stretcher->setDebugLevel(1);
    m_d->m_stretcher->setTimeRatio(m_d->m_timeRatio);
    m_d->m_stretcher->setPitchScale(m_d->m_pitchRatio);

    m_d->m_counter = 0;
    m_d->m_accumulatedIncrement = 0;

    m_d->m_outputDump = 0;

    return true;
}

void
RubberBandVampPlugin::reset()
{
//    delete m_stretcher;  //!!! or just if (m_stretcher) m_stretcher->reset();
//    m_stretcher = new RubberBand::RubberBandStretcher(lrintf(m_inputSampleRate), channels);
    if (m_d->m_stretcher) m_d->m_stretcher->reset();
}

RubberBandVampPlugin::FeatureSet
RubberBandVampPlugin::process(const float *const *inputBuffers,
                              Vamp::RealTime timestamp)
{
    if (m_d->m_realtime) {
        return m_d->processRealTime(inputBuffers, timestamp);
    } else {
        return m_d->processOffline(inputBuffers, timestamp);
    }        
}

RubberBandVampPlugin::FeatureSet
RubberBandVampPlugin::getRemainingFeatures()
{
    if (m_d->m_realtime) {
        return m_d->getRemainingFeaturesRealTime();
    } else {
        return m_d->getRemainingFeaturesOffline();
    }
}

RubberBandVampPlugin::FeatureSet
RubberBandVampPlugin::Impl::processOffline(const float *const *inputBuffers,
                                           Vamp::RealTime timestamp)
{
    if (!m_stretcher) {
	cerr << "ERROR: RubberBandVampPlugin::processOffline: "
	     << "RubberBandVampPlugin has not been initialised"
	     << endl;
	return FeatureSet();
    }

    m_stretcher->study(inputBuffers, m_blockSize, false);
    return FeatureSet();
}

RubberBandVampPlugin::FeatureSet
RubberBandVampPlugin::Impl::getRemainingFeaturesOffline()
{
    m_stretcher->study(0, 0, true);

    m_stretcher->calculateStretch();

    int rate = m_sampleRate;

    RubberBand::StretchCalculator sc(rate,
                                     m_stretcher->getInputIncrement(),
                                     true);

    size_t inputIncrement = m_stretcher->getInputIncrement();
    std::vector<int> outputIncrements = m_stretcher->getOutputIncrements();
    std::vector<float> phaseResetDf = m_stretcher->getPhaseResetCurve();
    std::vector<int> peaks = m_stretcher->getExactTimePoints();
    std::vector<float> smoothedDf = sc.smoothDF(phaseResetDf);

    FeatureSet features = createFeatures
        (inputIncrement, outputIncrements, phaseResetDf, peaks, smoothedDf,
         0, true);

    return features;
}

RubberBandVampPlugin::FeatureSet
RubberBandVampPlugin::Impl::processRealTime(const float *const *inputBuffers,
                                            Vamp::RealTime timestamp)
{
    // This function is not in any way a real-time function (i.e. it
    // has no requirement to be RT safe); it simply operates the
    // stretcher in RT mode.

    if (!m_stretcher) {
	cerr << "ERROR: RubberBandVampPlugin::processRealTime: "
	     << "RubberBandVampPlugin has not been initialised"
	     << endl;
	return FeatureSet();
    }

    m_stretcher->process(inputBuffers, m_blockSize, false);
    
    size_t inputIncrement = m_stretcher->getInputIncrement();
    std::vector<int> outputIncrements = m_stretcher->getOutputIncrements();
    std::vector<float> phaseResetDf = m_stretcher->getPhaseResetCurve();
    std::vector<float> smoothedDf; // not meaningful in RT mode
    std::vector<int> dummyPoints;
    FeatureSet features = createFeatures
        (inputIncrement, outputIncrements, phaseResetDf, dummyPoints, smoothedDf, 
         m_counter, false);
    m_counter += outputIncrements.size();

    int available = 0;
    while ((available = m_stretcher->available()) > 0) {
        if (!m_outputDump) {
            m_outputDump = new float *[m_stretcher->getChannelCount()];
            for (size_t i = 0; i < m_stretcher->getChannelCount(); ++i) {
                m_outputDump[i] = new float[m_blockSize];
            }
        }
        m_stretcher->retrieve(m_outputDump,
                              std::min(int(m_blockSize), available));
    }

    return features;
}

RubberBandVampPlugin::FeatureSet
RubberBandVampPlugin::Impl::getRemainingFeaturesRealTime()
{
    return FeatureSet();
}

RubberBandVampPlugin::FeatureSet
RubberBandVampPlugin::Impl::createFeatures(size_t inputIncrement,
                                           std::vector<int> &outputIncrements,
                                           std::vector<float> &phaseResetDf,
                                           std::vector<int> &exactPoints,
                                           std::vector<float> &smoothedDf,
                                           size_t baseCount,
                                           bool includeFinal)
{
    size_t actual = m_accumulatedIncrement;

    double overallRatio = m_timeRatio * m_pitchRatio;

    char label[200];

    FeatureSet features;

    int rate = m_sampleRate;

    size_t epi = 0;

    for (size_t i = 0; i < outputIncrements.size(); ++i) {

        size_t frame = (baseCount + i) * inputIncrement;

        int oi = outputIncrements[i];
        bool hard = false;
        bool soft = false;

        if (oi < 0) {
            oi = -oi;
            hard = true;
        }

        if (epi < exactPoints.size() && int(i) == exactPoints[epi]) {
            soft = true;
            ++epi;
        }

        double linear = (frame * overallRatio);

        Vamp::RealTime t = Vamp::RealTime::frame2RealTime(frame, rate);

        Feature feature;
        feature.hasTimestamp = true;
        feature.timestamp = t;
        feature.values.push_back(float(oi));
        feature.label = Vamp::RealTime::frame2RealTime(oi, rate).toText();
        features[m_incrementsOutput].push_back(feature);

        feature.values.clear();
        feature.values.push_back(float(actual));
        feature.label = Vamp::RealTime::frame2RealTime(actual, rate).toText();
        features[m_aggregateIncrementsOutput].push_back(feature);

        feature.values.clear();
        feature.values.push_back(actual - linear);

        sprintf(label, "expected %ld, actual %ld, difference %ld (%s ms)",
                long(linear), long(actual), long(actual - linear),
                // frame2RealTime expects an integer frame number,
                // hence our multiplication factor
                (Vamp::RealTime::frame2RealTime
                 (lrintf((actual - linear) * 1000), rate) / 1000)
                .toText().c_str());
        feature.label = label;

        features[m_divergenceOutput].push_back(feature);
        actual += oi;
        
        char buf[30];

        if (i < phaseResetDf.size()) {
            feature.values.clear();
            feature.values.push_back(phaseResetDf[i]);
            sprintf(buf, "%d", int(baseCount + i));
            feature.label = buf;
            features[m_phaseResetDfOutput].push_back(feature);
        }

        if (i < smoothedDf.size()) {
            feature.values.clear();
            feature.values.push_back(smoothedDf[i]);
            features[m_smoothedPhaseResetDfOutput].push_back(feature);
        }

        if (hard) {
            feature.values.clear();
            feature.label = "Phase Reset";
            features[m_phaseResetPointsOutput].push_back(feature);
        }

        if (hard || soft) {
            feature.values.clear();
            feature.label = "Time Sync";
            features[m_timeSyncPointsOutput].push_back(feature);
        }            
    }

    if (includeFinal) {
        Vamp::RealTime t = Vamp::RealTime::frame2RealTime
            (inputIncrement * (baseCount + outputIncrements.size()), rate);
        Feature feature;
        feature.hasTimestamp = true;
        feature.timestamp = t;
        feature.label = Vamp::RealTime::frame2RealTime(actual, rate).toText();
        feature.values.clear();
        feature.values.push_back(float(actual));
        features[m_aggregateIncrementsOutput].push_back(feature);

        float linear = ((baseCount + outputIncrements.size())
                        * inputIncrement * overallRatio);
        feature.values.clear();
        feature.values.push_back(actual - linear);
        feature.label =  // see earlier comment
            (Vamp::RealTime::frame2RealTime //!!! update this as earlier label
             (lrintf((actual - linear) * 1000), rate) / 1000)
            .toText();
        features[m_divergenceOutput].push_back(feature);
    }

    m_accumulatedIncrement = actual;

    return features;
}

