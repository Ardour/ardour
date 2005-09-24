/*
    Copyright (C) 2004 Paul Davis 

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

#ifndef __ardour_click_h__
#define __ardour_click_h__

#include <ardour/io.h>

namespace ARDOUR {

class ClickIO : public IO
{
  public:
	ClickIO (Session& s, const string& name, 

	       int input_min = -1, int input_max = -1, 

	       int output_min = -1, int output_max = -1)
	: IO (s, name, input_min, input_max, output_min, output_max) {}

	~ClickIO() {}

  protected:
	uint32_t pans_required () const { return 1; }
};

}; /* namespace ARDOUR */

#endif /*__ardour_click_h__ */
