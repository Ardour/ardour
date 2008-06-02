/*
 * Copyright 2002, The libsigc++ Development Team
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */
#ifndef _SIGC_COMPATIBILITY_HPP_
#define _SIGC_COMPATIBILITY_HPP_

#include <sigc++/signal.h>
#include <sigc++/connection.h>
#include <sigc++/object_slot.h>

#ifndef LIBSIGC_DISABLE_DEPRECATED

namespace SigC {

/** @defgroup compat Compatibility module
 * This set of types and functions provides an API that is compatible to
 * libsigc++-1.2. Some internal structures of libsigc++-1.2 are not available.
 *
 * All types and functions that are defined in namespace SigC are deprecated.
 * Use the new libsigc++2 API that is defined in namespace sigc.
 */

}

#endif /* LIBSIGC_DISABLE_DEPRECATED */

#endif /* _SIGC_COMPATIBILITY_HPP_ */
