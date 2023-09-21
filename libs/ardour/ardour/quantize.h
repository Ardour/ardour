/*
 * Copyright (C) 2007-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_quantize_h__
#define __ardour_quantize_h__

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"
#include "ardour/midi_operator.h"

namespace ARDOUR {

class LIBARDOUR_API Quantize : public MidiOperator {
public:
	Quantize (bool snap_start, bool snap_end,
	          Temporal::Beats start_grid, Temporal::Beats end_grid,
	          float strength, float swing, Temporal::Beats const & threshold);
	~Quantize ();

	PBD::Command* operator() (std::shared_ptr<ARDOUR::MidiModel>,
	                     Temporal::Beats position,
	                     std::vector<Evoral::Sequence<Temporal::Beats>::Notes>&);
	std::string name() const { return std::string ("quantize"); }
	bool empty() const { return !_snap_start && !_snap_end; }

	Temporal::Beats start_grid() const { return _start_grid; }
	Temporal::Beats end_grid() const { return _end_grid; }
	void set_start_grid (Temporal::Beats const &);
	void set_end_grid (Temporal::Beats const &);

private:
	bool            _snap_start;
	bool            _snap_end;
	Temporal::Beats _start_grid;
	Temporal::Beats _end_grid;
	float           _strength;
	float           _swing;
	Temporal::Beats _threshold;
};

} /* namespace */

#endif /* __ardour_quantize_h__ */
