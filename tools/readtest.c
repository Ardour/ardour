/* gcc -o readtest readtest.cc `pkg-config --cflags --libs glib-2.0` */

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <stdbool.h>

#include <glib.h>

char* data = 0;

int
read_one (int fd, ssize_t sz)
{
	return 0;
}

void
usage ()
{
	fprintf (stderr, "readtest [ -b BLOCKSIZE ] [ -s ] [ -D ] filename-template\n");
}

int
main (int argc, char* argv[])
{
	int* files;
	char optstring[] = "b:D";
	uint32_t block_size = 64 * 1024 * 4;
#ifdef __APPLE__
	bool direct = false;
#endif
	const struct option longopts[] = {
		{ "blocksize", 1, 0, 'b' },
		{ "direct", 0, 0, 'D' },
		{ 0, 0, 0, 0 }
	};

	int option_index = 0;
	int c = 0;
	char const * name_template = 0;
	int flags = O_RDONLY;
	int n = 0;
	int nfiles = 0;

	while (1) {
		if ((c = getopt_long (argc, argv, optstring, longopts, &option_index)) == -1) {
			break;
		}

		switch (c) {
			case 'b':
				block_size = atoi (optarg);
				break;
			case 'D':
#ifdef __APPLE__
				direct = true;
#endif
				break;
			default:
				usage ();
				return 0;
		}
	}

	if (optind < argc) {
		name_template = argv[optind];
	} else {
		usage ();
		return 1;
	}

	while (1) {
		char path[PATH_MAX+1];

		snprintf (path, sizeof (path), name_template, n+1);

		if (access (path, R_OK) != 0) {
			break;
		}

		++n;
	}

	if (n == 0) {
		fprintf (stderr, "No matching files found for %s\n", name_template);
		return 1;
	}

	printf ("Discovered %d files using %s\n", n, name_template);

	nfiles = n;
	files = (int *) malloc (sizeof (int) * nfiles);

	for (n = 0; n < nfiles; ++n) {

		char path[PATH_MAX+1];
		int fd;

		snprintf (path, sizeof (path), name_template, n+1);

		if ((fd = open (path, flags, 0644)) < 0) {
			fprintf (stderr, "Could not open file #%d @ %s (%s)\n", n, path, strerror (errno));
			return 1;
		}

#ifdef __APPLE__
		if (direct) {
			/* Apple man pages say only that it returns "a value other than -1 on success",
				 which probably means zero, but you just can't be too careful with
				 those guys.
				 */
			if (fcntl (fd, F_NOCACHE, 1) == -1) {
				fprintf (stderr, "Cannot set F_NOCACHE on file #%d\n", n);
			}
		}
#endif

		files[n] = fd;
	}

	data = (char*) malloc (sizeof (char) * block_size);
	uint64_t _read = 0;
	double min_throughput = -1;
	double total_time = 0;

	while (true) {
		gint64 before;
		before = g_get_monotonic_time();

		for (n = 0; n < nfiles; ++n) {

			if (read (files[n], (char*) data, block_size) != block_size) {
				fprintf (stderr, "read failed on file %d (%s)\n", n, strerror (errno));
				goto out;
			}
		}

		_read += block_size;
		gint64 elapsed = g_get_monotonic_time() - before;
		double bandwidth = ((nfiles * block_size)/1048576.0) / (elapsed/1000000.0);

		printf ("BW @ %Lu %.3f seconds bandwidth %.4f MB/sec\n", _read, elapsed/1000000.0, bandwidth);

		total_time += elapsed;
		if (min_throughput > bandwidth || min_throughput < 0) {
			min_throughput = bandwidth;
		}

	}

out:
	if (min_throughput > 0 && total_time > 0) {
		double bandwidth = ((nfiles * _read)/1048576.0) / (total_time/1000000.0);
		printf ("Min: %.4f MB/sec  Avg: %.4f MB/sec\n", min_throughput, bandwidth);
	}

	return 0;
}
