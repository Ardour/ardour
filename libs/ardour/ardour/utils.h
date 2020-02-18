/*
 * Copyright (C) 2000-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005-2006 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2007-2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2008-2013 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_utils_h__
#define __ardour_utils_h__

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include <iostream>
#include <string>
#include <cmath>

#include "boost/shared_ptr.hpp"

#if __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif /* __APPLE__ */

#include "ardour/ardour.h"
#include "ardour/data_type.h"
#include "ardour/dB.h"
#include "ardour/types.h"

#include "ardour/libardour_visibility.h"

class XMLNode;

namespace ARDOUR {

class Route;
class Track;

LIBARDOUR_API std::string legalize_for_path (const std::string& str);
LIBARDOUR_API std::string legalize_for_universal_path (const std::string& str);
LIBARDOUR_API std::string legalize_for_uri (const std::string& str);
LIBARDOUR_API std::string legalize_for_path_2X (const std::string& str);
LIBARDOUR_API XMLNode* find_named_node (const XMLNode& node, std::string name);
LIBARDOUR_API std::string bool_as_string (bool);

static inline float f_max(float x, float a) {
	x -= a;
	x += fabsf (x);
	x *= 0.5f;
	x += a;

	return (x);
}

LIBARDOUR_API std::string bump_name_once(const std::string& s, char delimiter);
LIBARDOUR_API std::string bump_name_number(const std::string& s);

LIBARDOUR_API int cmp_nocase (const std::string& s, const std::string& s2);
LIBARDOUR_API int cmp_nocase_utf8 (const std::string& s1, const std::string& s2);

LIBARDOUR_API std::string region_name_from_path (std::string path, bool strip_channels, bool add_channel_suffix = false, uint32_t total = 0, uint32_t this_one = 0);
LIBARDOUR_API bool path_is_paired (std::string path, std::string& pair_base);

LIBARDOUR_API void compute_equal_power_fades (ARDOUR::samplecnt_t nframes, float* in, float* out);

LIBARDOUR_API const char* edit_mode_to_string (ARDOUR::EditMode);
LIBARDOUR_API ARDOUR::EditMode string_to_edit_mode (std::string);

LIBARDOUR_API double gain_to_slider_position_with_max (double g, double max_gain = 2.0);
LIBARDOUR_API double slider_position_to_gain_with_max (double g, double max_gain = 2.0);

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

LIBARDOUR_API float meter_falloff_to_float (ARDOUR::MeterFalloff);
LIBARDOUR_API ARDOUR::MeterFalloff meter_falloff_from_float (float);
LIBARDOUR_API float meter_falloff_to_db_per_sec (float);

LIBARDOUR_API const char* native_header_format_extension (ARDOUR::HeaderFormat, const ARDOUR::DataType& type);
LIBARDOUR_API bool matching_unsuffixed_filename_exists_in (const std::string& dir, const std::string& name);

LIBARDOUR_API uint32_t how_many_dsp_threads ();

LIBARDOUR_API std::string compute_sha1_of_file (std::string path);

template<typename T> boost::shared_ptr<ControlList> route_list_to_control_list (boost::shared_ptr<RouteList> rl, boost::shared_ptr<T> (Stripable::*get_control)() const) {
	boost::shared_ptr<ControlList> cl (new ControlList);
	if (!rl) { return cl; }
	for (RouteList::const_iterator r = rl->begin(); r != rl->end(); ++r) {
		boost::shared_ptr<AutomationControl> ac = ((*r).get()->*get_control)();
		if (ac) {
			cl->push_back (ac);
		}
	}
	return cl;
}

template<typename T> boost::shared_ptr<ControlList> stripable_list_to_control_list (StripableList& sl, boost::shared_ptr<T> (Stripable::*get_control)() const) {
	boost::shared_ptr<ControlList> cl (new ControlList);
	for (StripableList::const_iterator s = sl.begin(); s != sl.end(); ++s) {
		boost::shared_ptr<AutomationControl> ac = ((*s).get()->*get_control)();
		if (ac) {
			cl->push_back (ac);
		}
	}
	return cl;
}

#if __APPLE__
LIBARDOUR_API std::string CFStringRefToStdString(CFStringRef stringRef);
#endif // __APPLE__

} //namespave

#endif /* __ardour_utils_h__ */
