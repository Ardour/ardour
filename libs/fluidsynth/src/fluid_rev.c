/******************************************************************************
 * FluidSynth - A Software Synthesizer
 *
 * Copyright (C) 2003  Peter Hanappe and others.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 *
 *
 *                           FDN REVERB
 *
 * Freeverb used by fluidsynth (v.1.1.10 and previous) is based on
 * Schroeder-Moorer reverberator:
 * https://ccrma.stanford.edu/~jos/pasp/Freeverb.html
 *
 * This FDN reverberation is based on jot FDN reverberator.
 * https://ccrma.stanford.edu/~jos/Reverb/FDN_Late_Reverberation.html
 * Like Freeverb it is a late reverb which is convenient for Fluidsynth.
 *
 *
 *                                        .-------------------.
 *                      .-----------------|                   |
 *                      |              -  |      Feedback     |
 *                      |  .--------------|       Matrix      |
 *                      |  |              |___________________|
 *                      |  |                         /|\   /|\
 *                     \|/ |   .---------. .-------.  |  -  |   .------.
 *                   .->+ ---->| Delay 0 |-|L.P.F 0|--*-------->|      |-> out
 *      .---------.  |     |   |_________| |_______|        |   |      |  left
 *      |Tone     |  |     |       -           -            |   |Stereo|
 * In ->|corrector|--*     |       -           -            |   | unit |
 * mono |_________|  |    \|/  .---------. .-------.        |   |      |-> out
 *                    ---->+ ->| Delay 7 |-|L.P.F 7|--------*-->|      |  right
 *                             |_________| |_______|            |______|
 *                                          /|\ /|\              /|\ /|\
 *                                           |   |                |   |
 *                                roomsize --/   |       width  --/   |
 *                                    damp ------/       level  ------/
 *
 * It takes a monophonic input and produces a stereo output.
 *
 * The parameters are the same than for Freeverb.
 * Also the default response of these parameters are the same than for Freeverb:
 *  - roomsize (0 to 1): control the reverb time from 0.7 to 12.5 s.
 *    This reverberation time is ofen called T60DC.
 *
 *  - damp (0 to 1): controls the reverb time frequency dependency.
 *    This controls the reverb time for the frequency sample rate/2
 *
 *    When 0, the reverb time for high frequencies is the same as
 *    for DC frequency.
 *    When > 0, high frequencies have less reverb time than lower frequencies.
 *
 *  - width (0 to 100): controls the left/right output separation.
 *    When 0, there are no separation and the signal on left and right.
 *    output is the same. This sounds like a monophonic signal.
 *    When 100, the separation between left and right is maximum.
 *
 *  - level (0 to 1), controls the ouput level reverberation.
 *
 * This FDN reverb produces a better quality reverberation tail than Freeverb with
 * far less ringing by using modulated delay lines that help to cancel
 * the building of a lot of resonances in the reverberation tail even when
 * using only 8 delays lines (NBR_DELAYS = 8) (default).
 *
 * Although 8 lines give good result, using 12 delays lines brings the overall
 * frequency density quality a bit higher. This quality augmentation is noticeable
 * particularly when using long reverb time (roomsize = 1) on solo instrument with
 * long release time. Of course the cpu load augmentation is +50% relatively
 * to 8 lines.
 *
 * As a general rule the reverberation tail quality is easier to perceive by ear
 * when using:
 * - percussive instruments (i.e piano and others).
 * - long reverb time (roomsize = 1).
 * - no damping (damp = 0).
 *
 *
 * The cpu load for 8 lines is a bit lower than for freeverb (- 3%),
 * but higher for 12 lines (+ 41%).
 *
 *
 * The memory consumption is less than for freeverb. This saves 147480 bytes
 * for 8 lines (- 72%) and 110152 bytes for 12 lines (- 54 %).
 * (see the results table below).
 *
 * Two macros are usable at compiler time:
 * - NBR_DELAYS: number of delay lines. 8 (default) or 12.
 * - ROOMSIZE_RESPONSE_LINEAR: allows to choose an alternate response of
 *   roomsize parameter.
 *   When this macro is not defined (the default), roomsize has the same
 *   response that Freeverb, that is:
 *   - roomsize (0 to 1) controls concave reverb time (0.7 to 12.5 s).
 *
 *   When this macro is defined, roomsize behaves linearly:
 *   - roomsize (0 to 1) controls reverb time linearly  (0.7 to 12.5 s).
 *   This linear response is convenient when using GUI controls.
 *
 * --------------------------------------------------------------------------
 * Compare table:
 * Note: the cpu load in % are relative each to other. These values are
 * given by the fluidsynth profile commands.
 * --------------------------------------------------------------------------
 * reverb    | NBR_DELAYS     | Performances    | memory size     | quality
 *           |                | (cpu_load: %)   | (bytes)         |
 * ==========================================================================
 * freeverb  | 2 x 8 comb     |  0.670 %        | 204616          | ringing
 *           | 2 x 4 all-pass |                 |                 |
 * ----------|---------------------------------------------------------------
 *    FDN    | 8              |  0.650 %        | 57136           | far less
 * modulated |                |(feeverb - 3%)   |(freeverb - 72%) | ringing
 *           |---------------------------------------------------------------
 *           | 12             |  0.942 %        | 94464           | best than
 *           |                |(freeverb + 41%) |(freeverb - 54%) | 8 lines
 *---------------------------------------------------------------------------
 *
 *
 *----------------------------------------------------------------------------
 * 'Denormalise' method to avoid loss of performance.
 * --------------------------------------------------
 * According to music-dsp thread 'Denormalise', Pentium processors
 * have a hardware 'feature', that is of interest here, related to
 * numeric underflow.  We have a recursive filter. The output decays
 * exponentially, if the input stops.  So the numbers get smaller and
 * smaller... At some point, they reach 'denormal' level.  This will
 * lead to drastic spikes in the CPU load.  The effect was reproduced
 * with the reverb - sometimes the average load over 10 s doubles!!.
 *
 * The 'undenormalise' macro fixes the problem: As soon as the number
 * is close enough to denormal level, the macro forces the number to
 * 0.0f.  The original macro is:
 *
 * #define undenormalise(sample) if(((*(unsigned int*)&sample)&0x7f800000)==0) sample=0.0f
 *
 * This will zero out a number when it reaches the denormal level.
 * Advantage: Maximum dynamic range Disadvantage: We'll have to check
 * every sample, expensive.  The alternative macro comes from a later
 * mail from Jon Watte. It will zap a number before it reaches
 * denormal level. Jon suggests to run it once per block instead of
 * every sample.
 */

