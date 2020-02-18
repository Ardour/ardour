/*
 * Copyright (C) 2010 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2013-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2016 Robin Gareus <robin@gareus.org>
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

#include "audiographer/general/sr_converter.h"

#include "audiographer/exception.h"
#include "audiographer/type_utils.h"

#include <cmath>
#include <boost/format.hpp>

namespace AudioGrapher
{
using boost::format;
using boost::str;

SampleRateConverter::SampleRateConverter (uint32_t channels)
  : active (false)
  , channels (channels)
  , max_samples_in(0)
  , leftover_data (0)
  , leftover_samples (0)
  , max_leftover_samples (0)
  , data_out (0)
  , data_out_size (0)
  , src_state (0)
{
	add_supported_flag (ProcessContext<>::EndOfInput);
}

void
SampleRateConverter::init (samplecnt_t in_rate, samplecnt_t out_rate, int quality)
{
	reset();

	if (in_rate == out_rate) {
		src_data.src_ratio = 1;
		return;
	}

	active = true;
	int err;
	src_state = src_new (quality, channels, &err);
	if (throw_level (ThrowObject) && !src_state) {
		throw Exception (*this, str (format
			("Cannot initialize sample rate converter: %1%")
			% src_strerror (err)));
	}

	src_data.src_ratio = (double) out_rate / (double) in_rate;
}

SampleRateConverter::~SampleRateConverter ()
{
	reset();
}

samplecnt_t
SampleRateConverter::allocate_buffers (samplecnt_t max_samples)
{
	if (!active) { return max_samples; }

	samplecnt_t max_samples_out = (samplecnt_t) ceil (max_samples * src_data.src_ratio);
	max_samples_out -= max_samples_out % channels;

	if (data_out_size < max_samples_out) {

		delete[] data_out;
		data_out = new float[max_samples_out];
		src_data.data_out = data_out;

		max_leftover_samples = 4 * max_samples;
		leftover_data = (float *) realloc (leftover_data, max_leftover_samples * sizeof (float));
		if (throw_level (ThrowObject) && !leftover_data) {
			throw Exception (*this, "A memory allocation error occurred");
		}

		max_samples_in = max_samples;
		data_out_size = max_samples_out;
	}

	return max_samples_out;
}

void
SampleRateConverter::process (ProcessContext<float> const & c)
{
	check_flags (*this, c);

	if (!active) {
		output (c);
		return;
	}

	samplecnt_t samples = c.samples();
	float * in = const_cast<float *> (c.data()); // TODO check if this is safe!

	if (throw_level (ThrowProcess) && samples > max_samples_in) {
		throw Exception (*this, str (format (
			"process() called with too many samples, %1% instead of %2%")
			% samples % max_samples_in));
	}

	int err;
	bool first_time = true;

	do {
		src_data.output_frames = data_out_size / channels;
		src_data.data_out = data_out;

		if (leftover_samples > 0) {

			/* input data will be in leftover_data rather than data_in */

			src_data.data_in = leftover_data;

			if (first_time) {

				/* first time, append new data from data_in into the leftover_data buffer */

				TypeUtils<float>::copy (in, &leftover_data [leftover_samples * channels], samples);
				src_data.input_frames = samples / channels + leftover_samples;
			} else {

				/* otherwise, just use whatever is still left in leftover_data; the contents
					were adjusted using memmove() right after the last SRC call (see
					below)
				*/

				src_data.input_frames = leftover_samples;
			}

		} else {
			src_data.data_in = in;
			src_data.input_frames = samples / channels;
		}

		first_time = false;

		if (debug_level (DebugVerbose)) {
			debug_stream() << "data_in: " << src_data.data_in <<
				", input_frames: " << src_data.input_frames <<
				", data_out: " << src_data.data_out <<
				", output_frames: " << src_data.output_frames << std::endl;
		}

		err = src_process (src_state, &src_data);
		if (throw_level (ThrowProcess) && err) {
			throw Exception (*this, str (format
			("An error occurred during sample rate conversion: %1%")
			% src_strerror (err)));
		}

		leftover_samples = src_data.input_frames - src_data.input_frames_used;

		if (leftover_samples > 0) {
			if (throw_level (ThrowProcess) && leftover_samples > max_leftover_samples) {
				throw Exception(*this, "leftover samples overflowed");
			}
			TypeUtils<float>::move (&src_data.data_in[src_data.input_frames_used * channels],
			                        leftover_data, leftover_samples * channels);
		}

		ProcessContext<float> c_out (c, data_out, src_data.output_frames_gen * channels);
		if (!src_data.end_of_input || leftover_samples) {
			c_out.remove_flag (ProcessContext<float>::EndOfInput);
		}
		output (c_out);

		if (debug_level (DebugProcess)) {
			debug_stream() <<
				"src_data.output_frames_gen: " << src_data.output_frames_gen <<
				", leftover_samples: " << leftover_samples << std::endl;
		}

		if (throw_level (ThrowProcess) && src_data.output_frames_gen == 0 && leftover_samples) {
			throw Exception (*this, boost::str (boost::format
				("No output samples generated with %1% leftover samples")
				% leftover_samples));
		}

	} while (leftover_samples > samples);

	// src_data.end_of_input has to be checked to prevent infinite recursion
	if (!src_data.end_of_input && c.has_flag(ProcessContext<float>::EndOfInput)) {
		set_end_of_input (c);
	}
}

void SampleRateConverter::set_end_of_input (ProcessContext<float> const & c)
{
	src_data.end_of_input = true;

	float f;
	ProcessContext<float> const dummy (c, &f, 0, channels);

	/* No idea why this has to be done twice for all data to be written,
	 * but that just seems to be the way it is...
	 */
	dummy.remove_flag (ProcessContext<float>::EndOfInput);
	process (dummy);
	dummy.set_flag (ProcessContext<float>::EndOfInput);
	process (dummy);
}


void SampleRateConverter::reset ()
{
	active = false;
	max_samples_in = 0;
	src_data.end_of_input = false;

	if (src_state) {
		src_delete (src_state);
	}

	leftover_samples = 0;
	max_leftover_samples = 0;
	if (leftover_data) {
		free (leftover_data);
	}

	data_out_size = 0;
	delete [] data_out;
	data_out = 0;
}

} // namespace
