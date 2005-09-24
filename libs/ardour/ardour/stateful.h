/*
    Copyright (C) 2000 Paul Davis 

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

#ifndef __ardour_stateful_h__
#define __ardour_stateful_h__

#include <string>

class XMLNode;

class Stateful {
  public:
	Stateful();
	virtual ~Stateful();

	virtual XMLNode& get_state (void) = 0;

	virtual int set_state (const XMLNode&) = 0;

	/* Extra XML nodes */

	void add_extra_xml (XMLNode&);
	XMLNode *extra_xml (const std::string& str);

	virtual void add_instant_xml (XMLNode&, const std::string& dir);
	XMLNode *instant_xml (const std::string& str, const std::string& dir);

  protected:
	XMLNode *_extra_xml;
	XMLNode *_instant_xml;
};

#endif /* __ardour_stateful_h__ */

