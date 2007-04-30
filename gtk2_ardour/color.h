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

#ifndef __gtk_ardour_color_h__
#define __gtk_ardour_color_h__

#include <sigc++/signal.h>

#undef COLORID
#define COLORID(a) a,
enum ColorID {
	 #include "colors.h"
};
#undef COLORID

typedef std::map<ColorID,int> ColorMap;
extern ColorMap color_map;

extern sigc::signal<void>                  ColorsChanged;
extern sigc::signal<void,ColorID,uint32_t> ColorChanged;

#endif /* __gtk_ardour_color_h__ */
