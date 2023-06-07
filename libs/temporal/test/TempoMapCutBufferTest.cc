#include <stdlib.h>

#include "temporal/tempo.h"

#include "TempoMapCutBufferTest.h"

CPPUNIT_TEST_SUITE_REGISTRATION(TempoMapCutBufferTest);

using namespace Temporal;

void
TempoMapCutBufferTest::createTest()
{
	CPPUNIT_ASSERT (TempoMap::use() != 0);
}

void
TempoMapCutBufferTest::cutTest()
{
	TempoMap::WritableSharedPtr tmap (TempoMap::write_copy());

	(void) tmap->set_tempo (Tempo (180, 4), BBT_Argument (6, 1, 0));
	(void) tmap->set_meter (Meter (6, 8), BBT_Argument (3, 1, 0));

	(void) tmap->set_tempo (Tempo (180, 4), BBT_Argument (15, 1, 0));
	(void) tmap->set_meter (Meter (3, 4), BBT_Argument (15, 1, 0));

	(void) tmap->set_tempo (Tempo (180, 4), BBT_Argument (31, 1, 0));
	(void) tmap->set_meter (Meter (5, 4), BBT_Argument (32, 1, 0));

	std::cerr << "\n\nBefore cut\n";
	tmap->dump (std::cerr);

	TempoMapCutBuffer* cb = tmap->cut (timepos_t::from_superclock (tmap->superclock_at (BBT_Argument (62, 1, 0))),
	                                   timepos_t::from_superclock (tmap->superclock_at (BBT_Argument (300, 1, 0))),
	                                   false);

	std::cerr << "Cut Buffer:\n";
	cb->dump (std::cerr);

	std::cerr << "After cut\n";
	tmap->dump (std::cerr);

	tmap->abort_update ();
}

void
TempoMapCutBufferTest::copyTest()
{
}

void
TempoMapCutBufferTest::pasteTest()
{
	TempoMap::WritableSharedPtr tmap (TempoMap::write_copy());

	(void) tmap->set_tempo (Tempo (180, 4), BBT_Argument (6, 1, 0));
	(void) tmap->set_meter (Meter (6, 8), BBT_Argument (3, 1, 0));

	(void) tmap->set_tempo (Tempo (180, 4), BBT_Argument (15, 1, 0));
	(void) tmap->set_meter (Meter (3, 4), BBT_Argument (15, 1, 0));

	(void) tmap->set_tempo (Tempo (180, 4), BBT_Argument (31, 1, 0));
	(void) tmap->set_meter (Meter (5, 4), BBT_Argument (32, 1, 0));

	TempoMapCutBuffer* cb = tmap->copy (timepos_t::from_superclock (tmap->superclock_at (BBT_Argument (8, 1, 0))),
	                                    timepos_t::from_superclock (tmap->superclock_at (BBT_Argument (31, 1, 0))));

	TempoMap* new_map = new TempoMap (Tempo (120, 4), Meter (7, 8));

#if 0
	std::cerr << "\n\nCut Buffer:\n";
	cb->dump (std::cerr);

	std::cerr << "Before paste\n";
	new_map->dump (std::cerr);
#endif

	new_map->paste (*cb, timepos_t::from_superclock (tmap->superclock_at (BBT_Argument (6, 1, 0))),
	                false);

#if 0
	std::cerr << "After paste\n";
	new_map->dump (std::cerr);
#endif

	Meter nmm (new_map->meter_at (BBT_Argument (21,7,34)));
	Tempo nmt (new_map->tempo_at (BBT_Argument (21,7,34)));
	Meter omm (tmap->meter_at (BBT_Argument (21,7,34)));
	Tempo omt (tmap->tempo_at (BBT_Argument (21,7,34)));

	CPPUNIT_ASSERT_EQUAL (nmm, omm);
	CPPUNIT_ASSERT_EQUAL (omt, nmt);

	delete new_map;
}
