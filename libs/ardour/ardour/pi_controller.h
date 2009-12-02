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

class PIController {

  public:
    PIController (double resample_factor, int fir_size);
    ~PIController();
        
    void reset (double resample_factor) {
            resample_mean = resample_factor;
            static_resample_factor = resample_factor;
	    out_of_bounds ();
    }
        
    double get_ratio (int fill_level);
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
};

#endif /* __libardour_pi_controller__ */
