/* g++ -o sfrtest sfrtest.cc `pkg-config --cflags --libs sndfile` `pkg-config --cflags --libs glibmm-2.4` */

#include <vector>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstdlib>
#include <cerrno>

#include <unistd.h>
#include <sndfile.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>

#include <glibmm/miscutils.h>
#include <glibmm/fileutils.h>

using namespace std;

SF_INFO format_info;
float* data = 0;
int
read_one (SNDFILE* sf, uint32_t nframes)
{
	if (sf_read_float (sf, (float*) data, nframes) != nframes) {
		return -1;
	}

	if (with_sync) {
		sf_write_sync (sf);
	}

	return 0;
}

void
usage ()
{
	cout << "sfrtest [ -n NFILES ] [ -b BLOCKSIZE ] [ -s ] [ -D ] filename-template" << endl;
}

int
main (int argc, char* argv[])
{
	vector<SNDFILE*> sndfiles;
	uint32_t sample_size = sizeof (float);
	char optstring[] = "n:b:sD";
	uint32_t block_size = 64 * 1024;
	uint32_t nfiles = 100;
        bool direct = false;
	const struct option longopts[] = {
		{ "nfiles", 1, 0, 'n' },
		{ "blocksize", 1, 0, 'b' },
		{ "direct", 0, 0, 'D' },
		{ 0, 0, 0, 0 }
	};

	int option_index = 0;
	int c = 0;
	char const * name_template = 0;
	int samplerate;

	while (1) {
		if ((c = getopt_long (argc, argv, optstring, longopts, &option_index)) == -1) {
			break;
		}

		switch (c) {
		case 'n':
			nfiles = atoi (optarg);
			break;
		case 'b':
			block_size = atoi (optarg);
			break;
                case 'D':
                        direct = true;
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

	for (uint32_t n = 1; n <= nfiles; ++n) {
		SNDFILE* sf;
		char path[PATH_MAX+1];

		snprintf (path, sizeof (path), name_template, n);

		if (!Glib::file_test (path, Glib::FILE_TEST_EXISTS|Glib::FILE_TEST_IS_REGULAR)) {
			break;
		}

                int flags = O_RDONLY;
                int fd = open (path, flags, 0644);

                if (fd < 0) {
			cerr << "Could not open file #" << n << " @ " << path << " (" << strerror (errno) << ")" << endl;
			return 1;
                }

#ifdef __APPLE__
                if (direct) {
                        /* Apple man pages say only that it returns "a value other than -1 on success",
                           which probably means zero, but you just can't be too careful with
                           those guys.
                        */
                        if (fcntl (fd, F_NOCACHE, 1) == -1) {
                                cerr << "Cannot set F_NOCACHE on file # " << n << endl;
                        }
                }
#endif

		if ((sf = sf_open_fd (fd, SFM_READ, &format_info, true)) == 0) {
			cerr << "Could not open SNDFILE #" << n << " @ " << path << " (" << sf_strerror (0) << ")" << endl;
			return 1;
		}

		samplerate = format_info.samplerate;

		sndfiles.push_back (sf);
	}

	cout << "Discovered " << sndfiles.size() << " files using " << name_template << endl;

	data = new float[block_size];
	uint64_t read = 0;

	while (true) {
		gint64 before;
		before = g_get_monotonic_time();
		for (vector<SNDFILE*>::iterator s = sndfiles.begin(); s != sndfiles.end(); ++s) {
			if (read_one (*s, block_size)) {
				cerr << "Read failed for file #" << distance (sndfiles.begin(), s) << endl;
				return 1;
			}
		}
		read += block_size;
		gint64 elapsed = g_get_monotonic_time() - before;
                double bandwidth = ((sndfiles.size() * block_size * sample_size)/1048576.0) / (elapsed/1000000.0);

                printf ("BW @ %Lu %.3f seconds bandwidth %.4f MB/sec\n", read, elapsed/1000000.0, bandwidth);
	}

	return 0;
}


