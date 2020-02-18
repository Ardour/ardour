/*
 * Copyright (C) 2006-2007 John Anderson
 * Copyright (C) 2008-2015 Paul Davis <paul@linuxaudiosystems.com>
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
#ifndef mackie_control_exception_h
#define mackie_control_exception_h

#include <stdexcept>

namespace ArdourSurface {
namespace Mackie {

class MackieControlException : public std::exception
{
public:
	MackieControlException( const std::string & msg )
	: _msg( msg )
	{
	}

	virtual ~MackieControlException() throw () {}

	const char * what() const throw ()
	{
		return _msg.c_str();
	}

private:
	std::string _msg;
};

}
}

#endif
