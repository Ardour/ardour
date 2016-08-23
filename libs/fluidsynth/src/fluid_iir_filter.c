/* FluidSynth - A Software Synthesizer
 *
 * Copyright (C) 2003  Peter Hanappe and others.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 */

#include "fluid_iir_filter.h"
#include "fluid_sys.h"
#include "fluid_conv.h"

/**
 * Applies a lowpass filter with variable cutoff frequency and quality factor.
 * Also modifies filter state accordingly.
 * @param iir_filter Filter parameter
 * @param dsp_buf Pointer to the synthesized audio data
 * @param count Count of samples in dsp_buf
 */
/*
 * Variable description:
 * - dsp_a1, dsp_a2, dsp_b0, dsp_b1, dsp_b2: Filter coefficients
 *
 * A couple of variables are used internally, their results are discarded:
 * - dsp_i: Index through the output buffer
 * - dsp_phase_fractional: The fractional part of dsp_phase
 * - dsp_coeff: A table of four coefficients, depending on the fractional phase.
 *              Used to interpolate between samples.
 * - dsp_process_buffer: Holds the processed signal between stages
 * - dsp_centernode: delay line for the IIR filter
 * - dsp_hist1: same
 * - dsp_hist2: same
 */
void 
fluid_iir_filter_apply(fluid_iir_filter_t* iir_filter,
                       fluid_real_t *dsp_buf, int count)
{
  /* IIR filter sample history */
  fluid_real_t dsp_hist1 = iir_filter->hist1;
  fluid_real_t dsp_hist2 = iir_filter->hist2;

  /* IIR filter coefficients */
  fluid_real_t dsp_a1 = iir_filter->a1;
  fluid_real_t dsp_a2 = iir_filter->a2;
  fluid_real_t dsp_b02 = iir_filter->b02;
  fluid_real_t dsp_b1 = iir_filter->b1;
  int dsp_filter_coeff_incr_count = iir_filter->filter_coeff_incr_count;

  fluid_real_t dsp_centernode;
  int dsp_i;

  /* filter (implement the voice filter according to SoundFont standard) */

  /* Check for denormal number (too close to zero). */
  if (fabs (dsp_hist1) < 1e-20) dsp_hist1 = 0.0f;  /* FIXME JMG - Is this even needed? */

  /* Two versions of the filter loop. One, while the filter is
  * changing towards its new setting. The other, if the filter
  * doesn't change.
  */

  if (dsp_filter_coeff_incr_count > 0)
  {
    fluid_real_t dsp_a1_incr = iir_filter->a1_incr;
    fluid_real_t dsp_a2_incr = iir_filter->a2_incr;
    fluid_real_t dsp_b02_incr = iir_filter->b02_incr;
    fluid_real_t dsp_b1_incr = iir_filter->b1_incr;


    /* Increment is added to each filter coefficient filter_coeff_incr_count times. */
    for (dsp_i = 0; dsp_i < count; dsp_i++)
    {
      /* The filter is implemented in Direct-II form. */
      dsp_centernode = dsp_buf[dsp_i] - dsp_a1 * dsp_hist1 - dsp_a2 * dsp_hist2;
      dsp_buf[dsp_i] = dsp_b02 * (dsp_centernode + dsp_hist2) + dsp_b1 * dsp_hist1;
      dsp_hist2 = dsp_hist1;
      dsp_hist1 = dsp_centernode;

      if (dsp_filter_coeff_incr_count-- > 0)
      {
        fluid_real_t old_b02 = dsp_b02;
	dsp_a1 += dsp_a1_incr;
	dsp_a2 += dsp_a2_incr;
	dsp_b02 += dsp_b02_incr;
	dsp_b1 += dsp_b1_incr;

        /* Compensate history to avoid the filter going havoc with large frequency changes */
	if (iir_filter->compensate_incr && fabs(dsp_b02) > 0.001) {
          fluid_real_t compensate = old_b02 / dsp_b02;
          dsp_centernode *= compensate;
          dsp_hist1 *= compensate;
          dsp_hist2 *= compensate;
        }
      }
    } /* for dsp_i */
  }
  else /* The filter parameters are constant.  This is duplicated to save time. */
  {
    for (dsp_i = 0; dsp_i < count; dsp_i++)
    { /* The filter is implemented in Direct-II form. */
      dsp_centernode = dsp_buf[dsp_i] - dsp_a1 * dsp_hist1 - dsp_a2 * dsp_hist2;
      dsp_buf[dsp_i] = dsp_b02 * (dsp_centernode + dsp_hist2) + dsp_b1 * dsp_hist1;
      dsp_hist2 = dsp_hist1;
      dsp_hist1 = dsp_centernode;
    }
  }

  iir_filter->hist1 = dsp_hist1;
  iir_filter->hist2 = dsp_hist2;
  iir_filter->a1 = dsp_a1;
  iir_filter->a2 = dsp_a2;
  iir_filter->b02 = dsp_b02;
  iir_filter->b1 = dsp_b1;
  iir_filter->filter_coeff_incr_count = dsp_filter_coeff_incr_count;

  fluid_check_fpe ("voice_filter");
}


