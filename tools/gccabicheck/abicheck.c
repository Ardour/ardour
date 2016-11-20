/* Copyright (C) 2015 Robin Gareus <robin@gareus.org>
 *
 * This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://www.wtfpl.net/ for more details.
 */

// gcc -Wall -o gcc-glibmm-abi-check abicheck.c -ldl
// help2man -N -n 'glib gcc4/5 C++11 ABI compatibility test' -o gcc-glibmm-abi-check.1 ./gcc-glibmm-abi-check

#include <stdio.h>
#include <dlfcn.h>
#include <getopt.h>

#ifndef VERSION
#define VERSION "0.1"
#endif

static void print_usage (void) {
	printf ("gcc-glibmm-abi-check - gcc4/5 C++11 ABI compatibility test\n\n");

	printf ("Usage: gcc-glibmm-abi-check [ OPTIONS ]\n\n");
	printf (
			"This tool checks for C++ specific symbols in libglimm which are different in\n"
			"the gcc4 and gcc5/c++11 ABI in order to determine system-wide use of gcc5.\n"
			// TODO document error codes,...
			);

	printf ("\nOptions:\n"
			" -f, --fail                fail if system cannot be determined.\n"
			" -h, --help                Display this help and exit.\n"
			" -4, --gcc4                Test succeeds if gcc4 ABI is found.\n"
			" -5, --gcc5                Test succeeds if gcc5 ABI is found.\n"
			" -g <soname>, --glibmm <soname>\n"
			"                           Specify alternative file for libglibmm-2.4.so\n"
			" -v, --verbose             Print information.\n"
			" -V, --version             Print version information and exit.\n"
			);
}

static void print_version (void) {
	printf ("gcc-glibmm-abi-check version %s\n\n", VERSION);
	printf (
			"Copyright (C) 2015 Robin Gareus <robin@gareus.org>\n"
			"This is free software; see the source for copying conditions.  There is NO\n"
			"warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");
}


int main (int argc, char **argv) {
	int expect = 0;
	int error_fail = 0;
	int verbose = 0;

	char const * glibmm = "libglibmm-2.4.so.1";

	const struct option long_options[] = {
		{ "fail",       no_argument,       0, 'f' },
		{ "help",       no_argument,       0, 'h' },
		{ "gcc4",       no_argument,       0, '4' },
		{ "gcc5",       no_argument,       0, '5' },
		{ "glibmm",     required_argument, 0, 'g' },
		{ "verbose",    no_argument,       0, 'v' },
		{ "version",    no_argument,       0, 'V' },
	};

	const char *optstring = "fh45g:vV";

	int c;
	while ((c = getopt_long (argc, argv, optstring, long_options, NULL)) != -1) {
		switch (c) {
			case 'f':
				error_fail = 1;
				break;
			case 'h':
				print_usage ();
				return 0;
				break;
			case '4':
				expect |= 1;
				break;
			case '5':
				expect |= 2;
				break;
			case 'g':
				glibmm = optarg;
				break;
			case 'v':
				verbose = 1;
				break;
			case 'V':
				print_version ();
				return 0;
				break;
			default:
				fprintf (stderr, "invalid argument.\n");
				print_usage ();
				return -1;
				break;
		}
	}

	int gcc5 = 0;
	int gcc4 = 0;

	dlerror (); // reset error

	void *h = dlopen (glibmm, RTLD_LAZY);
	if (!h) {
		if (verbose) {
			fprintf (stderr, "Cannot open '%s': %s.\n", glibmm, dlerror ());
		}
		return error_fail ? 3 : 0;
	}

	// Glib::ustring::ustring(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&)
	if (dlsym (h, "_ZN4Glib7ustringC1ERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE")) {
		gcc5 |= 1;
	}

	// Glib::ustring::ustring(std::basic_string<char, std::char_traits<char>, std::allocator<char> > const&)
	if (dlsym (h, "_ZN4Glib7ustringC1ERKSs")) {
		gcc4 |= 1;
	}


	// Glib::Module::Module(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, Glib::ModuleFlags)
	if (dlsym (h, "_ZN4Glib6ModuleC1ERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEENS_11ModuleFlagsE")) {
		gcc5 |= 2;
	}

	// Glib::Module::Module(std::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, Glib::ModuleFlags)
	if (dlsym (h, "_ZN4Glib6ModuleC1ERKSsNS_11ModuleFlagsE")) {
		gcc4 |= 2;
	}


	// Glib::ustring::operator=(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&)
	if (dlsym (h, "_ZN4Glib7ustringaSERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE")) {
		gcc5 |= 4;
	}

	// Glib::ustring::operator=(std::basic_string<char, std::char_traits<char>, std::allocator<char> > const&)
	if (dlsym (h, "_ZN4Glib7ustringaSERKSs")) {
		gcc4 |= 4;
	}

	dlclose (h);

	if (7 != (gcc4 ^ gcc5)) {
		if (verbose) {
			fprintf (stderr, "Inconsistent result: gcc4=%x gcc5=%x\n", gcc4, gcc5);
		}
	}
	else if (gcc4 == 7) {
		if (verbose) {
			printf ("System uses gcc4 c++ ABI\n");
		}
		if (expect != 0) {
			return (expect & 1) ? 0 : 1;
		}
	}
	else if (gcc5 == 7) {
		if (verbose) {
			printf ("System uses gcc5 c++11 ABI\n");
		}
		if (expect != 0) {
			return (expect & 2) ? 0 : 1;
		}
	}
	else if (verbose) {
		fprintf (stderr, "Incomplete result: gcc4=%x gcc5=%x\n", gcc4, gcc5);
	}

	return error_fail ? 2 : 0;
}
