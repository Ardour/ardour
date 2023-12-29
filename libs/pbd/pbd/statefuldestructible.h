/*
 * Copyright (C) 2000-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __pbd_stateful_destructible_h__
#define __pbd_stateful_destructible_h__

#include "pbd/libpbd_visibility.h"
#include "pbd/stateful.h"
#include "pbd/destructible.h"

namespace PBD {

/** Base class for objects with saveable and undoable state with destruction notification */
class LIBPBD_API StatefulDestructible : virtual public Stateful, virtual public Destructible
{
};

}

#endif /* __pbd_stateful_destructible_h__ */
