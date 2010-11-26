/*
    Copyright (C) 2010 Paul Davis

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
#include <math.h>

#include "pbd/cartesian.h"

using namespace std;

void
PBD::azi_ele_to_cart (double azi, double ele, double& x, double& y, double& z)
{
        /* convert from cylindrical coordinates in degrees to cartesian */

        static const double atorad = 2.0 * M_PI / 360.0 ;

        x = cos (azi * atorad) * cos (ele * atorad);
        y = sin (azi * atorad) * cos (ele * atorad);
        z = sin (ele * atorad);
}

void 
PBD::cart_to_azi_ele (double x, double y, double z, double& azimuth, double& elevation)
{
        /* converts cartesian coordinates to cylindrical in degrees*/

        const double atorad = 2.0 * M_PI / 360.0;
        double atan_y_per_x, atan_x_pl_y_per_z;
        double distance;

        if(x == 0.0) {
                atan_y_per_x = M_PI / 2;
        } else {
                atan_y_per_x = atan2 (y,x);
        }

        if (y < 0.0) {
                /* below x-axis: atan2 returns 0 .. -PI (negative) so convert to degrees and ADD to 180 */
                azimuth = 180.0 + (atan_y_per_x / (M_PI/180.0) + 180.0);
        } else {
                /* above x-axis: atan2 returns 0 .. +PI so convert to degrees */
                azimuth = atan_y_per_x / atorad;
        }

        distance = sqrt (x*x + y*y);

        if (z == 0.0) {
                atan_x_pl_y_per_z = 0.0;
        } else {
                atan_x_pl_y_per_z = atan2 (z,distance);
        }

        if (distance == 0.0) {
                if (z < 0.0) {
                        atan_x_pl_y_per_z = -M_PI/2.0;
                } else if (z > 0.0) {
                        atan_x_pl_y_per_z = M_PI/2.0;
                }
        }

        elevation = atan_x_pl_y_per_z / atorad;

        // distance = sqrtf (x*x + y*y + z*z);
}

