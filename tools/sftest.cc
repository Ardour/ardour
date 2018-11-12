/* g++ -o sftest sftest.cc `pkg-config --cflags --libs sndfile` `pkg-config --cflags --libs glibmm-2.4` */

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
#include <signal.h>
#include <float.h>

#include <glibmm/miscutils.h>

using namespace std;

SF_INFO format_info;
float* data = 0;
bool with_sync = false;
bool keep_writing = true;

void
signal_handler (int)
{
	keep_writing = false;
}

int
write_one (SNDFILE* sf, uint32_t nframes)
{
	if (sf_write_float (sf, (float*) data, nframes) != nframes) {
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
	cout << "sftest [ -f HEADER-FORMAT ] [ -F DATA-FORMAT ] [ -r SAMPLERATE ] [ -n NFILES ] [ -b BLOCKSIZE ] [ -s ]";

#ifdef __APPLE__
	cout << " [ -D ]";
#endif
	cout << endl;

	cout << "\tHEADER-FORMAT is one of:" << endl
	     << "\t\tWAV" << endl
	     << "\t\tCAF" << endl
	     << "\t\tW64" << endl;
	cout << "\tDATA-FORMAT is one of:" << endl
	     << "\t\tFLOAT" << endl
	     << "\t\t32" << endl
	     << "\t\t24" << endl
	     << "\t\t16" << endl;
}

int
main (int argc, char* argv[])
{
	vector<SNDFILE*> sndfiles;
	uint32_t sample_size;
	char optstring[] = "f:r:F:n:c:b:sd:qS:"
#ifdef __APPLE__
		"D"
#endif
		;
	int channels = 1;
	int samplerate = 48000;
	char const *suffix = ".wav";
	char const *header_format = "wav";
	char const *data_format = "float";
	size_t filesize = 10 * 1048576;
	uint32_t block_size = 64 * 1024;
	uint32_t nfiles = 100;
	string dirname = "/tmp";
	bool quiet = false;
#ifdef __APPLE__
        bool direct = false;
#endif
	const struct option longopts[] = {
		{ "header-format", 1, 0, 'f' },
		{ "data-format", 1, 0, 'F' },
		{ "rate", 1, 0, 'r' },
		{ "nfiles", 1, 0, 'n' },
		{ "blocksize", 1, 0, 'b' },
		{ "channels", 1, 0, 'c' },
		{ "sync", 0, 0, 's' },
		{ "dirname", 1, 0, 'd' },
		{ "quiet", 0, 0, 'q' },
		{ "filesize", 1, 0, 'S' },
#ifdef __APPLE__
		{ "direct", 0, 0, 'D' },
#endif
		{ 0, 0, 0, 0 }
	};

	int option_index = 0;
	int c = 0;

	while (1) {
		if ((c = getopt_long (argc, argv, optstring, longopts, &option_index)) == -1) {
			break;
		}

		switch (c) {
		case 'f':
			header_format = optarg;
			break;

		case 'F':
			data_format = optarg;
			break;

		case 'r':
			samplerate = atoi (optarg);
			break;

		case 'n':
			nfiles = atoi (optarg);
			break;

		case 'c':
			channels = atoi (optarg);

		case 'b':
			block_size = atoi (optarg);
			break;
		case 's':
			with_sync = true;
			break;
		case 'S':
			filesize = atoi (optarg);
			break;
#ifdef __APPLE__
                case 'D':
                        direct = true;
                        break;
#endif
		case 'd':
			dirname = optarg;
			break;
		case 'q':
			quiet = true;
			break;
		default:
			usage ();
			return 0;
		}
	}

	/* setup file format */
	memset (&format_info, 0, sizeof (format_info));

	if (samplerate == 0 || nfiles == 0 || block_size == 0 || channels == 0) {
		usage ();
		return 1;
	}

	format_info.samplerate = samplerate;
	format_info.channels = channels;

	if (strcasecmp (header_format, "wav") == 0) {
		format_info.format |= SF_FORMAT_WAV;
		suffix = ".wav";
	} else if (strcasecmp (header_format, "caf") == 0) {
		format_info.format |= SF_FORMAT_CAF;
		suffix = ".caf";
	} else if (strcasecmp (header_format, "w64") == 0) {
		format_info.format |= SF_FORMAT_W64;
		suffix = ".w64";
	} else {
		usage ();
		return 0;
	}

	if (strcasecmp (data_format, "float") == 0) {
		format_info.format |= SF_FORMAT_FLOAT;
		sample_size = sizeof (float);
	} else if (strcasecmp (data_format, "32") == 0) {
		format_info.format |= SF_FORMAT_PCM_32;
		sample_size = 4;
	} else if (strcasecmp (data_format, "24") == 0) {
		format_info.format |= SF_FORMAT_PCM_24;
		sample_size = 3;
	} else if (strcasecmp (data_format, "16") == 0) {
		format_info.format |= SF_FORMAT_PCM_16;
		sample_size = 2;
	} else {
		usage ();
		return 0;
	}

	string tmpdirname = Glib::build_filename (dirname, "sftest");
	if (g_mkdir_with_parents (tmpdirname.c_str(), 0755)) {
		cerr << "Cannot create output directory\n";
		return 1;
	}

	for (uint32_t n = 0; n < nfiles; ++n) {
		SNDFILE* sf;
		string path;
		stringstream ss;

		ss << "sf-";
		ss << n;
		ss << suffix;

		path = Glib::build_filename (tmpdirname, ss.str());

                int flags = O_RDWR|O_CREAT|O_TRUNC;
                int fd = open (path.c_str(), flags, 0644);

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
		if ((sf = sf_open_fd (fd, SFM_RDWR, &format_info, true)) == 0) {
			cerr << "Could not open SNDFILE #" << n << " @ " << path << " (" << sf_strerror (0) << ")" << endl;
			return 1;
		}

		sndfiles.push_back (sf);
	}

	if (!quiet) {
		cout << nfiles << " files are in " << tmpdirname;
#ifdef __APPLE__
		cout << " all used " << (direct ? "without" : "with") << " OS buffer cache";
#endif
		cout << endl;
		cout << "Format is " << suffix << ' ' << channels << " channel" << (channels > 1 ? "s" : "") << " written in chunks of " << block_size << " samples, synced ? " << (with_sync ? "yes" : "no") << endl;
	}

	data = new float[block_size*channels];
	uint64_t written = 0;

	signal (SIGINT, signal_handler);
	signal (SIGSTOP, signal_handler);


	double max_bandwidth = 0;
	double min_bandwidth = DBL_MAX;

	while (keep_writing && written < filesize) {
		gint64 before;
		before = g_get_monotonic_time();
		for (vector<SNDFILE*>::iterator s = sndfiles.begin(); s != sndfiles.end(); ++s) {
			if (write_one (*s, block_size)) {
				cerr << "Write failed for file #" << distance (sndfiles.begin(), s) << endl;
				return 1;
			}
		}
		written += block_size;
		gint64 elapsed = g_get_monotonic_time() - before;
		double bandwidth = (sndfiles.size() * block_size * channels * sample_size) / (elapsed/1000000.0);
		double data_minutes = written / (double) (60.0 * 48000.0);
		const double data_rate = sndfiles.size() * channels * sample_size * samplerate;
		stringstream ds;
		ds << setprecision (1) << data_minutes;

		max_bandwidth = max (max_bandwidth, bandwidth);
		min_bandwidth = min (min_bandwidth, bandwidth);

		if (!quiet) {
			cout << "BW @ " << written << " samples (" << ds.str() << " minutes) = " << (bandwidth/1048576.0) <<  " MB/sec " << bandwidth / data_rate << " x faster than necessary " << endl;
		}
	}

	cout << "Max bandwidth = " << max_bandwidth / 1048576.0 << " MB/sec" << endl;
	cout << "Min bandwidth = " << min_bandwidth / 1048576.0 << " MB/sec" << endl;

	if (!quiet) {
		cout << "Closing files ...\n";
		for (vector<SNDFILE*>::iterator s = sndfiles.begin(); s != sndfiles.end(); ++s) {
		sf_close (*s);
		}
		cout << "Done.\n";
	}

	return 0;
}


