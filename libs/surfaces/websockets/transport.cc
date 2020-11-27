/*
 * Copyright (C) 2020 Luciano Iam <oss@lucianoiam.com>
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

#include "ardour/tempo.h"

#include "transport.h"

using namespace ARDOUR;
using namespace ArdourSurface;
using namespace Temporal;

double
ArdourTransport::tempo () const
{
	const Tempo& tempo (TempoMap::use()->metric_at (0).tempo());
	return tempo.note_types_per_minute ();
}

void
ArdourTransport::set_tempo (double bpm)
{
	bpm = std::max (0.01, bpm);

	TempoMap::SharedPtr tmap (TempoMap::write_copy());

	Tempo tempo (bpm, tmap->metric_at (0).tempo().note_type ());

	tmap->set_tempo (tempo, timepos_t());
	TempoMap::update (tmap);
}

double
ArdourTransport::time () const
{
	samplepos_t t = session ().transport_sample ();
	samplecnt_t f = session ().sample_rate ();
	return static_cast<double>(t) / static_cast<double>(f);
}

bool
ArdourTransport::roll () const
{
	return basic_ui ().transport_rolling ();
}

void
ArdourTransport::set_roll (bool value)
{
	if ((value && !roll ()) || (!value && roll ())) {
		// this call is equivalent to hitting the spacebar
		basic_ui ().toggle_roll (false);
	}
}

bool
ArdourTransport::record () const
{
	return session ().get_record_enabled ();
}

void
ArdourTransport::set_record (bool value)
{
	if ((value && !record ()) || (!value && record ())) {
		basic_ui ().rec_enable_toggle ();
	}
}
