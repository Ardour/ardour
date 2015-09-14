/*
 * Copyright (C) 2015 Tim Mayberry <mojofunk@gmail.com>
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

#ifndef PBD_WINDOWS_MMCSS_H
#define PBD_WINDOWS_MMCSS_H

#include <windows.h>

#include <string>

namespace PBD {

namespace MMCSS {

enum AVRT_PRIORITY {
	AVRT_PRIORITY_VERYLOW = -2,
	AVRT_PRIORITY_LOW,
	AVRT_PRIORITY_NORMAL,
	AVRT_PRIORITY_HIGH,
	AVRT_PRIORITY_CRITICAL
};

enum error_codes {
	ERROR_INVALID_TASK_NAME = 1550,
	ERROR_INVALID_TASK_INDEX = 1551
};

bool initialize ();

bool deinitialize ();

bool set_thread_characteristics (const std::string& task_name, HANDLE *task_handle);

bool revert_thread_characteristics (HANDLE task_handle);

bool set_thread_priority (HANDLE, AVRT_PRIORITY);

} // namespace MMCSS

} // namespace PBD

#endif // PBD_WINDOWS_MMCSS_H
