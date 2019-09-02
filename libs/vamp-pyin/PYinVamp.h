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

#ifndef _PYINVAMP_H_
#define _PYINVAMP_H_

#include <vamp-sdk/Plugin.h>

#include "Yin.h"

class PYinVamp : public Vamp::Plugin
{
public:
    PYinVamp(float inputSampleRate);
    virtual ~PYinVamp();

    std::string getIdentifier() const;
    std::string getName() const;
    std::string getDescription() const;
    std::string getMaker() const;
    int getPluginVersion() const;
    std::string getCopyright() const;

    InputDomain getInputDomain() const;
    size_t getPreferredBlockSize() const;
    size_t getPreferredStepSize() const;
    size_t getMinChannelCount() const;
    size_t getMaxChannelCount() const;

    ParameterList getParameterDescriptors() const;
    float getParameter(std::string identifier) const;
    void setParameter(std::string identifier, float value);

    ProgramList getPrograms() const;
    std::string getCurrentProgram() const;
    void selectProgram(std::string name);

    OutputList getOutputDescriptors() const;

    bool initialise(size_t channels, size_t stepSize, size_t blockSize);
    void reset();

    FeatureSet process(const float *const *inputBuffers,
                       Vamp::RealTime timestamp);

    FeatureSet getRemainingFeatures();

protected:
    size_t m_channels;
    size_t m_stepSize;
    size_t m_blockSize;
    float m_fmin;
    float m_fmax;
    Yin m_yin;
    
    mutable int m_oF0Candidates;
    mutable int m_oF0Probs;
    mutable int m_oVoicedProb;
    mutable int m_oCandidateSalience;
    mutable int m_oSmoothedPitchTrack;
    mutable int m_oNotes;

    float m_threshDistr;
    float m_outputUnvoiced;
    float m_preciseTime;
    float m_lowAmp;
    float m_onsetSensitivity;
    float m_pruneThresh;
    vector<vector<pair<double, double> > > m_pitchProb;
    vector<Vamp::RealTime> m_timestamp;
    vector<float> m_level;
};

#endif
