/*
    Copyright (C) 2008 Paul Davis
    Author: Sakari Bergen

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

#ifndef __ardour_export_failed_h__
#define __ardour_export_failed_h__

#include <exception>
#include <string>
#include <pbd/error.h>

#include "i18n.h"

using namespace PBD;

namespace ARDOUR
{

class ExportFailed : public std::exception
{
  public:
	ExportFailed (std::string const & reason) :
	  reason (reason.c_str())
	{
		error << string_compose (_("Export failed: %1"), reason) << endmsg;
	}
	
	~ExportFailed () throw() { }
	
	const char* what() const throw()
	{
		return reason;
	}
	
  private:

	const char * reason;

};

} // namespace ARDOUR

#endif /* __ardour_export_failed_h__ */
