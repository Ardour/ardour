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

#ifndef __ardour_fft_result_h
#define __ardour_fft_result_h

#include <ardour/types.h>
#include <fftw3.h>

#include <gdkmm/color.h>

#include <string>

class FFTGraph;

class FFTResult
{
	public:
		
		~FFTResult();

		void analyzeWindow(float *window);
		void finalize();

		const int length() { return _dataSize; }

		float sampleAt(int x);
		
		const float minimum() { return _minimum; }
		const float maximum() { return _maximum; }
		
		const Gdk::Color get_color() { return _color; }
		
	private:
		FFTResult(FFTGraph *graph, Gdk::Color color, std::string trackname);
		
		int 	_averages;

		float*	_data;
		float*	_work;

		int		_windowSize;
		int 	_dataSize;

		float 	_minimum;
		float 	_maximum;

		FFTGraph *_graph;
		
		Gdk::Color _color;
		std::string _trackname;
		
	friend class FFTGraph;
};

#endif /* __ardour_fft_result_h */
