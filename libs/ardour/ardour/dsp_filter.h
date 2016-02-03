/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */
#ifndef _dsp_filter_h_
#define _dsp_filter_h_

#include <stdint.h>
#include "ardour/libardour_visibility.h"

namespace ARDOUR { namespace DSP {

	void memset (float *data, const float val, const uint32_t n_samples);
	void mmult (float *data, float *mult, const uint32_t n_samples);


	class LIBARDOUR_API LowPass {
		public:
			LowPass (double samplerate, float freq);
			void proc (float *data, const uint32_t n_samples);
			void ctrl (float *data, const float val, const uint32_t n_samples);
			void set_cutoff (float freq);
			void reset () { _z =  0.f; }
		private:
			float _rate;
			float _z;
			float _a;
	};

	class LIBARDOUR_API BiQuad {
		public:
			enum Type {
				LowPass,
				HighPass,
				BandPassSkirt,
				BandPass0dB,
				Notch,
				AllPass,
				Peaking,
				LowShelf,
				HighShelf
			};

			BiQuad (double samplerate);
			BiQuad (const BiQuad &other);

			void run (float *data, const uint32_t n_samples);
			void compute (Type, double freq, double Q, double gain);
			void reset () { _z1 = _z2 = 0.0; }
		private:
			double _rate;
			float  _z1, _z2;
			double _a1, _a2;
			double _b0, _b1, _b2;
	};

} } /* namespace */
#endif
