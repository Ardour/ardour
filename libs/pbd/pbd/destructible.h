/*
 * Copyright (C) 2006-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __pbd_destructible_h__
#define __pbd_destructible_h__

#include "pbd/signals.h"
#include "pbd/libpbd_visibility.h"

namespace PBD {

class LIBPBD_API Destructible {
  public:
        Destructible() {}
	virtual ~Destructible () { Destroyed(); }

	PBD::Signal0<void> Destroyed;
	PBD::Signal0<void> DropReferences;

	virtual void drop_references () { DropReferences();  }
};

}

#endif /* __pbd_destructible_h__ */
