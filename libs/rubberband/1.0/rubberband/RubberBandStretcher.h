/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

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

#ifndef _RUBBERBANDSTRETCHER_H_
#define _RUBBERBANDSTRETCHER_H_

#include "TimeStretcher.h"

#include <vector>

namespace RubberBand
{

class RubberBandStretcher : public TimeStretcher
{
public:

    /**
     * Processing options for the timestretcher.  The preferred
     * options should normally be set in the constructor, as a bitwise
     * OR of the option flags.  The default value (DefaultOptions) is
     * intended to give good results in most situations.
     *
     * 1. Flags prefixed \c OptionProcess determine how the timestretcher
     * will be invoked.  These options may not be changed after
     * construction.
     * 
     *   \li \c OptionProcessOffline - Run the stretcher in offline
     *   mode.  In this mode the input data needs to be provided
     *   twice, once to study(), which calculates a stretch profile
     *   for the audio, and once to process(), which stretches it.
     *
     *   \li \c OptionProcessRealTime - Run the stretcher in real-time
     *   mode.  In this mode only process() should be called, and the
     *   stretcher adjusts dynamically in response to the input audio.
     * 
     * The Process setting is likely to depend on your architecture:
     * non-real-time operation on seekable files: Offline; real-time
     * or streaming operation: RealTime.
     *
     * 2. Flags prefixed \c OptionStretch control the profile used for
     * variable timestretching.  Rubber Band always adjusts the
     * stretch profile to minimise stretching of busy broadband
     * transient sounds, but the degree to which it does so is
     * adjustable.  These options may not be changed after
     * construction.
     *
     *   \li \c OptionStretchElastic - Only meaningful in offline
     *   mode, and the default in that mode.  The audio will be
     *   stretched at a variable rate, aimed at preserving the quality
     *   of transient sounds as much as possible.  The timings of low
     *   activity regions between transients may be less exact than
     *   when the precise flag is set.
     * 
     *   \li \c OptionStretchPrecise - Although still using a variable
     *   stretch rate, the audio will be stretched so as to maintain
     *   as close as possible to a linear stretch ratio throughout.
     *   Timing may be better than when using \c OptionStretchElastic, at
     *   slight cost to the sound quality of transients.  This setting
     *   is always used when running in real-time mode.
     *
     * 3. Flags prefixed \c OptionTransients control the component
     * frequency phase-reset mechanism that may be used at transient
     * points to provide clarity and realism to percussion and other
     * significant transient sounds.  These options may be changed
     * after construction when running in real-time mode, but not when
     * running in offline mode.
     * 
     *   \li \c OptionTransientsCrisp - Reset component phases at the
     *   peak of each transient (the start of a significant note or
     *   percussive event).  This, the default setting, usually
     *   results in a clear-sounding output; but it is not always
     *   consistent, and may cause interruptions in stable sounds
     *   present at the same time as transient events.
     *
     *   \li \c OptionTransientsMixed - Reset component phases at the
     *   peak of each transient, outside a frequency range typical of
     *   musical fundamental frequencies.  The results may be more
     *   regular for mixed stable and percussive notes than
     *   \c OptionTransientsCrisp, but with a "phasier" sound.  The
     *   balance may sound very good for certain types of music and
     *   fairly bad for others.
     *
     *   \li \c OptionTransientsSmooth - Do not reset component phases
     *   at any point.  The results will be smoother and more regular
     *   but may be less clear than with either of the other
     *   transients flags.
     *
     * 4. Flags prefixed \c OptionPhase control the adjustment of
     * component frequency phases from one analysis window to the next
     * during non-transient segments.  These options may be changed at
     * any time.
     *
     *   \li \c OptionPhaseAdaptive - Lock the adjustments of phase
     *   for frequencies close to peak frequencies to those of the
     *   peak, but reduce the degree of locking as the stretch ratio
     *   gets longer.  This, the default setting, should give a good
     *   balance between clarity and smoothness in most situations.
     *
     *   \li \c OptionPhasePeakLocked - Lock the adjustments of phase
     *   for frequencies close to peak frequencies to those of the
     *   peak.  This should give a clear result in situations with
     *   relatively low stretch ratios, but a relatively metallic
     *   sound at longer stretches.
     *
     *   \li \c OptionPhaseIndependent - Do not lock phase adjustments
     *   to peak frequencies.  This usually results in a softer,
     *   phasier sound.
     *
     * 5. Flags prefixed \c OptionThreading control the threading
     * model of the stretcher.  These options may not be changed after
     * construction.
     *
     *   \li \c OptionThreadingAuto - Permit the stretcher to
     *   determine its own threading model.  Usually this means using
     *   one processing thread per audio channel in offline mode if
     *   the stretcher is able to determine that more than one CPU is
     *   available, and one thread only in realtime mode.
     *
     *   \li \c OptionThreadingNever - Never use more than one thread.
     *  
     *   \li \c OptionThreadingAlways - Use multiple threads in any
     *   situation where \c OptionThreadingAuto would do so, except omit
     *   the check for multiple CPUs and instead assume it to be true.
     *
     * 6. Flags prefixed \c OptionWindow control the window size for
     * FFT processing.  The window size actually used will depend on
     * many factors, but it can be influenced.  These options may not
     * be changed after construction.
     *
     *   \li \c OptionWindowStandard - Use the default window size.
     *   The actual size will vary depending on other parameters.
     *   This option is expected to produce better results than the
     *   other window options in most situations.
     *
     *   \li \c OptionWindowShort - Use a shorter window.  This may
     *   result in crisper sound for audio that depends strongly on
     *   its timing qualities.
     *
     *   \li \c OptionWindowLong - Use a longer window.  This is
     *   likely to result in a smoother sound at the expense of
     *   clarity and timing.
     */
    typedef int Options;
    
