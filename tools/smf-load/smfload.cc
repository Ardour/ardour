#include <cstdio>
#include <iostream>

#include <glib/gstdio.h>

#include "evoral/SMF.h"
#include "libsmf/smf.h"

int
main (int argc, char** argv)
{
	const char* fn = "";

	if (argc > 1) {
		fn = argv[1];
	} else {
		std::cerr << "Usage: " << argv[0] << " <midi file>.\n";
		::exit (EXIT_FAILURE);
	}

#if 0
	Evoral::SMF smf;
	smf.open (fn);
	printf ("SMF '%s' tracks=%d, ppqn=%d (n_notes: %ld)\n", fn, smf.num_tracks (), smf.ppqn(), smf.n_note_on_events ());
#else
	FILE* f = g_fopen(fn, "r");
	if (!f) {
		printf ("SMF failed to open file '%s'\n", fn);
		::exit (EXIT_FAILURE);
	}

	printf ("SMF loading file '%s'\n", fn);
	smf_t* smf = smf_load (f);
	fclose(f);

	if (!smf) {
		printf ("SMF failed to load '%s'\n", fn);
		::exit (EXIT_FAILURE);
	}
	printf ("SMF '%s' tracks=%d, ppqn=%d\n", fn, smf->number_of_tracks, smf->ppqn);

	smf_delete (smf);
#endif

	return 0;
}
