/*
    Copyright (C) 2008 Paul Davis 
    Author:  Dave Robillard

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

#include "pbd/error.h"

#include "ardour/types.h"
#include "ardour/midi_stretch.h"
#include "ardour/session.h"
#include "ardour/midi_region.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

MidiStretch::MidiStretch (Session& s, TimeFXRequest& req)
	: Filter (s)
	, _request (req)
{
}

MidiStretch::~MidiStretch ()
{
}

int
MidiStretch::run (boost::shared_ptr<Region> r)
{
	SourceList nsrcs;
	char suffix[32];

	boost::shared_ptr<MidiRegion> region = boost::dynamic_pointer_cast<MidiRegion>(r);
	if (!region)
		return -1;

	/* the name doesn't need to be super-precise, but allow for 2 fractional
	   digits just to disambiguate close but not identical stretches.
	*/
	
	snprintf (suffix, sizeof (suffix), "@%d", (int) floor (_request.time_fraction * 100.0f));
	
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

	// FIXME: how to make a whole file region if it isn't?
	//assert(region->whole_file());
	
	boost::shared_ptr<MidiSource> src = region->midi_source(0);
	src->load_model();

	boost::shared_ptr<MidiModel> old_model = src->model();

	boost::shared_ptr<MidiSource> new_src = boost::dynamic_pointer_cast<MidiSource>(nsrcs[0]);
	assert(new_src);

	boost::shared_ptr<MidiModel> new_model = new_src->model();
	new_model->start_write();
	
	for (Evoral::Sequence<MidiModel::TimeType>::const_iterator i = old_model->begin();
			i != old_model->end(); ++i) {
		const double new_time = i->time() * _request.time_fraction;
		
		// FIXME: double copy
		Evoral::Event<MidiModel::TimeType> ev(*i, true);
		ev.time() = new_time;
		new_model->append(ev);
	}

	new_model->end_write();
	new_model->set_edited(true);
	
	const int ret = finish (region, nsrcs, new_name);
	
	results[0]->set_length((nframes_t) floor (r->length() * _request.time_fraction), NULL);

	return ret;
}

