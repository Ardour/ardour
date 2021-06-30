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

#ifndef NDEBUG
#include <iostream>
#include <iomanip>
#endif

#include <stdlib.h>
#include <string.h>
#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>

#include <glib.h>
#include "pbd/gstdio_compat.h"
#include <glibmm.h>

#include <archive.h>
#include <archive_entry.h>
#include <curl/curl.h>

#include "pbd/failed_constructor.h"
#include "pbd/file_archive.h"
#include "pbd/file_utils.h"
#include "pbd/pthread_utils.h"

using namespace PBD;

static size_t
write_callback (void* buffer, size_t size, size_t nmemb, void* d)
{
	FileArchive::MemPipe* p = (FileArchive::MemPipe*)d;
	size_t realsize = size * nmemb;

	p->lock ();
	p->data = (uint8_t*) realloc ((void*) p->data, p->size + realsize);
	memcpy (&p->data[p->size], buffer, realsize);
	p->size += realsize;
	p->signal ();
	p->unlock ();
	return realsize;
}

static void*
get_url (void* arg)
{
	pthread_set_name ("FileArchiveURL");
	FileArchive::Request* r = (FileArchive::Request*) arg;
	CURL* curl;

	curl = curl_easy_init ();
	curl_easy_setopt (curl, CURLOPT_URL, r->url);
	curl_easy_setopt (curl, CURLOPT_FOLLOWLOCATION, 1L);

	/* get size */
	if (r->mp.progress) {
		curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
		curl_easy_setopt(curl, CURLOPT_HEADER, 0L);
		curl_easy_perform (curl);
		curl_easy_getinfo (curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &r->mp.length);
	}

	curl_easy_setopt(curl, CURLOPT_NOBODY, 0L);
	curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt (curl, CURLOPT_WRITEDATA, (void*) &r->mp);
	curl_easy_perform (curl);
	curl_easy_cleanup (curl);

	r->mp.lock ();
	r->mp.done = 1;
	r->mp.signal ();
	r->mp.unlock ();

	return NULL;
}

static ssize_t
ar_read (struct archive* a, void* d, const void** buff)
{
	FileArchive::MemPipe* p = (FileArchive::MemPipe*)d;
	size_t rv;

	p->lock ();
	while (p->size == 0) {
		if (p->done) {
			p->unlock ();
			return 0;
		}
		p->wait ();
	}

	rv = p->size > 8192 ? 8192 : p->size;
	memcpy (p->buf, p->data, rv);
	if (p->size > rv) {
		memmove (p->data, &p->data[rv], p->size - rv);
	}
	p->size -= rv;
	p->processed += rv;
	*buff = p->buf;
	if (p->progress) {
		p->progress->progress (p->processed, p->length);
	}
	p->unlock ();
	return rv;
}

static int
ar_copy_data (struct archive *ar, struct archive *aw)
{
	for (;;) {
		const void *buff;
		size_t size;
		int64_t offset;
		int r;
		r = archive_read_data_block (ar, &buff, &size, &offset);
		if (r == ARCHIVE_EOF) {
			return (ARCHIVE_OK);
		}
		if (r != ARCHIVE_OK) {
			return (r);
		}
		r = archive_write_data_block (aw, buff, size, offset);
		if (r != ARCHIVE_OK) {
			fprintf (stderr, "Extract/Write Archive: %s", archive_error_string(aw));
			return (r);
		}
	}
}

static struct archive*
setup_archive ()
{
	struct archive* a;
	a = archive_read_new ();
	archive_read_support_filter_all (a);
	archive_read_support_format_all (a);
	return a;
}

FileArchive::FileArchive (const std::string& url)
	: _req (url)
	, _current_entry (0)
	, _archive (0)
{
	if (!_req.url) {
		fprintf (stderr, "Invalid Archive URL/filename\n");
		throw failed_constructor ();
	}

	if (_req.is_remote ()) {
		_req.mp.progress = this;
	} else {
		_req.mp.progress = 0;
	}
}

FileArchive::~FileArchive ()
{
	if (_archive) {
		archive_read_close (_archive);
		archive_read_free (_archive);
	}
}

int
FileArchive::inflate (const std::string& destdir)
{
	int rv = -1;
	std::string pwd (Glib::get_current_dir ());

	if (g_chdir (destdir.c_str ())) {
		fprintf (stderr, "Archive: cannot chdir to '%s'\n", destdir.c_str ());
		return rv;
	}

	if (_req.is_remote ()) {
		rv = extract_url ();
	} else {
		rv = extract_file ();
	}

	g_chdir (pwd.c_str());
	return rv;
}

std::vector<std::string>
FileArchive::contents ()
{
	if (_req.is_remote ()) {
		return contents_url ();
	} else {
		return contents_file ();
	}
}

