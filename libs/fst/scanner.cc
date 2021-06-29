/*
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <vector>

#ifdef COMPILER_MSVC
#include <sys/utime.h>
#else
#include <utime.h>
#endif

#include <glib.h>

#include "pbd/pbd.h"
#include "pbd/transmitter.h"
#include "pbd/receiver.h"
#include "pbd/win_console.h"

#ifdef __MINGW64__
#define NO_OLDNAMES // no backwards compat _pid_t, conflict with w64 pthread/sched
#endif

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
#ifdef MACVST_SUPPORT
#include "../ardour/mac_vst_support.cc"
#endif
#include "../ardour/filesystem_paths.cc"
#include "../ardour/directory_names.cc"

#include "../ardour/vst_state.cc"

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
				case Transmitter::Debug:
					/* ignore */
					return;
				case Transmitter::Info:
					/* ignore */
					return;
				case Transmitter::Warning:
					prefix = "[WARNING]: ";
					break;
				case Transmitter::Error:
					prefix = "[ERROR]: ";
					break;
				case Transmitter::Fatal:
					prefix = "[FATAL]: ";
					break;
				case Transmitter::Throw:
					abort ();
			}

			std::cerr << prefix << str << std::endl;

			if (chn == Transmitter::Fatal) {
				console_madness_end ();
				::exit (EXIT_FAILURE);
			}
		}
};

DummyReceiver dummy_receiver;

int main (int argc, char **argv) {
	char *dllpath = NULL;
	console_madness_begin ();

	if (argc == 3 && !strcmp("-f", argv[1])) {
		dllpath = argv[2];
		const size_t slen = strlen (dllpath);
		if (
				(slen > 3 && 0 == g_ascii_strcasecmp (&dllpath[slen-3], ".so"))
				||
				(slen > 4 && 0 == g_ascii_strcasecmp (&dllpath[slen-4], ".dll"))
		   ) {
			vstfx_remove_infofile(dllpath);
			vstfx_un_blacklist(dllpath);
		}

	}
	else if (argc != 2) {
		fprintf(stderr, "usage: %s [-f] <vst>\n", argv[0]);
		console_madness_end ();
		return EXIT_FAILURE;
	} else {
		dllpath = argv[1];
	}

	PBD::init();

	dummy_receiver.listen_to (warning);
	dummy_receiver.listen_to (error);
	dummy_receiver.listen_to (fatal);

	std::vector<VSTInfo *> *infos = 0;

	const size_t slen = strlen (dllpath);
	if (0) { }
#ifdef LXVST_SUPPORT
	else if (slen > 3 && 0 == g_ascii_strcasecmp (&dllpath[slen-3], ".so")) {
		infos = vstfx_get_info_lx(dllpath);
	}
#endif

#ifdef WINDOWS_VST_SUPPORT
	else if (slen > 4 && 0 == g_ascii_strcasecmp (&dllpath[slen-4], ".dll")) {
		infos = vstfx_get_info_fst(dllpath);
	}
#endif

#ifdef MACVST_SUPPORT
	else if (slen > 4 && 0 == g_ascii_strcasecmp (&dllpath[slen-4], ".vst")) {
		infos = vstfx_get_info_mac(dllpath);
	}
#endif
	else {
		fprintf(stderr, "'%s' is not a supported VST plugin.\n", dllpath);
	}

	PBD::cleanup();

	console_madness_end ();

	if (!infos || infos->empty()) {
		return EXIT_FAILURE;
	} else {
		return EXIT_SUCCESS;
	}
}
