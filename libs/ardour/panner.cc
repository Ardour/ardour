/*
 * Copyright (C) 2005-2006 Jesse Chappell <jesse@essej.net>
 * Copyright (C) 2005-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2012 Carl Hetherington <carl@carlh.net>
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

#include "ardour/buffer_set.h"
#include "ardour/debug.h"
#include "ardour/panner.h"
#include "ardour/pannable.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;

Panner::Panner (boost::shared_ptr<Pannable> p)
	: _frozen (0)
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
                              samplepos_t start, samplepos_t end, pframes_t nframes, pan_t** buffers)
{
	uint32_t which = 0;

	for (BufferSet::audio_iterator src = ibufs.audio_begin(); src != ibufs.audio_end(); ++src, ++which) {
		distribute_one_automated (*src, obufs, start, end, nframes, buffers, which);
	}
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
