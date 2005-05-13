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

#ifndef __qm_platform_h__
#define __qm_platform_h__

class Platform 

{
  public:
	Platform () { 
		thePlatform = this; 
	}
	virtual ~Platform () {}

	virtual int pre_config () { return 0;}
	virtual int post_config () { return 0;}
	virtual int pre_ui () { return 0; }
	virtual int post_ui () { return 0; }
	virtual int dsp_startup() { return 0; }
	
	static Platform *instance() { return thePlatform; }

  private:
	static Platform *thePlatform;
};

#endif // __qm_platform_h__
