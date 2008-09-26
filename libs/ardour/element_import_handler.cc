/*
    Copyright (C) 2008 Paul Davis
    Author: Sakari Bergen

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

#include <ardour/element_import_handler.h>

#include <algorithm>

using namespace ARDOUR;

bool ElementImportHandler::_dirty = false;
bool ElementImportHandler::_errors = false;

ElementImportHandler::~ElementImportHandler ()
{
	_dirty = false;
	_errors = false;
}

bool
ElementImportHandler::check_name (const string & name) const
{
	return std::find (names.begin(), names.end(), name) == names.end();
}

void
ElementImportHandler::add_name (string name)
{
	names.push_back (name);
}

void
ElementImportHandler::remove_name (const string & name)
{
	std::list<string>::iterator it = std::find (names.begin(), names.end(), name);
	if (it != names.end()) {
		names.erase(it);
	}
}
