/*
 * Copyright (C) 2025 Robin Gareus <robin@gareus.org>
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

#pragma once

#include <curl/curl.h>

#include "pbd/libpbd_visibility.h"

namespace PBD {

class LIBPBD_API CCurl {
public:
	CCurl ();
	~CCurl ();

	void  reset ();
	CURL* curl () const;

	static void ca_setopt (CURL*);

	// called from PBD::init
	static void setup_certificate_paths ();

private:
	mutable CURL* _curl;

	static const char* ca_path;
	static const char* ca_info;
};

} // namespace PBD
