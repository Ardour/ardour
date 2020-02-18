/*
 * Copyright (C) 2009-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
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

#ifndef __pbd_locale_guard__
#define __pbd_locale_guard__

#include "pbd/libpbd_visibility.h"

#include <locale>

namespace PBD {

struct LIBPBD_API LocaleGuard {
  public:
	LocaleGuard ();
	~LocaleGuard ();

  private:
	std::locale old_cpp_locale;
	std::locale pre_cpp_locale;
	char const * old_c_locale;
};
}

#endif /* __pbd_locale_guard__ */
