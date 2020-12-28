/*
 * Copyright (C) 2008-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2010 Carl Hetherington <carl@carlh.net>
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

#include "pbd/error.h"

#include "ardour/midi_model.h"
#include "ardour/midi_region.h"
#include "ardour/midi_source.h"
#include "ardour/midi_stretch.h"
#include "ardour/session.h"
#include "ardour/tempo.h"
#include "ardour/types.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

MidiStretch::MidiStretch (Session& s, const TimeFXRequest& req)
	: Filter (s)
	, _request (req)
{
}

MidiStretch::~MidiStretch ()
{
}

int
MidiStretch::run (boost::shared_ptr<Region> r, Progress*)
{
	SourceList nsrcs;
	char suffix[32];

	boost::shared_ptr<MidiRegion> region = boost::dynamic_pointer_cast<MidiRegion>(r);
	if (!region) {
		return -1;
	}

	snprintf (suffix, sizeof (suffix), "@%d", (int) floor ((_request.time_fraction.numerator()/ (double) _request.time_fraction.denominator()) * 100.0f));

	string new_name = region->name();
	string::size_type at = new_name.find ('@');

	// remove any existing stretch indicator

	if (at != string::npos && at > 2) {
		new_name = new_name.substr (0, at - 1);
	}

	new_name += suffix;

	/* create new sources */

	if (make_new_sources (region, nsrcs, suffix))
		return -1;

	boost::shared_ptr<MidiSource> src = region->midi_source(0);
	{
		Source::Lock lock(src->mutex());
		src->load_model(lock);
	}

	boost::shared_ptr<MidiModel> old_model = src->model();

	boost::shared_ptr<MidiSource> new_src = boost::dynamic_pointer_cast<MidiSource>(nsrcs[0]);
	if (!new_src) {
		error << _("MIDI stretch created non-MIDI source") << endmsg;
		return -1;
	}

	Glib::Threads::Mutex::Lock sl (new_src->mutex ());

	new_src->load_model(sl, true);
	boost::shared_ptr<MidiModel> new_model = new_src->model();
	new_model->start_write();

#ifdef DEBUG_MIDI_STRETCH
	printf ("stretch start: %f end: %f  [* %f] * %f\n", r_start, r_end, _request.time_fraction, mtfrac);
#endif

	/* Note: pass true into force_discrete for the begin() iterator so that the model doesn't
	 * do interpolation of controller data when we stretch.
	 */
	MidiModel::TimeType final_time;

	for (Evoral::Sequence<MidiModel::TimeType>::const_iterator i = old_model->begin (MidiModel::TimeType(), true); i != old_model->end(); ++i) {

		const MidiModel::TimeType new_time = i->time() * _request.time_fraction;

		// FIXME: double copy
		Evoral::Event<MidiModel::TimeType> ev(*i, true);
		ev.set_time (new_time);
		new_model->append(ev, Evoral::next_event_id());

		final_time = max (final_time, new_time);
	}

	new_model->end_write (Evoral::Sequence<Temporal::Beats>::ResolveStuckNotes);
	new_model->set_edited (true);

	new_src->copy_interpolation_from (src);

	const int ret = finish (region, nsrcs, new_name);

	/* set length of new region to precisely match source length */

	results[0]->set_length (region->length() * _request.time_fraction);

	return ret;
}
