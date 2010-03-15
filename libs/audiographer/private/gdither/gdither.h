/*
 *  Copyright (C) 2002 Steve Harris <steve@plugin.org.uk>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef GDITHER_H
#define GDITHER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "gdither_types.h"

/* Create and initialise a state structure, takes a dither type, a number of
 * channels and a bit depth as input
 *
 * The Dither type is one of
 *
 *   GDitherNone - straight nearest neighbour rounding. Theres no pressing
 *   reason to do this at 8 or 16 bit, but you might want to at 24, for some
 *   reason. At the lest it will save you writing int->float conversion code,
 *   which is arder than it sounds.
 *
 *   GDitherRect - mathematically most accurate, lowest noise floor, but not
 *   that good for audio. It is the fastest though.
 *
 *   GDitherTri - a happy medium between Rectangular and Shaped, reasonable
 *   noise floor, not too obvious, quite fast.
 *
 *   GDitherShaped - should have the least audible impact, but has the highest
 *   noise floor, fairly CPU intensive. Not advisible if your going to apply
 *   any frequency manipulation afterwards.
 *
 * channels, sets the number of channels in the output data, output data will
 * be written interleaved into the area given to gdither_run(). Set to 1
 * if you are not working with interleaved buffers.
 *
 * bit depth, sets the bit width of the output sample data, it can be one of:
 *
 *   GDither8bit   - 8 bit unsiged
 *   GDither16bit  - 16 bit signed
 *   GDither32bit  - 24+bits in upper bits of a 32 bit word
 *   GDitherFloat  - IEEE floating point (32bits)
 *   GDitherDouble - Double precision IEEE floating point (64bits)
 *
 * dither_depth, set the number of bits before the signal will be truncated to,
 * eg. 16 will produce an output stream with 16bits-worth of signal. Setting to
 * zero or greater than the width of the output format will dither to the
 * maximum precision allowed by the output format.
 */
GDither gdither_new(GDitherType type, uint32_t channels,

                    GDitherSize bit_depth, int dither_depth);

/* Frees memory used by gdither_new.
 */
void gdither_free(GDither s);

/* Applies dithering to the supplied signal.
 *
 * channel is the channel number you are processing (0 - channles-1), length is
 * the length of the input, in samples, x is the input samples (float), y is
 * where the output samples will be written, it should have the approaprate
 * type for the chosen bit depth
 */
void gdither_runf(GDither s, uint32_t channel, uint32_t length,
		   float const *x, void *y);

/* see gdither_runf, vut input argument is double format */
void gdither_run(GDither s, uint32_t channel, uint32_t length,
		   double const *x, void *y);

#ifdef __cplusplus
}
#endif

#endif
