/*
 * Copyright (C) 2026 Ronan Keryell <ronan@keryell.fr>
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

#include "pbd/file_utils.h"

#include "ardour/lua_api.h"
#include "ardour/midi_model.h"
#include "ardour/midi_region.h"
#include "ardour/midi_source.h"
#include "ardour/midi_track.h"
#include "ardour/playlist.h"
#include "ardour/region.h"
#include "ardour/session.h"

#include "lua_api_import_test.h"
#include "test_util.h"

CPPUNIT_TEST_SUITE_REGISTRATION (LuaApiImportTest);

using namespace std;
using namespace ARDOUR;

/* MetaText.mid (in libs/ardour/test/data) is a one-track Type-0 SMF carrying
 * a handful of text meta-events plus two notes. These counts are the values
 * that survive into the in-memory MidiModel after import (tempo / time-sig /
 * key-sig / end-of-track / Ardour-note-id meta-events are intentionally NOT
 * stored in the model, so the model count is smaller than the raw file count).
 */
static const size_t expected_notes       = 2;
static const size_t expected_meta_events = 9;

static std::shared_ptr<MidiModel>
first_model (std::shared_ptr<MidiTrack> track)
{
	std::shared_ptr<Playlist> playlist = track->playlist ();
	CPPUNIT_ASSERT (playlist);

	std::shared_ptr<RegionList> regions = playlist->region_list ();
	CPPUNIT_ASSERT_EQUAL ((size_t) 1, regions->size ());

	std::shared_ptr<MidiRegion> mr = std::dynamic_pointer_cast<MidiRegion> (regions->front ());
	CPPUNIT_ASSERT (mr);

	std::shared_ptr<MidiModel> model = mr->model ();
	CPPUNIT_ASSERT (model);
	return model;
}

void
LuaApiImportTest::importMidiToTrackTest ()
{
	std::string path;
	CPPUNIT_ASSERT (PBD::find_file (test_search_path (), "MetaText.mid", path));

	std::list<std::shared_ptr<MidiTrack> > tracks =
		LuaAPI::import_midi (_session, path, true /*tempo map*/, true /*markers*/, false /*split*/);

	/* one SMF track -> exactly one new MIDI track */
	CPPUNIT_ASSERT_EQUAL ((size_t) 1, tracks.size ());

	std::shared_ptr<MidiTrack> track = tracks.front ();
	CPPUNIT_ASSERT (track);

	/* the imported notes and text meta-events must survive into the model */
	std::shared_ptr<MidiModel> model = first_model (track);
	CPPUNIT_ASSERT_EQUAL (expected_notes,       model->n_notes ());
	CPPUNIT_ASSERT_EQUAL (expected_meta_events, model->meta_events ().size ());

	/* the new track must actually be registered in the session */
	CPPUNIT_ASSERT (_session->route_by_name (track->name ()));
}

void
LuaApiImportTest::importMidiRejectsNonMidiTest ()
{
	/* a non-MIDI file (test.wav ships in the same test-data dir) must be
	 * rejected by the safe_midi_file_extension() guard, returning no tracks
	 * and creating no new route.
	 */
	std::string path;
	CPPUNIT_ASSERT (PBD::find_file (test_search_path (), "test.wav", path));

	std::list<std::shared_ptr<MidiTrack> > tracks =
		LuaAPI::import_midi (_session, path);

	CPPUNIT_ASSERT (tracks.empty ());
}

void
LuaApiImportTest::importMidiRejectsMissingFileTest ()
{
	std::list<std::shared_ptr<MidiTrack> > tracks =
		LuaAPI::import_midi (_session, "/does/not/exist.mid");

	CPPUNIT_ASSERT (tracks.empty ());
}

void
LuaApiImportTest::importMidiRejectsNullSessionTest ()
{
	std::list<std::shared_ptr<MidiTrack> > tracks =
		LuaAPI::import_midi (0, "/does/not/matter.mid");

	CPPUNIT_ASSERT (tracks.empty ());
}
