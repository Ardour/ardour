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

#if defined(HAVE_COREAUDIO) || defined(HAVE_AUDIOUNITS)
#include <CoreFoundation/CoreFoundation.h>
#endif

#include "ardour.h"

class XMLNode;

Glib::ustring legalize_for_path (Glib::ustring str);
std::ostream& operator<< (std::ostream& o, const ARDOUR::BBT_Time& bbt);
XMLNode* find_named_node (const XMLNode& node, std::string name);
std::string bool_as_string (bool);
bool string_is_affirmative (const std::string&);

static inline float f_max(float x, float a) {
	x -= a;
	x += fabsf (x);
	x *= 0.5f;
	x += a;
	
	return (x);
}

std::string bump_name_once(std::string s);

int cmp_nocase (const std::string& s, const std::string& s2);

int touch_file(Glib::ustring path);

Glib::ustring path_expand (Glib::ustring);
Glib::ustring region_name_from_path (Glib::ustring path, bool strip_channels, bool add_channel_suffix = false, uint32_t total = 0, uint32_t this_one = 0);
bool path_is_paired (Glib::ustring path, Glib::ustring& pair_base);

void compute_equal_power_fades (nframes_t nframes, float* in, float* out);

const char* slave_source_to_string (ARDOUR::SlaveSource src);
ARDOUR::SlaveSource string_to_slave_source (std::string str);

const char* edit_mode_to_string (ARDOUR::EditMode);
ARDOUR::EditMode string_to_edit_mode (std::string);


/* I don't really like hard-coding these falloff rates here
 * Probably should use a map of some kind that could be configured
 * These rates are db/sec.
*/

#define METER_FALLOFF_OFF     0.0f
#define METER_FALLOFF_SLOWEST 6.6f // BBC standard
#define METER_FALLOFF_SLOW    8.6f // BBC standard
#define METER_FALLOFF_MEDIUM  20.0f
#define METER_FALLOFF_FAST    32.0f
#define METER_FALLOFF_FASTER  46.0f
#define METER_FALLOFF_FASTEST 70.0f

float meter_falloff_to_float (ARDOUR::MeterFalloff);
ARDOUR::MeterFalloff meter_falloff_from_float (float);
float meter_falloff_to_db_per_sec (float);

#if defined(HAVE_COREAUDIO) || defined(HAVE_AUDIOUNITS)
std::string CFStringRefToStdString(CFStringRef stringRef);
#endif // HAVE_COREAUDIO

#endif /* __ardour_utils_h__ */

