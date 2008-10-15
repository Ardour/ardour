/*
    Copyright (C) 2008 Paul Davis
    Author: Sampo Savolainen

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/
#include "fft.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

FFT::FFT(uint32_t windowSize)
	: _window_size(windowSize),
	  _data_size(_window_size/2),
	_iterations(0)
{
	_fftInput  = (float *) fftwf_malloc(sizeof(float) * _window_size);

	_fftOutput = (float *) fftwf_malloc(sizeof(float) * _window_size);

	_power_at_bin  = (float *) malloc(sizeof(float) * _data_size);
	_phase_at_bin  = (float *) malloc(sizeof(float) * _data_size);

	_plan = fftwf_plan_r2r_1d(_window_size, _fftInput, _fftOutput, FFTW_R2HC, FFTW_ESTIMATE);

	reset();
}

void
FFT::reset()
{
	memset(_power_at_bin, 0, sizeof(float) * _data_size);
	memset(_phase_at_bin, 0, sizeof(float) * _data_size);
	
	_iterations = 0;
}

void
FFT::analyze(ARDOUR::Sample *input)
{
	_iterations++;

	memcpy(_fftInput, input, sizeof(float) * _window_size);

	fftwf_execute(_plan);

	_power_at_bin[0] += _fftOutput[0] * _fftOutput[0];
	_phase_at_bin[0] += 0.0;

	float power;
	float phase;

#define Re (_fftOutput[i])
#define Im (_fftOutput[_window_size-i])
       	for (uint32_t i=1; i < _data_size - 1; i++) { 

		power = (Re * Re) + (Im * Im);
		phase = atanf(Im / Re);
	
		if (Re < 0.0 && Im > 0.0) {
			phase += M_PI;
		} else if (Re < 0.0 && Im < 0.0) {
			phase -= M_PI;
		}

		_power_at_bin[i] += power;
		_phase_at_bin[i] += phase;
	}
#undef Re
#undef Im
}

void
FFT::calculate()
{
	if (_iterations > 1) {
	       	for (uint32_t i=0; i < _data_size - 1; i++) { 
			_power_at_bin[i] /= (float)_iterations;
			_phase_at_bin[i] /= (float)_iterations;
		}
		_iterations = 1;
	}
}


FFT::~FFT()
{
	fftwf_destroy_plan(_plan);
	free(_power_at_bin);
	free(_phase_at_bin);
	free(_fftOutput);
	free(_fftInput);
}