    static const int OptionProcessOffline   = 0x00000000;
    static const int OptionProcessRealTime  = 0x00000001;

    static const int OptionStretchElastic   = 0x00000000;
    static const int OptionStretchPrecise   = 0x00000010;
    
    static const int OptionTransientsCrisp  = 0x00000000;
    static const int OptionTransientsMixed  = 0x00000100;
    static const int OptionTransientsSmooth = 0x00000200;

    static const int OptionPhaseAdaptive    = 0x00000000;
    static const int OptionPhasePeakLocked  = 0x00001000;
    static const int OptionPhaseIndependent = 0x00002000;
    
    static const int OptionThreadingAuto    = 0x00000000;
    static const int OptionThreadingNever   = 0x00010000;
    static const int OptionThreadingAlways  = 0x00020000;

    static const int OptionWindowStandard   = 0x00000000;
    static const int OptionWindowShort      = 0x00100000;
    static const int OptionWindowLong       = 0x00200000;

    static const int DefaultOptions         = 0x00000000;
    static const int PercussiveOptions      = OptionWindowShort | \
                                              OptionPhaseIndependent;

    /**
     * Construct a time and pitch stretcher object to run at the given
     * sample rate, with the given number of channels.  Processing
     * options and the time and pitch scaling ratios may be provided.
     * The time and pitch ratios may be changed after construction,
     * but most of the options may not.  See the option documentation
     * above for more details.
     */
    RubberBandStretcher(size_t sampleRate,
                        size_t channels,
                        Options options = DefaultOptions,
                        double initialTimeRatio = 1.0,
                        double initialPitchScale = 1.0);
    virtual ~RubberBandStretcher();

    /**
     * Reset the stretcher's internal buffers.  The stretcher should
     * subsequently behave as if it had just been constructed
     * (although retaining the current time and pitch ratio).
     */
    virtual void reset();

    /**
     * Set the time ratio for the stretcher.  This is the ratio of
     * stretched to unstretched duration -- not tempo.  For example, a
     * ratio of 2.0 would make the audio twice as long (i.e. halve the
     * tempo); 0.5 would make it half as long (i.e. double the tempo);
     * 1.0 would leave the duration unaffected.
     *
     * If the stretcher was constructed in Offline mode, the time
     * ratio is fixed throughout operation; this function may be
     * called any number of times between construction (or a call to
     * reset()) and the first call to study() or process(), but may
     * not be called after study() or process() has been called.
     *
     * If the stretcher was constructed in RealTime mode, the time
     * ratio may be varied during operation; this function may be
     * called at any time, so long as it is not called concurrently
     * with process().  You should either call this function from the
     * same thread as process(), or provide your own mutex or similar
     * mechanism to ensure that setTimeRatio and process() cannot be
     * run at once (there is no internal mutex for this purpose).
     */
    virtual void setTimeRatio(double ratio);

    /**
     * Set the pitch scaling ratio for the stretcher.  This is the
     * ratio of target frequency to source frequency.  For example, a
     * ratio of 2.0 would shift up by one octave; 0.5 down by one
     * octave; or 1.0 leave the pitch unaffected.
     *
     * To put this in musical terms, a pitch scaling ratio
     * corresponding to a shift of S equal-tempered semitones (where S
     * is positive for an upwards shift and negative for downwards) is
     * pow(2.0, S / 12.0).
     *
     * If the stretcher was constructed in Offline mode, the pitch
     * scaling ratio is fixed throughout operation; this function may
     * be called any number of times between construction (or a call
     * to reset()) and the first call to study() or process(), but may
     * not be called after study() or process() has been called.
     *
     * If the stretcher was constructed in RealTime mode, the pitch
     * scaling ratio may be varied during operation; this function may
     * be called at any time, so long as it is not called concurrently
     * with process().  You should either call this function from the
     * same thread as process(), or provide your own mutex or similar
     * mechanism to ensure that setPitchScale and process() cannot be
     * run at once (there is no internal mutex for this purpose).
     */
    virtual void setPitchScale(double scale);

    /**
     * Return the last time ratio value that was set (either on
     * construction or with setTimeRatio()).
     */
    virtual double getTimeRatio() const;