/* Denormalising part II:
 *
 * Another method fixes the problem cheaper: Use a small DC-offset in
 * the filter calculations.  Now the signals converge not against 0,
 * but against the offset.  The constant offset is invisible from the
 * outside world (i.e. it does not appear at the output.  There is a
 * very small turn-on transient response, which should not cause
 * problems.
 */
#include "fluid_rev.h"
#include "fluid_sys.h"

/*----------------------------------------------------------------------------
                        Configuration macros at compiler time.

 3 macros are usable at compiler time:
  - NBR_DELAYs: number of delay lines. 8 (default) or 12.
  - ROOMSIZE_RESPONSE_LINEAR: allows to choose an alternate response for
    roomsize parameter.
  - DENORMALISING enable denormalising handling.
-----------------------------------------------------------------------------*/
/* Number of delay lines (must be only 8 or 12)
  8 is the default.
 12 produces a better quality but is +50% cpu expensive
*/
#define NBR_DELAYS        8 /* default*/

/* response curve of parameter roomsize  */
/*
    The default response is the same as Freeverb:
    - roomsize (0 to 1) controls concave reverb time (0.7 to 12.5 s).

    when ROOMSIZE_RESPONSE_LINEAR is defined, the response is:
    - roomsize (0 to 1) controls reverb time linearly  (0.7 to 12.5 s).
*/
//#define ROOMSIZE_RESPONSE_LINEAR

/* DENORMALISING enable denormalising handling */
#define DENORMALISING

#ifdef DENORMALISING
#define DC_OFFSET 1e-8
#else
#define DC_OFFSET  0.0
#endif

/*----------------------------------------------------------------------------
 Initial internal reverb settings (at reverb creation time)
-----------------------------------------------------------------------------*/
/* SCALE_WET_WIDTH is a compensation weight factor to get an output
   amplitude (wet) rather independent of the width setting.
    0: the output amplitude is fully dependant on the width setting.
   >0: the output amplitude is less dependant on the width setting.
   With a SCALE_WET_WIDTH of 0.2 the output amplitude is rather
   independent of width setting (see fluid_revmodel_update()).
 */
#define SCALE_WET_WIDTH 0.2f

/* It is best to inject the input signal less ofen. This contributes to obtain
a flatter response on comb filter. So the input gain is set to 0.1 rather 1.0. */
#define FIXED_GAIN 0.1f /* input gain */

/* SCALE_WET is adjusted to 5.0 to get internal output level equivalent to freeverb */
#define SCALE_WET 5.0f /* scale output gain */

/*----------------------------------------------------------------------------
 Internal FDN late reverb settings
-----------------------------------------------------------------------------*/

/*-- Reverberation time settings ----------------------------------
 MIN_DC_REV_TIME est defined egal to the minimum value of freeverb:
 MAX_DC_REV_TIME est defined egal to the maximum value of freeverb:
 T60DC is computed from gi and the longuest delay line in freeverb: L8 = 1617
 T60 = -3 * Li * T / log10(gi)
 T60 = -3 * Li *  / (log10(gi) * sr)

  - Li: length of comb filter delay line.
  - sr: sample rate.
  - gi: the feedback gain.

 The minimum value for freeverb correspond to gi = 0.7.
 with Mi = 1617, sr at 44100 Hz, and gi = 0.7 => MIN_DC_REV_TIME = 0.7 s

 The maximum value for freeverb correspond to gi = 0.98.
 with Mi = 1617, sr at 44100 Hz, and gi = 0.98 => MAX_DC_REV_TIME = 12.5 s
*/

#define MIN_DC_REV_TIME 0.7f	/* minimum T60DC reverb time: seconds */
#define MAX_DC_REV_TIME 12.5f	/* maximumm T60DC time in seconds */
#define RANGE_REV_TIME (MAX_DC_REV_TIME - MIN_DC_REV_TIME)

/* macro to compute internal reverberation time versus roomsize parameter  */
#define GET_DC_REV_TIME(roomsize) (MIN_DC_REV_TIME + RANGE_REV_TIME * roomsize)

/*-- Modulation related settings ----------------------------------*/
/* For many instruments, the range for MOD_FREQ and MOD_DEPTH should be:

 MOD_DEPTH: [3..6] (in samples).
 MOD_FREQ: [0.5 ..2.0] (in Hz).

 Values below the lower limits are often not sufficient to cancel unwanted
 "ringing"(resonant frequency).
 Values above upper limits augment the unwanted "chorus".

 With NBR_DELAYS to 8:
  MOD_DEPTH must be >= 4 to cancel the unwanted "ringing".[4..6].
 With NBR_DELAYS to 12:
  MOD_DEPTH to 3 is sufficient to cancel the unwanted "ringing".[3..6]
*/
#define MOD_DEPTH 4		/* modulation depth (samples)*/
#define MOD_RATE 50		/* modulation rate  (samples)*/
#define MOD_FREQ 1.0f	/* modulation frequency (Hz) */
/*
 Number of samples to add to the desired length of a delay line. This
 allow to take account of modulation interpolation.
 1 is sufficient with MOD_DEPTH equal to 6.
*/
#define INTERP_SAMPLES_NBR 1

/* phase offset between modulators waveform */
#define MOD_PHASE  (360.0/(float) NBR_DELAYS)

#if (NBR_DELAYS == 8)
    #define DELAY_L0 601
    #define DELAY_L1 691
    #define DELAY_L2 773
    #define DELAY_L3 839
    #define DELAY_L4 919
    #define DELAY_L5 997
    #define DELAY_L6 1061
    #define DELAY_L7 1129
#elif (NBR_DELAYS == 12)
    #define DELAY_L0 601
    #define DELAY_L1 691
    #define DELAY_L2 773
    #define DELAY_L3 839
    #define DELAY_L4 919
    #define DELAY_L5 997
    #define DELAY_L6 1061
    #define DELAY_L7 1093
    #define DELAY_L8 1129
    #define DELAY_L9 1151
    #define DELAY_L10 1171
    #define DELAY_L11 1187
#endif

