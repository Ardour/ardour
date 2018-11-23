/* FluidSynth - A Software Synthesizer
 *
 * Copyright (C) 2003  Peter Hanappe, Markus Nentwig and others.
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
 */

/*
  based on a chrous implementation made by Juergen Mueller And Sundry Contributors in 1998

  CHANGES

  - Adapted for fluidsynth, Peter Hanappe, March 2002

  - Variable delay line implementation using bandlimited
    interpolation, code reorganization: Markus Nentwig May 2002

 */


/*
 * 	Chorus effect.
 *
 * Flow diagram scheme for n delays ( 1 <= n <= MAX_CHORUS ):
 *
 *        * gain-in                                           ___
 * ibuff -----+--------------------------------------------->|   |
 *            |      _________                               |   |
 *            |     |         |                   * level 1  |   |
 *            +---->| delay 1 |----------------------------->|   |
 *            |     |_________|                              |   |
 *            |        /|\                                   |   |
 *            :         |                                    |   |
 *            : +-----------------+   +--------------+       | + |
 *            : | Delay control 1 |<--| mod. speed 1 |       |   |
 *            : +-----------------+   +--------------+       |   |
 *            |      _________                               |   |
 *            |     |         |                   * level n  |   |
 *            +---->| delay n |----------------------------->|   |
 *                  |_________|                              |   |
 *                     /|\                                   |___|
 *                      |                                      |
 *              +-----------------+   +--------------+         | * gain-out
 *              | Delay control n |<--| mod. speed n |         |
 *              +-----------------+   +--------------+         +----->obuff
 *
 *
 * The delay i is controlled by a sine or triangle modulation i ( 1 <= i <= n).
 *
 * The delay of each block is modulated between 0..depth ms
 *
 */


/* Variable delay line implementation
 * ==================================
 *
 * The modulated delay needs the value of the delayed signal between
 * samples.  A lowpass filter is used to obtain intermediate values
 * between samples (bandlimited interpolation).  The sample pulse
 * train is convoluted with the impulse response of the low pass
 * filter (sinc function).  To make it work with a small number of
 * samples, the sinc function is windowed (Hamming window).
 *
 */

#include "fluid_chorus.h"
#include "fluid_sys.h"

#define MAX_CHORUS	99
#define MAX_DELAY	100
#define MAX_DEPTH	10
#define MIN_SPEED_HZ	0.29
#define MAX_SPEED_HZ    5

/* Length of one delay line in samples:
 * Set through MAX_SAMPLES_LN2.
 * For example:
 * MAX_SAMPLES_LN2=12
 * => MAX_SAMPLES=pow(2,12-1)=2048
 * => MAX_SAMPLES_ANDMASK=2047
 */
#define MAX_SAMPLES_LN2 12

#define MAX_SAMPLES (1 << (MAX_SAMPLES_LN2-1))
#define MAX_SAMPLES_ANDMASK (MAX_SAMPLES-1)


/* Interpolate how many steps between samples? Must be power of two
   For example: 8 => use a resolution of 256 steps between any two
   samples
*/
#define INTERPOLATION_SUBSAMPLES_LN2 8
#define INTERPOLATION_SUBSAMPLES (1 << (INTERPOLATION_SUBSAMPLES_LN2-1))
#define INTERPOLATION_SUBSAMPLES_ANDMASK (INTERPOLATION_SUBSAMPLES-1)

/* Use how many samples for interpolation? Must be odd.  '7' sounds
   relatively clean, when listening to the modulated delay signal
   alone.  For a demo on aliasing try '1' With '3', the aliasing is
   still quite pronounced for some input frequencies
*/
#define INTERPOLATION_SAMPLES 5

/* Private data for SKEL file */
struct _fluid_chorus_t
{
    int type;
    fluid_real_t depth_ms;
    fluid_real_t level;
    fluid_real_t speed_Hz;
    int number_blocks;

