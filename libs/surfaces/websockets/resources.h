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

#ifndef _ardour_surface_websockets_resources_h_
#define _ardour_surface_websockets_resources_h_

#include <string>
#include <vector>

#include "manifest.h"

namespace ArdourSurface {

typedef std::vector<SurfaceManifest> SurfaceManifestVector;

class ServerResources
{
public:
	ServerResources ();

	const std::string& index_dir ();
	const std::string& builtin_dir ();
	const std::string& user_dir ();

	std::string scan ();

private:

	std::string _index_dir;
	std::string _builtin_dir;
	std::string _user_dir;

	std::string server_data_dir ();

	SurfaceManifestVector read_manifests (std::string);

};

} // namespace ArdourSurface

#endif // _ardour_surface_websockets_resources_h_