/* Delay lines length table (in samples) */
static const int delay_length[NBR_DELAYS] =
{
    DELAY_L0, DELAY_L1, DELAY_L2, DELAY_L3,
    DELAY_L4, DELAY_L5, DELAY_L6, DELAY_L7,
#if (NBR_DELAYS == 12)
    DELAY_L8, DELAY_L9, DELAY_L10, DELAY_L11
#endif
};


/*---------------------------------------------------------------------------*/
/* The FDN late feed back matrix: A
                            T
  A   = P  -  2 / N * u  * u
   N     N             N    N

  N: the matrix dimension (i.e NBR_DELAYS).
  P: permutation matrix.
  u: is a colomn vector of 1.

*/
#define FDN_MATRIX_FACTOR (fluid_real_t)(-2.0 / NBR_DELAYS)

/*----------------------------------------------------------------------------
             Internal FDN late structures and static functions
-----------------------------------------------------------------------------*/


/*-----------------------------------------------------------------------------
 Delay absorbent low pass filter
-----------------------------------------------------------------------------*/
typedef struct
{
    fluid_real_t buffer;
    fluid_real_t b0, a1;         /* filter coefficients */
} fdn_delay_lpf;

/*-----------------------------------------------------------------------------
 Sets coefficients for delay absorbent low pass filter.
 @param lpf pointer on low pass filter structure.
 @param b0,a1 coefficients.
-----------------------------------------------------------------------------*/
static void set_fdn_delay_lpf(fdn_delay_lpf *lpf,
                              fluid_real_t b0, fluid_real_t  a1)
{
    lpf->b0 = b0;
    lpf->a1 = a1;
}

/*-----------------------------------------------------------------------------
 Process delay absorbent low pass filter.
 @param mod_delay modulated delay line.
 @param in, input sample.
 @param out output sample.
-----------------------------------------------------------------------------*/
/* process low pass damping filter (input, output, delay) */
#define process_damping_filter(in,out,mod_delay) \
{\
    out = in * mod_delay->dl.damping.b0 - mod_delay->dl.damping.buffer * \
                                            mod_delay->dl.damping.a1;\
    mod_delay->dl.damping.buffer = out;\
}\


/*-----------------------------------------------------------------------------
 Delay line :
 The delay line is composed of the line plus an absorbent low pass filter
 to get frequency dependant reverb time.
-----------------------------------------------------------------------------*/
typedef struct
{
    fluid_real_t *line; /* buffer line */
    int   size;    /* effective internal size (in samples) */
    /*-------------*/
    int line_in;  /* line in position */
    int line_out; /* line out position */
    /*-------------*/
    fdn_delay_lpf damping; /* damping low pass filter */
} delay_line;


/*-----------------------------------------------------------------------------
 Clears a delay line to DC_OFFSET float value.
 @param dl pointer on delay line structure
-----------------------------------------------------------------------------*/
static void clear_delay_line(delay_line *dl)
{
    int i;

    for(i = 0; i < dl->size; i++)
    {
        dl->line[i] = DC_OFFSET;
    }
}

/*-----------------------------------------------------------------------------
 Push a sample val into the delay line
-----------------------------------------------------------------------------*/
#define push_in_delay_line(dl, val) \
{\
    dl->line[dl->line_in] = val;\
    /* Incrementation and circular motion if necessary */\
    if(++dl->line_in >= dl->size) dl->line_in -= dl->size;\
}\

/*-----------------------------------------------------------------------------
 Modulator for modulated delay line
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
 Sinusoidal modulator
-----------------------------------------------------------------------------*/
/* modulator are integrated in modulated delay line */
typedef struct
{
    fluid_real_t   a1;          /* Coefficient: a1 = 2 * cos(w) */
    fluid_real_t   buffer1;     /* buffer1 */
    fluid_real_t   buffer2;     /* buffer2 */
    fluid_real_t   reset_buffer2;/* reset value of buffer2 */
} sinus_modulator;

/*-----------------------------------------------------------------------------
 Sets the frequency of sinus oscillator.

 @param mod pointer on modulator structure.
 @param freq frequency of the oscillator in Hz.
 @param sample_rate sample rate on audio output in Hz.
 @param phase initial phase of the oscillator in degree (0 to 360).
-----------------------------------------------------------------------------*/
static void set_mod_frequency(sinus_modulator *mod,
                              float freq, float sample_rate, float phase)
{
    fluid_real_t w = 2 * M_PI * freq / sample_rate; /* intial angle */

    mod->a1 = 2 * cos(w);
    mod->buffer2 = sin(2 * M_PI * phase / 360 - w); /* y(n-1) = sin(-intial angle) */
    mod->buffer1 = sin(2 * M_PI * phase / 360); /* y(n) = sin(initial phase) */
    mod->reset_buffer2 = sin(M_PI / 2.0 - w); /* reset value for PI/2 */
}

/*-----------------------------------------------------------------------------
 Gets current value of sinus modulator:
   y(n) = a1 . y(n-1)  -  y(n-2)
   out = a1 . buffer1  -  buffer2

 @param pointer on modulator structure.
 @return current value of the modulator sine wave.
-----------------------------------------------------------------------------*/
static FLUID_INLINE fluid_real_t get_mod_sinus(sinus_modulator *mod)
{
    fluid_real_t out;
    out = mod->a1 * mod->buffer1 - mod->buffer2;
    mod->buffer2 = mod->buffer1;

    if(out >= 1.0) /* reset in case of instability near PI/2 */
    {
        out = 1.0; /* forces output to the right value */
        mod->buffer2 = mod->reset_buffer2;
    }

    if(out <= -1.0) /* reset in case of instability near -PI/2 */
    {
        out = -1.0; /* forces output to the right value */
        mod->buffer2 = - mod->reset_buffer2;
    }

    mod->buffer1 = out;
    return  out;
}

/*-----------------------------------------------------------------------------
 Modulated delay line. The line is composed of:
 - the delay line with its damping low pass filter.
 - the sinusoidal modulator.
 - center output position modulated by the modulator.
 - variable rate control of center output position.
 - first order All-Pass interpolator.
-----------------------------------------------------------------------------*/
typedef struct
{
    /* delay line with damping low pass filter member */
    delay_line dl; /* delayed line */
    /*---------------------------*/
    /* Sinusoidal modulator member */
    sinus_modulator mod; /* sinus modulator */
    /*-------------------------*/
    /* center output position members */
    fluid_real_t  center_pos_mod; /* center output position modulated by modulator */
    int          mod_depth;   /* modulation depth (in samples) */
    /*-------------------------*/
    /* variable rate control of center output position */
    int index_rate;  /* index rate to know when to update center_pos_mod */
    int mod_rate;    /* rate at which center_pos_mod is updated */
    /*-------------------------*/
    /* first order All-Pass interpolator members */
    fluid_real_t  frac_pos_mod; /* fractional position part between samples) */
    /* previous value used when interpolating using fractional */
    fluid_real_t  buffer;
} mod_delay_line;


