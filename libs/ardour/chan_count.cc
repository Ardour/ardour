/*
 * Copyright (C) 2006-2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2016 Paul Davis <paul@linuxaudiosystems.com>
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

#include <stdint.h>
#include "ardour/chan_count.h"
#include "ardour/types_convert.h"

#include "pbd/i18n.h"

static const char* state_node_name = "Channels";

using namespace std;

namespace ARDOUR {

// infinite/zero chan count stuff, for setting minimums and maximums, etc.
// FIXME: implement this in a less fugly way

ChanCount::ChanCount(const XMLNode& node)
{
	reset();
	XMLNodeConstIterator iter = node.children().begin();
	for ( ; iter != node.children().end(); ++iter) {
		if ((*iter)->name() == X_(state_node_name)) {
			DataType type (DataType::NIL);
			uint32_t count;
			if ((*iter)->get_property ("type", type) && (*iter)->get_property ("count", count)) {
				set(type, count);
			}
		}
	}
}

XMLNode*
ChanCount::state(const std::string& name) const
{
	XMLNode* node = new XMLNode (name);
	for (DataType::iterator t = DataType::begin(); t != DataType::end(); ++t) {
		uint32_t count = get(*t);
		if (count > 0) {
			XMLNode* n = new XMLNode(X_(state_node_name));
			n->set_property("type", *t);
			n->set_property("count", count);
			node->add_child_nocopy(*n);
		}
	}
	return node;
}

// Statics
const ChanCount ChanCount::ZERO     = ChanCount();

} // namespace ARDOUR

std::ostream& operator<<(std::ostream& o, const ARDOUR::ChanCount& c) {
	return o << "AUDIO=" << c.n_audio() << ":MIDI=" << c.n_midi();
}
