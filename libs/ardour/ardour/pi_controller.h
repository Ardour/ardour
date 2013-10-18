/*
  Copyright (C) 2008 Torben Hohn

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

#ifndef __libardour_pi_controller__
#define __libardour_pi_controller__

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"

class LIBARDOUR_API PIController {
public:
	PIController (double resample_factor, int fir_size);
	~PIController();

	void reset (double resample_factor) {
		resample_mean = resample_factor;
		static_resample_factor = resample_factor;
		out_of_bounds ();
	}

	double get_ratio (int fill_level, int period_size);
	void out_of_bounds();

public:
	double  resample_mean;
	double  static_resample_factor;
	double* offset_array;
	double* window_array;
	int     offset_differential_index;
	double  offset_integral;
	double  catch_factor;
	double  catch_factor2;
	double  pclamp;
	double  controlquant;
	int     smooth_size;
	double  smooth_offset;
	double  current_resample_factor;
	bool    fir_empty;
};

#define ESTIMATOR_SIZE 16

class LIBARDOUR_API PIChaser {
public:
	PIChaser();
	~PIChaser();

	double get_ratio( framepos_t chasetime_measured, framepos_t chasetime, framepos_t slavetime_measured, framepos_t slavetime, bool in_control, int period_size );
	void reset();
	framepos_t want_locate() { return want_locate_val; }

private:
	PIController *pic;
	framepos_t realtime_stamps[ESTIMATOR_SIZE];
	framepos_t chasetime_stamps[ESTIMATOR_SIZE];
	int array_index;
	framepos_t want_locate_val;

	void feed_estimator( framepos_t realtime, framepos_t chasetime );
	double get_estimate();

	double speed;

	double speed_threshold;
	framepos_t pos_threshold;
};

#endif /* __libardour_pi_controller__ */
