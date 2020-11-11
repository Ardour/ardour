/*
 * Copyright (C) 2013-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015-2016 Nick Mainsbridge <mainsbridge@gmail.com>
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


#include <time.h>

#include <glibmm/miscutils.h>

#include "pbd/localtime_r.h"

#include "ardour/audioengine.h"
#include "ardour/filename_extensions.h"
#include "ardour/session.h"
#include "ardour/session_directory.h"
#include "ardour/tempo.h"

#include "jack_session.h"

using namespace ARDOUR;
using namespace Temporal;
using std::string;

JACKSession::JACKSession (Session* s)
	: SessionHandlePtr (s)
{
}

JACKSession::~JACKSession ()
{
}

void
JACKSession::session_event (jack_session_event_t* event)
{
        char timebuf[128], *tmp;
        time_t n;
        struct tm local_time;

        time (&n);
        localtime_r (&n, &local_time);
        strftime (timebuf, sizeof(timebuf), "JS_%FT%T", &local_time);

        while ((tmp = strchr(timebuf, ':'))) { *tmp = '.'; }

        if (event->type == JackSessionSaveTemplate)
        {
                if (_session->save_template( timebuf )) {
                        event->flags = JackSessionSaveError;
                } else {
                        string cmd ("ardour3 -P -U ");
                        cmd += event->client_uuid;
                        cmd += " -T ";
                        cmd += timebuf;

                        event->command_line = strdup (cmd.c_str());
                }
        }
        else
        {
                if (_session->save_state (timebuf)) {
                        event->flags = JackSessionSaveError;
                } else {
			std::string xml_path (_session->session_directory().root_path());
			std::string legalized_filename = legalize_for_path (timebuf) + statefile_suffix;
			xml_path = Glib::build_filename (xml_path, legalized_filename);

                        string cmd ("ardour3 -P -U ");
                        cmd += event->client_uuid;
                        cmd += " \"";
                        cmd += xml_path;
                        cmd += '\"';

                        event->command_line = strdup (cmd.c_str());
                }
        }

	/* this won't be called if the port engine in use is not JACK, so we do
	   not have to worry about the type of PortEngine::private_handle()
	*/

	jack_client_t* jack_client = (jack_client_t*) AudioEngine::instance()->port_engine().private_handle();

	if (jack_client) {
		jack_session_reply (jack_client, event);
	}

	if (event->type == JackSessionSaveAndQuit) {
		_session->Quit (); /* EMIT SIGNAL */
	}

	jack_session_event_free (event);
}

void
JACKSession::timebase_callback (jack_transport_state_t /*state*/,
				 pframes_t /*nframes*/,
				 jack_position_t* pos,
				 int /*new_position*/)
{
	Temporal::BBT_Time bbt;
	TempoMap& tempo_map (_session->tempo_map());
	samplepos_t tf;

	/* see commit msg for e2c26e1b9 and Session::start_locate() for
	   details.
	*/

	tf = _session->nominal_jack_transport_sample().value_or (_session->transport_sample());

	/* BBT info */

	TempoMetric metric (tempo_map.metric_at (tf));

	try {
		bbt = tempo_map.bbt_at (tf);

		pos->bar = bbt.bars;
		pos->beat = bbt.beats;
		pos->tick = bbt.ticks;

		pos->beats_per_bar = metric.meter().divisions_per_bar();
		pos->beat_type = metric.meter().note_value();
		pos->ticks_per_beat = Temporal::ticks_per_beat;
		pos->beats_per_minute = metric.tempo().note_types_per_minute();

		double current_tick = tempo_map.quarter_note_at_bbt_rt (bbt) / 4 * pos->beat_type * pos->ticks_per_beat;
		pos->bar_start_tick = current_tick - ((pos->beat - 1) * pos->ticks_per_beat + pos->tick);

		pos->valid = jack_position_bits_t (pos->valid | JackPositionBBT);

	} catch (...) {
		/* no message */
	}

#ifdef HAVE_JACK_VIDEO_SUPPORT
	//poke audio video ratio so Ardour can track Video Sync
	pos->audio_frames_per_video_frame = _session->sample_rate() / _session->timecode_frames_per_second();
	pos->valid = jack_position_bits_t (pos->valid | JackAudioVideoRatio);
#endif

#ifdef HAVE_JACK_TIMCODE_SUPPORT
	/* This is not yet defined in JACK */

	/* Timecode info */

	pos->timecode_offset = _session->config.get_timecode_offset();
	t.timecode_frame_rate = _session->timecode_frames_per_second();
	pos->valid = jack_position_bits_t (pos->valid | JackPositionTimecode);
#endif

#ifdef HAVE_JACK_LOOPING_SUPPORT
	/* This is not yet defined in JACK */
	if (_transport_speed) {

		if (play_loop) {

			Location* location = _session->locations()->auto_loop_location();

			if (location) {

				t.transport_state = JackTransportLooping;
				t.loop_start = location->start();
				t.loop_end = location->end();
				t.valid = jack_transport_bits_t (t.valid | JackTransportLoop);

			} else {

				t.loop_start = 0;
				t.loop_end = 0;
				t.transport_state = JackTransportRolling;

			}

		} else {

			t.loop_start = 0;
			t.loop_end = 0;
			t.transport_state = JackTransportRolling;

		}

	}
#endif
}
