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

#include "LocalCandidatePYIN.h"
#include "MonoPitch.h"
#include "YinUtil.h"

#include "vamp-sdk/FFT.h"

#include <vector>
#include <algorithm>

#include <cstdio>
#include <sstream>
// #include <iostream>
#include <cmath>
#include <complex>
#include <map>

#include <boost/math/distributions.hpp>

using std::string;
using std::vector;
using std::map;
using Vamp::RealTime;


LocalCandidatePYIN::LocalCandidatePYIN(float inputSampleRate) :
    Plugin(inputSampleRate),
    m_channels(0),
    m_stepSize(256),
    m_blockSize(2048),
    m_fmin(40),
    m_fmax(700),
    m_oPitchTrackCandidates(0),
    m_threshDistr(2.0f),
    m_outputUnvoiced(0.0f),
    m_preciseTime(0.0f),
    m_pitchProb(0),
    m_timestamp(0),
    m_nCandidate(13)
{
}

LocalCandidatePYIN::~LocalCandidatePYIN()
{
}

string
LocalCandidatePYIN::getIdentifier() const
{
    return "localcandidatepyin";
}

string
LocalCandidatePYIN::getName() const
{
    return "Local Candidate PYIN";
}

string
LocalCandidatePYIN::getDescription() const
{
    return "Monophonic pitch and note tracking based on a probabilistic Yin extension.";
}

string
LocalCandidatePYIN::getMaker() const
{
    return "Matthias Mauch";
}

int
LocalCandidatePYIN::getPluginVersion() const
{
    // Increment this each time you release a version that behaves
    // differently from the previous one
    return 2;
}

string
LocalCandidatePYIN::getCopyright() const
{
    return "GPL";
}

LocalCandidatePYIN::InputDomain
LocalCandidatePYIN::getInputDomain() const
{
    return TimeDomain;
}

size_t
LocalCandidatePYIN::getPreferredBlockSize() const
{
    return 2048;
}

size_t
LocalCandidatePYIN::getPreferredStepSize() const
{
    return 256;
}

size_t
LocalCandidatePYIN::getMinChannelCount() const
{
    return 1;
}

size_t
LocalCandidatePYIN::getMaxChannelCount() const
{
    return 1;
}

LocalCandidatePYIN::ParameterList
LocalCandidatePYIN::getParameterDescriptors() const
{
    ParameterList list;

    ParameterDescriptor d;

    d.identifier = "threshdistr";
    d.name = "Yin threshold distribution";
    d.description = ".";
    d.unit = "";
    d.minValue = 0.0f;
    d.maxValue = 7.0f;
    d.defaultValue = 2.0f;
    d.isQuantized = true;
    d.quantizeStep = 1.0f;
    d.valueNames.push_back("Uniform");
    d.valueNames.push_back("Beta (mean 0.10)");
    d.valueNames.push_back("Beta (mean 0.15)");
    d.valueNames.push_back("Beta (mean 0.20)");
    d.valueNames.push_back("Beta (mean 0.30)");
    d.valueNames.push_back("Single Value 0.10");
    d.valueNames.push_back("Single Value 0.15");
    d.valueNames.push_back("Single Value 0.20");
    list.push_back(d);

    d.identifier = "outputunvoiced";
    d.valueNames.clear();
    d.name = "Output estimates classified as unvoiced?";
    d.description = ".";
    d.unit = "";
    d.minValue = 0.0f;
    d.maxValue = 2.0f;
    d.defaultValue = 0.0f;
    d.isQuantized = true;
    d.quantizeStep = 1.0f;
    d.valueNames.push_back("No");
    d.valueNames.push_back("Yes");
    d.valueNames.push_back("Yes, as negative frequencies");
    list.push_back(d);

    d.identifier = "precisetime";
    d.valueNames.clear();
    d.name = "Use non-standard precise YIN timing (slow).";
    d.description = ".";
    d.unit = "";
    d.minValue = 0.0f;
    d.maxValue = 1.0f;
    d.defaultValue = 0.0f;
    d.isQuantized = true;
    d.quantizeStep = 1.0f;
    list.push_back(d);

    return list;
}

float
LocalCandidatePYIN::getParameter(string identifier) const
{
    if (identifier == "threshdistr") {
            return m_threshDistr;
    }
    if (identifier == "outputunvoiced") {
            return m_outputUnvoiced;
    }
    if (identifier == "precisetime") {
            return m_preciseTime;
    }
    return 0.f;
}

void
LocalCandidatePYIN::setParameter(string identifier, float value)
{
    if (identifier == "threshdistr")
    {
        m_threshDistr = value;
    }
    if (identifier == "outputunvoiced")
    {
        m_outputUnvoiced = value;
    }
    if (identifier == "precisetime")
    {
        m_preciseTime = value;
    }
}

LocalCandidatePYIN::ProgramList
LocalCandidatePYIN::getPrograms() const
{
    ProgramList list;
    return list;
}

string
LocalCandidatePYIN::getCurrentProgram() const
{
    return ""; // no programs
}

