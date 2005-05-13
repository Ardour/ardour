/*
    Copyright (C) 1999 Paul Barton-Davis 

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

    $Id$
*/

#ifndef __linux_platform_h__
#define __linux_platform_h__

#include <pbd/platform.h>

class LinuxPlatform : public Platform

{
  public:
	LinuxPlatform ();
	~LinuxPlatform () {};

	int pre_config ();
	int post_config ();
	int pre_ui ();
	int post_ui ();

	int dsp_startup() { return 0; }
};

#endif // __linux_platform_h__