/*-----------------------------------------------------------------------------
 Modulated delay line initialization.

 Sets the length line ( alloc delay samples).
 Remark: the function sets the internal size accordling to the length delay_length.
 As the delay line is a modulated line, its internal size is augmented by mod_depth.
 The size is also augmented by INTERP_SAMPLES_NBR to take account of interpolation.

 @param mdl, pointer on modulated delay line.
 @param delay_length the length of the delay line in samples.
 @param mod_depth depth of the modulation in samples (amplitude of the sine wave).
 @param mod_rate the rate of the modulation in samples.
 @return FLUID_OK if success , FLUID_FAILED if memory error.

 Return FLUID_OK if success, FLUID_FAILED if memory error.
-----------------------------------------------------------------------------*/
static int set_mod_delay_line(mod_delay_line *mdl,
                              int delay_length,
                              int mod_depth,
                              int mod_rate
                             )
{
    /*-----------------------------------------------------------------------*/
    /* checks parameter */
    if(delay_length < 1)
    {
        return FLUID_FAILED;
    }

    /* limits mod_depth to the requested delay length */
    if(mod_depth >= delay_length)
    {
        FLUID_LOG(FLUID_INFO,
                  "fdn reverb: modulation depth has been limited");
        mod_depth = delay_length - 1;
    }

    mdl->mod_depth = mod_depth;
    /*-----------------------------------------------------------------------
     allocates delay_line and initialize members:
       - line, size, line_in, line_out...
    */
    {
        /* total size of the line:
        size = INTERP_SAMPLES_NBR + mod_depth + delay_length */
        mdl->dl.size = delay_length + mod_depth + INTERP_SAMPLES_NBR;
        mdl->dl.line = FLUID_ARRAY(fluid_real_t, mdl->dl.size);

        if(! mdl->dl.line)
        {
            return FLUID_FAILED;
        }

        clear_delay_line(&mdl->dl); /* clears the buffer */

        /* Initializes line_in to the start of the buffer */
        mdl->dl.line_in = 0;
        /*  Initializes line_out index INTERP_SAMPLES_NBR samples after line_in */
        /*  so that the delay between line_out and line_in is:
            mod_depth + delay_length */
        mdl->dl.line_out = mdl->dl.line_in + INTERP_SAMPLES_NBR;
    }

    /* Damping low pass filter -------------------*/
    mdl->dl.damping.buffer = 0;
    /*------------------------------------------------------------------------
     Initializes modulation members:
     - modulated center position: center_pos_mod
     - index rate to know when to update center_pos_mod:index_rate
     - modulation rate (the speed at which center_pos_mod is modulated: mod_rate
     - interpolator member: buffer, frac_pos_mod
     -------------------------------------------------------------------------*/
    /* Sets the modulation rate. This rate defines how often
     the  center position (center_pos_mod ) is modulated .
     The value is expressed in samples. The default value is 1 that means that
     center_pos_mod is updated at every sample.
     For example with a value of 2, the center position position will be
     updated only one time every 2 samples only.
    */
    mdl->mod_rate = 1; /* default modulation rate: every one sample */

    if(mod_rate > mdl->dl.size)
    {
        FLUID_LOG(FLUID_INFO,
                  "fdn reverb: modulation rate is out of range");
    }
    else
    {
        mdl->mod_rate = mod_rate;
    }

    /* Initializes the modulated center position (center_pos_mod) so that:
        - the delay between line_out and center_pos_mod is mod_depth.
        - the delay between center_pos_mod and line_in is delay_length.
     */
    mdl->center_pos_mod = (fluid_real_t) INTERP_SAMPLES_NBR + mod_depth;

    /* index rate to control when to update center_pos_mod */
    /* Important: must be set to get center_pos_mod immediatly used for the
       reading of first sample (see get_mod_delay()) */
    mdl->index_rate = mdl->mod_rate;

    /* initializes 1st order All-Pass interpolator members */
    mdl->buffer = 0;       /* previous delay sample value */
    mdl->frac_pos_mod = 0; /* fractional position (between consecutives sample) */
    return FLUID_OK;
}

/*-----------------------------------------------------------------------------
 Reads the sample value out of the modulated delay line.
 @param mdl, pointer on modulated delay line.
 @return the sample value.
-----------------------------------------------------------------------------*/
static FLUID_INLINE fluid_real_t get_mod_delay(mod_delay_line *mdl)
{
    fluid_real_t out_index;  /* new modulated index position */
    int int_out_index; /* integer part of out_index */
    fluid_real_t out; /* value to return */

    /* Checks if the modulator must be updated (every mod_rate samples). */
    /* Important: center_pos_mod must be used immediatly for the
       first sample. So, mdl->index_rate must be initialized
       to mdl->mod_rate (set_mod_delay_line())  */

    if(++mdl->index_rate >= mdl->mod_rate)
    {
        mdl->index_rate = 0;

        /* out_index = center position (center_pos_mod) + sinus waweform */
        out_index = mdl->center_pos_mod +
                    get_mod_sinus(&mdl->mod) * mdl->mod_depth;

        /* extracts integer part in int_out_index */
        if(out_index >= 0.0f)
        {
            int_out_index = (int)out_index; /* current integer part */

            /* forces read index (line_out)  with integer modulation value  */
            /* Boundary check and circular motion as needed */
            if((mdl->dl.line_out = int_out_index) >= mdl->dl.size)
            {
                mdl->dl.line_out -= mdl->dl.size;
            }
        }
        else /* negative */
        {
            int_out_index = (int)(out_index - 1); /* previous integer part */
            /* forces read index (line_out) with integer modulation value  */
            /* circular motion as needed */
            mdl->dl.line_out   = int_out_index + mdl->dl.size;
        }

        /* extracts fractionnal part. (it will be used when interpolating
          between line_out and line_out +1) and memorize it.
          Memorizing is necessary for modulation rate above 1 */
        mdl->frac_pos_mod = out_index - int_out_index;

        /* updates center position (center_pos_mod) to the next position
           specified by modulation rate */
        if((mdl->center_pos_mod += mdl->mod_rate) >= mdl->dl.size)
        {
            mdl->center_pos_mod -= mdl->dl.size;
        }
    }

    /*  First order all-pass interpolation ----------------------------------*/
    /* https://ccrma.stanford.edu/~jos/pasp/First_Order_Allpass_Interpolation.html */
    /*  begins interpolation: read current sample */
    out = mdl->dl.line[mdl->dl.line_out];

    /* updates line_out to the next sample.
       Boundary check and circular motion as needed */
    if(++mdl->dl.line_out >= mdl->dl.size)
    {
        mdl->dl.line_out -= mdl->dl.size;
    }

    /* Fractional interpolation beetween next sample (at next position) and
       previous output added to current sample.
    */
    out += mdl->frac_pos_mod * (mdl->dl.line[mdl->dl.line_out] - mdl->buffer);
    mdl->buffer = out; /* memorizes current output */
    return out;
}

