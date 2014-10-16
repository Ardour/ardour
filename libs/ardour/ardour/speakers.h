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

#ifndef __libardour_speakers_h__
#define __libardour_speakers_h__

#include <vector>
#include <iostream>

#include <pbd/signals.h>
#include <pbd/stateful.h>

#include "ardour/speaker.h"

class XMLNode;

namespace ARDOUR  {

class LIBARDOUR_API Speakers : public PBD::Stateful {
public:
	Speakers ();
	Speakers (const Speakers&);
	virtual ~Speakers ();

	Speakers& operator= (const Speakers&);

	virtual int  add_speaker (const PBD::AngularVector&);
	virtual void remove_speaker (int id);
	virtual void move_speaker (int id, const PBD::AngularVector& new_position);
	virtual void clear_speakers ();
	uint32_t size() const { return _speakers.size(); }

	void setup_default_speakers (uint32_t nspeakers);

	std::vector<Speaker>& speakers() { return _speakers; }

	void dump_speakers (std::ostream&);

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

	PBD::Signal0<void> Changed;

protected:
	std::vector<Speaker>  _speakers;

	virtual void update () {}
};

} /* namespace */

#endif /* __libardour_speakers_h__ */
