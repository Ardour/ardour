// ------------------------------------------------------------------------
//
//  Copyright (C) 2010-2011 Fons Adriaensen <fons@linuxaudio.org>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
// ------------------------------------------------------------------------

#ifndef _EBU_R128_PROC_H
#define _EBU_R128_PROC_H

#define MAXCH 5

namespace FonsEBU {

class Ebu_r128_proc
{
public:
	Ebu_r128_proc ();
	~Ebu_r128_proc ();

	void init (int nchan, float fsamp);
	void reset ();
	void process (int nfram, const float* const* input);

	void integr_reset ();
	void integr_pause () { _integr = false; }
	void integr_start () { _integr = true; }

	float loudness_M () const { return _loudness_M; }
	float maxloudn_M () const { return _maxloudn_M; }
	float loudness_S () const { return _loudness_S; }
	float maxloudn_S () const { return _maxloudn_S; }
	float integrated () const { return _integrated; }

	float integ_thr () const { return _integ_thr; }
	float range_min () const { return _range_min; }
	float range_max () const { return _range_max; }
	float range_thr () const { return _range_thr; }

	const int* histogram_M () const { return _hist_M._histc; }
	const int* histogram_S () const { return _hist_S._histc; }

	int hist_M_count () const { return _hist_M._count; }
	int hist_S_count () const { return _hist_S._count; }

private:
	class Ebu_r128_fst
	{
	private:
		friend class Ebu_r128_proc;

		void reset ()
		{
			_z1 = _z2 = _z3 = _z4 = 0;
		}

		float _z1, _z2, _z3, _z4;
	};

	class Ebu_r128_hist
	{
	private:
		Ebu_r128_hist ();
		~Ebu_r128_hist ();

		friend class Ebu_r128_proc;

		void  reset ();
		void  initstat ();
		void  addpoint (float v);
		float integrate (int ind);
		void  calc_integ (float* vi, float* th);
		void  calc_range (float* v0, float* v1, float* th);

		int* _histc;
		int  _count;
		int  _error;

		static float _bin_power[100];
	};

	float addfrags (int nfrag);
	void  detect_init (float fsamp);
	void  detect_reset ();
	float detect_process (int nfram);

	bool  _integr;    // Integration on/off.
	int   _nchan;     // Number of channels, 2 or 5.
	float _fsamp;     // Sample rate.
	int   _fragm;     // Fragmenst size, 1/20 second.
	int   _frcnt;     // Number of samples remaining in current fragment.
	float _frpwr;     // Power accumulated for current fragment.
	float _power[64]; // Array of fragment powers.
	int   _wrind;     // Write index into _frpwr
	int   _div1;      // M period counter, 200 ms;
	int   _div2;      // S period counter, 1s;
	float _loudness_M;
	float _maxloudn_M;
	float _loudness_S;
	float _maxloudn_S;
	float _integrated;
	float _integ_thr;
	float _range_min;
	float _range_max;
	float _range_thr;

	// Filter coefficients and states.
	float         _a0, _a1, _a2;
	float         _b1, _b2;
	float         _c3, _c4;
	float const*  _ipp[MAXCH];
	Ebu_r128_fst  _fst[MAXCH];
	Ebu_r128_hist _hist_M;
	Ebu_r128_hist _hist_S;

	// Default channel gains.
	static float _chan_gain[5];
};

};

#endif
