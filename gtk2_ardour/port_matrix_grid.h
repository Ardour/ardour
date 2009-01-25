/*
    Copyright (C) 2002-2009 Paul Davis 

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

#ifndef  __gtk_ardour_port_matrix_grid_h__ 
#define  __gtk_ardour_port_matrix_grid_h__ 

#include <string>
#include <vector>
#include <boost/shared_ptr.hpp>
#include "port_matrix_component.h"

class PortMatrix;
class PortMatrixBody;

namespace ARDOUR {
	class Bundle;
}

/// The grid part of the port matrix
class PortMatrixGrid : public PortMatrixComponent
{
public:
	PortMatrixGrid (PortMatrix *, PortMatrixBody *);

	void button_press (double, double, int);

private:
	void compute_dimensions ();
	void render (cairo_t *);

	PortMatrix* _port_matrix;
};

#endif
