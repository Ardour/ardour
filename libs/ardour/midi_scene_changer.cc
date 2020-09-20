/*
 * Copyright (C) 2014-2016 David Robillard <d@drobilla.net>
 * Copyright (C) 2014-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#include "evoral/Event.h"
#include "midi++/channel.h"
#include "midi++/parser.h"
#include "midi++/port.h"

#include "ardour/async_midi_port.h"
#include "ardour/event_type_map.h"
#include "ardour/midi_buffer.h"
#include "ardour/midi_port.h"
#include "ardour/midi_scene_change.h"
#include "ardour/midi_scene_changer.h"
#include "ardour/session.h"

#include "pbd/i18n.h"

using namespace ARDOUR;

MIDISceneChanger::MIDISceneChanger (Session& s)
	: SceneChanger (s)
	, _recording (true)
	, have_seen_bank_changes (false)
	, last_program_message_time (-1)
	, last_delivered_program (-1)
	, last_delivered_bank (-1)

{
	/* catch any add/remove/clear etc. for all Locations */
	_session.locations()->changed.connect_same_thread (*this, boost::bind (&MIDISceneChanger::locations_changed, this));
	_session.locations()->added.connect_same_thread (*this, boost::bind (&MIDISceneChanger::locations_changed, this));
	_session.locations()->removed.connect_same_thread (*this, boost::bind (&MIDISceneChanger::locations_changed, this));

	/* catch class-based signal that notifies of us changes in the scene change state of any Location */
	Location::scene_changed.connect_same_thread (*this, boost::bind (&MIDISceneChanger::locations_changed, this));
}

MIDISceneChanger::~MIDISceneChanger ()
{
}

void
MIDISceneChanger::locations_changed ()
{
	_session.locations()->apply (*this, &MIDISceneChanger::gather);
}

/** Use the session's list of locations to collect all patch changes.
 *
 * This is called whenever the locations change in anyway.
 */
void
MIDISceneChanger::gather (const Locations::LocationList& locations)
{
	boost::shared_ptr<SceneChange> sc;

	Glib::Threads::RWLock::WriterLock lm (scene_lock);

	scenes.clear ();

	for (Locations::LocationList::const_iterator l = locations.begin(); l != locations.end(); ++l) {

		if ((sc = (*l)->scene_change()) != 0) {

			boost::shared_ptr<MIDISceneChange> msc = boost::dynamic_pointer_cast<MIDISceneChange> (sc);

			if (msc) {

				if (msc->bank() >= 0) {
					have_seen_bank_changes = true;
				}

				scenes.insert (std::make_pair ((*l)->start_sample(), msc));
			}
		}
	}
}

void
MIDISceneChanger::rt_deliver (MidiBuffer& mbuf, samplepos_t when, boost::shared_ptr<MIDISceneChange> msc)
{
        if (!msc->active()) {
                return;
        }

	uint8_t buf[4];
	size_t cnt;

	MIDIOutputActivity (); /* EMIT SIGNAL */

	if ((cnt = msc->get_bank_msb_message (buf, sizeof (buf))) > 0) {
		mbuf.push_back (when, Evoral::MIDI_EVENT, cnt, buf);

		if ((cnt = msc->get_bank_lsb_message (buf, sizeof (buf))) > 0) {
			mbuf.push_back (when, Evoral::MIDI_EVENT, cnt, buf);
		}

		last_delivered_bank = msc->bank();
	}

	if ((cnt = msc->get_program_message (buf, sizeof (buf))) > 0) {
		mbuf.push_back (when, Evoral::MIDI_EVENT, cnt, buf);

		last_delivered_program = msc->program();
	}
}

void
MIDISceneChanger::non_rt_deliver (boost::shared_ptr<MIDISceneChange> msc)
{
        if (!msc->active()) {
                return;
        }

	uint8_t buf[4];
	size_t cnt;
	boost::shared_ptr<AsyncMIDIPort> aport = boost::dynamic_pointer_cast<AsyncMIDIPort>(output_port);

	/* We use zero as the timestamp for these messages because we are in a
	   non-RT/process context. Using zero means "deliver them as early as
	   possible" (practically speaking, in the next process callback).
	*/

	MIDIOutputActivity (); /* EMIT SIGNAL */

	if ((cnt = msc->get_bank_msb_message (buf, sizeof (buf))) > 0) {
		aport->write (buf, cnt, 0);

		if ((cnt = msc->get_bank_lsb_message (buf, sizeof (buf))) > 0) {
			aport->write (buf, cnt, 0);
		}

		last_delivered_bank = msc->bank();
	}

	if ((cnt = msc->get_program_message (buf, sizeof (buf))) > 0) {
		aport->write (buf, cnt, 0);
		last_delivered_program = msc->program();
	}
}

void
MIDISceneChanger::run (samplepos_t start, samplepos_t end)
{
	if (!output_port || recording() || !_session.transport_rolling()) {
		return;
	}

	Glib::Threads::RWLock::ReaderLock lm (scene_lock, Glib::Threads::TRY_LOCK);

	if (!lm.locked()) {
		return;
	}

	/* get lower bound of events to consider */

	Scenes::const_iterator i = scenes.lower_bound (start);
	MidiBuffer& mbuf (output_port->get_midi_buffer (end-start));

	while (i != scenes.end()) {

		if (i->first >= end) {
			break;
		}

		rt_deliver (mbuf, i->first - start, i->second);

		++i;
	}
}

