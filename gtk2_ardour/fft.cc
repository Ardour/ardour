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
	: _windowSize(windowSize),
	  _dataSize(_windowSize/2),
	_iterations(0)
{
	_fftInput  = (float *) fftwf_malloc(sizeof(float) * _windowSize);

	_fftOutput = (float *) fftwf_malloc(sizeof(float) * _windowSize);

	_powerAtBin  = (float *) malloc(sizeof(float) * _dataSize);
	_phaseAtBin  = (float *) malloc(sizeof(float) * _dataSize);

	_plan = fftwf_plan_r2r_1d(_windowSize, _fftInput, _fftOutput, FFTW_R2HC, FFTW_ESTIMATE);

	reset();
}

void
FFT::reset()
{
	memset(_powerAtBin, 0, sizeof(float) * _dataSize);
	memset(_phaseAtBin, 0, sizeof(float) * _dataSize);
	
	_iterations = 0;
}

void
FFT::analyze(ARDOUR::Sample *input)
{
	_iterations++;

	memcpy(_fftInput, input, sizeof(float) * _windowSize);

	fftwf_execute(_plan);

	_powerAtBin[0] += _fftOutput[0] * _fftOutput[0];
	_phaseAtBin[0] += 0.0;

	float power;
	float phase;

#define Re (_fftOutput[i])
#define Im (_fftOutput[_windowSize-i])
       	for (uint32_t i=1; i < _dataSize - 1; i++) { 

		power = (Re * Re) + (Im * Im);
		phase = atanf(Im / Re);
	
		if (Re < 0.0 && Im > 0.0) {
			phase += M_PI;
		} else if (Re < 0.0 && Im < 0.0) {
			phase -= M_PI;
		}

		_powerAtBin[i] += power;
		_phaseAtBin[i] += phase;
	}
#undef Re
#undef Im
}

void
FFT::calculate()
{
	if (_iterations > 1) {
	       	for (uint32_t i=0; i < _dataSize - 1; i++) { 
			_powerAtBin[i] /= (float)_iterations;
			_phaseAtBin[i] /= (float)_iterations;
		}
		_iterations = 1;
	}
}


FFT::~FFT()
{
	fftwf_destroy_plan(_plan);
	free(_powerAtBin);
	free(_phaseAtBin);
	free(_fftOutput);
	free(_fftInput);
}
