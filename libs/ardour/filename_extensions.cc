/*
 * Copyright (C) 2007 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2008-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#include "ardour/filename_extensions.h"

#include "pbd/i18n.h"

namespace ARDOUR {

const char* const template_suffix = X_(".template");
const char* const statefile_suffix = X_(".ardour");
const char* const pending_suffix = X_(".pending");
const char* const peakfile_suffix = X_(".peak");
const char* const backup_suffix = X_(".bak");
const char* const temp_suffix = X_(".tmp");
const char* const history_suffix = X_(".history");
const char* const export_preset_suffix = X_(".preset");
const char* const export_format_suffix = X_(".format");
const char* const session_archive_suffix = X_(".ardour-session-archive");
const char* const template_archive_suffix = X_(".ardour-template-archive");

}
