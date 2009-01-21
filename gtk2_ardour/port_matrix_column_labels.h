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

#ifndef __port_matrix_column_labels_h__
#define __port_matrix_column_labels_h__

#include <boost/shared_ptr.hpp>
#include "port_matrix_component.h"

class PortMatrixBody;

namespace ARDOUR {
	class Bundle;
}

/** The column labels part of the port matrix */
class PortMatrixColumnLabels : public PortMatrixComponent
{
public:

	enum Location {
		TOP,
		BOTTOM
	};
	
	PortMatrixColumnLabels (PortMatrixBody *, Location);

private:
	void render (cairo_t *);
	void compute_dimensions ();
	double basic_text_x_pos (int) const;

	std::vector<boost::shared_ptr<ARDOUR::Bundle> > _bundles;
	double _longest_bundle_name;
	double _longest_channel_name;
	double _highest_text;
	Location _location;
};

#endif
