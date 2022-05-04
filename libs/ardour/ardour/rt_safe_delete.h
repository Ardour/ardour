/*
 * Copyright (C) 2022 Robin Gareus <robin@gareus.org>
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

#pragma once

#include "ardour/audioengine.h"
#include "ardour/butler.h"
#include "ardour/session.h"

namespace ARDOUR {

template <class C>
void rt_safe_delete (ARDOUR::Session* s, C* gc) {
	if (s->deletion_in_progress () || !s->engine ().in_process_thread ()) {
		delete gc;
		return;
	}
	if (!s->butler ()->delegate (sigc::bind ([] (C* p) { delete p; }, gc))) {
		delete gc;
		return;
	}
}

}
