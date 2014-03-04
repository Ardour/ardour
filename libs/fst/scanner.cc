#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

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
#include "../pbd/error.cc"
#include "../pbd/basename.cc"
#include "../pbd/search_path.cc"
#include "../pbd/transmitter.cc"
#include "../pbd/whitespace.cc"

#ifdef LXVST_SUPPORT
void
vstfx_destroy_editor (VSTState* /*vstfx*/) { }
#endif

int main (int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "usage: %s <vst>\n", argv[0]);
		return EXIT_FAILURE;
	}

	char *dllpath = argv[1];
	std::vector<VSTInfo *> *infos;
#ifdef LXVST_SUPPORT
	if (strstr (dllpath, ".so")) {
		infos = vstfx_get_info_lx(dllpath);
	}
#endif

#ifdef WINDOWS_VST_SUPPORT
	if (strstr (dllpath, ".dll")) {
		infos = vstfx_get_info_fst(dllpath);
	}
#endif

	if (infos->empty()) {
		return EXIT_FAILURE;
	} else {
		return EXIT_SUCCESS;
	}
}

