/*
    Copyright (C) 2004-2011 Paul Davis

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

#include "ardour/buffer_set.h"
#include "ardour/debug.h"
#include "ardour/panner.h"
#include "ardour/pannable.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;

Panner::Panner (boost::shared_ptr<Pannable> p)
{
	// boost_debug_shared_ptr_mark_interesting (this, "panner");
	_pannable = p;
}

Panner::~Panner ()
{
	DEBUG_TRACE(PBD::DEBUG::Destruction, string_compose ("panner @ %1 destructor, pannable is %2 @ %3\n", this, _pannable, &_pannable));
}

XMLNode&
Panner::get_state ()
{
	return *(new XMLNode (X_("Panner")));
}

void
Panner::distribute (BufferSet& ibufs, BufferSet& obufs, gain_t gain_coeff, pframes_t nframes)
{
	uint32_t which = 0;

	for (BufferSet::audio_iterator src = ibufs.audio_begin(); src != ibufs.audio_end(); ++src, ++which) {
		distribute_one (*src, obufs, gain_coeff, nframes, which);
	}
}

void
Panner::distribute_automated (BufferSet& ibufs, BufferSet& obufs,
                              framepos_t start, framepos_t end, pframes_t nframes, pan_t** buffers)
{
	uint32_t which = 0;

	for (BufferSet::audio_iterator src = ibufs.audio_begin(); src != ibufs.audio_end(); ++src, ++which) {
		distribute_one_automated (*src, obufs, start, end, nframes, buffers, which);
	}
}

void
Panner::set_automation_style (AutoStyle style)
{
	_pannable->set_automation_style (style);
}

void
Panner::set_automation_state (AutoState state)
{
	_pannable->set_automation_state (state);
}

AutoState
Panner::automation_state () const
{
	return _pannable->automation_state();
}

AutoStyle
Panner::automation_style () const
{
	return _pannable->automation_style ();
}

bool
Panner::touching () const
{
	return _pannable->touching ();
}

set<Evoral::Parameter>
Panner::what_can_be_automated() const
{
	return _pannable->what_can_be_automated ();
}

string
Panner::describe_parameter (Evoral::Parameter p)
{
	return _pannable->describe_parameter (p);
}

string
Panner::value_as_string (boost::shared_ptr<AutomationControl> ac) const
{
	return _pannable->value_as_string (ac);
}

int
Panner::set_state (XMLNode const &, int)
{
	return 0;
}

void
Panner::freeze ()
{
	_frozen++;
}

void
Panner::thaw ()
{
	if (_frozen > 0.0) {
		_frozen--;
	}
}
