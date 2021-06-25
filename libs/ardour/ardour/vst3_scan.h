/*
 * Copyright (C) 2019 Robin Gareus <robin@gareus.org>
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

#ifndef _ardour_vst3_scan_h_
#define _ardour_vst3_scan_h_

#include <string>
#include <vector>
#include <boost/shared_ptr.hpp>

#include "pbd/xml++.h"

#include "ardour/libardour_visibility.h"

namespace ARDOUR {
class VST3PluginModule;
}

namespace ARDOUR {

struct VST3Info {
	VST3Info ()
		: index (0)
		, n_inputs (0)
		, n_outputs (0)
		, n_aux_inputs (0)
		, n_aux_outputs (0)
		, n_midi_inputs (0)
		, n_midi_outputs (0)
	{}

	VST3Info (XMLNode const&);
	XMLNode& state () const;

	int         index;
	std::string uid;
	std::string name;
	std::string vendor;
	std::string category;
	std::string version;
	std::string sdk_version;
	std::string url;
	std::string email;

	int n_inputs;
	int n_outputs;
	int n_aux_inputs;
	int n_aux_outputs;
	int n_midi_inputs;
	int n_midi_outputs;
};

LIBARDOUR_API extern std::string
module_path_vst3 (std::string const& path);

LIBARDOUR_API extern std::string
vst3_cache_file (std::string const& module_path);

LIBARDOUR_API extern std::string
vst3_valid_cache_file (std::string const& module_path, bool verbose = false, bool* is_new = NULL);

LIBARDOUR_API extern bool
vst3_scan_and_cache (std::string const& module_path, std::string const& bundle_path, boost::function<void (std::string const&, std::string const&, VST3Info const&)> cb, bool verbose = false);

} // namespace ARDOUR

#endif
