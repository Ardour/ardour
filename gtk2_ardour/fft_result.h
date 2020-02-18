/*
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_fft_result_h
#define __ardour_fft_result_h

#include <math.h>
#include <fftw3.h>

#include <gdkmm/color.h>

#include <string>

class FFTGraph;

class FFTResult
{
public:

	~FFTResult ();

	void analyzeWindow (float *window);
	void finalize ();

	unsigned int length () const { return _dataSize; }

	float avgAt (unsigned int x, bool p) const
	{ return p ? _data_prop_avg[x] : _data_flat_avg[x]; }
	float maxAt (unsigned int x, bool p) const
	{ return p ? _data_prop_max[x] : _data_flat_max[x]; }
	float minAt (unsigned int x, bool p) const
	{ return p ? _data_prop_min[x] : _data_flat_min[x]; }

	float minimum (bool p) const
	{ return p ? _min_prop : _min_flat; }
	float maximum (bool p) const
	{ return p ? _max_prop : _max_flat; }

	const Gdk::Color& get_color () const { return _color; }

private:
	FFTResult (FFTGraph *graph, Gdk::Color color, std::string trackname);
	friend class FFTGraph;

	int _averages;

	float* _data_flat_avg;
	float* _data_flat_max;
	float* _data_flat_min;
	float* _data_prop_avg;
	float* _data_prop_max;
	float* _data_prop_min;

	unsigned int _windowSize;
	unsigned int _dataSize;

	float _min_flat;
	float _max_flat;
	float _min_prop;
	float _max_prop;

	FFTGraph *_graph;

	Gdk::Color _color;
	std::string _trackname;

	static float power_to_db (float v) { return v > 1e-20 ? 10.0f * log10f (v) : -200.0f; }
};

#endif /* __ardour_fft_result_h */