    /**
     * Return the last pitch scaling ratio value that was set (either
     * on construction or with setPitchScale()).
     */
    virtual double getPitchScale() const;

    /**
     * Return the processing latency of the stretcher.  This is the
     * number of audio samples that one would have to discard at the
     * start of the output in order to ensure that the resulting audio
     * aligned with the input audio at the start.  In Offline mode,
     * latency is automatically adjusted for and the result is zero.
     * In RealTime mode, the latency may depend on the time and pitch
     * ratio and other options.
     */
    virtual size_t getLatency() const;

    /**
     * Change an OptionTransients configuration setting.  This may be
     * called at any time in RealTime mode.  It may not be called in
     * Offline mode (for which the transients option is fixed on
     * construction).
     */
    virtual void setTransientsOption(Options options);

    /**
     * Change an OptionPhase configuration setting.  This may be
     * called at any time in any mode.
     *
     * Note that if running multi-threaded in Offline mode, the change
     * may not take effect immediately if processing is already under
     * way when this function is called.
     */
    virtual void setPhaseOption(Options options);

    /**
     * Tell the stretcher exactly how many input samples it will
     * receive.  This is only useful in Offline mode, when it allows
     * the stretcher to ensure that the number of output samples is
     * exactly correct.  In RealTime mode no such guarantee is
     * possible and this value is ignored.
     */
    virtual void setExpectedInputDuration(size_t samples);

    /**
     * Ask the stretcher how many audio sample frames should be
     * provided as input in order to ensure that some more output
     * becomes available.  Normal usage consists of querying this
     * function, providing that number of samples to process(),
     * reading the output using available() and retrieve(), and then
     * repeating.
     *
     * Note that this value is only relevant to process(), not to
     * study() (to which you may pass any number of samples at a time,
     * and from which there is no output).
     */
     virtual size_t getSamplesRequired() const;

    /**
     * Tell the stretcher the maximum number of sample frames that you
     * will ever be passing in to a single process() call. If you
     * don't call this function, the stretcher will assume that you
     * never pass in more samples than getSamplesRequired() suggested
     * you should.  You should not pass in more samples than that
     * unless you have called setMaxProcessSize first.
     *
     * This function may not be called after the first call to study()
     * or process().
     *
     * Note that this value is only relevant to process(), not to
     * study() (to which you may pass any number of samples at a time,
     * and from which there is no output).
     */
    virtual void setMaxProcessSize(size_t samples);

    /**
     * Provide a block of "samples" sample frames for the stretcher to
     * study and calculate a stretch profile from.
     *
     * This is only meaningful in Offline mode, and is required if
     * running in that mode.  You should pass the entire input through
     * study() before any process() calls are made, as a sequence of
     * blocks in individual study() calls, or as a single large block.
     *
     * "input" should point to de-interleaved audio data with one
     * float array per channel.  "samples" supplies the number of
     * audio sample frames available in "input".  If "samples" is
     * zero, "input" may be NULL.
     * 
     * Set "final" to true if this is the last block of data that will
     * be provided to study() before the first process() call.
     */
    virtual void study(const float *const *input, size_t samples, bool final);

    /**
     * Provide a block of "samples" sample frames for processing.
     * See also getSamplesRequired() and setMaxProcessSize().
     *
     * Set "final" to true if this is the last block of input data.
     */
    virtual void process(const float *const *input, size_t samples, bool final);

    /**
     * Ask the stretcher how many audio sample frames of output data
     * are available for reading (via retrieve()).
     * 
     * This function returns 0 if no frames are available: this
     * usually means more input data needs to be provided, but if the
     * stretcher is running in threaded mode it may just mean that not
     * enough data has yet been processed.  Call getSamplesRequired()
     * to discover whether more input is needed.
     *
     * This function returns -1 if all data has been fully processed
     * and all output read, and the stretch process is now finished.
     */
    virtual int available() const;

    /**
     * Obtain some processed output data from the stretcher.  Up to
     * "samples" samples will be stored in the output arrays (one per
     * channel for de-interleaved audio data) pointed to by "output".
     * The return value is the actual number of sample frames
     * retrieved.
     */
    virtual size_t retrieve(float *const *output, size_t samples) const;

    virtual float getFrequencyCutoff(int n) const;
    virtual void setFrequencyCutoff(int n, float f);
    
    virtual size_t getInputIncrement() const;
    virtual std::vector<int> getOutputIncrements() const; //!!! document particular meaning in RT mode
    virtual std::vector<float> getPhaseResetCurve() const; //!!! document particular meaning in RT mode
    virtual std::vector<int> getExactTimePoints() const; //!!! meaningless in RT mode

    virtual size_t getChannelCount() const;
    
    virtual void calculateStretch();

    virtual void setDebugLevel(int level);

    static void setDefaultDebugLevel(int level);

protected:
    class Impl;
    Impl *m_d;
};

}

#endif
