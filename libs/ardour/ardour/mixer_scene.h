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

#ifndef _libardour_mixer_scene_h_
#define _libardour_mixer_scene_h_

#include <boost/shared_ptr.hpp>

#include "pbd/stateful.h"

#include "ardour/libardour_visibility.h"
#include "ardour/session_handle.h"

namespace PBD {
	class Controllable;
}

namespace ARDOUR {

class LIBARDOUR_API MixerScene : public SessionHandleRef, public PBD::Stateful
{
public:
	MixerScene (Session&);
	~MixerScene ();

	void snapshot ();
	bool apply () const;
	void clear ();
	bool empty () const { return _ctrl_map.empty (); }

	std::string name () const { return _name; }
	bool        set_name (std::string const& name);

	XMLNode& get_state () const;
	int set_state (XMLNode const&, int version);

	static PBD::Signal0<void> Change;

private:
	typedef std::map<PBD::ID, double> ControllableValueMap;

	bool recurse_to_master (boost::shared_ptr<PBD::Controllable>, std::set <PBD::ID>&) const;

	ControllableValueMap       _ctrl_map;
	std::string                _name;
};

}

#endif
