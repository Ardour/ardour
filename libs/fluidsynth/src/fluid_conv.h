/* FluidSynth - A Software Synthesizer
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
 */

#ifndef _FLUID_CONV_H
#define _FLUID_CONV_H

#include "fluidsynth_priv.h"

/*
 Attenuation range in centibels.
 Attenuation range is the dynamic range of the volume envelope generator
 from 0 to the end of attack segment.
 fluidsynth is a 24 bit synth, it could (should??) be 144 dB of attenuation.
 However the spec makes no distinction between 16 or 24 bit synths, so use
 96 dB here.

 Note about usefulness of 24 bits:
 1)Even fluidsynth is a 24 bit synth, this format is only relevant if
 the sample format coming from the soundfont is 24 bits and the audio sample format
 choosen by the application (audio.sample.format) is not 16 bits.

 2)When the sample soundfont is 16 bits, the internal 24 bits number have
 16 bits msb and lsb to 0. Consequently, at the DAC output, the dynamic range of
 this 24 bit sample is reduced to the the dynamic of a 16 bits sample (ie 90 db)
 even if this sample is produced by the audio driver using an audio sample format
 compatible for a 24 bit DAC.

 3)When the audio sample format settings is 16 bits (audio.sample.format), the
 audio driver will make use of a 16 bit DAC, and the dynamic will be reduced to 96 dB
 even if the initial sample comes from a 24 bits soundfont.

 In both cases (2) or (3), the real dynamic range is only 96 dB.

 Other consideration for FLUID_NOISE_FLOOR related to case (1),(2,3):
 - for case (1), FLUID_NOISE_FLOOR should be the noise floor for 24 bits (i.e -138 dB).
 - for case (2) or (3), FLUID_NOISE_FLOOR should be the noise floor for 16 bits (i.e -90 dB).
 */
#define FLUID_PEAK_ATTENUATION  960.0f

void fluid_conversion_config(void);

fluid_real_t fluid_ct2hz_real(fluid_real_t cents);
fluid_real_t fluid_ct2hz(fluid_real_t cents);
fluid_real_t fluid_cb2amp(fluid_real_t cb);
fluid_real_t fluid_tc2sec(fluid_real_t tc);
fluid_real_t fluid_tc2sec_delay(fluid_real_t tc);
fluid_real_t fluid_tc2sec_attack(fluid_real_t tc);
fluid_real_t fluid_tc2sec_release(fluid_real_t tc);
fluid_real_t fluid_act2hz(fluid_real_t c);
fluid_real_t fluid_hz2ct(fluid_real_t c);
fluid_real_t fluid_pan(fluid_real_t c, int left);
fluid_real_t fluid_balance(fluid_real_t balance, int left);
fluid_real_t fluid_concave(fluid_real_t val);
fluid_real_t fluid_convex(fluid_real_t val);

#endif /* _FLUID_CONV_H */