void
MIDISceneChanger::locate (samplepos_t pos)
{
	boost::shared_ptr<MIDISceneChange> msc;

	{
		Glib::Threads::RWLock::ReaderLock lm (scene_lock);

		if (scenes.empty()) {
			return;
		}

		Scenes::const_iterator i = scenes.lower_bound (pos);

		if (i != scenes.end()) {

			if (i->first != pos) {
				/* i points to first scene with position > pos, so back
				 * up, if possible.
				 */
				if (i != scenes.begin()) {
					--i;
				} else {
					return;
				}
			}
		} else {
			/* go back to the final scene and use it */
			--i;
		}

		msc = i->second;
	}

	if (msc->program() != last_delivered_program || msc->bank() != last_delivered_bank) {
		non_rt_deliver (msc);
	}
}

void
MIDISceneChanger::set_input_port (boost::shared_ptr<MidiPort> mp)
{
	incoming_connections.drop_connections();
	input_port.reset ();

	boost::shared_ptr<AsyncMIDIPort> async = boost::dynamic_pointer_cast<AsyncMIDIPort> (mp);

	if (async) {

		input_port = mp;

		/* midi port is asynchronous. MIDI parsing will be carried out
		 * by the MIDI UI thread which will emit the relevant signals
		 * and thus invoke our callbacks as necessary.
		 */

		for (int channel = 0; channel < 16; ++channel) {
			async->parser()->channel_bank_change[channel].connect_same_thread (incoming_connections, boost::bind (&MIDISceneChanger::bank_change_input, this, _1, _2, channel));
			async->parser()->channel_program_change[channel].connect_same_thread (incoming_connections, boost::bind (&MIDISceneChanger::program_change_input, this, _1, _2, channel));
		}
	}
}

void
MIDISceneChanger::set_output_port (boost::shared_ptr<MidiPort> mp)
{
	output_port = mp;
}

void
MIDISceneChanger::set_recording (bool yn)
{
	_recording = yn;
}

bool
MIDISceneChanger::recording() const
{
	return _session.transport_rolling() && _session.get_record_enabled();
}

void
  MIDISceneChanger::bank_change_input (MIDI::Parser& /*parser*/, unsigned short, int)
{
	if (recording()) {
		have_seen_bank_changes = true;
	}
	MIDIInputActivity (); /* EMIT SIGNAL */
}

void
MIDISceneChanger::program_change_input (MIDI::Parser& parser, MIDI::byte program, int channel)
{
	samplecnt_t time = parser.get_timestamp ();

	last_program_message_time = time;

	if (!recording()) {

		MIDIInputActivity (); /* EMIT SIGNAL */

		int bank = -1;
		if (have_seen_bank_changes) {
			bank = boost::dynamic_pointer_cast<AsyncMIDIPort>(input_port)->channel (channel)->bank();
		}

		jump_to (bank, program);
		return;
	}

	Locations* locations (_session.locations ());
	Location* loc;
	bool new_mark = false;

	/* check for marker at current location */

	loc = locations->mark_at (time, Config->get_inter_scene_gap_samples());

	if (!loc) {
		/* create a new marker at the desired position */

		std::string new_name;

		if (!locations->next_available_name (new_name, _("Scene "))) {
			std::cerr << "No new marker name available\n";
			return;
		}

		loc = new Location (_session, timepos_t (time), timepos_t (time), new_name, Location::IsMark);
		new_mark = true;
	}

	int bank = -1;
	if (have_seen_bank_changes) {
		bank = boost::dynamic_pointer_cast<AsyncMIDIPort>(input_port)->channel (channel)->bank();
	}

	MIDISceneChange* msc =new MIDISceneChange (channel, bank, program & 0x7f);

	/* check for identical scene change so we can re-use color, if any */

	Locations::LocationList copy (locations->list());

	for (Locations::LocationList::const_iterator l = copy.begin(); l != copy.end(); ++l) {
		boost::shared_ptr<MIDISceneChange> sc = boost::dynamic_pointer_cast<MIDISceneChange>((*l)->scene_change());

		if (sc && (*sc.get()) == *msc) {
			msc->set_color (sc->color ());
			break;
		}
	}

	loc->set_scene_change (boost::shared_ptr<MIDISceneChange> (msc));

	/* this will generate a "changed" signal to be emitted by locations,
	   and we will call ::gather() to update our list of MIDI events.
	*/

	if (new_mark) {
		locations->add (loc);
	}

	MIDIInputActivity (); /* EMIT SIGNAL */
}

void
MIDISceneChanger::jump_to (int bank, int program)
{
	const Locations::LocationList& locations (_session.locations()->list());
	boost::shared_ptr<SceneChange> sc;
	timepos_t where = timepos_t::max (Temporal::AudioTime);

	for (Locations::LocationList::const_iterator l = locations.begin(); l != locations.end(); ++l) {

		if ((sc = (*l)->scene_change()) != 0) {

			boost::shared_ptr<MIDISceneChange> msc = boost::dynamic_pointer_cast<MIDISceneChange> (sc);

			if (msc->bank() == bank && msc->program() == program && (*l)->start() < where) {
				where = (*l)->start();
			}
		}
	}

	if (where != timepos_t::max (Temporal::AudioTime)) {
		_session.request_locate (where.samples());
	}
}
