/*
 * Copyright (C) 2011-2012 Carl Hetherington <carl@carlh.net>
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

#include "ardour/playlist.h"
#include "ardour/region.h"
#include "ardour/audioplaylist.h"
#include "playlist_layering_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION (PlaylistLayeringTest);

using namespace std;
using namespace ARDOUR;

void
PlaylistLayeringTest::basicsTest ()
{
	_playlist->add_region (_r[0], timepos_t(0));
	_playlist->add_region (_r[1], timepos_t(10));
	_playlist->add_region (_r[2], timepos_t(20));

	CPPUNIT_ASSERT_EQUAL (layer_t (0), _r[0]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _r[1]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (2), _r[2]->layer ());

	_r[0]->set_position (timepos_t(5));

	/* region move should have no effect */
	CPPUNIT_ASSERT_EQUAL (layer_t (0), _r[0]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (1), _r[1]->layer ());
	CPPUNIT_ASSERT_EQUAL (layer_t (2), _r[2]->layer ());
}

void
PlaylistLayeringTest::compingLayerTest ()
{
	_r[0]->set_length (timecnt_t (300));
	_playlist->add_region (_r[0], timepos_t (0));
	_playlist->add_region (_r[1], timepos_t (100));
	_playlist->add_region (_r[2], timepos_t (150));

	CPPUNIT_ASSERT (_audio_playlist->initialize_comping_layer ());

	std::shared_ptr<RegionList> regions = _playlist->region_list ();
	size_t comp_region_count = 0;
	for (auto const& region : *regions) {
		if (!region->comp_source_id().empty()) {
			++comp_region_count;
			CPPUNIT_ASSERT_EQUAL (region->layer(), _playlist->top_layer ());
		}
	}
	CPPUNIT_ASSERT_EQUAL (size_t (4), comp_region_count);

	auto assert_source = [this] (samplepos_t position, std::shared_ptr<Region> const& source) {
		std::shared_ptr<Region> comp = _playlist->top_unmuted_region_at (timepos_t (position));
		CPPUNIT_ASSERT (comp);
		CPPUNIT_ASSERT_EQUAL (source->id().to_s(), comp->comp_source_id());
		CPPUNIT_ASSERT (comp->opaque());
	};

	assert_source (99, _r[0]);
	assert_source (100, _r[1]);
	assert_source (149, _r[1]);
	assert_source (150, _r[2]);
	assert_source (249, _r[2]);
	assert_source (250, _r[0]);
	assert_source (299, _r[0]);

	CPPUNIT_ASSERT_EQUAL (timepos_t (0), _r[0]->position());
	CPPUNIT_ASSERT_EQUAL (timecnt_t (300), _r[0]->length());
	CPPUNIT_ASSERT (!_audio_playlist->initialize_comping_layer ());

	/* overlap with the end of one fragment and the start of another */
	CPPUNIT_ASSERT (_audio_playlist->add_to_comping_layer (_r[0], 120, 180));
	assert_source (119, _r[1]);
	assert_source (120, _r[0]);
	assert_source (179, _r[0]);
	assert_source (180, _r[2]);

	/* in the middle of a fragment: splits it */
	CPPUNIT_ASSERT (_audio_playlist->add_to_comping_layer (_r[0], 200, 220));
	assert_source (199, _r[2]);
	assert_source (200, _r[0]);
	assert_source (219, _r[0]);
	assert_source (220, _r[2]);
	assert_source (249, _r[2]);

	/* shrinking the span of _r[0] picked at 120..180 leaves a gap of comp, not a different source */
	CPPUNIT_ASSERT (_audio_playlist->remove_from_comping_layer (_r[0], 160, 180));
	assert_source (159, _r[0]);
	std::shared_ptr<Region> top = _playlist->top_unmuted_region_at (timepos_t (170));
	CPPUNIT_ASSERT (!top || top->comp_source_id().empty());
}

void
PlaylistLayeringTest::compingRangeTest ()
{
	_r[0]->set_length (timecnt_t (300));
	_playlist->add_region (_r[0], timepos_t (0));
	_playlist->add_region (_r[1], timepos_t (100));
	_playlist->add_region (_r[2], timepos_t (150));

	auto comp_source_at = [this] (samplepos_t position) {
		std::shared_ptr<Region> top = _playlist->top_unmuted_region_at (timepos_t (position));
		return top ? top->comp_source_id() : std::string ("none");
	};

	/* only the range 120..220 is comped; elsewhere no comp fragments exist */
	CPPUNIT_ASSERT (_audio_playlist->initialize_comping_layer (120, 220));
	CPPUNIT_ASSERT (comp_source_at (119).empty());
	CPPUNIT_ASSERT_EQUAL (_r[1]->id().to_s(), comp_source_at (120));
	CPPUNIT_ASSERT_EQUAL (_r[2]->id().to_s(), comp_source_at (150));
	CPPUNIT_ASSERT_EQUAL (_r[2]->id().to_s(), comp_source_at (219));
	CPPUNIT_ASSERT (comp_source_at (220).empty());

	/* a second range does not touch the first */
	CPPUNIT_ASSERT (_audio_playlist->initialize_comping_layer (0, 50));
	CPPUNIT_ASSERT_EQUAL (_r[0]->id().to_s(), comp_source_at (10));
	CPPUNIT_ASSERT_EQUAL (_r[1]->id().to_s(), comp_source_at (120));

	/* finishing a range only affects the fragments within it */
	CPPUNIT_ASSERT (_audio_playlist->finish_comping_layer (0, 50));
	CPPUNIT_ASSERT (comp_source_at (10).empty());
	CPPUNIT_ASSERT_EQUAL (_r[1]->id().to_s(), comp_source_at (120));

	/* discarding a range shortens the fragment it overlaps */
	CPPUNIT_ASSERT (_audio_playlist->discard_comping_layer (180, 220));
	CPPUNIT_ASSERT_EQUAL (_r[2]->id().to_s(), comp_source_at (179));
	CPPUNIT_ASSERT (comp_source_at (180).empty());
}
