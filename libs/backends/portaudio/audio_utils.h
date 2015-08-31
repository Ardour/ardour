/*
 * Copyright (C) 2015 Tim Mayberry <mojofunk@gmail.com>
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef AUDIO_UTILS_H
#define AUDIO_UTILS_H

#include <stdint.h>

inline
void
deinterleave_audio_data(const float* interleaved_input,
                        float* output,
                        uint32_t frame_count,
                        uint32_t channel,
                        uint32_t channel_count)
{
	const float* ptr = interleaved_input + channel;
	while (frame_count-- > 0) {
		*output++ = *ptr;
		ptr += channel_count;
	}
}

inline
void
interleave_audio_data(float* input,
                      float* interleaved_output,
                      uint32_t frame_count,
                      uint32_t channel,
                      uint32_t channel_count)
{
	float* ptr = interleaved_output + channel;
	while (frame_count-- > 0) {
		*ptr = *input++;
		ptr += channel_count;
	}
}

#endif // AUDIO_UTILS_H