/*-----------------------------------------------------------------------------
 Late structure
-----------------------------------------------------------------------------*/
struct _fluid_late
{
    fluid_real_t samplerate;       /* sample rate */
    /*----- High pass tone corrector -------------------------------------*/
    fluid_real_t tone_buffer;
    fluid_real_t b1, b2;
    /*----- Modulated delay lines lines ----------------------------------*/
    mod_delay_line mod_delay_lines[NBR_DELAYS];
    /*-----------------------------------------------------------------------*/
    /* Output coefficients for separate Left and right stereo outputs */
    fluid_real_t out_left_gain[NBR_DELAYS]; /* Left delay lines' output gains */
    fluid_real_t out_right_gain[NBR_DELAYS];/* Right delay lines' output gains*/
};

typedef struct _fluid_late   fluid_late;
/*-----------------------------------------------------------------------------
 fluidsynth reverb structure
-----------------------------------------------------------------------------*/
struct _fluid_revmodel_t
{
    /* reverb parameters */
    fluid_real_t roomsize; /* acting on reverb time */
    fluid_real_t damp; /* acting on frequency dependent reverb time */
    fluid_real_t level, wet1, wet2; /* output level */
    fluid_real_t width; /* width stereo separation */

    /* fdn reverberation structure */
    fluid_late  late;
};

/*-----------------------------------------------------------------------------
 Updates Reverb time and absorbent filters coefficients from parameters:

 @param late pointer on late structure.
 @param roomsize (0 to 1): acting on reverb time.
 @param damping (0 to 1): acting on absorbent damping filter.

 Design formulas:
 https://ccrma.stanford.edu/~jos/Reverb/First_Order_Delay_Filter_Design.html
 https://ccrma.stanford.edu/~jos/Reverb/Tonal_Correction_Filter.html
-----------------------------------------------------------------------------*/
static void update_rev_time_damping(fluid_late *late,
                                    fluid_real_t roomsize, fluid_real_t damp)
{
    int i;
    fluid_real_t sample_period = 1 / late->samplerate; /* Sampling period */
    fluid_real_t dc_rev_time;       /* Reverb time at 0 Hz (in seconds) */

    fluid_real_t alpha;

    /*--------------------------------------------
         Computes dc_rev_time and alpha
    ----------------------------------------------*/
    {
        fluid_real_t gi_tmp, ai_tmp;
#ifdef ROOMSIZE_RESPONSE_LINEAR
        /*   roomsize parameter behave linearly:
         *   - roomsize (0 to 1) controls reverb time linearly  (0.7 to 10 s).
         *   This linear response is convenient when using GUI controls.
        */
        /*-----------------------------------------
              Computes dc_rev_time
        ------------------------------------------*/
        dc_rev_time = GET_DC_REV_TIME(roomsize);
        /* computes gi_tmp from dc_rev_time using relation E2 */
        gi_tmp = (fluid_real_t) pow(10, -3 * delay_length[NBR_DELAYS - 1] *
                                    sample_period / dc_rev_time); /* E2 */
#else
        /*   roomsize parameters have the same response that Freeverb, that is:
         *   - roomsize (0 to 1) controls concave reverb time (0.7 to 10 s).
        */
        {
            /*-----------------------------------------
             Computes dc_rev_time
            ------------------------------------------*/
            fluid_real_t gi_min, gi_max;
            /* values gi_min et gi_max are computed using E2 for the line with
              maximum delay */
            gi_max = (fluid_real_t)pow(10, -3 * delay_length[NBR_DELAYS - 1] *
                                       sample_period / MAX_DC_REV_TIME); /* E2 */
            gi_min = (fluid_real_t)pow(10, -3 * delay_length[NBR_DELAYS - 1] *
                                       sample_period / MIN_DC_REV_TIME); /* E2 */
            /* gi = f(roomsize, gi_max, gi_min) */
            gi_tmp = gi_min + roomsize * (gi_max - gi_min);
            /* Computes T60DC from gi using inverse of relation E2.*/
            dc_rev_time = -3 * delay_length[NBR_DELAYS - 1] * sample_period / log10(gi_tmp);
        }
#endif /* ROOMSIZE_RESPONSE_LINEAR */
        /*--------------------------------------------
            Computes alpha
        ----------------------------------------------*/
        /* Computes alpha from damp,ai_tmp,gi_tmp using relation R */
        /* - damp (0 to 1) controls concave reverb time for fs/2 frequency (T60DC to 0) */
        ai_tmp = 1.0 * damp;
        alpha = sqrt(1 / (1 - ai_tmp / (20 * log10(gi_tmp) * log(10) / 80))); /* R */
    }

    /* updates tone corrector coefficients b1,b2 from alpha */
    {
        /*
         Beta = (1 - alpha)  / (1 + alpha)
         b1 = 1/(1-beta)
         b2 = beta * b1
        */
        fluid_real_t beta = (1 - alpha)  / (1 + alpha);
        late->b1 = 1 / (1 - beta);
        late->b2 = beta * late->b1;
        late->tone_buffer = 0.0f;
    }

    /* updates damping  coefficients of all lines (gi , ai) from dc_rev_time, alpha */
    for(i = 0; i < NBR_DELAYS; i++)
    {
        /* iir low pass filter gain */
        fluid_real_t gi = (fluid_real_t)pow(10, -3 * delay_length[i] *
                                            sample_period / dc_rev_time);

        /* iir low pass filter feedback gain */
        fluid_real_t ai = (fluid_real_t)(20 * log10(gi) * log(10) / 80 *
                                         (1 -  1 / pow(alpha, 2)));
        /* b0 = gi * (1 - ai),  a1 = - ai */
        set_fdn_delay_lpf(&late->mod_delay_lines[i].dl.damping,
                          gi * (1 - ai), -ai);
    }
}

