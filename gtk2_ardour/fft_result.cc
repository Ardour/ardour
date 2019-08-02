/*
 * Copyright (C) 2008-2009 David Robillard <d@drobilla.net>
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

#include "fft_result.h"
#include "fft_graph.h"
#include <cstdlib>
#include <cstring>
#include <string>
#include <cmath>
#include <algorithm>

using namespace std;

FFTResult::FFTResult(FFTGraph *graph, Gdk::Color color, string trackname)
{
	_graph = graph;

	_windowSize = _graph->windowSize();
	_dataSize   = _windowSize / 2;
	_averages = 0;
	_min_flat = _max_flat = 0.0;
	_min_prop = _max_prop = 0.0;

	_data_flat_avg = (float *) malloc (sizeof(float) * _dataSize);
	_data_flat_min = (float *) malloc (sizeof(float) * _dataSize);
	_data_flat_max = (float *) malloc (sizeof(float) * _dataSize);
	_data_prop_avg = (float *) malloc (sizeof(float) * _dataSize);
	_data_prop_min = (float *) malloc (sizeof(float) * _dataSize);
	_data_prop_max = (float *) malloc (sizeof(float) * _dataSize);

	for (unsigned int i = 0; i < _dataSize; i++) {
		_data_flat_min[i] = FLT_MAX;
		_data_flat_max[i] = FLT_MIN;
		_data_flat_avg[i] = 0;
		_data_prop_min[i] = FLT_MAX;
		_data_prop_max[i] = FLT_MIN;
		_data_prop_avg[i] = 0;
	}

	_color     = color;
	_trackname = trackname;
}

void
FFTResult::analyzeWindow(float *window)
{
	float const * const _hanning = _graph->_hanning;
	float *_in = _graph->_in;
	float *_out = _graph->_out;

	// Copy the data and apply the hanning window
	for (unsigned int i = 0; i < _windowSize; ++i) {
		_in[i] = window[i] * _hanning[i];
	}

	fftwf_execute(_graph->_plan);

	// calculate signal power per bin
	float b = _out[0] * _out[0];

	_data_flat_avg[0] += b;
	if (b < _data_flat_min[0]) _data_flat_min[0] = b;
	if (b > _data_flat_max[0]) _data_flat_max[0] = b;

	for (unsigned int i = 1; i < _dataSize - 1; ++i) {
		b = (_out[i] * _out[i]) + (_out[_windowSize - i] * _out[_windowSize - i]);
		_data_flat_avg[i] += b;
		if (_data_flat_min[i] > b)  _data_flat_min[i] = b;
		if (_data_flat_max[i] < b ) _data_flat_max[i] = b;
	}

	_averages++;
}

void
FFTResult::finalize()
{
	if (_averages == 0) {
		_min_flat = _max_flat = 0.0;
		_min_prop = _max_prop = 0.0;
		return;
	}

	// Average & scale
	for (unsigned int i = 0; i < _dataSize - 1; ++i) {
		_data_flat_avg[i] /= _averages;
		// proportional, pink spectrum @ -18dB
		_data_prop_avg[i] = _data_flat_avg [i] * i / 63.096f;
		_data_prop_min[i] = _data_flat_min [i] * i / 63.096f;
		_data_prop_max[i] = _data_flat_max [i] * i / 63.096f;
	}

	_data_prop_avg[0] = _data_flat_avg [0] / 63.096f;
	_data_prop_min[0] = _data_flat_min [0] / 63.096f;
	_data_prop_max[0] = _data_flat_max [0] / 63.096f;

	// calculate power
	for (unsigned int i = 0; i < _dataSize - 1; ++i) {
		_data_flat_min[i] = power_to_db (_data_flat_min[i]);
		_data_flat_max[i] = power_to_db (_data_flat_max[i]);
		_data_flat_avg[i] = power_to_db (_data_flat_avg[i]);
		_data_prop_min[i] = power_to_db (_data_prop_min[i]);
		_data_prop_max[i] = power_to_db (_data_prop_max[i]);
		_data_prop_avg[i] = power_to_db (_data_prop_avg[i]);
	}

	// find min & max
	_min_flat = _max_flat = _data_flat_avg[0];
	_min_prop = _max_prop = _data_prop_avg[0];

	for (unsigned int i = 1; i < _dataSize - 1; ++i) {
		_min_flat = std::min (_min_flat, _data_flat_avg[i]);
		_max_flat = std::max (_max_flat, _data_flat_avg[i]);
		_min_prop = std::min (_min_prop, _data_prop_avg[i]);
		_max_prop = std::max (_max_prop, _data_prop_avg[i]);
	}

	_averages = 0;
}

FFTResult::~FFTResult()
{
	free(_data_flat_avg);
	free(_data_flat_min);
	free(_data_flat_max);
	free(_data_prop_avg);
	free(_data_prop_min);
	free(_data_prop_max);
}