void
LocalCandidatePYIN::selectProgram(string name)
{
}

LocalCandidatePYIN::OutputList
LocalCandidatePYIN::getOutputDescriptors() const
{
    OutputList outputs;

    OutputDescriptor d;

    d.identifier = "pitchtrackcandidates";
    d.name = "Pitch track candidates";
    d.description = "Multiple candidate pitch tracks.";
    d.unit = "Hz";
    d.hasFixedBinCount = false;
    d.hasKnownExtents = true;
    d.minValue = m_fmin;
    d.maxValue = 500; //!!!???
    d.isQuantized = false;
    d.sampleType = OutputDescriptor::FixedSampleRate;
    d.sampleRate = (m_inputSampleRate / m_stepSize);
    d.hasDuration = false;
    outputs.push_back(d);

    return outputs;
}

bool
LocalCandidatePYIN::initialise(size_t channels, size_t stepSize, size_t blockSize)
{
    if (channels < getMinChannelCount() ||
	channels > getMaxChannelCount()) return false;

/*
    std::cerr << "LocalCandidatePYIN::initialise: channels = " << channels
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
LocalCandidatePYIN::reset()
{
    m_pitchProb.clear();
    m_timestamp.clear();
/*
    std::cerr << "LocalCandidatePYIN::reset"
          << ", blockSize = " << m_blockSize
          << std::endl;
*/
}

LocalCandidatePYIN::FeatureSet
LocalCandidatePYIN::process(const float *const *inputBuffers, RealTime timestamp)
{
    int offset = m_preciseTime == 1.0 ? m_blockSize/2 : m_blockSize/4;
    timestamp = timestamp + Vamp::RealTime::frame2RealTime(offset, lrintf(m_inputSampleRate));

    double *dInputBuffers = new double[m_blockSize];
    for (size_t i = 0; i < m_blockSize; ++i) dInputBuffers[i] = inputBuffers[0][i];

    size_t yinBufferSize = m_blockSize/2;
    double* yinBuffer = new double[yinBufferSize];
    if (!m_preciseTime) YinUtil::fastDifference(dInputBuffers, yinBuffer, yinBufferSize);
    else YinUtil::slowDifference(dInputBuffers, yinBuffer, yinBufferSize);

    delete [] dInputBuffers;

    YinUtil::cumulativeDifference(yinBuffer, yinBufferSize);

    float minFrequency = 60;
    float maxFrequency = 900;
    vector<double> peakProbability = YinUtil::yinProb(yinBuffer,
                                                      m_threshDistr,
                                                      yinBufferSize,
                                                      m_inputSampleRate/maxFrequency,
                                                      m_inputSampleRate/minFrequency);

    vector<pair<double, double> > tempPitchProb;
    for (size_t iBuf = 0; iBuf < yinBufferSize; ++iBuf)
    {
        if (peakProbability[iBuf] > 0)
        {
            double currentF0 =
                m_inputSampleRate * (1.0 /
                YinUtil::parabolicInterpolation(yinBuffer, iBuf, yinBufferSize));
            double tempPitch = 12 * std::log(currentF0/440)/std::log(2.) + 69;
            tempPitchProb.push_back(pair<double, double>(tempPitch, peakProbability[iBuf]));
        }
    }
    m_pitchProb.push_back(tempPitchProb);
    m_timestamp.push_back(timestamp);

    delete[] yinBuffer;

    return FeatureSet();
}

