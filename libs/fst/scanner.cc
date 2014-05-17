#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "pbd/pbd.h"
#include "pbd/transmitter.h"
#include "pbd/receiver.h"

#include "ardour/filesystem_paths.h"
#ifdef LXVST_SUPPORT
#include "ardour/linux_vst_support.h"
#endif
#include "ardour/vst_info_file.h"

/* make stupid waf happy.
 * waf cannot build multiple variants of .o object files from the same
 * source using different wscripts.. it mingles e.g.
 * build/libs/ardour/vst_info_file.cc.1.o for
 * both lib/ardour/wscript and lib/fst/wscript
 *
 * ...but waf does track include dependencies.
 */
#include "../ardour/vst_info_file.cc"
#ifdef LXVST_SUPPORT
#include "../ardour/linux_vst_support.cc"
#endif
#include "../ardour/filesystem_paths.cc"
#include "../ardour/directory_names.cc"


#ifdef LXVST_SUPPORT
void
vstfx_destroy_editor (VSTState* /*vstfx*/) { }
#endif

using namespace PBD;

class DummyReceiver : public Receiver {
	protected:
		void receive (Transmitter::Channel chn, const char * str) {
			const char *prefix = "";
			switch (chn) {
				case Transmitter::Error:
					prefix = "[ERROR]: ";
					break;
				case Transmitter::Info:
					/* ignore */
					return;
				case Transmitter::Warning:
					prefix = "[WARNING]: ";
					break;
				case Transmitter::Fatal:
					prefix = "[FATAL]: ";
					break;
				case Transmitter::Throw:
					abort ();
			}

			std::cerr << prefix << str << std::endl;

			if (chn == Transmitter::Fatal) {
				::exit (1);
			}
		}
};

DummyReceiver dummy_receiver;

int main (int argc, char **argv) {
	char *dllpath = NULL;
	if (argc == 3 && !strcmp("-f", argv[1])) {
		dllpath = argv[2];
		if (strstr (dllpath, ".so" ) || strstr(dllpath, ".dll")) {
			vstfx_remove_infofile(dllpath);
			vstfx_un_blacklist(dllpath);
		}

	}
	else if (argc != 2) {
		fprintf(stderr, "usage: %s [-f] <vst>\n", argv[0]);
		return EXIT_FAILURE;
	} else {
		dllpath = argv[1];
	}

	PBD::init();

	dummy_receiver.listen_to (error);
	dummy_receiver.listen_to (info);
	dummy_receiver.listen_to (fatal);
	dummy_receiver.listen_to (warning);

	std::vector<VSTInfo *> *infos = 0;

	if (0) { }
#ifdef LXVST_SUPPORT
	else if (strstr (dllpath, ".so")) {
		infos = vstfx_get_info_lx(dllpath);
	}
#endif

#ifdef WINDOWS_VST_SUPPORT
	else if (strstr (dllpath, ".dll")) {
		infos = vstfx_get_info_fst(dllpath);
	}
#endif
	else {
		fprintf(stderr, "'%s' is not a supported VST plugin.\n", dllpath);
	}

	PBD::cleanup();

	if (!infos || infos->empty()) {
		return EXIT_FAILURE;
	} else {
		return EXIT_SUCCESS;
	}
}
