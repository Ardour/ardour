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

#include "ardour/libardour_visibility.h"
#include "ardour/element_import_handler.h"

#include <algorithm>

using namespace std;
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
	return !names.count (name);
}

void
ElementImportHandler::add_name (string name)
{
	names.insert (name);
}

void
ElementImportHandler::remove_name (const string & name)
{
	names.erase (name);
}
