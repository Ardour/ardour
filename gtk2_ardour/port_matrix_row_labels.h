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

#ifndef __port_matrix_row_labels_h__
#define __port_matrix_row_labels_h__

#include <boost/shared_ptr.hpp>
#include "port_matrix_component.h"

class PortMatrix;
class PortMatrixBody;

namespace ARDOUR {
	class Bundle;
}

namespace Gtk {
	class Menu;
}

class PortMatrixRowLabels : public PortMatrixComponent
{
public:
	PortMatrixRowLabels (PortMatrix *, PortMatrixBody *);
	~PortMatrixRowLabels ();

	void button_press (double, double, int, uint32_t);
  
private:
	void render (cairo_t *);
	void compute_dimensions ();
	void remove_channel_proxy (boost::weak_ptr<ARDOUR::Bundle>, uint32_t);
	void rename_channel_proxy (boost::weak_ptr<ARDOUR::Bundle>, uint32_t);

	PortMatrix* _port_matrix;
	uint32_t _longest_port_name;
	uint32_t _longest_bundle_name;
	Gtk::Menu* _menu;
};

#endif