/*-----------------------------------------------------------------------------
 Updates stereo coefficients
 @param late pointer on late structure
 @param wet level integrated in stereo coefficients.
-----------------------------------------------------------------------------*/
static void update_stereo_coefficient(fluid_late *late, fluid_real_t wet1)
{
    int i;

    for(i = 0; i < NBR_DELAYS; i++)
    {
        /*  delay lines output gains vectors Left and Right

                           L    R
                       0 | 1    1|
                       1 |-1    1|
                       2 | 1   -1|
                       3 |-1   -1|

                       4 | 1    1|
                       5 |-1    1|
         stereo gain = 6 | 1   -1|
                       7 |-1   -1|

                       8 | 1    1|
                       9 |-1    1|
                       10| 1   -1|
                       11|-1   -1|
        */

        late->out_left_gain[i] = wet1;

        /*  Sets Left coefficients first */
        if(i % 2) /* Left is 1,-1, 1,-1, 1,-1,.... */
        {
            late->out_left_gain[i] *= -1;
        }

        /* Now sets right gain as function of Left */
        /* for right line 1,2,5,6,9,10,13,14, right = - left */
        if((i == 1) || (i == 2) || (i == 5) || (i == 6) || (i == 9) || (i == 10) || (i == 13) || (i == 14))
        {
            /* Right is reverse of Left */
            late->out_right_gain[i] = -1 * late->out_left_gain[i];
        }
        else /* for Right : line 0,3,4,7,8,11,12,15 */
        {
            /* Right is same as Left */
            late->out_right_gain[i] = late->out_left_gain[i];
        }
    }
}

/*-----------------------------------------------------------------------------
 fluid_late destructor.
 @param late pointer on late structure.
-----------------------------------------------------------------------------*/
static void delete_fluid_rev_late(fluid_late *late)
{
    int i;
    fluid_return_if_fail(late != NULL);

    /* free the delay lines */
    for(i = 0; i < NBR_DELAYS; i++)
    {
        FLUID_FREE(late->mod_delay_lines[i].dl.line);
    }
}

/*-----------------------------------------------------------------------------
 Creates the fdn reverb.
 @param late, pointer on the fnd late reverb to initialize.
 @param sample_rate the sample rate.
 @return FLUID_OK if success, FLUID_FAILED otherwise.
-----------------------------------------------------------------------------*/
static int create_fluid_rev_late(fluid_late *late, fluid_real_t sample_rate)
{
    int result; /* return value */
    int i;

    FLUID_MEMSET(late, 0,  sizeof(fluid_late));

    late->samplerate = sample_rate;

    /*--------------------------------------------------------------------------
      First initialize the modulated delay lines
    */

    for(i = 0; i < NBR_DELAYS; i++)
    {
        /* sets local delay lines's parameters */
        result = set_mod_delay_line(&late->mod_delay_lines[i],
                                    delay_length[i], MOD_DEPTH, MOD_RATE);

        if(result == FLUID_FAILED)
        {
            return FLUID_FAILED;
        }

        /* Sets local Modulators parameters: frequency and phase
         Each modulateur are shifted of MOD_PHASE degree
        */
        set_mod_frequency(&late->mod_delay_lines[i].mod,
                          MOD_FREQ * MOD_RATE,
                          sample_rate,
                          (float)(MOD_PHASE * i));
    }

    /*-----------------------------------------------------------------------*/
    update_stereo_coefficient(late, 1.0f);
    return FLUID_OK;
}

/*
 Clears the delay lines.

 @param rev pointer on the reverb.
*/
static void
fluid_revmodel_init(fluid_revmodel_t *rev)
{
    int i;

    /* clears all the delay lines */
    for(i = 0; i < NBR_DELAYS; i ++)
    {
        clear_delay_line(&rev->late.mod_delay_lines[i].dl);
    }
}


/*
 updates internal parameters.

 @param rev pointer on the reverb.
*/
static void
fluid_revmodel_update(fluid_revmodel_t *rev)
{
    /* Recalculate internal values after parameters change */

    /* The stereo amplitude equation (wet1 and wet2 below) have a
    tendency to produce high amplitude with high width values ( 1 < width < 100).
    This results in an unwanted noisy output clipped by the audio card.
    To avoid this dependency, we divide by (1 + rev->width * SCALE_WET_WIDTH)
    Actually, with a SCALE_WET_WIDTH of 0.2, (regardless of level setting),
    the output amplitude (wet) seems rather independent of width setting */
    fluid_real_t wet = (rev->level * SCALE_WET) /
                       (1.0f + rev->width * SCALE_WET_WIDTH);

    /* wet1 and wet2 are used by the stereo effect controled by the width setting
    for producing a stereo ouptput from a monophonic reverb signal.
    Please see the note above about a side effect tendency */

    rev->wet1 = wet * (rev->width / 2.0f + 0.5f);
    rev->wet2 = wet * ((1.0f - rev->width) / 2.0f);

    /* integrates wet1 in stereo coefficient (this will save one multiply) */
    update_stereo_coefficient(&rev->late, rev->wet1);

    if(rev->wet1 > 0.0)
    {
        rev->wet2 /= rev->wet1;
    }

    /* Reverberation time and damping */
    update_rev_time_damping(&rev->late, rev->roomsize, rev->damp);
}

/*----------------------------------------------------------------------------
                            Reverb API
-----------------------------------------------------------------------------*/

/*
* Creates a reverb.
* @param sample_rate sample rate in Hz.
* @return pointer on the new reverb or NULL if memory error.
* Reverb API.
*/
fluid_revmodel_t *
new_fluid_revmodel(fluid_real_t sample_rate)
{
    fluid_revmodel_t *rev;
    rev = FLUID_NEW(fluid_revmodel_t);

    if(rev == NULL)
    {
        return NULL;
    }

    /* create fdn reverb */
    if(create_fluid_rev_late(&rev->late, sample_rate) != FLUID_OK)
    {
        delete_fluid_revmodel(rev);
        return NULL;
    }

    return rev;
}

