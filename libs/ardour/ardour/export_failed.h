/*
 * Copyright (C) 2008-2013 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009 David Robillard <d@drobilla.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __ardour_export_failed_h__
#define __ardour_export_failed_h__

#include <exception>
#include <string>

#include "ardour/libardour_visibility.h"

namespace ARDOUR
{

class LIBARDOUR_API ExportFailed : public std::exception
{
  public:
	ExportFailed (std::string const &);
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
