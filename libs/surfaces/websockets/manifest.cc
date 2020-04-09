/*
 * Copyright (C) 2020 Luciano Iam <lucianito@gmail.com>
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
#include <glibmm/miscutils.h>

#include "pbd/xml++.h"

#include "manifest.h"

SurfaceManifest::SurfaceManifest (std::string xml_path)
{
	XMLTree tree;

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

		if (name == "Id") {
			_id = value;
		} else if (name == "Name") {
			_name = value;
		} else if (name == "Description") {
			_description = value;
		}
	}

#ifndef NDEBUG
	if (_name.empty () || _description.empty ()) {
		std::cerr << "SurfaceManifest: missing properties in " << xml_path << std::endl;
		return;
	}
#endif

	if (_id.empty ()) {
		// default to manifest subdirectory name
		_id = Glib::path_get_basename (Glib::path_get_dirname (xml_path));
	}

	_valid = true;
}

std::string
SurfaceManifest::to_json ()
{
	std::stringstream ss;

	ss << "{"
		<< "\"id\":\"" << _id << "\""
		<< ",\"name\":\"" << _name << "\""
		<< ",\"description\":\"" << _description << "\""
		<< "}";

	return ss.str ();
}
