/*
    Copyright (C) 2001 Paul Davis 

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

#include <glibmm/thread.h>

#include "pbd/error.h"

#include "ardour/audio_diskstream.h"
#include "ardour/audioregion.h"
#include "ardour/audioengine.h"
#include "ardour/delivery.h"
#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/auditioner.h"
#include "ardour/audioplaylist.h"
#include "ardour/panner.h"
#include "ardour/data_type.h"
#include "ardour/region_factory.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

#include "i18n.h"

Auditioner::Auditioner (Session& s)
	: AudioTrack (s, "auditioner", Route::Hidden)
{
	string left = _session.config.get_auditioner_output_left();
	string right = _session.config.get_auditioner_output_right();

	if (left == "default") {
		left = _session.engine().get_nth_physical_output (DataType::AUDIO, 0);	
	}

	if (right == "default") {
		right = _session.engine().get_nth_physical_output (DataType::AUDIO, 1);
	}
	
	if ((left.length() == 0) && (right.length() == 0)) {
		warning << _("no outputs available for auditioner - manual connection required") << endmsg;
		return;
	}

	_main_outs->defer_pan_reset ();

	if (left.length()) {
		_output->add_port (left, this, DataType::AUDIO);
	}

	if (right.length()) {
		audio_diskstream()->add_channel (1);
		_output->add_port (right, this, DataType::AUDIO);
	}
	
	_main_outs->allow_pan_reset ();
	_main_outs->reset_panner ();

	_output->changed.connect (mem_fun (*this, &Auditioner::output_changed));

	the_region.reset ((AudioRegion*) 0);
	g_atomic_int_set (&_active, 0);
}

Auditioner::~Auditioner ()
{
}

AudioPlaylist&
Auditioner::prepare_playlist ()
{
	// FIXME auditioner is still audio-only
	boost::shared_ptr<AudioPlaylist> apl = boost::dynamic_pointer_cast<AudioPlaylist>(_diskstream->playlist());
	assert(apl);

	apl->clear ();
	return *apl;
}

void
Auditioner::audition_current_playlist ()
{
	if (g_atomic_int_get (&_active)) {
		/* don't go via session for this, because we are going
		   to remain active.
		*/
		cancel_audition ();
	}

	Glib::Mutex::Lock lm (lock);
	_diskstream->seek (0);
	length = _diskstream->playlist()->get_maximum_extent();
	current_frame = 0;

	/* force a panner reset now that we have all channels */

	_main_outs->panner()->reset (n_outputs().n_audio(), _diskstream->n_channels().n_audio());

	g_atomic_int_set (&_active, 1);
}

void
Auditioner::audition_region (boost::shared_ptr<Region> region)
{
	if (g_atomic_int_get (&_active)) {
		/* don't go via session for this, because we are going
		   to remain active.
		*/
		cancel_audition ();
	}

	if (boost::dynamic_pointer_cast<AudioRegion>(region) == 0) {
		error << _("Auditioning of non-audio regions not yet supported") << endmsg;
		return;
	}

	Glib::Mutex::Lock lm (lock);

	/* copy it */

	boost::shared_ptr<AudioRegion> the_region (boost::dynamic_pointer_cast<AudioRegion> (RegionFactory::create (region)));
	the_region->set_position (0, this);

	_diskstream->playlist()->drop_regions ();
	_diskstream->playlist()->add_region (the_region, 0, 1);

	if (_diskstream->n_channels().n_audio() < the_region->n_channels()) {
		audio_diskstream()->add_channel (the_region->n_channels() - _diskstream->n_channels().n_audio());
	} else if (_diskstream->n_channels().n_audio() > the_region->n_channels()) {
		audio_diskstream()->remove_channel (_diskstream->n_channels().n_audio() - the_region->n_channels());
	}

	/* force a panner reset now that we have all channels */

	_main_outs->reset_panner();

	length = the_region->length();

	int dir;
	nframes_t offset = the_region->sync_offset (dir);

	/* can't audition from a negative sync point */

	if (dir < 0) {
		offset = 0;
	}

	_diskstream->seek (offset);
	current_frame = offset;

	g_atomic_int_set (&_active, 1);
}

int
Auditioner::play_audition (nframes_t nframes)
{
	bool need_butler;
	nframes_t this_nframes;
	int ret;

	if (g_atomic_int_get (&_active) == 0) {
		silence (nframes);
		return 0;
	}

	this_nframes = min (nframes, length - current_frame);

	_diskstream->prepare ();

	if ((ret = roll (this_nframes, current_frame, current_frame + nframes, false, false, false)) != 0) {
		silence (nframes);
		return ret;
	}

	need_butler = _diskstream->commit (this_nframes);
	current_frame += this_nframes;

	if (current_frame >= length) {
		_session.cancel_audition ();
		return 0;
	} else {
		return need_butler ? 1 : 0;
	}
}

void
Auditioner::output_changed (IOChange change, void* /*src*/)
{
	string phys;

	if (change & ConnectionsChanged) {
		vector<string> connections;
		if (_output->nth (0)->get_connections (connections)) {
			phys = _session.engine().get_nth_physical_output (DataType::AUDIO, 0);
			if (phys != connections[0]) {
				_session.config.set_auditioner_output_left (connections[0]);
			} else {
				_session.config.set_auditioner_output_left ("default");
			}
		} else {
			_session.config.set_auditioner_output_left ("");
		}
		
		connections.clear ();

		if (_output->nth (1)->get_connections (connections)) {
			phys = _session.engine().get_nth_physical_output (DataType::AUDIO, 1);
			if (phys != connections[0]) {
				_session.config.set_auditioner_output_right (connections[0]);
			} else {
				_session.config.set_auditioner_output_right ("default");
			}
		} else {
			_session.config.set_auditioner_output_right ("");
		}
	}
}