/*
* free the reverb.
* @param pointer on rev to free.
* Reverb API.
*/
void
delete_fluid_revmodel(fluid_revmodel_t *rev)
{
    fluid_return_if_fail(rev != NULL);
    delete_fluid_rev_late(&rev->late);
    FLUID_FREE(rev);
}

/*
* Sets one or more reverb parameters.
* @param rev Reverb instance.
* @param set One or more flags from #fluid_revmodel_set_t indicating what
*   parameters to set (#FLUID_REVMODEL_SET_ALL to set all parameters).
* @param roomsize Reverb room size.
* @param damping Reverb damping.
* @param width Reverb width.
* @param level Reverb level.
*
* Reverb API.
*/
void
fluid_revmodel_set(fluid_revmodel_t *rev, int set, fluid_real_t roomsize,
                   fluid_real_t damping, fluid_real_t width, fluid_real_t level)
{
    /*-----------------------------------*/
    if(set & FLUID_REVMODEL_SET_ROOMSIZE)
    {
        fluid_clip(roomsize, 0.0f, 1.0f);
        rev->roomsize = roomsize;
    }

    /*-----------------------------------*/
    if(set & FLUID_REVMODEL_SET_DAMPING)
    {
        fluid_clip(damping, 0.0f, 1.0f);
        rev->damp = damping;
    }

    /*-----------------------------------*/
    if(set & FLUID_REVMODEL_SET_WIDTH)
    {
        rev->width = width;
    }

    /*-----------------------------------*/
    if(set & FLUID_REVMODEL_SET_LEVEL)
    {
        fluid_clip(level, 0.0f, 1.0f);
        rev->level = level;
    }

    /* updates internal parameters */
    fluid_revmodel_update(rev);
}

/*
* Applies a sample rate change on the reverb.
* @param rev the reverb.
* @param sample_rate new sample rate value.
*
* Reverb API.
*/
void
fluid_revmodel_samplerate_change(fluid_revmodel_t *rev, fluid_real_t sample_rate)
{
    int i;

    /* updates modulator frequency according to sample rate change */
    for(i = 0; i < NBR_DELAYS; i++)
    {
        set_mod_frequency(&rev->late.mod_delay_lines[i].mod,
                          MOD_FREQ * MOD_RATE,
                          sample_rate,
                          (float)(MOD_PHASE * i));
    }

    /* updates damping filter coefficients according to sample rate change */
    rev->late.samplerate = sample_rate;
    update_rev_time_damping(&rev->late, rev->roomsize, rev->damp);

    /* clears all delay lines */
    fluid_revmodel_init(rev);
}

/*
* Damps the reverb by clearing the delay lines.
* @param rev the reverb.
*
* Reverb API.
*/
void
fluid_revmodel_reset(fluid_revmodel_t *rev)
{
    fluid_revmodel_init(rev);
}

/*-----------------------------------------------------------------------------
* fdn reverb process replace.
* @param rev pointer on reverb.
* @param in monophonic buffer input (FLUID_BUFSIZE sample).
* @param left_out stereo left processed output (FLUID_BUFSIZE sample).
* @param right_out stereo right processed output (FLUID_BUFSIZE sample).
*
* The processed reverb is replacing anything there in out.
* Reverb API.
-----------------------------------------------------------------------------*/
void
fluid_revmodel_processreplace(fluid_revmodel_t *rev, const fluid_real_t *in,
                              fluid_real_t *left_out, fluid_real_t *right_out)
{
    int i, k;

    fluid_real_t xn;                   /* mono input x(n) */
    fluid_real_t out_tone_filter;      /* tone corrector output */
    fluid_real_t out_left, out_right;  /* output stereo Left  and Right  */
    fluid_real_t matrix_factor;        /* partial matrix computation */
    fluid_real_t delay_out_s;          /* sample */
    fluid_real_t delay_out[NBR_DELAYS]; /* Line output + damper output */

    for(k = 0; k < FLUID_BUFSIZE; k++)
    {
        /* stereo output */
        out_left = out_right = 0;

#ifdef DENORMALISING
        /* Input is adjusted by DC_OFFSET. */
        xn = (in[k]) * FIXED_GAIN + DC_OFFSET;
#else
        xn = (in[k]) * FIXED_GAIN;
#endif

        /*--------------------------------------------------------------------
         tone correction.
        */
        out_tone_filter = xn * rev->late.b1 - rev->late.b2 * rev->late.tone_buffer;
        rev->late.tone_buffer = xn;
        xn = out_tone_filter;
        /*--------------------------------------------------------------------
         process  feedback delayed network:
          - xn is the input signal.
          - before inserting in the line input we first we get the delay lines
            output, filter them and compute output in delay_out[].
          - also matrix_factor is computed (to simplify further matrix product)
        ---------------------------------------------------------------------*/
        /* We begin with the modulated output delay line + damping filter */
        matrix_factor = 0;

        for(i = 0; i < NBR_DELAYS; i++)
        {
            mod_delay_line *mdl = &rev->late.mod_delay_lines[i];
            /* get current modulated output */
            delay_out_s = get_mod_delay(mdl);

            /* process low pass damping filter
              (input:delay_out_s, output:delay_out_s) */
            process_damping_filter(delay_out_s, delay_out_s, mdl);

            /* Result in delay_out[], and matrix_factor.
               These wil be use later during input line process */
            delay_out[i] = delay_out_s;   /* result in delay_out[] */
            matrix_factor += delay_out_s; /* result in matrix_factor */

            /* Process stereo output */
            /* stereo left = left + out_left_gain * delay_out */
            out_left += rev->late.out_left_gain[i] * delay_out_s;
            /* stereo right= right+ out_right_gain * delay_out */
            out_right += rev->late.out_right_gain[i] * delay_out_s;
        }

        /* now we process the input delay line.Each input is a combination of
           - xn: input signal
           - delay_out[] the output of a delay line given by a permutation matrix P
           - and matrix_factor.
          This computes: in_delay_line = xn + (delay_out[] * matrix A) with
          an algorithm equivalent but faster than using a product with matrix A.
        */
        /* matrix_factor = output sum * (-2.0)/N  */
        matrix_factor *= FDN_MATRIX_FACTOR;
        matrix_factor += xn; /* adds reverb input signal */

        for(i = 1; i < NBR_DELAYS; i++)
        {
            /* delay_in[i-1] = delay_out[i] + matrix_factor */
            delay_line *dl = &rev->late.mod_delay_lines[i - 1].dl;
            push_in_delay_line(dl, delay_out[i] + matrix_factor);
        }

        /* last line input (NB_DELAY-1) */
        /* delay_in[0] = delay_out[NB_DELAY -1] + matrix_factor */
        {
            delay_line *dl = &rev->late.mod_delay_lines[NBR_DELAYS - 1].dl;
            push_in_delay_line(dl, delay_out[0] + matrix_factor);
        }

        /*-------------------------------------------------------------------*/
#ifdef DENORMALISING
        /* Removes the DC offset */
        out_left -= DC_OFFSET;
        out_right -= DC_OFFSET;
#endif

        /* Calculates stereo output REPLACING anything already there: */
        /*
            left_out[k]  = out_left * rev->wet1 + out_right * rev->wet2;
            right_out[k] = out_right * rev->wet1 + out_left * rev->wet2;

            As wet1 is integrated in stereo coefficient wet 1 is now
            integrated in out_left and out_right we simplify previous
            relation by suppression of one multiply as this:

            left_out[k]  = out_left  + out_right * rev->wet2;
            right_out[k] = out_right + out_left * rev->wet2;
        */
        left_out[k]  = out_left  + out_right * rev->wet2;
        right_out[k] = out_right + out_left * rev->wet2;
    }
}


