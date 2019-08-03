/*
 * Copyright (C) 2010-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#include <math.h>

#include "pbd/cartesian.h"

using namespace std;

void
PBD::spherical_to_cartesian (double azi, double ele, double len, double& x, double& y, double& z)
{
	/* convert from cylindrical coordinates in degrees to cartesian */

	static const double atorad = 2.0 * M_PI / 360.0 ;

        if (len == 0.0) {
                len = 1.0;
        }

	x = len * cos (azi * atorad) * cos (ele * atorad);
	y = len * sin (azi * atorad) * cos (ele * atorad);
	z = len * sin (ele * atorad);
}

void
PBD::cartesian_to_spherical (double x, double y, double z, double& azimuth, double& elevation, double& length)
{
	/* converts cartesian coordinates to cylindrical in degrees*/

        double rho, theta, phi;

        rho = sqrt (x*x + y*y + z*z);
        //phi = acos (1.0 / rho);
        theta = atan2 (y, x);

        /* XXX for now, clamp phi to zero */

        phi = 0.0;

        if (theta < 0.0) {
                azimuth = 180.0 - (180.0 * (theta / M_PI)); /* LHS is negative */
        } else {
                azimuth = 180.0 * (theta / M_PI);
        }

        if (phi < 0.0) {
                elevation = 180.0 - (180.0 * (phi / M_PI)); /* LHS is negative */
        } else {
                elevation = 180.0 * (phi /  M_PI);
        }

        length = rho;
}

