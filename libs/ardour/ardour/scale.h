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

#include <string>
#include <vector>

#include "pbd/signals.h"

namespace ARDOUR {

enum ScaleType {
	AbsolutePitch,
	SemitoneSteps,
	RatioSteps,
	RatioFromRoot
};

class Scale {
   public:
	Scale (std::string const & name, ScaleType type, std::vector<float> const & elements);
	Scale (Scale const & other);

	std::string name() const { return _name; }
	ScaleType type() const { return _type; }

	void set_name (std::string const & str);

	PBD::Signal<void()> NameChanged;
	PBD::Signal<void()> Changed;

  private:
	std::string _name;
	ScaleType _type;
	std::vector<float> _elements;
};

} // namespace 

