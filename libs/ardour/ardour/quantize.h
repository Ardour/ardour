/*
    Copyright (C) 2007-2009 Paul Davis
    Author: David Robillard

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef __ardour_quantize_h__
#define __ardour_quantize_h__

#include "ardour/types.h"
#include "ardour/midi_operator.h"

namespace ARDOUR {

class Session;

class Quantize : public MidiOperator {
public:
	Quantize (ARDOUR::Session&, bool snap_start, bool snap_end,
			double start_grid, double end_grid,
			float strength, float swing, float threshold);
	~Quantize ();

	Command* operator() (boost::shared_ptr<ARDOUR::MidiModel>,
	                     double position,
	                     std::vector<Evoral::Sequence<Evoral::MusicalTime>::Notes>&);
	std::string name() const { return std::string ("quantize"); }

private:
	ARDOUR::Session& session;
	bool   _snap_start;
	bool   _snap_end;
	double _start_grid;
	double _end_grid;
	float  _strength;
	float  _swing;
	float  _threshold;
};

} /* namespace */

#endif /* __ardour_quantize_h__ */
