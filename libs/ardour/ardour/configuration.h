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

#ifndef __ardour_configuration_h__
#define __ardour_configuration_h__

#include <boost/function.hpp>
#include "pbd/signals.h"
#include "pbd/stateful.h"
#include "ardour/configuration_variable.h"

class XMLNode;

namespace ARDOUR {

class LIBARDOUR_API Configuration : public PBD::Stateful
{
  public:
	Configuration();
	virtual ~Configuration();

	virtual void map_parameters (boost::function<void (std::string)>&) = 0;
	virtual int set_state (XMLNode const &, int) = 0;
	virtual XMLNode & get_state () = 0;
	virtual XMLNode & get_variables () = 0;
	virtual void set_variables (XMLNode const &) = 0;

	PBD::Signal1<void,std::string> ParameterChanged;
};

} // namespace ARDOUR

#endif /* __ardour_configuration_h__ */