LocalCandidatePYIN::FeatureSet
LocalCandidatePYIN::getRemainingFeatures()
{
    // timestamp -> candidate number -> value
    map<RealTime, map<int, float> > featureValues;

    // std::cerr << "in remaining features" << std::endl;

    if (m_pitchProb.empty()) {
        return FeatureSet();
    }

    // MONO-PITCH STUFF
    MonoPitch mp;
    size_t nFrame = m_timestamp.size();
    vector<vector<float> > pitchTracks;
    vector<float> freqSum = vector<float>(m_nCandidate);
    vector<float> freqNumber = vector<float>(m_nCandidate);
    vector<float> freqMean = vector<float>(m_nCandidate);

    boost::math::normal normalDist(0, 8); // semitones sd
    float maxNormalDist = boost::math::pdf(normalDist, 0);

    // Viterbi-decode multiple times with different frequencies emphasised
    for (size_t iCandidate = 0; iCandidate < m_nCandidate; ++iCandidate)
    {
        pitchTracks.push_back(vector<float>(nFrame));
        vector<vector<pair<double,double> > > tempPitchProb;
        float centrePitch = 45 + 3 * iCandidate;

        for (size_t iFrame = 0; iFrame < nFrame; ++iFrame) {
            tempPitchProb.push_back(vector<pair<double,double> >());
            float sumProb = 0;
            float pitch = 0;
            float prob = 0;
            for (size_t iProb = 0; iProb < m_pitchProb[iFrame].size(); ++iProb)
            {
                pitch = m_pitchProb[iFrame][iProb].first;
                prob  = m_pitchProb[iFrame][iProb].second *
                    boost::math::pdf(normalDist, pitch-centrePitch) /
                    maxNormalDist * 2;
                sumProb += prob;
                tempPitchProb[iFrame].push_back(
                    pair<double,double>(pitch,prob));
            }
            for (size_t iProb = 0; iProb < m_pitchProb[iFrame].size(); ++iProb)
            {
                tempPitchProb[iFrame][iProb].second /= sumProb;
            }
        }

        vector<float> mpOut = mp.process(tempPitchProb);
        //float prevFreq = 0;
        for (size_t iFrame = 0; iFrame < nFrame; ++iFrame)
        {
            if (mpOut[iFrame] > 0) {

                pitchTracks[iCandidate][iFrame] = mpOut[iFrame];
                freqSum[iCandidate] += mpOut[iFrame];
                freqNumber[iCandidate]++;
                //prevFreq = mpOut[iFrame];

            }
        }
        freqMean[iCandidate] = freqSum[iCandidate]*1.0/freqNumber[iCandidate];
    }

    // find near duplicate pitch tracks
    vector<size_t> duplicates;
    for (size_t iCandidate = 0; iCandidate < m_nCandidate; ++iCandidate) {
        for (size_t jCandidate = iCandidate+1; jCandidate < m_nCandidate; ++jCandidate) {
            size_t countEqual = 0;
            for (size_t iFrame = 0; iFrame < nFrame; ++iFrame)
            {
                if ((pitchTracks[jCandidate][iFrame] == 0 && pitchTracks[iCandidate][iFrame] == 0) ||
                fabs(pitchTracks[iCandidate][iFrame]/pitchTracks[jCandidate][iFrame]-1)<0.01)
                countEqual++;
            }
            // std::cerr << "proportion equal: " << (countEqual * 1.0 / nFrame) << std::endl;
            if (countEqual * 1.0 / nFrame > 0.8) {
                if (freqNumber[iCandidate] > freqNumber[jCandidate]) {
                    duplicates.push_back(jCandidate);
                } else if (iCandidate < jCandidate) {
                    duplicates.push_back(iCandidate);
                }
            }
        }
    }

    // now find non-duplicate pitch tracks
    map<int, int> candidateActuals;
    map<int, std::string> candidateLabels;

    vector<vector<float> > outputFrequencies;
    for (size_t iFrame = 0; iFrame < nFrame; ++iFrame) outputFrequencies.push_back(vector<float>());

    int actualCandidateNumber = 0;
    for (size_t iCandidate = 0; iCandidate < m_nCandidate; ++iCandidate)
    {
        bool isDuplicate = false;
        for (size_t i = 0; i < duplicates.size(); ++i) {

            if (duplicates[i] == iCandidate) {
                isDuplicate = true;
                break;
            }
        }
        if (!isDuplicate && freqNumber[iCandidate] > 0.5*nFrame)
        {
            std::ostringstream convert;
            convert << actualCandidateNumber++;
            candidateLabels[iCandidate] = convert.str();
            candidateActuals[iCandidate] = actualCandidateNumber;
            // std::cerr << iCandidate << " " << actualCandidateNumber << " " << freqNumber[iCandidate] << " " << freqMean[iCandidate] << std::endl;
            for (size_t iFrame = 0; iFrame < nFrame; ++iFrame)
            {
                if (pitchTracks[iCandidate][iFrame] > 0)
                {
                    // featureValues[m_timestamp[iFrame]][iCandidate] =
                    //     pitchTracks[iCandidate][iFrame];
                    outputFrequencies[iFrame].push_back(pitchTracks[iCandidate][iFrame]);
                } else {
                    outputFrequencies[iFrame].push_back(0);
                }
            }
        }
        // fs[m_oPitchTrackCandidates].push_back(f);
    }

    // adapt our features so as to return a stack of candidate values
    // per frame

    FeatureSet fs;

    for (size_t iFrame = 0; iFrame < nFrame; ++iFrame){
        Feature f;
        f.hasTimestamp = true;
        f.timestamp = m_timestamp[iFrame];
        f.values = outputFrequencies[iFrame];
        fs[0].push_back(f);
    }

    // I stopped using Chris's map stuff below because I couldn't get my head around it
    //
    // for (map<RealTime, map<int, float> >::const_iterator i =
    //          featureValues.begin(); i != featureValues.end(); ++i) {
    //     Feature f;
    //     f.hasTimestamp = true;
    //     f.timestamp = i->first;
    //     int nextCandidate = candidateActuals.begin()->second;
    //     for (map<int, float>::const_iterator j =
    //              i->second.begin(); j != i->second.end(); ++j) {
    //         while (candidateActuals[j->first] > nextCandidate) {
    //             f.values.push_back(0);
    //             ++nextCandidate;
    //         }
    //         f.values.push_back(j->second);
    //         nextCandidate = j->first + 1;
    //     }
    //     //!!! can't use labels?
    //     fs[0].push_back(f);
    // }

    return fs;
}
