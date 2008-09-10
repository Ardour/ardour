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

#ifndef _RUBBERBAND_STRETCHERIMPL_H_
#define _RUBBERBAND_STRETCHERIMPL_H_

#include "RubberBandStretcher.h"

#include "Window.h"
#include "Thread.h"
#include "RingBuffer.h"
#include "FFT.h"
#include "sysutils.h"

#include <set>

namespace RubberBand
{

class AudioCurve;
class StretchCalculator;

class RubberBandStretcher::Impl
{
public:
    Impl(size_t sampleRate, size_t channels, Options options,
         double initialTimeRatio, double initialPitchScale);
    ~Impl();
    
    void reset();
    void setTimeRatio(double ratio);
    void setPitchScale(double scale);

    double getTimeRatio() const;
    double getPitchScale() const;

    size_t getLatency() const;

    void setTransientsOption(Options);
    void setPhaseOption(Options);
    void setFormantOption(Options);
    void setPitchOption(Options);

    void setExpectedInputDuration(size_t samples);
    void setMaxProcessSize(size_t samples);

    size_t getSamplesRequired() const;

    void study(const float *const *input, size_t samples, bool final);
    void process(const float *const *input, size_t samples, bool final);

    int available() const;
    size_t retrieve(float *const *output, size_t samples) const;

    float getFrequencyCutoff(int n) const;
    void setFrequencyCutoff(int n, float f);

    size_t getInputIncrement() const {
        return m_increment;
    }

    std::vector<int> getOutputIncrements() const;
    std::vector<float> getPhaseResetCurve() const;
    std::vector<int> getExactTimePoints() const;

    size_t getChannelCount() const {
        return m_channels;
    }
    
    void calculateStretch();

    void setDebugLevel(int level);
    static void setDefaultDebugLevel(int level) { m_defaultDebugLevel = level; }

protected:
    size_t m_sampleRate;
    size_t m_channels;

    size_t consumeChannel(size_t channel, const float *input,
                          size_t samples, bool final);
    void processChunks(size_t channel, bool &any, bool &last);
    bool processOneChunk(); // across all channels, for real time use
    bool processChunkForChannel(size_t channel, size_t phaseIncrement,
                                size_t shiftIncrement, bool phaseReset);
    bool testInbufReadSpace(size_t channel);
    void calculateIncrements(size_t &phaseIncrement,
                             size_t &shiftIncrement, bool &phaseReset);
    bool getIncrements(size_t channel, size_t &phaseIncrement,
                       size_t &shiftIncrement, bool &phaseReset);
    void analyseChunk(size_t channel);
    void modifyChunk(size_t channel, size_t outputIncrement, bool phaseReset);
    void formantShiftChunk(size_t channel);
    void synthesiseChunk(size_t channel);
    void writeChunk(size_t channel, size_t shiftIncrement, bool last);

    void calculateSizes();
    void configure();
    void reconfigure();

    double getEffectiveRatio() const;
    
    size_t roundUp(size_t value); // to next power of two

    bool resampleBeforeStretching() const;
    
    double m_timeRatio;
    double m_pitchScale;

    size_t m_windowSize;
    size_t m_increment;
    size_t m_outbufSize;

    size_t m_maxProcessSize;
    size_t m_expectedInputDuration;
    
    bool m_threaded;
    bool m_realtime;
    Options m_options;
    int m_debugLevel;

    enum ProcessMode {
        JustCreated,
        Studying,
        Processing,
        Finished
    };

    ProcessMode m_mode;

    std::map<size_t, Window<float> *> m_windows;
    Window<float> *m_window;
    FFT *m_studyFFT;

    Condition m_spaceAvailable;
    
    class ProcessThread : public Thread
    {
    public:
        ProcessThread(Impl *s, size_t c);
        void run();
        void signalDataAvailable();
        void abandon();
    private:
        Impl *m_s;
        size_t m_channel;
        Condition m_dataAvailable;
        bool m_abandoning;
    };

    mutable Mutex m_threadSetMutex;
    typedef std::set<ProcessThread *> ThreadSet;
    ThreadSet m_threadSet;
    

    size_t m_inputDuration;
    std::vector<float> m_phaseResetDf;
    std::vector<float> m_stretchDf;
    std::vector<bool> m_silence;
    int m_silentHistory;

    class ChannelData; 
    std::vector<ChannelData *> m_channelData;

    std::vector<int> m_outputIncrements;

    mutable RingBuffer<int> m_lastProcessOutputIncrements;
    mutable RingBuffer<float> m_lastProcessPhaseResetDf;

    AudioCurve *m_phaseResetAudioCurve;
    AudioCurve *m_stretchAudioCurve;
    AudioCurve *m_silentAudioCurve;
    StretchCalculator *m_stretchCalculator;

    float m_freq0;
    float m_freq1;
    float m_freq2;

    size_t m_baseWindowSize;
    float m_rateMultiple;

    void writeOutput(RingBuffer<float> &to, float *from,
                     size_t qty, size_t &outCount, size_t theoreticalOut);

    static int m_defaultDebugLevel;
    static const size_t m_defaultIncrement;
    static const size_t m_defaultWindowSize;
};

}

#endif
