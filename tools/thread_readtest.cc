/* g++ -o thread_readtest thread_readtest.cc `pkg-config --cflags --libs glibmm-2.4` -lm */

#ifndef _WIN32
#  define HAVE_MMAP
#endif

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <math.h>

#undef HAVE_MMAP
#ifdef HAVE_MMAP
#  include <sys/stat.h>
#  include <sys/mman.h>
#endif

#include <glibmm.h>

char* data = 0;

void
usage ()
{
	fprintf (stderr, "thread_readtest [ -b BLOCKSIZE ] [ -l FILELIMIT] [ -n NTHREADS ] [ -D ] [ -R ] [ -M ] filename-template\n");
}

Glib::Threads::Cond pool_run;
Glib::Threads::Cond pool_done;
Glib::Threads::Mutex pool_lock;
std::list<int> pool_work;
std::vector<Glib::Threads::Thread*> thread_pool;
int pool_errors = 0;
bool thread_pool_lives = true;

struct ThreadData {
	int id;
	char *data;
	size_t block_size;
};

void
thread_pool_work (ThreadData* td)
{
	Glib::Threads::Mutex::Lock lm (pool_lock);

	while (thread_pool_lives) {
		pool_run.wait (pool_lock);

		if (!thread_pool_lives) {
			return;
		}

		/* we're awake ... get some work */

		while (!pool_work.empty()) {

			int file_descriptor = pool_work.front ();
			pool_work.pop_front ();

			/* release the lock while we do work */

			lm.release ();

			/* do the work */

			int err = 0;

			ssize_t nread;

		  again:
			if ((nread = ::read (file_descriptor, td->data, td->block_size)) != td->block_size) {
				if (nread != 0 && errno == EAGAIN) {
					fprintf (stderr, "read requires retry\n");
					goto again;
				}
				if (nread != 0) {
					fprintf (stderr, "thread %d has error = %s\n", td->id, strerror (errno));
				}
				err++;
			}

			/* reacquire lock so that we can check the status of
			 * things and possibly wake the master.
			 */

			lm.acquire ();
			pool_errors += err;

			if (pool_work.empty()) {
				/* work is finished, tell the master */
				pool_done.signal ();
			}
		}

	}

	return;
}

void
stop_thread_pool ()
{
	{
		Glib::Threads::Mutex::Lock lm (pool_lock);
		thread_pool_lives = false;
		pool_work.clear ();
		pool_run.broadcast ();
	}

	/* XXX wait for each thread to finish */
}

void
build_thread_pool (int nthreads, size_t block_size)
{
	for (int n = 0; n < nthreads; ++n) {
		ThreadData* td = new ThreadData;
		td->data = (char*) malloc (sizeof (char) * block_size);
		td->block_size = block_size;
		td->id = n;

		thread_pool.push_back (Glib::Threads::Thread::create (sigc::bind (sigc::ptr_fun (thread_pool_work), td)));
	}
}

int
run_thread_pool (int* files, int nfiles)
{
	Glib::Threads::Mutex::Lock lm (pool_lock);

	/* Queue up all the files */
	for (int n = 0; n < nfiles; ++n) {
		pool_work.push_back (files[n]);
	}

	pool_errors = 0;

	/* wake everybody up */
	pool_run.broadcast ();

	/* wait for everyone to finish */

	pool_done.wait (pool_lock);

	if (pool_errors) {
		return -1;
	}

	return 0;
}

