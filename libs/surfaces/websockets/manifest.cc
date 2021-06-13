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

#include <iostream>
#include <sstream>

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "pbd/xml++.h"

#include "manifest.h"
#include "resources.h"
#include "json.h"

using namespace ArdourSurface;

static const char* const manifest_filename = "manifest.xml";

SurfaceManifest::SurfaceManifest (std::string path)
	: _path (path)
{
	XMLTree tree;
	std::string xml_path = Glib::build_filename (_path, manifest_filename);

	if (!tree.read (xml_path.c_str())) {
#ifndef NDEBUG
		std::cerr << "SurfaceManifest: could not parse " << xml_path << std::endl;
#endif
		return;
	}

	XMLNodeList nlist = tree.root ()->children ();

	for (XMLNodeConstIterator niter = nlist.begin(); niter != nlist.end(); ++niter) {
		XMLNode*    node = *niter;
		std::string name = node->name ();
		std::string value;
		
		node->get_property ("value", value);

		if (name == "Name") {
			_name = value;
		} else if (name == "Description") {
			_description = value;
		} else if (name == "Version") {
			_version = value;
		}
	}

	if (_name.empty () || _description.empty () || _version.empty ()) {
#ifndef NDEBUG
		std::cerr << "SurfaceManifest: missing properties in " << xml_path << std::endl;
#endif
		return;
	}

	_valid = true;
}

std::string
SurfaceManifest::to_json ()
{
	std::stringstream ss;

	ss << "{"
		<< "\"path\":\"" << WebSocketsJSON::escape (Glib::path_get_basename (_path)) << "\""
		<< ",\"name\":\"" << WebSocketsJSON::escape (_name) << "\""
		<< ",\"description\":\"" << WebSocketsJSON::escape (_description) << "\""
		<< ",\"version\":\"" << WebSocketsJSON::escape (_version) << "\""
		<< "}";

	return ss.str ();
}

bool
SurfaceManifest::exists_at_path (std::string path)
{
	std::string xml_path = Glib::build_filename (path, manifest_filename);
	return Glib::file_test (xml_path, Glib::FILE_TEST_EXISTS);
}
