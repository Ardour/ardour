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
#include <string.h>
#include <assert.h>
#include <glib.h>
#include "ardour/libardour_visibility.h"

namespace ARDOUR { namespace DSP {

	/** C/C++ Shared Memory
	 *
	 * A convenience class representing a C array of float[] or int32_t[]
	 * data values. This is useful for lua scripts to perform DSP operations
	 * directly using C/C++ with CPU Hardware acceleration.
	 *
	 * Access to this memory area is always 4 byte aligned. The data
	 * is interpreted either as float or as int.
	 *
	 * This memory area can also be shared between different instances
	 * or the same lua plugin (DSP, GUI).
	 *
	 * Since memory allocation is not realtime safe it should be
	 * allocated during dsp_init() or dsp_configure().
	 * The memory is free()ed automatically when the lua instance is
	 * destroyed.
	 */
	class DspShm {
		public:
			DspShm ()
				: _data (0)
				, _size (0)
			{
				assert (sizeof(float) == sizeof (int32_t));
				assert (sizeof(float) == sizeof (int));
			}

			~DspShm () {
				free (_data);
			}

			/** [re] allocate memory in host's memory space
			 *
			 * @param s size, total number of float or integer elements to store.
			 */
			void allocate (size_t s) {
				_data = realloc (_data, sizeof(float) * s);
				if (_data) { _size = s; }
			}

			/** clear memory (set to zero) */
			void clear () {
				memset (_data, 0, sizeof(float) * _size);
			}

			/** access memory as float array
			 *
			 * @param off offset in shared memory region
			 * @returns float[]
			 */
			float* to_float (size_t off) {
				if (off >= _size) { return 0; }
				return &(((float*)_data)[off]);
			}

			/** access memory as integer array
			 *
			 * @param off offset in shared memory region
			 * @returns int_32_t[]
			 */
			int32_t* to_int (size_t off) {
				if (off >= _size) { return 0; }
				return &(((int32_t*)_data)[off]);
			}

			/** atomically set integer at offset
			 *
			 * This involves a memory barrier. This call
			 * is intended for buffers which are
			 * shared with another instance.
			 *
			 * @param off offset in shared memory region
			 * @param val value to set
			 */
			void atomic_set_int (size_t off, int32_t val) {
				g_atomic_int_set (&(((int32_t*)_data)[off]), val);
			}

			/** atomically read integer at offset
			 *
			 * This involves a memory barrier. This call
			 * is intended for buffers which are
			 * shared with another instance.
			 *
			 * @param off offset in shared memory region
			 * @returns value at offset
			 */
			int32_t atomic_get_int (size_t off) {
				return g_atomic_int_get (&(((int32_t*)_data)[off]));
			}

		private:
			void* _data;
			size_t _size;
	};

	/** lua wrapper to memset() */
	void memset (float *data, const float val, const uint32_t n_samples);
	/** matrix multiply
	 * multiply every sample of `data' with the corresponding sample at `mult'.
	 *
	 * @param data multiplicand
	 * @param mult multiplicand
	 * @param n_samples number of samples in data and mmult
	 */
	void mmult (float *data, float *mult, const uint32_t n_samples);
	/** calculate peaks
	 *
	 * @param data data to analyze
	 * @param min result, minimum value found in range
	 * @param max result, max value found in range
	 * @param n_samples number of samples to analyze
	 */
	void peaks (float *data, float &min, float &max, uint32_t n_samples);

	/** non-linear power-scale meter deflection
	 *
	 * @param power signal power (dB)
	 * @returns deflected value
	 */
	float log_meter (float power);
	/** non-linear power-scale meter deflection
	 *
	 * @param coeff signal value
	 * @returns deflected value
	 */
	float log_meter_coeff (float coeff);

	/** 1st order Low Pass filter */
	class LIBARDOUR_API LowPass {
		public:
			/** instantiate a LPF
			 *
			 * @param samplerate samplerate
			 * @param freq cut-off frequency
			 */
			LowPass (double samplerate, float freq);
			/** process audio data
			 *
			 * @param data pointer to audio-data
			 * @param n_samples number of samples to process
			 */
			void proc (float *data, const uint32_t n_samples);
			/** filter control data
			 *
			 * This is useful for parameter smoothing.
			 *
			 * @param data pointer to control-data array
			 * @param val target value
			 * @param array length
			 */
			void ctrl (float *data, const float val, const uint32_t n_samples);
			/** update filter cut-off frequency
			 *
			 * @param freq cut-off frequency
			 */
			void set_cutoff (float freq);
			/** reset filter state */
			void reset () { _z =  0.f; }
		private:
			float _rate;
			float _z;
			float _a;
	};

	/** Biquad Filter */
	class LIBARDOUR_API Biquad {
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

			/** Instantiate Biquad Filter
			 *
			 * @param samplerate Samplerate
			 */
			Biquad (double samplerate);
			Biquad (const Biquad &other);

			/** process audio data
			 *
			 * @param data pointer to audio-data
			 * @param n_samples number of samples to process
			 */
			void run (float *data, const uint32_t n_samples);
			/** setup filter, compute coefficients
			 *
			 * @param t filter type (LowPass, HighPass, etc)
			 * @param freq filter frequency
			 * @param Q filter quality
			 * @param gain filter gain
			 */
			void compute (Type t, double freq, double Q, double gain);

			/** filter transfer function (filter response for spectrum visualization)
			 * @param freq frequency
			 * @return gain at given frequency in dB (clamped to -120..+120)
			 */
			float dB_at_freq (float freq) const;

			/** reset filter state */
			void reset () { _z1 = _z2 = 0.0; }
		private:
			double _rate;
			float  _z1, _z2;
			double _a1, _a2;
			double _b0, _b1, _b2;
	};

} } /* namespace */
#endif
