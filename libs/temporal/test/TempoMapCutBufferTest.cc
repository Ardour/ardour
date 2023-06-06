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
	(void) tmap->set_meter (Meter (6, 8), BBT_Argument (15, 1, 0));

	(void) tmap->set_tempo (Tempo (180, 4), BBT_Argument (31, 1, 0));
	(void) tmap->set_meter (Meter (6, 8), BBT_Argument (32, 1, 0));

	std::cerr << "Before cut\n";
	tmap->dump (std::cerr);

	TempoMapCutBuffer* cb = tmap->cut (timepos_t::from_superclock (tmap->superclock_at (BBT_Argument (8, 1, 0))),
	                                   timepos_t::from_superclock (tmap->superclock_at (BBT_Argument (31, 1, 0))),
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
}
