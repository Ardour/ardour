/*
 * Copyright (C) 2021 Robin Gareus <robin@gareus.org>
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

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <getopt.h>
#include <iostream>
#include <signal.h>
#include <string>
#include <strings.h>
#include <unistd.h>

#include "pbd/error.h"
#include "pbd/transmitter.h"
#include "pbd/receiver.h"
#include "pbd/pbd.h"
#include "pbd/stacktrace.h"

using namespace std;

#include "../ardour/auv2_scan.cc"
#include "../ardour/filesystem_paths.cc"

using namespace PBD;

class LogReceiver : public Receiver
{
protected:
	void receive (Transmitter::Channel chn, const char * str) {
		const char *prefix = "";
		switch (chn) {
			case Transmitter::Debug:
				/* ignore */
				break;
			case Transmitter::Info:
				prefix = "[Info]: ";
				break;
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

		std::cout << prefix << str << std::endl;

		if (chn == Transmitter::Fatal) {
			::exit (EXIT_FAILURE);
		}
	}
};

LogReceiver log_receiver;

static void auv2_plugin (CAComponentDescription const&, AUv2Info const& i)
{
	info << "Found Plugin: '" << i.id << "' " << i.name << endmsg;
}

static bool
scan_auv2 (CAComponentDescription& desc, bool force, bool verbose)
{
	info << "Scanning AU: " << desc.Type () << "-" << desc.SubType () << "-" << desc.Manu() << endmsg;

	if (!auv2_valid_cache_file (desc, verbose).empty ()) {
		if (!force) {
			info << "Skipping scan." << endmsg;
			return true;
		}
	}
	if (auv2_scan_and_cache (desc, sigc::ptr_fun (&auv2_plugin), verbose)) {
		info << string_compose (_("Saved AUV2 plugin cache to %1"), auv2_cache_file (desc)) << endmsg;
	}

	return true;
}

static void
sig_handler (int sig)
{
	printf ("Error: signal %d\n ---8<---\n", sig);
	PBD::stacktrace (std::cout, 15, 2);
	printf (" --->8---\n");
	fflush(stdout);
	fflush(stderr);
	_exit (EXIT_FAILURE);
}

static void
usage ()
{
	// help2man compatible format (standard GNU help-text)
	printf ("ardour-au-scanner - load and index AudioUnit plugins.\n\n");
	printf ("Usage: ardour-au-scanner [ OPTIONS ] <TYPE> <SUBT> <MANU>\n\n");
	printf ("Options:\n\
  -f, --force          Force update of cache file\n\
  -h, --help           Display this help and exit\n\
  -q, --quiet          Hide usual output, only print errors\n\
  -v, --verbose        Give verbose output (unless quiet)\n\
  -V, --version        Print version information and exit\n\
\n");

	printf ("\n\
This tool ...\n\
\n");

	printf ("Report bugs to <http://tracker.ardour.org/>\n"
		"Website: <http://ardour.org/>\n");

	::exit (EXIT_SUCCESS);
}

int
main (int argc, char **argv)
{
	bool print_log = true;
	bool force = false;
	bool verbose = false;

	const char* optstring = "fhqvV";

	/* clang-format off */
	const struct option longopts[] = {
		{ "force",           no_argument,       0, 'f' },
		{ "help",            no_argument,       0, 'h' },
		{ "quiet",           no_argument,       0, 'q' },
		{ "verbose",         no_argument,       0, 'v' },
		{ "version",         no_argument,       0, 'V' },
	};
	/* clang-format on */

	int c = 0;
	while (EOF != (c = getopt_long (argc, argv, optstring, longopts, (int*)0))) {
		switch (c) {
			case 'V':
				printf ("ardour-au-scanner version %s\n\n", VERSIONSTRING);
				printf ("Copyright (C) GPL 2021 Robin Gareus <robin@gareus.org>\n");
				exit (EXIT_SUCCESS);
				break;

			case 'f':
				force = true;
				break;

			case 'h':
				usage ();
				break;

			case 'q':
				print_log = false;
				break;

			case 'v':
				verbose = true;
				break;

			default:
				std::cerr << "Error: unrecognized option. See --help for usage information.\n";
				::exit (EXIT_FAILURE);
				break;
		}
	}

	if (optind + 3 != argc) {
		std::cerr << "Error: Missing parameter. See --help for usage information.\n";
		::exit (EXIT_FAILURE);
	}


	PBD::init();

	if (print_log) {
		log_receiver.listen_to (info);
		log_receiver.listen_to (warning);
		log_receiver.listen_to (error);
		log_receiver.listen_to (fatal);
	} else {
		verbose = false;
	}

	signal (SIGSEGV, sig_handler);
	signal (SIGBUS, sig_handler);
	signal (SIGILL, sig_handler);
	signal (SIGABRT, sig_handler);

	bool err = false;

	CFStringRef s_type = CFStringCreateWithCString (kCFAllocatorDefault, argv[optind++], kCFStringEncodingUTF8);
	CFStringRef s_subt = CFStringCreateWithCString (kCFAllocatorDefault, argv[optind++], kCFStringEncodingUTF8);
	CFStringRef s_manu = CFStringCreateWithCString (kCFAllocatorDefault, argv[optind], kCFStringEncodingUTF8);

	OSType type = UTGetOSTypeFromString (s_type);
	OSType subt = UTGetOSTypeFromString (s_subt);
	OSType manu = UTGetOSTypeFromString (s_manu);

	CAComponentDescription desc (type, subt, manu);

	if (!scan_auv2 (desc, force, verbose)) {
		err = true;
	}

	CFRelease (s_type);
	CFRelease (s_subt);
	CFRelease (s_manu);

	PBD::cleanup();

	if (err) {
		return EXIT_FAILURE;
	} else {
		return EXIT_SUCCESS;
	}
}
