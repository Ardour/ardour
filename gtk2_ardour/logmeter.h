/*
    Copyright (C) 2000-2007 Paul Davis

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

#ifndef __ardour_gtk_log_meter_h__
#define __ardour_gtk_log_meter_h__

#if 1
static inline float
_log_meter (float power, double lower_db, double upper_db, double non_linearity)
{
	return (power < lower_db ? 0.0 : pow((power-lower_db)/(upper_db-lower_db), non_linearity));
}

static inline float
alt_log_meter (float power)
{
	return _log_meter (power, -192.0, 0.0, 8.0);
}
#endif

/* prototypes - avoid compiler warning */
inline float log_meter (float db);
inline float meter_deflect_ppm (float);
inline float meter_deflect_din (float);
inline float meter_deflect_nordic (float);
inline float meter_deflect_vu (float);
inline float meter_deflect_k (float, float);



inline float
log_meter (float db)
{
         gfloat def = 0.0f; /* Meter deflection %age */

         if (db < -70.0f) {
                 def = 0.0f;
         } else if (db < -60.0f) {
                 def = (db + 70.0f) * 0.25f;
         } else if (db < -50.0f) {
                 def = (db + 60.0f) * 0.5f + 2.5f;
         } else if (db < -40.0f) {
                 def = (db + 50.0f) * 0.75f + 7.5f;
         } else if (db < -30.0f) {
                 def = (db + 40.0f) * 1.5f + 15.0f;
         } else if (db < -20.0f) {
                 def = (db + 30.0f) * 2.0f + 30.0f;
         } else if (db < 6.0f) {
                 def = (db + 20.0f) * 2.5f + 50.0f;
         } else {
		 def = 115.0f;
	 }

	 /* 115 is the deflection %age that would be
	    when db=6.0. this is an arbitrary
	    endpoint for our scaling.
	 */

         return def/115.0f;
}

inline float
meter_deflect_ppm (float db)
{
	if (db < -30) {
		// 2.258 == ((-30 + 32.0)/ 28.0) / 10^(-30 / 20);
		return (dB_to_coefficient(db) * 2.258769757f);
	} else {
		const float rv = (db + 32.0f) / 28.0f;
		if (rv < 1.0) {
			return rv;
		} else {
			return 1.0;
		}
	}
}

inline float
meter_deflect_din (float db)
{
	float rv = dB_to_coefficient(db);
	rv = sqrtf (sqrtf (2.3676f * rv)) - 0.1803f;
	if (rv >= 1.0) {
		return 1.0;
	} else {
		return (rv > 0 ? rv : 0.0);
	}
}

inline float
meter_deflect_nordic (float db)
{
	if (db < -60) {
		return 0.0;
	} else {
		const float rv = (db + 60.0f) / 54.0f;
		if (rv < 1.0) {
			return rv;
		} else {
			return 1.0;
		}
	}
}

inline float
meter_deflect_vu (float db)
{
	const float rv = 6.77165f * dB_to_coefficient(db);
	if (rv > 1.0) return 1.0;
	return rv;
}

inline float
meter_deflect_k (float db, float krange)
{
	db+=krange;
	if (db < -40.0f) {
		return (dB_to_coefficient(db) * 500.0f / (krange + 45.0f));
	} else {
		const float rv = (db + 45.0f) / (krange + 45.0f);
		if (rv < 1.0) {
			return rv;
		} else {
			return 1.0;
		}
	}
}

#endif /* __ardour_gtk_log_meter_h__ */
