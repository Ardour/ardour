/*
 * Copyright (C) 2017 Ben Loftis <ben@harrisonconsoles.com>
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

#ifndef us2400_jog_wheel_h
#define us2400_jog_wheel_h

#include "timer.h"

#include <stack>
#include <deque>
#include <queue>

namespace ArdourSurface {

class US2400Protocol;

namespace US2400
{

class JogWheel
{
  public:
	enum Mode { scroll };

	JogWheel (US2400Protocol & mcp);

	/// As the wheel turns...
	void jog_event (float delta);
	void set_mode (Mode m);
	Mode mode() const { return _mode; }

private:
	US2400Protocol & _mcp;
	Mode _mode;
};

}
}

#endif
