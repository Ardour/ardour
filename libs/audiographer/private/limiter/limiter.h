/*
 * Copyright (C) 2010-2018 Fons Adriaensen <fons@linuxaudio.org>
 * Copyright (C) 2021 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _PEAKLIM_H
#define _PEAKLIM_H

#include <stdint.h>

namespace AudioGrapherDSP {

class Limiter
{
public:
	Limiter ();
	~Limiter ();

	void init (float fsamp, int nchan);
	void fini ();

	void set_inpgain (float);
	void set_threshold (float);
	void set_release (float);
	void set_truepeak (bool);

	int
	get_latency () const
	{
		return _delay;
	}

	void
	get_stats (float* peak, float* gmax, float* gmin)
	{
		*peak  = _peak;
		*gmax  = _gmax;
		*gmin  = _gmin;
		_rstat = true;
	}

	void process (int nsamp, float const* inp, float* out);

private:
	class Histmin
	{
	public:
		void  init (int hlen);
		float write (float v);
		float vmin () { return _vmin; }

	private:
		enum {
			SIZE = 32,
			MASK = SIZE - 1
		};

		int   _hlen;
		int   _hold;
		int   _wind;
		float _vmin;
		float _hist[SIZE];
	};

	class Upsampler
	{
	public:
		Upsampler ();
		~Upsampler ();

		void init (int nchan);
		void fini ();

		int
		get_latency () const
		{
			return 23;
		}

		float process_one (int chn, float const x);

	private:
		int     _nchan;
		float** _z;
	};

	float _fsamp;
	int   _nchan;
	bool  _truepeak;

	float** _dly_buf;
	float*  _zlf;

	int   _delay;
	int   _dly_mask;
	int   _dly_ridx;
	int   _div1, _div2;
	int   _c1, _c2;
	float _g0, _g1, _dg;
	float _gt, _m1, _m2;
	float _w1, _w2, _w3, _wlf;
	float _z1, _z2, _z3;

	bool  _rstat;
	float _peak;
	float _gmax;
	float _gmin;

	Upsampler _upsampler;
	Histmin   _hist1;
	Histmin   _hist2;
};

}
#endif