    fluid_real_t *chorusbuf;
    int counter;
    long phase[MAX_CHORUS];
    long modulation_period_samples;
    int *lookup_tab;
    fluid_real_t sample_rate;

    /* sinc lookup table */
    fluid_real_t sinc_table[INTERPOLATION_SAMPLES][INTERPOLATION_SUBSAMPLES];
};

static void fluid_chorus_triangle(int *buf, int len, int depth);
static void fluid_chorus_sine(int *buf, int len, int depth);


fluid_chorus_t *
new_fluid_chorus(fluid_real_t sample_rate)
{
    int i;
    int ii;
    fluid_chorus_t *chorus;

    chorus = FLUID_NEW(fluid_chorus_t);

    if(chorus == NULL)
    {
        FLUID_LOG(FLUID_PANIC, "chorus: Out of memory");
        return NULL;
    }

    FLUID_MEMSET(chorus, 0, sizeof(fluid_chorus_t));

    chorus->sample_rate = sample_rate;

    /* Lookup table for the SI function (impulse response of an ideal low pass) */

    /* i: Offset in terms of whole samples */
    for(i = 0; i < INTERPOLATION_SAMPLES; i++)
    {

        /* ii: Offset in terms of fractional samples ('subsamples') */
        for(ii = 0; ii < INTERPOLATION_SUBSAMPLES; ii++)
        {
            /* Move the origin into the center of the table */
            double i_shifted = ((double) i - ((double) INTERPOLATION_SAMPLES) / 2.
                                + (double) ii / (double) INTERPOLATION_SUBSAMPLES);

            if(fabs(i_shifted) < 0.000001)
            {
                /* sinc(0) cannot be calculated straightforward (limit needed
                   for 0/0) */
                chorus->sinc_table[i][ii] = (fluid_real_t)1.;

            }
            else
            {
                chorus->sinc_table[i][ii] = (fluid_real_t)sin(i_shifted * M_PI) / (M_PI * i_shifted);
                /* Hamming window */
                chorus->sinc_table[i][ii] *= (fluid_real_t)0.5 * (1.0 + cos(2.0 * M_PI * i_shifted / (fluid_real_t)INTERPOLATION_SAMPLES));
            };
        };
    };

    /* allocate lookup tables */
    chorus->lookup_tab = FLUID_ARRAY(int, (int)(chorus->sample_rate / MIN_SPEED_HZ));

    if(chorus->lookup_tab == NULL)
    {
        FLUID_LOG(FLUID_PANIC, "chorus: Out of memory");
        goto error_recovery;
    }

    /* allocate sample buffer */

    chorus->chorusbuf = FLUID_ARRAY(fluid_real_t, MAX_SAMPLES);

    if(chorus->chorusbuf == NULL)
    {
        FLUID_LOG(FLUID_PANIC, "chorus: Out of memory");
        goto error_recovery;
    }

    if(fluid_chorus_init(chorus) != FLUID_OK)
    {
        goto error_recovery;
    };

    return chorus;

error_recovery:
    delete_fluid_chorus(chorus);

    return NULL;
}

void
delete_fluid_chorus(fluid_chorus_t *chorus)
{
    fluid_return_if_fail(chorus != NULL);

    FLUID_FREE(chorus->chorusbuf);
    FLUID_FREE(chorus->lookup_tab);
    FLUID_FREE(chorus);
}

int
fluid_chorus_init(fluid_chorus_t *chorus)
{
    int i;

    for(i = 0; i < MAX_SAMPLES; i++)
    {
        chorus->chorusbuf[i] = 0.0;
    }

    return FLUID_OK;
}

void
fluid_chorus_reset(fluid_chorus_t *chorus)
{
    fluid_chorus_init(chorus);
}

