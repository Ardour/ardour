/*
    Copyright (C) 2010 Paul Davis

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

#ifndef __libardour_speaker_h__
#define __libardour_speaker_h__

#include "pbd/cartesian.h"
#include "pbd/signals.h"

#include "ardour/libardour_visibility.h"

namespace ARDOUR {

class LIBARDOUR_API Speaker {
public:
	Speaker (int, const PBD::AngularVector& position);
	Speaker (const Speaker &);
	Speaker& operator= (const Speaker &);

	void move (const PBD::AngularVector& new_position);

	const PBD::CartesianVector& coords() const { return _coords; }
	const PBD::AngularVector&   angles() const { return _angles; }

	int id;

	/** emitted when this speaker's position has changed */
	PBD::Signal0<void> PositionChanged;

private:
	PBD::CartesianVector _coords;
	PBD::AngularVector   _angles;
};

} /* namespace */

#endif /* __libardour_speaker_h__ */
