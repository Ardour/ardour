/*
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2013-2017 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <limits>
#include <cstdio>

#include <stdint.h>

#include "ardour/interpolation.h"
#include "ardour/midi_buffer.h"

using namespace ARDOUR;
using std::cerr;
using std::endl;

CubicInterpolation::CubicInterpolation ()
	: valid_z_bits (0)
{
}

samplecnt_t
CubicInterpolation::interpolate (int channel, samplecnt_t input_samples, Sample *input, samplecnt_t &  output_samples, Sample *output)
{
	assert (input_samples > 0);
	assert (output_samples > 0);
	assert (input);
	assert (output);
	assert (phase.size () > std::vector<double>::size_type (channel));

	_speed = fabs (_speed);

	if (invalid (0)) {

		/* z[0] not set. Two possibilities
		 *
		 * 1) we have just been constructed or ::reset()
		 *
		 * 2) we were only given 1 sample after construction or
		 *    ::reset, and stored it in z[1]
		 */

		if (invalid (1)) {

			/* first call after construction or after ::reset */

			switch (input_samples) {
			case 1:
				/* store one sample for use next time. We don't
				 * have enough points to interpolate or even
				 * compute the first z[0] value, but keep z[1]
				 * around.
				 */
				z[1] = input[0]; validate (1);
				output_samples = 0;
				return 0;
			case 2:
				/* store two samples for use next time, and
				 * compute a value for z[0] that will maintain
				 * the slope of the first actual segment. We
				 * still don't have enough samples to interpolate.
				 */
				z[0] = input[0] - (input[1] - input[0]); validate (0);
				z[1] = input[0]; validate (1);
				z[2] = input[1]; validate (2);
				output_samples = 0;
				return 0;
			default:
				/* We have enough samples to interpolate this time,
				 * but don't have a valid z[0] value because this is the
				 * first call after construction or ::reset.
				 *
				 * First point is based on a requirement to maintain
				 * the slope of the first actual segment
				 */
				z[0] = input[0] - (input[1] - input[0]); validate (0);
				break;
			}
		} else {

			/* at least one call since construction or
			 * after::reset, since we have z[1] set
			 *
			 * we can now compute z[0] as required
			 */

			z[0] = z[1] - (input[0] - z[1]); validate (0);

			/* we'll check the number of samples we've been given
			   in the next switch() statement below, and either
			   just save some more samples or actual interpolate
			*/
		}

		assert (is_valid (0));
	}

	switch (input_samples) {
	case 1:
		/* one more sample of input. find the right vX to store
		   it in, and decide if we're ready to interpolate
		*/
		if (invalid (1)) {
			z[1] = input[0]; validate (1);
			/* still not ready to interpolate */
			output_samples = 0;
			return 0;
		} else if (invalid (2)) {
			/* still not ready to interpolate */
			z[2] = input[0]; validate (2);
			output_samples = 0;
			return 0;
		} else if (invalid (3)) {
			z[3] = input[0]; validate (3);
			/* ready to interpolate */
		}
		break;
	case 2:
		/* two more samples of input. find the right vX to store
		   them in, and decide if we're ready to interpolate
		*/
		if (invalid (1)) {
			z[1] = input[0]; validate (1);
			z[2] = input[1]; validate (2);
			/* still not ready to interpolate */
			output_samples = 0;
			return 0;
		} else if (invalid (2)) {
			z[2] = input[0]; validate (2);
			z[3] = input[1]; validate (3);
			/* ready to interpolate */
		} else if (invalid (3)) {
			z[3] = input[0]; validate (3);
			/* ready to interpolate */
		}
		break;

	default:
		/* caller has given us at least enough samples to interpolate a
		   single value.
		*/
		z[1] = input[0]; validate (1);
		z[2] = input[1]; validate (2);
		z[3] = input[2]; validate (3);
	}

	/* ready to interpolate using z[0], z[1], z[2] and z[3] */

	assert (is_valid (0));
	assert (is_valid (1));
	assert (is_valid (2));
	assert (is_valid (3));

	/* we can use up to (input_samples - 2) of the input, so compute the
	 * maximum number of output samples that represents.
	 *
	 * Remember that the expected common case here is to be given
	 * input_samples that is substantially larger than output_samples,
	 * thus allowing us to always compute output_samples in one call.
	 */

	const samplecnt_t output_from_input = floor ((input_samples - 2) / _speed);

	/* limit output to either the caller's requested number or the number
	 * determined by the input size.
	 */

	const samplecnt_t limit = std::min (output_samples, output_from_input);

	samplecnt_t outsample = 0;
	double distance = phase[channel];
	samplecnt_t used = floor (distance);
	samplecnt_t i = 0;

	while (outsample < limit) {

		i = floor (distance);

		/* this call may stop the loop from being vectorized */
		float fractional_phase_part = fmod (distance, 1.0);

		/* Cubically interpolate into the output buffer */
		output[outsample++] = z[1] + 0.5f * fractional_phase_part *
			(z[2] - z[0] + fractional_phase_part * (4.0f * z[2] + 2.0f * z[0] - 5.0f * z[1] - z[3] +
			                                      fractional_phase_part * (3.0f * (z[1] - z[2]) - z[0] + z[3])));

		distance += _speed;

		z[0] = z[1];
		z[1] = input[i];
		z[2] = input[i+1];
		z[3] = input[i+2];
	}

	output_samples = outsample;
	phase[channel] = fmod (distance, 1.0);
	return i - used;
}

void
CubicInterpolation::reset ()
{
	Interpolation::reset ();
	valid_z_bits = 0;
}

samplecnt_t
CubicInterpolation::distance (samplecnt_t nsamples)
{
	assert (phase.size () > 0);
	return floor (floor (phase[0]) + (_speed * nsamples));
}