/**
 * Set one or more chorus parameters.
 * @param chorus Chorus instance
 * @param set Flags indicating which chorus parameters to set (#fluid_chorus_set_t)
 * @param nr Chorus voice count (0-99, CPU time consumption proportional to
 *   this value)
 * @param level Chorus level (0.0-10.0)
 * @param speed Chorus speed in Hz (0.29-5.0)
 * @param depth_ms Chorus depth (max value depends on synth sample rate,
 *   0.0-21.0 is safe for sample rate values up to 96KHz)
 * @param type Chorus waveform type (#fluid_chorus_mod)
 */
void
fluid_chorus_set(fluid_chorus_t *chorus, int set, int nr, fluid_real_t level,
                 fluid_real_t speed, fluid_real_t depth_ms, int type)
{
    int modulation_depth_samples;
    int i;

    if(set & FLUID_CHORUS_SET_NR)
    {
        chorus->number_blocks = nr;
    }

    if(set & FLUID_CHORUS_SET_LEVEL)
    {
        chorus->level = level;
    }

    if(set & FLUID_CHORUS_SET_SPEED)
    {
        chorus->speed_Hz = speed;
    }

    if(set & FLUID_CHORUS_SET_DEPTH)
    {
        chorus->depth_ms = depth_ms;
    }

    if(set & FLUID_CHORUS_SET_TYPE)
    {
        chorus->type = type;
    }

    if(chorus->number_blocks < 0)
    {
        FLUID_LOG(FLUID_WARN, "chorus: number blocks must be >=0! Setting value to 0.");
        chorus->number_blocks = 0;
    }
    else if(chorus->number_blocks > MAX_CHORUS)
    {
        FLUID_LOG(FLUID_WARN, "chorus: number blocks larger than max. allowed! Setting value to %d.",
                  MAX_CHORUS);
        chorus->number_blocks = MAX_CHORUS;
    }

    if(chorus->speed_Hz < MIN_SPEED_HZ)
    {
        FLUID_LOG(FLUID_WARN, "chorus: speed is too low (min %f)! Setting value to min.",
                  (double) MIN_SPEED_HZ);
        chorus->speed_Hz = MIN_SPEED_HZ;
    }
    else if(chorus->speed_Hz > MAX_SPEED_HZ)
    {
        FLUID_LOG(FLUID_WARN, "chorus: speed must be below %f Hz! Setting value to max.",
                  (double) MAX_SPEED_HZ);
        chorus->speed_Hz = MAX_SPEED_HZ;
    }

    if(chorus->depth_ms < 0.0)
    {
        FLUID_LOG(FLUID_WARN, "chorus: depth must be positive! Setting value to 0.");
        chorus->depth_ms = 0.0;
    }

    /* Depth: Check for too high value through modulation_depth_samples. */

    if(chorus->level < 0.0)
    {
        FLUID_LOG(FLUID_WARN, "chorus: level must be positive! Setting value to 0.");
        chorus->level = 0.0;
    }
    else if(chorus->level > 10)
    {
        FLUID_LOG(FLUID_WARN, "chorus: level must be < 10. A reasonable level is << 1! "
                  "Setting it to 0.1.");
        chorus->level = 0.1;
    }

    /* The modulating LFO goes through a full period every x samples: */
    chorus->modulation_period_samples = chorus->sample_rate / chorus->speed_Hz;

    /* The variation in delay time is x: */
    modulation_depth_samples = (int)
                               (chorus->depth_ms / 1000.0  /* convert modulation depth in ms to s*/
                                * chorus->sample_rate);

    if(modulation_depth_samples > MAX_SAMPLES)
    {
        FLUID_LOG(FLUID_WARN, "chorus: Too high depth. Setting it to max (%d).", MAX_SAMPLES);
        modulation_depth_samples = MAX_SAMPLES;
        // set depth to maximum to avoid spamming console with above warning
        chorus->depth_ms = (modulation_depth_samples * 1000) / chorus->sample_rate;
    }

    /* initialize LFO table */
    switch(chorus->type)
    {
    default:
        FLUID_LOG(FLUID_WARN, "chorus: Unknown modulation type. Using sinewave.");
        chorus->type = FLUID_CHORUS_MOD_SINE;
        /* fall-through */
        
    case FLUID_CHORUS_MOD_SINE:
        fluid_chorus_sine(chorus->lookup_tab, chorus->modulation_period_samples,
                          modulation_depth_samples);
        break;

    case FLUID_CHORUS_MOD_TRIANGLE:
        fluid_chorus_triangle(chorus->lookup_tab, chorus->modulation_period_samples,
                              modulation_depth_samples);
        break;
    }

    for(i = 0; i < chorus->number_blocks; i++)
    {
        /* Set the phase of the chorus blocks equally spaced */
        chorus->phase[i] = (int)((double) chorus->modulation_period_samples
                                 * (double) i / (double) chorus->number_blocks);
    }

    /* Start of the circular buffer */
    chorus->counter = 0;
}