void 
fluid_iir_filter_reset(fluid_iir_filter_t* iir_filter)
{
  iir_filter->hist1 = 0;
  iir_filter->hist2 = 0;
  iir_filter->last_fres = -1.;
  iir_filter->filter_startup = 1;
}

void 
fluid_iir_filter_set_fres(fluid_iir_filter_t* iir_filter, 
                          fluid_real_t fres)
{
  iir_filter->fres = fres;
  iir_filter->last_fres = -1.;
}


void 
fluid_iir_filter_set_q_dB(fluid_iir_filter_t* iir_filter, 
                          fluid_real_t q_dB)
{
    /* The 'sound font' Q is defined in dB. The filter needs a linear
       q. Convert. */
    iir_filter->q_lin = (fluid_real_t) (pow(10.0f, q_dB / 20.0f));

    /* SF 2.01 page 59:
     *
     *  The SoundFont specs ask for a gain reduction equal to half the
     *  height of the resonance peak (Q).  For example, for a 10 dB
     *  resonance peak, the gain is reduced by 5 dB.  This is done by
     *  multiplying the total gain with sqrt(1/Q).  `Sqrt' divides dB
     *  by 2 (100 lin = 40 dB, 10 lin = 20 dB, 3.16 lin = 10 dB etc)
     *  The gain is later factored into the 'b' coefficients
     *  (numerator of the filter equation).  This gain factor depends
     *  only on Q, so this is the right place to calculate it.
     */
    iir_filter->filter_gain = (fluid_real_t) (1.0 / sqrt(iir_filter->q_lin));

    /* The synthesis loop will have to recalculate the filter coefficients. */
    iir_filter->last_fres = -1.;

}


