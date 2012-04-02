/*
    Copyright (C) 2011 Paul Davis

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

#include <dlfcn.h>

#include "ardour/soundgrid.h"

#ifdef __APPLE__
const char* sndgrid_dll_name = "sndgrid.dylib";
#else
const char* sndgrid_dll_name = "sndgrid.so";
#endif

using namespace ARDOUR;
using std::vector;
using std::string;

SoundGrid* SoundGrid::_instance = 0;

SoundGrid::SoundGrid ()
	: dl_handle (0)
{
	if ((dl_handle = dlopen (sndgrid_dll_name, RTLD_NOW)) == 0) {
		return;
	}

}

SoundGrid::~SoundGrid()
{
	if (dl_handle) {
		dlclose (dl_handle);
	}
}

SoundGrid&
SoundGrid::instance ()
{
	if (_instance) {
		_instance = new SoundGrid;
	}

	return *_instance;
}

bool
SoundGrid::available ()
{
	return instance().dl_handle != 0;
}

vector<string>
SoundGrid::lan_port_names ()
{
	std::vector<string> names;
	names.push_back ("00:00:00:1e:af - builtin ethernet controller");
	return names;
}
