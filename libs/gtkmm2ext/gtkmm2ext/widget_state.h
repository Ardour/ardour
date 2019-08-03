/*
 * Copyright (C) 2011-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __gtkmm2ext_widget_state_h__
#define __gtkmm2ext_widget_state_h__

#include "gtkmm2ext/visibility.h"

namespace Gtkmm2ext {

	/* widget states: unlike GTK, visual states like "Selected" or "Prelight"
	   are orthogonal to active states.
	*/

	enum LIBGTKMM2EXT_API ActiveState {
		Off,
		ExplicitActive,
		ImplicitActive,
	};

	enum LIBGTKMM2EXT_API VisualState {
		/* these can be OR-ed together */
		NoVisualState = 0x0,
		Selected = 0x1,
		Prelight = 0x2,
		Insensitive = 0x4,
	};

};

#endif /*  __gtkmm2ext_widget_state_h__ */
