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

#ifndef __ardour_gtk_processor_selection_h__
#define __ardour_gtk_processor_selection_h__

#include "pbd/xml++.h"

class ProcessorSelection {
  public:
    ProcessorSelection() : node (0) {}
    ~ProcessorSelection() { if (node) { delete node; } }

    void set (XMLNode* n) {
	    if (node) {
		    delete node;
	    }
	    node = n;
    }

    void add (XMLNode* newchild) {
	    if (!node) {
		    node = new XMLNode ("add");
	    }
	    node->add_child_nocopy (*newchild);
    }

    void clear () { 
	    if (node) { 
		    delete node;
		    node = 0;
	    }
    }

    bool empty () const { return node == 0 || node->children().empty(); }
	    
    const XMLNode& get_node() const { return *node; }

  private:
    XMLNode* node;
};

#endif /* __ardour_gtk_processor_selection_h__ */
