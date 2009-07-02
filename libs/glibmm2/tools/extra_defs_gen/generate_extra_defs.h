/* $Id: generate_extra_defs.h 775 2009-01-12 00:41:05Z jaalburqu $ */

/* generate_extra_defs.h
 *
 * Copyright (C) 2001 The Free Software Foundation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include <glib-object.h>
#include <iostream>
#include <string>

std::string get_defs(GType gtype);

std::string get_properties(GType gtype);
std::string get_type_name(GType gtype);
std::string get_type_name_parameter(GType gtype);
std::string get_type_name_signal(GType gtype);
std::string get_signals(GType gtype);
