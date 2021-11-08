/*
 * Copyright (C) 2021 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_scale_h__
#define __ardour_scale_h__

#include "ardour/mode.h"

class MusicalScale : MusicalMode
{
  public:
	MusicalScale (Type t, int root) : MusicalMode (t), _root (root) {}
	~MusicalScale ();

	int root () const { return _root; }
	void set_root (int);

  private:
	int _root;

};

#endif /* __ardour_scale_h__ */
