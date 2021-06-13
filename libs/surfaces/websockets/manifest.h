/*
 * Copyright (C) 2020 Luciano Iam <oss@lucianoiam.com>
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
 * You should have received a copy of the/GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _ardour_surface_websockets_manifest_h_
#define _ardour_surface_websockets_manifest_h_

#include <string>

namespace ArdourSurface {

class SurfaceManifest
{
public:
	// all ardour control surfaces implement presets using xml format
	SurfaceManifest (std::string);

	bool valid () { return _valid; }

	std::string path () { return _path; }
	std::string name () { return _name; }
	std::string description () { return _description; }
	std::string version () { return _version; }

	std::string to_json ();

	static bool exists_at_path (std::string);

private:
	bool _valid;

	std::string _path;
	std::string _name;
	std::string _description;
	std::string _version;
};

} // namespace ArdourSurface

#endif // _ardour_surface_websockets_manifest_h_