std::string
FileArchive::next_file_name ()
{
	assert (!_req.is_remote () && "FileArchive: Iterating over archive files not supported for remote archives.\n");

	if (!_archive) {
		_archive = setup_file_archive();
		if (!_archive) {
			return std::string();
		}
	}

	int r = archive_read_next_header (_archive, &_current_entry);
	if (!_req.mp.progress) {
		// file i/o -- not URL
		const uint64_t read = archive_filter_bytes (_archive, -1);
		progress (read, _req.mp.length);
	}

	if (r == ARCHIVE_EOF) {
		goto no_next;
	}

	if (r != ARCHIVE_OK) {
		fprintf (stderr, "Error reading archive: %s\n", archive_error_string(_archive));
		goto no_next;
	}

	return archive_entry_pathname (_current_entry);

no_next:
	_current_entry = 0;
	return std::string();
}

int
FileArchive::extract_current_file (const std::string& destpath)
{
	if (!_archive || !_current_entry) {
		return 0;
	}

	int flags = ARCHIVE_EXTRACT_TIME;

	struct archive *ext;

	ext = archive_write_disk_new();
	archive_write_disk_set_options(ext, flags);

	archive_entry_set_pathname(_current_entry, destpath.c_str());
	int r = archive_write_header(ext, _current_entry);
	_current_entry = 0;
	if (r != ARCHIVE_OK) {
		fprintf (stderr, "Error reading archive: %s\n", archive_error_string(_archive));
		return -1;
	}

	ar_copy_data (_archive, ext);
	r = archive_write_finish_entry (ext);
	if (r != ARCHIVE_OK) {
		fprintf (stderr, "Error reading archive: %s\n", archive_error_string(_archive));
		return -1;
	}

	return 0;
}

std::vector<std::string>
FileArchive::contents_file ()
{
	struct archive* a = setup_archive ();
	GStatBuf statbuf;
	if (!g_stat (_req.url, &statbuf)) {
		_req.mp.length = statbuf.st_size;
	} else {
		_req.mp.length = -1;
	}
	if (ARCHIVE_OK != archive_read_open_filename (a, _req.url, 8192)) {
		fprintf (stderr, "Error opening archive: %s\n", archive_error_string(a));
		return std::vector<std::string> ();
	}
	return get_contents (a);
}

std::vector<std::string>
FileArchive::contents_url ()
{
	_req.mp.reset ();
	pthread_create (&_tid, NULL, get_url, (void*)&_req);

	struct archive* a = setup_archive ();
	archive_read_open (a, (void*)&_req.mp, NULL, ar_read, NULL);
	std::vector<std::string> rv (get_contents (a));

	pthread_join (_tid, NULL);
	return rv;
}

int
FileArchive::extract_file ()
{
	struct archive* a = setup_archive ();
	GStatBuf statbuf;
	if (!g_stat (_req.url, &statbuf)) {
		_req.mp.length = statbuf.st_size;
	} else {
		_req.mp.length = -1;
	}
	if (ARCHIVE_OK != archive_read_open_filename (a, _req.url, 8192)) {
		fprintf (stderr, "Error opening archive: %s\n", archive_error_string(a));
		return -1;
	}
	return do_extract (a);
}

int
FileArchive::extract_url ()
{
	_req.mp.reset ();
	pthread_create (&_tid, NULL, get_url, (void*)&_req);

	struct archive* a = setup_archive ();
	archive_read_open (a, (void*)&_req.mp, NULL, ar_read, NULL);
	int rv = do_extract (a);

	pthread_join (_tid, NULL);
	return rv;
}

std::vector<std::string>
FileArchive::get_contents (struct archive* a)
{
	std::vector<std::string> rv;
	struct archive_entry* entry;
	for (;;) {
		int r = archive_read_next_header (a, &entry);
		if (!_req.mp.progress) {
			// file i/o -- not URL
			const uint64_t read = archive_filter_bytes (a, -1);
			progress (read, _req.mp.length);
		}
		if (r == ARCHIVE_EOF) {
			break;
		}
		if (r != ARCHIVE_OK) {
			fprintf (stderr, "Error reading archive: %s\n", archive_error_string(a));
			break;
		}
		rv.push_back (archive_entry_pathname (entry));
	}

	archive_read_close (a);
	archive_read_free (a);
	return rv;
}

