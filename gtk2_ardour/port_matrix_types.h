/*
    Copyright (C) 2009 Paul Davis

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

#ifndef __ardour_gtk_port_matrix_types_h__
#define __ardour_gtk_port_matrix_types_h__

#include "ardour/bundle.h"

class PortMatrixNode
{
public:
	PortMatrixNode () {}
	PortMatrixNode (ARDOUR::BundleChannel r, ARDOUR::BundleChannel c) : row (r), column (c) {}

	bool operator== (PortMatrixNode const& other) const {
		return row == other.row && column == other.column;
	}
	bool operator!= (PortMatrixNode const& other) const {
		return row != other.row || column != other.column;
	}

	ARDOUR::BundleChannel row;
	ARDOUR::BundleChannel column;

	enum State {
		ASSOCIATED,     ///< the ports are associated
		NOT_ASSOCIATED, ///< the ports are not associated
		PARTIAL         ///< used when we are examining bundles; the bundles are partially associated
	};
};

#endif
