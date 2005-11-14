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

#include <iostream>

#include "glade_factory.h"

Glib::RefPtr<Gnome::Glade::Xml>
GladeFactory::create(const std::string& full_path_to_file,
		     const Glib::ustring& toplevel_widget)
{
	try {
		return Gnome::Glade::Xml::create(full_path_to_file,
						 toplevel_widget,
						 PACKAGE );
	} catch(const Gnome::Glade::XmlError& ex) {
		std::cerr << ex.what() << std::endl;
		throw ex;
	}
}
