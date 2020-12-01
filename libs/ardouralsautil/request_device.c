/*
 * Copyright (C) 2014 Robin Gareus <robin@gareus.org>
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

// NB generate man-page with
// help2man -N -n "alsa/ardour dbus device request tool" -o ardour-request-device.1 ./build/libs/ardouralsautil/ardour-request-device

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>

#include "ardouralsautil/reserve.h"

#ifndef ARD_PROG_NAME
#define ARD_PROG_NAME "alsa_request_device"
#endif
#ifndef ARD_APPL_NAME
#define ARD_APPL_NAME "ALSA User"
#endif
#ifndef VERSION
#define VERSION "v0.3"
#endif

static int run = 1;
static int release_wait_for_signal = 0;
static pid_t parent_pid = 0;

static void wearedone(int sig) {
	(void) sig; // skip 'unused variable' compiler warning;
	fprintf(stderr, "caught signal - shutting down.\n");
	run=0;
}

static int stdin_available(void) {
	errno = 0;
	if (fcntl(STDIN_FILENO, F_GETFD) == 1) return 0;
	return errno != EBADF;
}

static void print_version (void) {
	printf (ARD_PROG_NAME " " VERSION "\n\n");
	printf (
		"Copyright (C) 2014 Robin Gareus <robin@gareus.org>\n"
		"This is free software; see the source for copying conditions.  There is NO\n"
		"warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\n"
		);
	exit (EXIT_SUCCESS);
}

static void usage (void) {
	printf (ARD_PROG_NAME " - DBus Audio Reservation Utility.\n");
	printf ("Usage: " ARD_PROG_NAME " [ OPTIONS ] <Audio-Device-ID>\n");
	printf ("Options:\n\
      -h, --help                 display this help and exit\n\
      -p, --priority <int>       reservation priority (default: int32_max)\n\
      -P, --pid <int>            process-id to watch (default 0: none)\n\
      -n, --name <string>        application name to use for registration\n\
      -V, --version              print version information and exit\n\
      -w, --releasewait          wait for signal on yield-release\n\
");

	printf ("\n\
This tool issues a dbus request to reserve an ALSA Audio-device.\n\
If successful other users of the device (e.g. pulseaudio) will\n\
release the device.\n\
\n\
" ARD_PROG_NAME " by default announces itself as \"" ARD_APPL_NAME "\"\n\
and uses the maximum possible priority for requesting the device.\n\
These settings can be overridden using the -n and -p options respectively.\n\
\n\
If a PID is given the tool will watch the process and if that is not running\n\
release the device and exit.  Otherwise " ARD_PROG_NAME " runs until\n\
either stdin is closed, a SIGINT or SIGTERM is received or some other\n\
application requests the device with a higher priority.\n\
\n\
Without the -w option, " ARD_PROG_NAME " yields the device after 500ms to\n\
any higher-priority request. With the -w option this tool waits until it\n\
for SIGINT or SIGTERM - but at most 4 sec to acknowledge before releasing.\n\
\n\
The audio-device-id is a string e.g. 'Audio1'\n\
\n\
Examples:\n\
" ARD_PROG_NAME " Audio0\n\
\n");

	printf ("Report bugs to Robin Gareus <robin@gareus.org>\n");
	exit (EXIT_SUCCESS);
}

static struct option const long_options[] =
{
	{"help", no_argument, 0, 'h'},
	{"name", required_argument, 0, 'n'},
	{"pid", required_argument, 0, 'P'},
	{"priority", required_argument, 0, 'p'},
	{"version", no_argument, 0, 'V'},
	{"releasewait", no_argument, 0, 'w'},
	{NULL, 0, NULL, 0}
};

static int request_cb(rd_device *d, int forced) {
	(void) d; // skip 'unused variable' compiler warning;
	(void) forced; // skip 'unused variable' compiler warning;
	fprintf(stdout, "Received higher priority request - releasing device.\n");
	fflush(stdout);
	if(!release_wait_for_signal) {
		usleep (500000);
		run = 0;
	} else if (run) {
		int timeout = 4000;
		fprintf(stdout, "Waiting for acknowledge signal to release.\n");
		while (release_wait_for_signal && run && --timeout) {
			if (!stdin_available()) {
				break;
			}
			if (parent_pid > 0 && kill (parent_pid, 0)) {
				break;
			}
			usleep (1000);
		}
		run = 0;
	}
	return 1; // OK
}

int main(int argc, char **argv) {
	DBusConnection* dbus_connection = NULL;
	rd_device * reserved_device = NULL;
	DBusError error;
	int ret, c;

	int32_t priority = INT32_MAX;
	char *name = strdup(ARD_APPL_NAME);

	while ((c = getopt_long (argc, argv,
					"h"  /* help */
					"n:" /* name */
					"P:" /* pid */
					"p:" /* priority */
					"V"  /* version */
					"w", /* release wait for signal */
					long_options, (int *) 0)) != EOF)
	{
		switch (c) {
			case 'h':
				free(name);
				usage ();
				break;
			case 'n':
				free(name);
				name = strdup(optarg);
				break;
			case 'p':
				priority = atoi (optarg);
				if (priority < 0) priority = 0;
				break;
			case 'P':
				parent_pid = atoi (optarg);
				break;
			case 'V':
				free(name);
				print_version ();
				break;
			case 'w':
				release_wait_for_signal = 1;
				break;
			default:
				free(name);
				fprintf (stderr, "Error: unrecognized option. See --help for usage information.\n");
				exit (EXIT_FAILURE);
				break;
		}
	}

	if (optind + 1 != argc) {
		fprintf (stderr, "Error: Missing parameter. See --help for usage information.\n");
		free(name);
		return EXIT_FAILURE;
	}
	const char *device_name = argv[optind];

	if (parent_pid > 0 && kill (parent_pid, 0)) {
		fprintf(stderr, "Given PID to watch is not running.\n");
		free(name);
		return EXIT_FAILURE;
	}

	dbus_error_init(&error);

	if (!(dbus_connection = dbus_bus_get (DBUS_BUS_SESSION, &error))) {
		fprintf(stderr, "Failed to connect to session bus for device reservation: %s\n", error.message ? error.message : "unknown error.");
		dbus_error_free(&error);
		free(name);
		return EXIT_FAILURE;
	}

	if ((ret = rd_acquire (
					&reserved_device,
					dbus_connection,
					device_name,
					name,
					priority,
					request_cb,
					&error)) < 0)
	{
		fprintf(stderr, "Failed to acquire device: '%s'\n%s\n", device_name, (error.message ? error.message : strerror(-ret)));
		dbus_error_free(&error);
		dbus_connection_unref(dbus_connection);
		free(name);
		return EXIT_FAILURE;
	}

	fprintf(stdout, "Acquired audio-card '%s'\n", device_name);
	fprintf(stdout, "Press Ctrl+C or close stdin to release the device.\n");
	fflush(stdout);

	signal(SIGTERM, wearedone);
	signal(SIGINT, wearedone);

	while (run && dbus_connection_read_write_dispatch (dbus_connection, 200)) {
		if (!stdin_available()) {
			fprintf(stderr, "stdin closed - releasing device.\n");
			break;
		}
		if (parent_pid > 0 && kill (parent_pid, 0)) {
			fprintf(stderr, "watched PID no longer exists - releasing device.\n");
			break;
		}
	}

	rd_release (reserved_device);
	fprintf(stdout, "Released audio-card '%s'\n", device_name);

	dbus_connection_unref(dbus_connection);
	dbus_error_free(&error);
	free(name);
	return EXIT_SUCCESS;
}
