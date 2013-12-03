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

#ifndef _ONSET_PLUGIN_H_
#define _ONSET_PLUGIN_H_

#include <vamp-sdk/Plugin.h>
#include <aubio/aubio.h>

class Onset : public Vamp::Plugin
{
public:
    Onset(float inputSampleRate);
    virtual ~Onset();

    bool initialise(size_t channels, size_t stepSize, size_t blockSize);
    void reset();

    InputDomain getInputDomain() const { return TimeDomain; }

    std::string getIdentifier() const;
    std::string getName() const;
    std::string getDescription() const;
    std::string getMaker() const;
    int getPluginVersion() const;
    std::string getCopyright() const;

    ParameterList getParameterDescriptors() const;
    float getParameter(std::string) const;
    void setParameter(std::string, float);

    size_t getPreferredStepSize() const;
    size_t getPreferredBlockSize() const;

    OutputList getOutputDescriptors() const;

    FeatureSet process(const float *const *inputBuffers,
                       Vamp::RealTime timestamp);

    FeatureSet getRemainingFeatures();

protected:
    fvec_t *m_ibuf;
    cvec_t *m_fftgrain;
    fvec_t *m_onset;
    aubio_pvoc_t *m_pv;
    aubio_pickpeak_t *m_peakpick;
    aubio_onsetdetection_t *m_onsetdet;
    aubio_onsetdetection_type m_onsettype;
    float m_threshold;
    float m_silence;
    size_t m_stepSize;
    size_t m_blockSize;
    size_t m_channelCount;
    Vamp::RealTime m_delay;
    Vamp::RealTime m_lastOnset;
};


#endif
