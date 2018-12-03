/*
    Copyright (C) 2017 Paul Davis

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

#include <iostream>
#include <cstdio>
#include <cmath>
#include <cstring>

#include <unistd.h>
#include <stdint.h>

#include "pbd/i18n.h"

#include "evoral/midi_events.h"

#include "ardour/audioengine.h"
#include "ardour/beatbox.h"
#include "ardour/midi_buffer.h"
#include "ardour/session.h"
#include "ardour/smf_source.h"
#include "ardour/step_sequencer.h"
#include "ardour/tempo.h"

using std::cerr;
using std::endl;

using namespace ARDOUR;

BeatBox::BeatBox (Session& s)
	: Processor (s, _("BeatBox"))
	, _sequencer (0)
{
	_display_to_user = true;
	_sequencer = new StepSequencer (s.tempo_map(), 12, 32, Temporal::Beats (0, Temporal::Beats::PPQN/8), Temporal::Beats (4, 0), 40);
}

BeatBox::~BeatBox ()
{
	delete _sequencer;
}

void
BeatBox::silence (samplecnt_t, samplepos_t)
{
	/* do nothing, we have no inputs or outputs */
}

void
BeatBox::run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nsamples, bool result_required)
{
	if (bufs.count().n_midi() == 0) {
		return;
	}

	_sequencer->run (bufs.get_midi (0), start_sample, end_sample, speed, nsamples, result_required);
}

bool
BeatBox::can_support_io_configuration (const ChanCount& in, ChanCount& out)
{
	return true;
}

XMLNode&
BeatBox::get_state(void)
{
	return state ();
}

XMLNode&
BeatBox::state()
{
	XMLNode& node = Processor::state();
	node.set_property ("type", "beatbox");

	return node;
}
