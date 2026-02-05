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

enum MusicalScaleType {
	AbsolutePitch,
	SemitoneSteps,
	RatioSteps,
	RatioFromRoot
};

enum MusicalScaleTemperament {
	EqualTempered,
	NonTempered
};

class MusicalScale {
   public:
	MusicalScale (std::string const & name, MusicalScaleType type, MusicalScaleTemperament temperament, std::vector<float> const & elements);
	MusicalScale (MusicalScale const & other);

	std::string name() const { return _name; }
	MusicalScaleType type() const { return _type; }
	int size() const { return _elements.size(); }

	std::vector<float> pitches_from_root (float root, int steps) const;

	void set_name (std::string const & str);

	PBD::Signal<void()> NameChanged;
	PBD::Signal<void()> Changed;

  private:
	std::string _name;
	MusicalScaleType _type;
	MusicalScaleTemperament _temperament;
	std::vector<float> _elements;
};

class MusicalKey : public MusicalScale
{
    public:
	MusicalKey (float root, MusicalScale const &);

	float root() const { return _root; }

   private:
	float _root;

};

} // namespace 