void fluid_chorus_processmix(fluid_chorus_t *chorus, const fluid_real_t *in,
                             fluid_real_t *left_out, fluid_real_t *right_out)
{
    int sample_index;
    int i;
    fluid_real_t d_in, d_out;

    for(sample_index = 0; sample_index < FLUID_BUFSIZE; sample_index++)
    {

        d_in = in[sample_index];
        d_out = 0.0f;

# if 0
        /* Debug: Listen to the chorus signal only */
        left_out[sample_index] = 0;
        right_out[sample_index] = 0;
#endif

        /* Write the current sample into the circular buffer */
        chorus->chorusbuf[chorus->counter] = d_in;

        for(i = 0; i < chorus->number_blocks; i++)
        {
            int ii;
            /* Calculate the delay in subsamples for the delay line of chorus block nr. */

            /* The value in the lookup table is so, that this expression
             * will always be positive.  It will always include a number of
             * full periods of MAX_SAMPLES*INTERPOLATION_SUBSAMPLES to
             * remain positive at all times. */
            int pos_subsamples = (INTERPOLATION_SUBSAMPLES * chorus->counter
                                  - chorus->lookup_tab[chorus->phase[i]]);

            int pos_samples = pos_subsamples / INTERPOLATION_SUBSAMPLES;

            /* modulo divide by INTERPOLATION_SUBSAMPLES */
            pos_subsamples &= INTERPOLATION_SUBSAMPLES_ANDMASK;

            for(ii = 0; ii < INTERPOLATION_SAMPLES; ii++)
            {
                /* Add the delayed signal to the chorus sum d_out Note: The
                 * delay in the delay line moves backwards for increasing
                 * delay!*/

                /* The & in chorusbuf[...] is equivalent to a division modulo
                   MAX_SAMPLES, only faster. */
                d_out += chorus->chorusbuf[pos_samples & MAX_SAMPLES_ANDMASK]
                         * chorus->sinc_table[ii][pos_subsamples];

                pos_samples--;
            };

            /* Cycle the phase of the modulating LFO */
            chorus->phase[i]++;

            chorus->phase[i] %= (chorus->modulation_period_samples);
        } /* foreach chorus block */

        d_out *= chorus->level;

        /* Add the chorus sum d_out to output */
        left_out[sample_index] += d_out;
        right_out[sample_index] += d_out;

        /* Move forward in circular buffer */
        chorus->counter++;
        chorus->counter %= MAX_SAMPLES;

    } /* foreach sample */
}