static inline void 
fluid_iir_filter_calculate_coefficients(fluid_iir_filter_t* iir_filter, 
                                        int transition_samples, 
                                        fluid_real_t output_rate)
{

  /*
    * Those equations from Robert Bristow-Johnson's `Cookbook
    * formulae for audio EQ biquad filter coefficients', obtained
    * from Harmony-central.com / Computer / Programming. They are
    * the result of the bilinear transform on an analogue filter
    * prototype. To quote, `BLT frequency warping has been taken
    * into account for both significant frequency relocation and for
    * bandwidth readjustment'. */

  fluid_real_t omega = (fluid_real_t) (2.0 * M_PI * 
                       (iir_filter->last_fres / ((float) output_rate)));
  fluid_real_t sin_coeff = (fluid_real_t) sin(omega);
  fluid_real_t cos_coeff = (fluid_real_t) cos(omega);
  fluid_real_t alpha_coeff = sin_coeff / (2.0f * iir_filter->q_lin);
  fluid_real_t a0_inv = 1.0f / (1.0f + alpha_coeff);

  /* Calculate the filter coefficients. All coefficients are
   * normalized by a0. Think of `a1' as `a1/a0'.
   *
   * Here a couple of multiplications are saved by reusing common expressions.
   * The original equations should be:
   *  iir_filter->b0=(1.-cos_coeff)*a0_inv*0.5*iir_filter->filter_gain;
   *  iir_filter->b1=(1.-cos_coeff)*a0_inv*iir_filter->filter_gain;
   *  iir_filter->b2=(1.-cos_coeff)*a0_inv*0.5*iir_filter->filter_gain; */

  fluid_real_t a1_temp = -2.0f * cos_coeff * a0_inv;
  fluid_real_t a2_temp = (1.0f - alpha_coeff) * a0_inv;
  fluid_real_t b1_temp = (1.0f - cos_coeff) * a0_inv * iir_filter->filter_gain;
   /* both b0 -and- b2 */
  fluid_real_t b02_temp = b1_temp * 0.5f;

  iir_filter->compensate_incr = 0;

  if (iir_filter->filter_startup || (transition_samples == 0))
  {
   /* The filter is calculated, because the voice was started up.
    * In this case set the filter coefficients without delay.
    */
    iir_filter->a1 = a1_temp;
    iir_filter->a2 = a2_temp;
    iir_filter->b02 = b02_temp;
    iir_filter->b1 = b1_temp;
    iir_filter->filter_coeff_incr_count = 0;
    iir_filter->filter_startup = 0;
//       printf("Setting initial filter coefficients.\n");
  }
  else
  {

    /* The filter frequency is changed.  Calculate an increment
     * factor, so that the new setting is reached after one buffer
     * length. x_incr is added to the current value FLUID_BUFSIZE
     * times. The length is arbitrarily chosen. Longer than one
     * buffer will sacrifice some performance, though.  Note: If
     * the filter is still too 'grainy', then increase this number
     * at will.
     */

    iir_filter->a1_incr = (a1_temp - iir_filter->a1) / transition_samples;
    iir_filter->a2_incr = (a2_temp - iir_filter->a2) / transition_samples;
    iir_filter->b02_incr = (b02_temp - iir_filter->b02) / transition_samples;
    iir_filter->b1_incr = (b1_temp - iir_filter->b1) / transition_samples;
    if (fabs(iir_filter->b02) > 0.0001) {
      fluid_real_t quota = b02_temp / iir_filter->b02;
      iir_filter->compensate_incr = quota < 0.5 || quota > 2;
    }
    /* Have to add the increments filter_coeff_incr_count times. */
    iir_filter->filter_coeff_incr_count = transition_samples;
  }
  fluid_check_fpe ("voice_write filter calculation");
}


void fluid_iir_filter_calc(fluid_iir_filter_t* iir_filter, 
                           fluid_real_t output_rate, 
                           fluid_real_t fres_mod)
{
  fluid_real_t fres;

  /* calculate the frequency of the resonant filter in Hz */
  fres = fluid_ct2hz(iir_filter->fres + fres_mod);

  /* FIXME - Still potential for a click during turn on, can we interpolate
     between 20khz cutoff and 0 Q? */

  /* I removed the optimization of turning the filter off when the
   * resonance frequence is above the maximum frequency. Instead, the
   * filter frequency is set to a maximum of 0.45 times the sampling
   * rate. For a 44100 kHz sampling rate, this amounts to 19845
   * Hz. The reason is that there were problems with anti-aliasing when the
   * synthesizer was run at lower sampling rates. Thanks to Stephan
   * Tassart for pointing me to this bug. By turning the filter on and
   * clipping the maximum filter frequency at 0.45*srate, the filter
   * is used as an anti-aliasing filter. */

  if (fres > 0.45f * output_rate)
    fres = 0.45f * output_rate;
  else if (fres < 5)
    fres = 5;

  /* if filter enabled and there is a significant frequency change.. */
  if ((abs (fres - iir_filter->last_fres) > 0.01))
  {
   /* The filter coefficients have to be recalculated (filter
    * parameters have changed). Recalculation for various reasons is
    * forced by setting last_fres to -1.  The flag filter_startup
    * indicates, that the DSP loop runs for the first time, in this
    * case, the filter is set directly, instead of smoothly fading
    * between old and new settings. */
    iir_filter->last_fres = fres;
    fluid_iir_filter_calculate_coefficients(iir_filter, FLUID_BUFSIZE,  
                                            output_rate);
  }


  fluid_check_fpe ("voice_write DSP coefficients");

}

