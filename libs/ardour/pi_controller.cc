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

#include <iostream>
#include <cmath>
#include <cstdlib>

#include "ardour/pi_controller.h"

static inline double hann(double x) {
	return 0.5 * (1.0 - cos(2 * M_PI * x));
}
    
PIController::PIController (double resample_factor, int fir_size) 
{
	resample_mean = resample_factor;
	static_resample_factor = resample_factor;
	offset_array = new double[fir_size];
	window_array = new double[fir_size];
	offset_differential_index = 0;
	offset_integral = 0.0;
	smooth_size = fir_size;
        
	for (int i = 0; i < fir_size; i++) {
                offset_array[i] = 0.0;
                window_array[i] = hann(double(i) / (double(fir_size) - 1.0));
	}
	
	// These values could be configurable
	catch_factor = 20000;
	catch_factor2 = 4000;
	pclamp = 150.0;
	controlquant = 10000.0;
	fir_empty = false;
}

PIController::~PIController ()
{
	delete [] offset_array;
	delete [] window_array;
}

double
PIController::get_ratio (int fill_level)
{
	double offset = fill_level;
	double this_catch_factor = catch_factor;

	
	// Save offset.
	if( fir_empty ) {
	    for (int i = 0; i < smooth_size; i++) {
		    offset_array[i] = offset;
	    }
	    fir_empty = false;
	} else {
	    offset_array[(offset_differential_index++) % smooth_size] = offset;
	}
        
	// Build the mean of the windowed offset array basically fir lowpassing.
	smooth_offset = 0.0;
	for (int i = 0; i < smooth_size; i++) {
                smooth_offset += offset_array[(i + offset_differential_index - 1) % smooth_size] * window_array[i];
	}
	smooth_offset /= double(smooth_size);
        
	// This is the integral of the smoothed_offset
	offset_integral += smooth_offset;

	std::cerr << smooth_offset << " ";
	
	// Clamp offset : the smooth offset still contains unwanted noise which would go straigth onto the resample coeff.
	// It only used in the P component and the I component is used for the fine tuning anyways.
    
	if (fabs(smooth_offset) < pclamp)
                smooth_offset = 0.0;
	
	smooth_offset += (static_resample_factor - resample_mean) * this_catch_factor;
	
	// Ok, now this is the PI controller. 
	// u(t) = K * (e(t) + 1/T \int e(t') dt')
	// Kp = 1/catch_factor and T = catch_factor2  Ki = Kp/T 
	current_resample_factor 
                = static_resample_factor - smooth_offset / this_catch_factor - offset_integral / this_catch_factor / catch_factor2;
	
	// Now quantize this value around resample_mean, so that the noise which is in the integral component doesnt hurt.
	current_resample_factor = floor((current_resample_factor - resample_mean) * controlquant + 0.5) / controlquant + resample_mean;
	
	// Calculate resample_mean so we can init ourselves to saner values.
	// resample_mean = 0.9999 * resample_mean + 0.0001 * current_resample_factor;
	resample_mean = (1.0-0.01) * resample_mean + 0.01 * current_resample_factor;
	std::cerr << fill_level << " " << smooth_offset << " " << offset_integral << " " << current_resample_factor << " " << resample_mean << "\n";
	return current_resample_factor;
}
        
void 
PIController::out_of_bounds()
{
	int i;
	// Set the resample_rate... we need to adjust the offset integral, to do this.
	// first look at the PI controller, this code is just a special case, which should never execute once
	// everything is swung in. 
	offset_integral = - (resample_mean - static_resample_factor) * catch_factor * catch_factor2;
	// Also clear the array. we are beginning a new control cycle.
	for (i = 0; i < smooth_size; i++) {
                offset_array[i] = 0.0;
	}
	fir_empty = false;
}


PIChaser::PIChaser() {
	pic = new PIController( 1.0, 16 );
	array_index = 0;
	for( int i=0; i<ESTIMATOR_SIZE; i++ ) {
	    realtime_stamps[i] = 0;
	    chasetime_stamps[i] = 0;
	}

	speed_threshold = 0.2;
	pos_threshold = 4000;
	want_locate_val = 0;
}

void
PIChaser::reset() {
	array_index = 0;
	for( int i=0; i<ESTIMATOR_SIZE; i++ ) {
	    realtime_stamps[i] = 0;
	    chasetime_stamps[i] = 0;
	}
	pic->reset(1.0);
}
PIChaser::~PIChaser() {
	delete pic;
}

double
PIChaser::get_ratio(nframes64_t realtime, nframes64_t chasetime, nframes64_t slavetime, bool in_control ) {

	feed_estimator( realtime, chasetime );
	std::cerr << (double)realtime/48000.0 << " " << chasetime << " " << slavetime << " ";
	double crude = get_estimate();
	double fine;  

	    fine = pic->get_ratio( slavetime - chasetime );
	if (in_control) {
	    if (fabs(fine-crude) > crude*speed_threshold) {
		std::cout << "reset to " << crude << " fine = " << fine << "\n";
		pic->reset( crude );
		speed = crude;
	    } else {
		speed = fine;
	    }

	    if (abs(chasetime-slavetime) > pos_threshold) {
		pic->reset( crude );
		speed = crude;
		want_locate_val = chasetime;
		std::cout << "we are off by " << chasetime-slavetime << " want_locate:" << chasetime << "\n";
	    } else {
		want_locate_val = 0;
	    }
	} else {
	    std::cout << "not in control..." << crude << "\n";
	    speed = crude;
	    pic->reset( crude );
	}
	
	return speed;
}

void
PIChaser::feed_estimator( nframes64_t realtime, nframes64_t chasetime ) {
	array_index += 1;
	realtime_stamps [ array_index%ESTIMATOR_SIZE ] = realtime;
	chasetime_stamps[ array_index%ESTIMATOR_SIZE ] = chasetime;
}

double
PIChaser::get_estimate() {
	double est = 0;
	int num=0;
	int i;
	nframes64_t n1_realtime;
	nframes64_t n1_chasetime;
	for( i=(array_index + 1); i<=(array_index + ESTIMATOR_SIZE); i++ ) {
	    if( realtime_stamps[(i)%ESTIMATOR_SIZE] ) {
		n1_realtime = realtime_stamps[(i)%ESTIMATOR_SIZE];
		n1_chasetime = chasetime_stamps[(i)%ESTIMATOR_SIZE];
		i+=1;
		break;
	    }
	}

	for( ; i<=(array_index + ESTIMATOR_SIZE); i++ ) {
	    if( realtime_stamps[(i)%ESTIMATOR_SIZE] ) {
		if( (realtime_stamps[(i)%ESTIMATOR_SIZE] - n1_realtime) > 200 ) {
		    nframes64_t n_realtime = realtime_stamps[(i)%ESTIMATOR_SIZE];
		    nframes64_t n_chasetime = chasetime_stamps[(i)%ESTIMATOR_SIZE];
		    est += ((double)( n_chasetime - n1_chasetime ))
			  / ((double)( n_realtime - n1_realtime ));
		    n1_realtime = n_realtime;
		    n1_chasetime = n_chasetime;
		    num += 1;
		}
	    }
	}

	if(num)
	    return est/(double)num;
	else
	    return 0.0;
}
