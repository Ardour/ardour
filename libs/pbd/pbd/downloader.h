/*
 * Copyright (C) 2022 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __libpbd_downloader_h__
#define __libpbd_downloader_h__

#include <atomic>
#include <string>

#include <curl/curl.h>

#include "pbd/libpbd_visibility.h"

namespace PBD {

class Thread;

class LIBPBD_API Downloader {
  public:
	Downloader (std::string const & url, std::string const & destdir);
	~Downloader ();

	int start ();
	void cleanup ();
	void cancel ();
	double progress() const;

	uint64_t download_size() const { return _download_size; }
	uint64_t downloaded () const { return _downloaded; }

	/* public so it can be called from a static C function */
	size_t write (void *contents, size_t size, size_t nmemb);

	int status() const { return _status; }
	std::string download_path() const;

  private:
	std::string url;
	std::string destdir;
	std::string file_path;
	FILE* file;
	CURL* curl;
	bool _cancel;
	std::atomic<uint64_t> _download_size; /* read-only from requestor thread */
	std::atomic<uint64_t> _downloaded; /* read-only from requestor thread */
	std::atomic<int> _status;
	PBD::Thread* thread;

	void download ();
};

} /* namespace */

#endif
