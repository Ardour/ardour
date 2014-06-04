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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include "ardouralsautil/reserve.h"

static int run = 1;

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

static void usage(int status) {
	printf ("ardour-request-device - DBus Audio Reservation Utility.\n");
	printf ("Usage: ardour-request-device [ OPTIONS ] <Audio-Device-ID>\n");
	printf ("Options:\n\
      -h, --help                 display this help and exit\n\
      -V, --version              print version information and exit\n\
");

	printf ("\n\
This tool issues a dbus request to reserve an ALSA Audio-device.\n\
If successful other users of the device (e.g. pulseaudio) will\n\
release the device so that ardour can access it.\n\
\n\
ardour-request-device announces itself as \"Ardour ALSA Backend\" and uses the\n\
maximum possible priority for requesting the device.\n\
\n\
The audio-device-id is a string e.g. 'Audio1'\n\
\n\
Examples:\n\
ardour-request-device Audio0\n\
\n");

	printf ("Report bugs to Robin Gareus <robin@gareus.org>\n");
	exit (status);
}

static void print_version(void) {
	printf ("ardour-request-device 0.1\n\n");
	printf (
			"Copyright (C) 2014 Robin Gareus <robin@gareus.org>\n"
			"This is free software; see the source for copying conditions.  There is NO\n"
			"warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\n"
			);
	exit (0);
}

int main(int argc, char **argv) {
	DBusConnection* dbus_connection = NULL;
	rd_device * reserved_device = NULL;
	DBusError error;
	int ret;

	if (argc < 2 || argc > 2) {
		usage(EXIT_FAILURE);
	}
	if (!strcmp(argv[1], "-V") || !strcmp(argv[1], "--version")) {
		print_version();
	}
	if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
		usage(EXIT_SUCCESS);
	}

	const char *device_name = argv[1];

	dbus_error_init(&error);

	if (!(dbus_connection = dbus_bus_get (DBUS_BUS_SESSION, &error))) {
		fprintf(stderr, "Failed to connect to session bus for device reservation: %s\n", error.message ? error.message : "unknown error.");
		dbus_error_free(&error);
		return EXIT_FAILURE;
	}

	if ((ret = rd_acquire (
					&reserved_device,
					dbus_connection,
					device_name,
					"Ardour ALSA Backend",
					INT32_MAX,
					NULL,
					&error)) < 0)
	{
		fprintf(stderr, "Failed to acquire device: '%s'\n%s\n", device_name, (error.message ? error.message : strerror(-ret)));
		dbus_error_free(&error);
		dbus_connection_unref(dbus_connection);
		return EXIT_FAILURE;
	}

	fprintf(stdout, "Acquired audio-card '%s'\n", device_name);
	fprintf(stdout, "Press Ctrl+C or close stdin to release the device.\n");
	fflush(stdout);

	signal(SIGTERM, wearedone);
	signal(SIGINT, wearedone);

	while (run && dbus_connection_read_write_dispatch (dbus_connection, 200)) {
		if (!stdin_available()) {
			fprintf(stderr, "stdin closed - shutting down.\n");
			break;
		}
	}

	rd_release (reserved_device);
	fprintf(stdout, "Released audio-card '%s'\n", device_name);
	dbus_error_free(&error);
	dbus_connection_unref(dbus_connection);
	return EXIT_SUCCESS;
}
