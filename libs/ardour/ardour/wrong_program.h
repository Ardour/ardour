/*
 * Copyright (C) 2024 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __libardour_wrong_program_h__
#define __libardour_wrong_program_h__

#include <exception>
#include <string>

#include "pbd/compose.h"

#include "ardour/libardour_visibility.h"
#include "pbd/i18n.h"

namespace ARDOUR {

class LIBARDOUR_API WrongProgram : public std::exception {
  public:
	WrongProgram (std::string const & c) : creator (c) {}
	virtual const char *what() const throw() { return "Created or modified by the wrong program"; }
	std::string creator;
};

} /* namespace */

#endif /* __libardour_wrong_program_h__ */