/*-----------------------------------------------------------------------------
* fdn reverb process mix.
* @param rev pointer on reverb.
* @param in monophonic buffer input (FLUID_BUFSIZE samples).
* @param left_out stereo left processed output (FLUID_BUFSIZE samples).
* @param right_out stereo right processed output (FLUID_BUFSIZE samples).
*
* The processed reverb is mixed in out with samples already there in out.
* Reverb API.
-----------------------------------------------------------------------------*/
void fluid_revmodel_processmix(fluid_revmodel_t *rev, const fluid_real_t *in,
                               fluid_real_t *left_out, fluid_real_t *right_out)
{
    int i, k;

    fluid_real_t xn;                   /* mono input x(n) */
    fluid_real_t out_tone_filter;      /* tone corrector output */
    fluid_real_t out_left, out_right;  /* output stereo Left  and Right  */
    fluid_real_t matrix_factor;        /* partial matrix term */
    fluid_real_t delay_out_s;          /* sample */
    fluid_real_t delay_out[NBR_DELAYS]; /* Line output + damper output */

    for(k = 0; k < FLUID_BUFSIZE; k++)
    {
        /* stereo output */
        out_left = out_right = 0;
#ifdef DENORMALISING
        /* Input is adjusted by DC_OFFSET. */
        xn = (in[k]) * FIXED_GAIN + DC_OFFSET;
#else
        xn = (in[k]) * FIXED_GAIN;
#endif

        /*--------------------------------------------------------------------
         tone correction
        */
        out_tone_filter = xn * rev->late.b1 - rev->late.b2 * rev->late.tone_buffer;
        rev->late.tone_buffer = xn;
        xn = out_tone_filter;
        /*--------------------------------------------------------------------
         process feedback delayed network:
          - xn is the input signal.
          - before inserting in the line input we first we get the delay lines
            output, filter them and compute output in local delay_out[].
          - also matrix_factor is computed (to simplify further matrix product).
        ---------------------------------------------------------------------*/
        /* We begin with the modulated output delay line + damping filter */
        matrix_factor = 0;

        for(i = 0; i < NBR_DELAYS; i++)
        {
            mod_delay_line *mdl = &rev->late.mod_delay_lines[i];
            /* get current modulated output */
            delay_out_s = get_mod_delay(mdl);

            /* process low pass damping filter
              (input:delay_out_s, output:delay_out_s) */
            process_damping_filter(delay_out_s, delay_out_s, mdl);

            /* Result in delay_out[], and matrix_factor.
               These wil be use later during input line process */
            delay_out[i] = delay_out_s;   /* result in delay_out[] */
            matrix_factor += delay_out_s; /* result in matrix_factor */

            /* Process stereo output */
            /* stereo left = left + out_left_gain * delay_out */
            out_left += rev->late.out_left_gain[i] * delay_out_s;
            /* stereo right= right+ out_right_gain * delay_out */
            out_right += rev->late.out_right_gain[i] * delay_out_s;
        }

        /* now we process the input delay line. Each input is a combination of:
           - xn: input signal
           - delay_out[] the output of a delay line given by a permutation matrix P
           - and matrix_factor.
          This computes: in_delay_line = xn + (delay_out[] * matrix A) with
          an algorithm equivalent but faster than using a product with matrix A.
        */
        /* matrix_factor = output sum * (-2.0)/N  */
        matrix_factor *= FDN_MATRIX_FACTOR;
        matrix_factor += xn; /* adds reverb input signal */

        for(i = 1; i < NBR_DELAYS; i++)
        {
            /* delay_in[i-1] = delay_out[i] + matrix_factor */
            delay_line *dl = &rev->late.mod_delay_lines[i - 1].dl;
            push_in_delay_line(dl, delay_out[i] + matrix_factor);
        }

        /* last line input (NB_DELAY-1) */
        /* delay_in[0] = delay_out[NB_DELAY -1] + matrix_factor */
        {
            delay_line *dl = &rev->late.mod_delay_lines[NBR_DELAYS - 1].dl;
            push_in_delay_line(dl, delay_out[0] + matrix_factor);
        }

        /*-------------------------------------------------------------------*/
#ifdef DENORMALISING
        /* Removes the DC offset */
        out_left -= DC_OFFSET;
        out_right -= DC_OFFSET;
#endif
        /* Calculates stereo output MIXING anything already there: */
        /*
            left_out[k]  += out_left * rev->wet1 + out_right * rev->wet2;
            right_out[k] += out_right * rev->wet1 + out_left * rev->wet2;

            As wet1 is integrated in stereo coefficient wet 1 is now
            integrated in out_left and out_right we simplify previous
            relation by suppression of one multiply as this:

            left_out[k]  += out_left  + out_right * rev->wet2;
            right_out[k] += out_right + out_left * rev->wet2;
        */
        left_out[k]  += out_left  + out_right * rev->wet2;
        right_out[k] += out_right + out_left * rev->wet2;
    }
}