int
FileArchive::do_extract (struct archive* a)
{
	int flags = ARCHIVE_EXTRACT_TIME;

	int rv = 0;
	struct archive_entry* entry;
	struct archive *ext;

	ext = archive_write_disk_new();
	archive_write_disk_set_options(ext, flags);

	for (;;) {
		int r = archive_read_next_header (a, &entry);
		if (!_req.mp.progress) {
			// file i/o -- not URL
			const uint64_t read = archive_filter_bytes (a, -1);
			progress (read, _req.mp.length);
		}

		if (r == ARCHIVE_EOF) {
			break;
		}
		if (r != ARCHIVE_OK) {
			fprintf (stderr, "Error reading archive: %s\n", archive_error_string(a));
			break;
		}

#if 0 // hacky alternative to chdir
		const std::string full_path = Glib::build_filename (destdir, archive_entry_pathname (entry));
		archive_entry_set_pathname (entry, full_path.c_str());
#endif

		r = archive_write_header(ext, entry);
		if (r != ARCHIVE_OK) {
			fprintf (stderr, "Extracting archive: %s\n", archive_error_string(ext));
		} else {
			ar_copy_data (a, ext);
			r = archive_write_finish_entry (ext);
			if (r != ARCHIVE_OK) {
				fprintf (stderr, "Extracting archive: %s\n", archive_error_string(ext));
				rv = -1;
				break;
			}
		}
	}

	archive_read_close (a);
	archive_read_free (a);
	archive_write_close(ext);
	archive_write_free(ext);
	return rv;
}


int
FileArchive::create (const std::string& srcdir, CompressionLevel compression_level)
{
	if (_req.is_remote ()) {
		return -1;
	}

	std::string parent = Glib::path_get_dirname (srcdir);
	size_t p_len = parent.size () + 1;

	Searchpath sp (srcdir);
	std::vector<std::string> files;
	find_files_matching_pattern (files, sp, "*");

	std::map<std::string, std::string> filemap;

	for (std::vector<std::string>::const_iterator f = files.begin (); f != files.end (); ++f) {
		assert (f->size () > p_len);
		filemap[*f] = f->substr (p_len);
	}

	return create (filemap, compression_level);
}

int
FileArchive::create (const std::map<std::string, std::string>& filemap, CompressionLevel compression_level)
{
	struct archive *a;
	struct archive_entry *entry;

	size_t read_bytes = 0;
	size_t total_bytes = 0;

	for (std::map<std::string, std::string>::const_iterator f = filemap.begin (); f != filemap.end (); ++f) {
		GStatBuf statbuf;
		if (g_stat (f->first.c_str(), &statbuf)) {
			continue;
		}
		total_bytes += statbuf.st_size;
	}

	if (total_bytes == 0) {
		return -1;
	}

	progress (0, total_bytes);

	a = archive_write_new ();
	archive_write_set_format_pax_restricted (a);

	if (compression_level != CompressNone) {
		archive_write_add_filter_lzma (a);
		char buf[48];
		sprintf (buf, "lzma:compression-level=%u,lzma:threads=0", (uint32_t) compression_level);
		archive_write_set_options (a, buf);
	}

	archive_write_open_filename (a, _req.url);
	entry = archive_entry_new ();

#ifndef NDEBUG
	  const int64_t archive_start_time = g_get_monotonic_time();
#endif

	for (std::map<std::string, std::string>::const_iterator f = filemap.begin (); f != filemap.end (); ++f) {
		char buf[8192];
		const char* filepath = f->first.c_str ();
		const char* filename = f->second.c_str ();

		GStatBuf statbuf;
		if (g_stat (filepath, &statbuf)) {
			continue;
		}

		archive_entry_clear (entry);

#ifdef PLATFORM_WINDOWS
		archive_entry_set_size (entry, statbuf.st_size);
		archive_entry_set_atime (entry, statbuf.st_atime, 0);
		archive_entry_set_ctime (entry, statbuf.st_ctime, 0);
		archive_entry_set_mtime (entry, statbuf.st_mtime, 0);
#else
		archive_entry_copy_stat (entry, &statbuf);
#endif

		archive_entry_set_pathname (entry, filename);
		archive_entry_set_filetype (entry, AE_IFREG);
		archive_entry_set_perm (entry, 0644);

		archive_write_header (a, entry);

		int fd = g_open (filepath, O_RDONLY, 0444);
		assert (fd >= 0);

		ssize_t len = read (fd, buf, sizeof (buf));
		while (len > 0) {
			read_bytes += len;
			archive_write_data (a, buf, len);
			progress (read_bytes, total_bytes);
			len = read (fd, buf, sizeof (buf));
		}
		close (fd);
	}

	archive_entry_free (entry);
	archive_write_close (a);
	archive_write_free (a);

#ifndef NDEBUG
	const int64_t elapsed_time_us = g_get_monotonic_time() - archive_start_time;
	std::cerr << "archived in " << std::fixed << std::setprecision (2) << elapsed_time_us / 1000000. << " sec\n";
#endif

	return 0;
}

struct archive*
FileArchive::setup_file_archive ()
{
	struct archive* a = setup_archive ();
	GStatBuf statbuf;
	if (!g_stat (_req.url, &statbuf)) {
		_req.mp.length = statbuf.st_size;
	} else {
		_req.mp.length = -1;
	}
	if (ARCHIVE_OK != archive_read_open_filename (a, _req.url, 8192)) {
		fprintf (stderr, "Error opening archive: %s\n", archive_error_string(a));
		return 0;
	}

	return a;
}