/* Duplication of code ... (replaces sample data instead of mixing) */
void fluid_chorus_processreplace(fluid_chorus_t *chorus, const fluid_real_t *in,
                                 fluid_real_t *left_out, fluid_real_t *right_out)
{
    int sample_index;
    int i;
    fluid_real_t d_in, d_out;

    for(sample_index = 0; sample_index < FLUID_BUFSIZE; sample_index++)
    {

        d_in = in[sample_index];
        d_out = 0.0f;

# if 0
        /* Debug: Listen to the chorus signal only */
        left_out[sample_index] = 0;
        right_out[sample_index] = 0;
#endif

        /* Write the current sample into the circular buffer */
        chorus->chorusbuf[chorus->counter] = d_in;

        for(i = 0; i < chorus->number_blocks; i++)
        {
            int ii;
            /* Calculate the delay in subsamples for the delay line of chorus block nr. */

            /* The value in the lookup table is so, that this expression
             * will always be positive.  It will always include a number of
             * full periods of MAX_SAMPLES*INTERPOLATION_SUBSAMPLES to
             * remain positive at all times. */
            int pos_subsamples = (INTERPOLATION_SUBSAMPLES * chorus->counter
                                  - chorus->lookup_tab[chorus->phase[i]]);

            int pos_samples = pos_subsamples / INTERPOLATION_SUBSAMPLES;

            /* modulo divide by INTERPOLATION_SUBSAMPLES */
            pos_subsamples &= INTERPOLATION_SUBSAMPLES_ANDMASK;

            for(ii = 0; ii < INTERPOLATION_SAMPLES; ii++)
            {
                /* Add the delayed signal to the chorus sum d_out Note: The
                 * delay in the delay line moves backwards for increasing
                 * delay!*/

                /* The & in chorusbuf[...] is equivalent to a division modulo
                   MAX_SAMPLES, only faster. */
                d_out += chorus->chorusbuf[pos_samples & MAX_SAMPLES_ANDMASK]
                         * chorus->sinc_table[ii][pos_subsamples];

                pos_samples--;
            };

            /* Cycle the phase of the modulating LFO */
            chorus->phase[i]++;

            chorus->phase[i] %= (chorus->modulation_period_samples);
        } /* foreach chorus block */

        d_out *= chorus->level;

        /* Store the chorus sum d_out to output */
        left_out[sample_index] = d_out;
        right_out[sample_index] = d_out;

        /* Move forward in circular buffer */
        chorus->counter++;
        chorus->counter %= MAX_SAMPLES;

    } /* foreach sample */
}

/* Purpose:
 *
 * Calculates a modulation waveform (sine) Its value ( modulo
 * MAXSAMPLES) varies between 0 and depth*INTERPOLATION_SUBSAMPLES.
 * Its period length is len.  The waveform data will be used modulo
 * MAXSAMPLES only.  Since MAXSAMPLES is substracted from the waveform
 * a couple of times here, the resulting (current position in
 * buffer)-(waveform sample) will always be positive.
 */
static void
fluid_chorus_sine(int *buf, int len, int depth)
{
    int i;
    double angle, incr, mult;

    /* Pre-calculate increment between angles. */
    incr = (2. * M_PI) / (double)len;

    /* Pre-calculate 'depth' multiplier. */
    mult = (double) depth / 2.0 * (double) INTERPOLATION_SUBSAMPLES;

    /* Initialize to zero degrees. */
    angle = 0.;

    /* Build sine modulation waveform */
    for(i = 0; i < len; i++)
    {
        buf[i] = (int)((1. + sin(angle)) * mult) - 3 * MAX_SAMPLES * INTERPOLATION_SUBSAMPLES;

        angle += incr;
    }
}

/* Purpose:
 * Calculates a modulation waveform (triangle)
 * See fluid_chorus_sine for comments.
 */
static void
fluid_chorus_triangle(int *buf, int len, int depth)
{
    int *il = buf;
    int *ir = buf + len - 1;
    int ival;
    double val, incr;

    /* Pre-calculate increment for the ramp. */
    incr = 2.0 / len * (double)depth * (double) INTERPOLATION_SUBSAMPLES;

    /* Initialize first value */
    val = 0. - 3. * MAX_SAMPLES * INTERPOLATION_SUBSAMPLES;

    /* Build triangular modulation waveform */
    while(il <= ir)
    {
        /* Assume 'val' to be always negative for rounding mode */
        ival = (int)(val - 0.5);

        *il++ = ival;
        *ir-- = ival;

        val += incr;
    }
}
