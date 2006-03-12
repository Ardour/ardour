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

#include <fft_result.h>
#include <fft_graph.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <iostream>

using namespace std;

FFTResult::FFTResult(FFTGraph *graph, Gdk::Color color, string trackname)
{
	_graph = graph;
	
	_windowSize = _graph->windowSize();
	_dataSize   = _windowSize / 2;

	_averages = 0;

	_data = (float *) malloc(sizeof(float) * _dataSize);
	memset(_data,0,sizeof(float) * _dataSize);

	_color     = color;
	_trackname = trackname;
}

void
FFTResult::analyzeWindow(float *window)
{
	_graph->analyze(window, _data);
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
		_data[i] /= _averages;
		_data[i]  = 10.0f * log10f(_data[i]); 
	}

	// find min & max
	_minimum = _maximum = _data[0];
	
	for (int i = 1; i < _dataSize; i++) {
		if (_data[i] < _minimum        && !isinf(_data[i])) {
			_minimum = _data[i];
		} else if (_data[i] > _maximum && !isinf(_data[i])) {
			_maximum = _data[i];
		}
	}

	_averages = 0;
}

FFTResult::~FFTResult()
{
	free(_data);
}


float
FFTResult::sampleAt(int x)
{
	if (x < 0 || x>= _dataSize)
		return 0.0f;

	return _data[x];
}

