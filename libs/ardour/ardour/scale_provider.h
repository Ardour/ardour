/*
 * Copyright (C) 2026 Paul Davis <paul@linuxaudiosystems.com>
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

namespace Temporal {
	class timepos_t;
}

namespace ARDOUR {

class MusicalKey;

class ScaleProvider {
   public:
	ScaleProvider (ScaleProvider* parent);
	virtual ~ScaleProvider ();

	ScaleProvider* parent() const  { return _parent; }
	virtual MusicalKey const * key() const;
	virtual MusicalKey const * key_at (Temporal::timepos_t const &) const {
		/* by default, ignore time since there's only 1 answer */
		return key();
	}
	void set_key (MusicalKey const &);

  private:
	ScaleProvider* _parent;
	MusicalKey const * _key;
};

} // namespace 
