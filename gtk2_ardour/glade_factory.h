/*
    Copyright (C) 2005 Paul Davis 

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

// -*- c++ -*-

#ifndef GLADE_FACTORY_H
#define GLADE_FACTORY_H

#include <string>
#include <libglademm/xml.h>

typedef Glib::RefPtr<Gnome::Glade::Xml> GladeRef;

/**
   This is the base class for all glade 
   factories so that the same domain is
   used.
*/
class GladeFactory {
	
protected:
	static GladeRef
	create(const std::string& full_path,
	       const Glib::ustring& toplevel_widget = Glib::ustring());
};


#endif // GLADE_FACTORY_H
