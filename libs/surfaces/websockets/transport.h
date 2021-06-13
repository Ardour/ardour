/*
 * Copyright (C) 2020 Luciano Iam <oss@lucianoiam.com>
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

#ifndef _ardour_surface_websockets_transport_h_
#define _ardour_surface_websockets_transport_h_

#include "component.h"

namespace ArdourSurface {

class ArdourTransport : public SurfaceComponent
{
public:
	ArdourTransport (ArdourSurface::ArdourWebsockets& surface)
	    : SurfaceComponent (surface){};
	virtual ~ArdourTransport (){};

	double tempo () const;
	void   set_tempo (double);

	double time () const;

	bool roll () const;
	void set_roll (bool);

	bool record () const;
	void set_record (bool);
};

} // namespace ArdourSurface

#endif // _ardour_surface_websockets_transport_h_