int
main (int argc, char* argv[])
{
	int* files;
	char optstring[] = "b:DRMl:q";
	uint32_t block_size = 64 * 1024 * 4;
	int max_files = -1;
	int nthreads = 16;
#ifdef __APPLE__
	int direct = 0;
	int noreadahead = 0;
#endif
#ifdef HAVE_MMAP
	int use_mmap = 0;
	void  **addr;
	size_t *flen;
#endif
	const struct option longopts[] = {
		{ "blocksize", 1, 0, 'b' },
		{ "direct", 0, 0, 'D' },
		{ "mmap", 0, 0, 'M' },
		{ "noreadahead", 0, 0, 'R' },
		{ "limit", 1, 0, 'l' },
		{ "nthreads", 16, 0, 'n' },
		{ 0, 0, 0, 0 }
	};

	int option_index = 0;
	int c = 0;
	char const * name_template = 0;
	int flags = O_RDONLY;
	int n = 0;
	int nfiles = 0;
	int quiet = 0;

	while (1) {
		if ((c = getopt_long (argc, argv, optstring, longopts, &option_index)) == -1) {
			break;
		}

		switch (c) {
		case 'b':
			block_size = atoi (optarg);
			break;
		case 'l':
			max_files = atoi (optarg);
			break;
		case 'D':
#ifdef __APPLE__
			direct = 1;
#endif
			break;
		case 'M':
#ifdef HAVE_MMAP
			use_mmap = 1;
#endif
			break;
		case 'R':
#ifdef __APPLE__
			noreadahead = 1;
#endif
			break;
		case 'q':
			quiet = 1;
			break;
		case 'n':
			nthreads = atoi (optarg);
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

		if (max_files > 0 &&  n >= max_files) {
			break;
		}
	}

	if (n == 0) {
		fprintf (stderr, "No matching files found for %s\n", name_template);
		return 1;
	}

	if (!quiet) {
		printf ("# Discovered %d files using %s\n", n, name_template);
	}

	nfiles = n;
	files = (int *) malloc (sizeof (int) * nfiles);
#ifdef HAVE_MMAP
	if (use_mmap) {
		if (!quiet) {
			printf ("# Using mmap().\n");
		}
		addr = malloc (sizeof (void*) * nfiles);
		flen = (size_t*) malloc (sizeof (size_t) * nfiles);
	}
#endif

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

		if (noreadahead) {
			if (fcntl (fd, F_RDAHEAD, 0) == -1) {
				fprintf (stderr, "Cannot set F_READAHED on file #%d\n", n);
			}
		}
#endif

		files[n] = fd;

#ifdef HAVE_MMAP
		if (use_mmap) {
			struct stat s;
			if (fstat (fd, & s)) {
				fprintf (stderr, "Could not stat fd #%d @ %s\n", n, path);
				return 1;
			}
			if (s.st_size < block_size) {
				fprintf (stderr, "file is shorter than blocksize #%d @ %s\n", n, path);
				return 1;
			}
			flen[n] = s.st_size;
			addr[n] = mmap (0, s.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
			if (addr[n] == MAP_FAILED) {
				fprintf (stderr, "Could not mmap file #%d @ %s (%s)\n", n, path, strerror (errno));
				return 1;
			}
		}
#endif
	}

	data = (char*) malloc (sizeof (char) * block_size);
	uint64_t _read = 0;
	double max_elapsed = 0;
	double total_time = 0;
	double var_m = 0;
	double var_s = 0;
	uint64_t cnt = 0;

	build_thread_pool (nthreads, block_size);

	while (1) {
		gint64 before;
		before = g_get_monotonic_time();

		if (run_thread_pool (files, nfiles)) {
			fprintf (stderr, "thread pool error\n");
			goto out;
		}

		_read += block_size;
		gint64 elapsed = g_get_monotonic_time() - before;
		double bandwidth = ((nfiles * block_size)/1048576.0) / (elapsed/1000000.0);

		if (!quiet) {
			printf ("# BW @ %lu %.3f seconds bandwidth %.4f MB/sec\n", (long unsigned int)_read, elapsed/1000000.0, bandwidth);
		}

		total_time += elapsed;

		++cnt;
		if (max_elapsed == 0) {
			var_m = elapsed;
		} else {
			const double var_m1 = var_m;
			var_m = var_m + (elapsed - var_m) / (double)(cnt);
			var_s = var_s + (elapsed - var_m) * (elapsed - var_m1);
		}

		if (elapsed > max_elapsed) {
			max_elapsed = elapsed;
		}

	}

out:
	if (max_elapsed > 0 && total_time > 0) {
		double stddev = cnt > 1 ? sqrt(var_s / ((double)(cnt-1))) : 0;
		double bandwidth = ((nfiles * _read)/1048576.0) / (total_time/1000000.0);
		double min_throughput = ((nfiles * block_size)/1048576.0) / (max_elapsed/1000000.0);
		printf ("# Min: %.4f MB/sec Avg: %.4f MB/sec  || Max: %.3f sec \n", min_throughput, bandwidth, max_elapsed/1000000.0);
		printf ("# Max Track count: %d @ 48000SPS\n", (int) floor(1048576.0 * bandwidth / (4 * 48000.)));
		printf ("# Sus Track count: %d @ 48000SPS\n", (int) floor(1048576.0 * min_throughput / (4 * 48000.)));
		printf ("# seeks: %llu: bytes: %llu total_time: %f\n", cnt * nfiles, (nfiles * _read), total_time/1000000.0);
		printf ("%d %.4f %.4f %.4f %.5f\n", block_size, min_throughput, bandwidth, max_elapsed/1000000.0, stddev/1000000.0);
	}

	return 0;
}
