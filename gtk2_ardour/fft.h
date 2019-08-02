/*
 * Copyright (C) 2008 Sampo Savolainen <v2@iki.fi>
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

#ifndef __ardour_fft_h__
#define __ardour_fft_h__


#include <iostream>

#include <stdio.h>
#include <sys/types.h>
#include <complex> // this needs to be included before fftw3.h
#include <fftw3.h>


#include "ardour/types.h"

namespace GTKArdour {

class FFT
{
public:
	FFT (uint32_t);
	~FFT ();

	enum WindowingType {
		NONE,
		HANN
	};

	void reset ();
	void analyze (ARDOUR::Sample*, WindowingType w = NONE);
	void calculate ();

	uint32_t bins () const { return _data_size; }

	float power_at_bin (uint32_t i) const { return _power_at_bin[i]; }
	float phase_at_bin (uint32_t i) const { return _phase_at_bin[i]; }

private:
	float* get_hann_window ();

	uint32_t const _window_size;
	uint32_t const _data_size;
	uint32_t       _iterations;

	float* _hann_window;

	float* _fftInput;
	float* _fftOutput;

	float* _power_at_bin;
	float* _phase_at_bin;

	fftwf_plan _plan;
};

}

#endif
