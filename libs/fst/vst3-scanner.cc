/*
 * Copyright (C) 2020 Robin Gareus <robin@gareus.org>
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

#include <cstdlib>
#include <cstdio>
#include <getopt.h>
#include <iostream>
#include <string>

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#else
#include <signal.h>
#endif

#ifdef COMPILER_MSVC
#include <sys/utime.h>
#else
#include <utime.h>
#endif

#include "pbd/error.h"
#include "pbd/transmitter.h"
#include "pbd/receiver.h"
#include "pbd/pbd.h"
#include "pbd/stacktrace.h"
#include "pbd/win_console.h"
#include "pbd/xml++.h"

#ifdef __MINGW64__
#define NO_OLDNAMES // no backwards compat _pid_t, conflict with w64 pthread/sched
#endif

#include "../ardour/vst3_scan.cc"
#include "../ardour/vst3_host.cc"
#include "../ardour/vst3_module.cc"

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
			console_madness_end ();
			::exit (EXIT_FAILURE);
		}
	}
};

LogReceiver log_receiver;

static void vst3_plugin (string const&, string const&, VST3Info const& i)
{
	info << "Found Plugin: " << i.name << endmsg;
}

static bool
scan_vst3 (std::string const& bundle_path, bool force, bool verbose)
{
	info << "Scanning: " << bundle_path << endmsg;
	string module_path = module_path_vst3 (bundle_path);
	if (module_path.empty ()) {
		return false;
	}

	if (!vst3_valid_cache_file (module_path, verbose).empty()) {
		if (!force) {
			info << "Skipping scan." << endmsg;
			return true;
		}
	}

	if (vst3_scan_and_cache (module_path, bundle_path, sigc::ptr_fun (&vst3_plugin), verbose)) {
		info << string_compose (_("Saved VST3 plugin cache to %1"), vst3_cache_file (module_path)) << endmsg;
	}

	return true;
}

#ifdef PLATFORM_WINDOWS
static LONG WINAPI
crash_handler (EXCEPTION_POINTERS* exceptioninfo)
{
	// TODO consider DrMingw if HAVE_DRMINGW
	printf ("Error: %x\n ---8<---\n", exceptioninfo->ExceptionRecord->ExceptionCode);
	PBD::stacktrace (std::cout, 15, 2);
	printf (" --->8---\n");
	return EXCEPTION_CONTINUE_SEARCH;
}
#else
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
#endif

static void
usage ()
{
	// help2man compatible format (standard GNU help-text)
	printf ("ardour-vst3-scanner - load and index VST3 plugins.\n\n");
	printf ("Usage: ardour-vst3-scanner [ OPTIONS ] <VST3-bundle> [<VST3-bundle>]*\n\n");
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

	console_madness_end ();
	::exit (EXIT_SUCCESS);
}

int
main (int argc, char **argv)
{
	bool print_log = true;
	bool stop_on_error = false;
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

	console_madness_begin ();

	int c = 0;
	while (EOF != (c = getopt_long (argc, argv, optstring, longopts, (int*)0))) {
		switch (c) {
			case 'V':
				printf ("ardour-vst3-scanner version %s\n\n", VERSIONSTRING);
				printf ("Copyright (C) GPL 2020 Robin Gareus <robin@gareus.org>\n");
				console_madness_end ();
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
				console_madness_end ();
				::exit (EXIT_FAILURE);
				break;
		}
	}

	if (optind >= argc) {
		std::cerr << "Error: Missing parameter. See --help for usage information.\n";
		console_madness_end ();
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

#ifdef PLATFORM_WINDOWS
	::SetUnhandledExceptionFilter (crash_handler);
#else
	signal (SIGSEGV, sig_handler);
	signal (SIGBUS, sig_handler);
	signal (SIGILL, sig_handler);
	signal (SIGABRT, sig_handler);
#endif

	bool err = false;

	while (optind < argc) {
		if (!scan_vst3 (argv[optind++], force, verbose)) {
			err = true;
		}
		if (stop_on_error) {
			break;
		}
	}

	PBD::cleanup();

	console_madness_end ();

	if (err) {
		return EXIT_FAILURE;
	} else {
		return EXIT_SUCCESS;
	}
}
