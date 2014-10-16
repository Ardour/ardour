/*
    Copyright (C) 2006 Paul Davis
	Written by Sampo Savolainen

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

#include "fft_result.h"
#include "fft_graph.h"
#include <cstdlib>
#include <cstring>
#include <string>
#include <cmath>

#include <iostream>

using namespace std;

FFTResult::FFTResult(FFTGraph *graph, Gdk::Color color, string trackname)
{
	_graph = graph;

	_windowSize = _graph->windowSize();
	_dataSize   = _windowSize / 2;

	_averages = 0;

	_data_avg = (float *) malloc(sizeof(float) * _dataSize);
	memset(_data_avg,0,sizeof(float) * _dataSize);

	_data_min = (float *) malloc(sizeof(float) * _dataSize);
	_data_max = (float *) malloc(sizeof(float) * _dataSize);

	for (int i = 0; i < _dataSize; i++) {
		_data_min[i] = FLT_MAX;
		_data_max[i] = FLT_MIN;
	}

	_color     = color;
	_trackname = trackname;
}

void
FFTResult::analyzeWindow(float *window)
{
	float *_hanning = _graph->_hanning;
	float *_in = _graph->_in;
	float *_out = _graph->_out;

	int i;
	// Copy the data and apply the hanning window
	for (i = 0; i < _windowSize; i++) {
		_in[i] = window[ i ] * _hanning[ i ];
	}

	fftwf_execute(_graph->_plan);

	float b = _out[0] * _out[0];

	_data_avg[0] += b;
	if (b < _data_min[0]) _data_min[0] = b;
	if (b > _data_max[0]) _data_max[0] = b;

	for (i=1; i < _dataSize - 1; i++) { // TODO: check with Jesse whether this is really correct
		b = (_out[i] * _out[i]);

		_data_avg[i] += b;  // + (_out[_windowSize-i] * _out[_windowSize-i]);, TODO: thanks to Stefan Kost

		if (_data_min[i] > b)  _data_min[i] = b;
		if (_data_max[i] < b ) _data_max[i] = b;
	}


	_averages++;
}

void
FFTResult::finalize()
{
	if (_averages == 0) {
		_minimum = 0.0;
		_maximum = 0.0;
		return;
	}

	// Average & scale
	for (int i = 0; i < _dataSize; i++) {
		_data_avg[i] /= _averages;
		_data_avg[i]  = 10.0f * log10f(_data_avg[i]);

		_data_min[i]  = 10.0f * log10f(_data_min[i]);
		if (_data_min[i] < -10000.0f) {
			_data_min[i] = -10000.0f;
		}
		_data_max[i]  = 10.0f * log10f(_data_max[i]);
	}

	// find min & max
	_minimum = _maximum = _data_avg[0];

	for (int i = 1; i < _dataSize; i++) {
		if (_data_avg[i] < _minimum        && !isinf(_data_avg[i])) {
			_minimum = _data_avg[i];
		} else if (_data_avg[i] > _maximum && !isinf(_data_avg[i])) {
			_maximum = _data_avg[i];
		}
	}

	_averages = 0;
}

FFTResult::~FFTResult()
{
	free(_data_avg);
	free(_data_min);
	free(_data_max);
}


float
FFTResult::avgAt(int x)
{
	if (x < 0 || x>= _dataSize)
		return 0.0f;

	return _data_avg[x];
}

float
FFTResult::minAt(int x)
{
	if (x < 0 || x>= _dataSize)
		return 0.0f;

	return _data_min[x];
}

float
FFTResult::maxAt(int x)
{
	if (x < 0 || x>= _dataSize)
		return 0.0f;

	return _data_max[x];
}

