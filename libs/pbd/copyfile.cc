/*
    Copyright (C) 2000-2007 Paul Davis 

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

#include <fstream>
#include <unistd.h>

#include <pbd/copyfile.h>
#include <pbd/error.h>
#include <pbd/compose.h>

#include "i18n.h"

using namespace PBD;
using namespace std;

bool
PBD::copy_file (Glib::ustring from, Glib::ustring to)
{
	ifstream in (from.c_str());
	ofstream out (to.c_str());
	
	if (!in) {
		error << string_compose (_("Could not open %1 for copy"), from) << endmsg;
		return false;
	}
	
	if (!out) {
		error << string_compose (_("Could not open %1 as copy"), to) << endmsg;
		return false;
	}
	
	out << in.rdbuf();
	
	if (!in || !out) {
		error << string_compose (_("Could not copy existing file %1 to %2"), from, to) << endmsg;
		unlink (to.c_str());
		return false;
	}
	
	return true;
}
