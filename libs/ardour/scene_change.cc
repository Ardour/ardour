/*
 * Copyright (C) 2014-2016 Paul Davis <paul@linuxaudiosystems.com>
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

#include "ardour/midi_scene_change.h"

#include "pbd/i18n.h"

using namespace PBD;
using namespace ARDOUR;

std::string SceneChange::xml_node_name = X_("SceneChange");
const uint32_t SceneChange::out_of_bound_color = 0x00000000; /* note: zero alpha means invisible, which acts as out-of-bound signal */

boost::shared_ptr<SceneChange>
SceneChange::factory (const XMLNode& node, int version)
{
	XMLProperty const * prop = node.property (X_("type"));

	if (prop->value() == X_("MIDI")) {
		return boost::shared_ptr<SceneChange> (new MIDISceneChange (node, version));
	}

	return boost::shared_ptr<SceneChange>();
}

SceneChange::SceneChange ()
        : _color (out_of_bound_color)
        , _active (true)
{
}

void
SceneChange::set_color (uint32_t c)
{
        _color = c;
        ColorChanged (); /* EMIT SIGNAL */
}

uint32_t
SceneChange::color() const
{
        return _color;
}

void
SceneChange::set_active (bool yn)
{
        if (_active != yn) {
                _active = yn;
                ActiveChanged (); /* EMIT SIGNAL */
        }
}
