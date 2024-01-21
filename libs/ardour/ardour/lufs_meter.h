/*
 * Copyright (C) 2016,2023 Robin Gareus <robin@gareus.org>
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
#ifndef _lufs_meter_h_
#define _lufs_meter_h_

#include <cstdint>
#include <functional>
#include <map>

#include "pbd/stack_allocator.h"

#include "ardour/libardour_visibility.h"

namespace ARDOUR {

class LIBARDOUR_API LUFSMeter
{
public:
	LUFSMeter (double samplerate, uint32_t n_channels);
	LUFSMeter (LUFSMeter const& other) = delete;
	~LUFSMeter ();

	void run (float const** data, uint32_t n_samples);
	void reset ();

	float integrated_loudness () const;
	float momentary () const;
	float max_momentary () const;
	float dbtp () const;

private:
	void init ();

	float process (float const** data, const uint32_t n_samples, uint32_t offset);
	float sumfrag (uint32_t) const;

	void  calc_true_peak (float const** data, const uint32_t n_samples);
	float upsample_x4 (int chn, float const x);
	float upsample_x2 (int chn, float const x);
	std::function< float(int, const float) > upsample;

	const float _g[5] = { 1.0, 1.0, 1.0, 1.41, 1.41 };

	/* config */
	double   _samplerate;
	uint32_t _n_channels;
	uint32_t _n_fragment;

	/* filter coeff */
	float _a0, _a1, _a2;
	float _b1, _b2;
	float _c3, _c4;

	/* state */
	uint32_t _frag_pos;
	float    _frag_pwr;
	uint32_t _block_cnt;
	float    _block_pwr;
	float    _power[8];
	uint32_t _pow_idx;
	float    _thresh_rel;

	float    _momentary_l;

	float    _maxloudn_M;
	float    _integrated;
	float    _dbtp;

#if defined(_MSC_VER)
	typedef std::map<int, uint32_t> History;
#else
	typedef std::map<int, uint32_t, std::less<int>, PBD::StackAllocator<std::pair<const int, uint32_t>, 1000>> History;
#endif

	History _hist;

	struct FilterState {
		void reset ();
		void sanitize ();

		float z1, z2, z3, z4;
	};

	FilterState _fst[5];
	float*      _z[5];
};

} // namespace ARDOUR
#endif
