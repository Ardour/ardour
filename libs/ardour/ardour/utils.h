/*
    Copyright (C) 1999 Paul Davis

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

#ifndef __ardour_utils_h__
#define __ardour_utils_h__

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include <iostream>
#include <string>
#include <cmath>

#if __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif /* __APPLE__ */

#include "ardour/ardour.h"
#include "ardour/data_type.h"
#include "ardour/dB.h"

class XMLNode;

std::string legalize_for_path (const std::string& str);
std::string legalize_for_universal_path (const std::string& str);
std::string legalize_for_uri (const std::string& str);
std::string legalize_for_path_2X (const std::string& str);
XMLNode* find_named_node (const XMLNode& node, std::string name);
std::string bool_as_string (bool);

static inline float f_max(float x, float a) {
	x -= a;
	x += fabsf (x);
	x *= 0.5f;
	x += a;

	return (x);
}

std::string bump_name_once(const std::string& s, char delimiter);

int cmp_nocase (const std::string& s, const std::string& s2);
int cmp_nocase_utf8 (const std::string& s1, const std::string& s2);

int touch_file(std::string path);

std::string region_name_from_path (std::string path, bool strip_channels, bool add_channel_suffix = false, uint32_t total = 0, uint32_t this_one = 0);
bool path_is_paired (std::string path, std::string& pair_base);

void compute_equal_power_fades (ARDOUR::framecnt_t nframes, float* in, float* out);

const char* sync_source_to_string (ARDOUR::SyncSource src, bool sh = false);
ARDOUR::SyncSource string_to_sync_source (std::string str);

const char* edit_mode_to_string (ARDOUR::EditMode);
ARDOUR::EditMode string_to_edit_mode (std::string);

#undef  OLD_GAIN_MATH
#define OLD_GAIN_MATH

static inline double
gain_to_slider_position (ARDOUR::gain_t g)
{
	if (g == 0) return 0;

#ifndef OLD_GAIN_MATH
	/* Power Law With Exponential Cutoff 2D, fit to data from TC Spectra
	   console (image of fader gradations

	   y = C * x(-T) * exp(-x/K)

	   C =  8.2857630370864188E-01
	   T = -5.1526743785019269E-01
	   K =  7.8990885960495589E+00

	 */
	
	return 8.2857630370864188E-01 * pow(g,5.1526743785019269E-01) * exp (-g/7.8990885960495589E+00);
#else
	return pow((6.0*log(g)/log(2.0)+192.0)/198.0, 8.0);
#endif
}

static inline ARDOUR::gain_t
slider_position_to_gain (double pos)
{
	if (pos == 0.0) {
		return 0.0;
	}

#ifndef OLD_GAIN_MATH
	/* 5th order polynomial function fit to data from a TC Spectra console
	   fader (image of fader gradations).
	 
	   y = a + bx1 + cx2 + dx3 + fx4 + gx5
	   
	   a = -1.1945480381045521E-02
	   b =  1.5809476525537265E+00
	   c = -1.5850710838966151E+01
	   d =  6.1643128605961991E+01
	   f = -8.5525246160607693E+01
	   g =  4.1312725896188283E+01

	*/

	double p = pos;
	double g = -1.1945480381045521E-02;

	g +=  1.5809476525537265E+00 * pos;
	pos *= p;
	g += -1.5850710838966151E+01 * pos;
	pos *= p;
	g += 6.1643128605961991E+01 * pos;
	pos *= p;
	g += -8.5525246160607693E+01 * pos;
	pos *= p;
	g += 4.1312725896188283E+01 * pos;

	return g;
#else
	/* XXX Marcus writes: this doesn't seem right to me. but i don't have a better answer ... */
	if (pos == 0.0) return 0;
	return pow (2.0,(sqrt(sqrt(sqrt(pos)))*198.0-192.0)/6.0);
#endif
}
#undef OLD_GAIN_MATH

double gain_to_slider_position_with_max (double g, double max_gain = 2.0);
double slider_position_to_gain_with_max (double g, double max_gain = 2.0);

/* I don't really like hard-coding these falloff rates here
 * Probably should use a map of some kind that could be configured
 * These rates are db/sec.
*/

#define METER_FALLOFF_OFF       0.0f
#define METER_FALLOFF_SLOWEST   6.6f  // BBC standard
#define METER_FALLOFF_SLOW      8.6f  // BBC standard, EBU  24dB / 2.8sec
#define METER_FALLOFF_SLOWISH   12.0f // DIN  20dB / 1.7 sec
#define METER_FALLOFF_MODERATE  13.3f // EBU-PPM, IRT PPM-   20dB / 1.5 sec
#define METER_FALLOFF_MEDIUM    20.0f
#define METER_FALLOFF_FAST      32.0f
#define METER_FALLOFF_FASTER    46.0f
#define METER_FALLOFF_FASTEST   70.0f

float meter_falloff_to_float (ARDOUR::MeterFalloff);
ARDOUR::MeterFalloff meter_falloff_from_float (float);
float meter_falloff_to_db_per_sec (float);

const char* native_header_format_extension (ARDOUR::HeaderFormat, const ARDOUR::DataType& type);
bool matching_unsuffixed_filename_exists_in (const std::string& dir, const std::string& name);

uint32_t how_many_dsp_threads ();

#if __APPLE__
std::string CFStringRefToStdString(CFStringRef stringRef);
#endif // __APPLE__

#endif /* __ardour_utils_h__ */

