/*
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
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
#ifndef _pbd_archive_h_
#define _pbd_archive_h_

#include <pthread.h>

#include "pbd/signals.h"

#ifndef LIBPBD_API
#include "pbd/libpbd_visibility.h"
#endif


namespace PBD {

class LIBPBD_API FileArchive
{
	public:
		FileArchive (const std::string& url);
		~FileArchive ();

		int inflate (const std::string& destdir);
		std::vector<std::string> contents ();

		std::string next_file_name ();
		int extract_current_file (const std::string& destpath);

		/* these are mapped to libarchive's lzmaz
		 * compression level 0..9
		 */
		enum CompressionLevel {
			CompressNone = -1,
			CompressFast = 0,
			CompressGood = 6
		};

		int create (const std::string& srcdir, CompressionLevel compression_level = CompressGood);
		int create (const std::map <std::string, std::string>& filemap, CompressionLevel compression_level = CompressGood);

		PBD::Signal2<void, size_t, size_t> progress; // TODO

		struct MemPipe {
			public:
				MemPipe ()
					: data (NULL)
					, progress (0)
				{
					pthread_mutex_init (&_lock, NULL);
					pthread_cond_init (&_ready, NULL);
					reset ();
				}

				~MemPipe ()
				{
					lock ();
					free (data);
					unlock ();

					pthread_mutex_destroy (&_lock);
					pthread_cond_destroy (&_ready);
				}

				void reset ()
				{
					lock ();
					free (data);
					data = 0;
					size = 0;
					done = false;
					processed = 0;
					length = -1;
					unlock ();
				}

				void lock ()   { pthread_mutex_lock (&_lock); }
				void unlock () { pthread_mutex_unlock (&_lock); }
				void signal () { pthread_cond_signal (&_ready); }
				void wait ()   { pthread_cond_wait (&_ready, &_lock); }

				uint8_t  buf[8192];
				uint8_t* data;
				size_t   size;
				bool     done;

				double   processed;
				double   length;
				FileArchive* progress;

			private:
				pthread_mutex_t _lock;
				pthread_cond_t  _ready;
		};

		struct Request {
			public:
				Request (const std::string& u)
				{
					if (u.size () > 0) {
						url = strdup (u.c_str());
					} else {
						url = NULL;
					}
				}

				~Request ()
				{
					free (url);
				}

				bool is_remote () const
				{
					if (!strncmp (url, "https://", 8) || !strncmp (url, "http://", 7) || !strncmp (url, "ftp://", 6)) {
						return true;
					}
					return false;
				}

				char* url;
				MemPipe mp;
		};

	private:

		int process_file ();
		int process_url ();

		std::vector<std::string> contents_url ();
		std::vector<std::string> contents_file ();

		int extract_url ();
		int extract_file ();

		int do_extract (struct archive* a);
		std::vector<std::string> get_contents (struct archive *a);

		bool is_url ();

		struct archive* setup_file_archive ();

		Request   _req;
		pthread_t _tid;

		struct archive_entry* _current_entry;
		struct archive* _archive;
};

} /* namespace */
#endif // _reallocpool_h_
