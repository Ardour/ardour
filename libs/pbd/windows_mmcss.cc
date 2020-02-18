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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "pbd/windows_mmcss.h"

#include "pbd/compose.h"
#include "pbd/debug.h"

#define DEBUG_THREADS(msg) DEBUG_TRACE (PBD::DEBUG::Threads, msg);

typedef HANDLE (WINAPI* AvSetMmThreadCharacteristicsA_t)(LPCSTR TaskName,
                                                         LPDWORD TaskIndex);

typedef BOOL (WINAPI* AvRevertMmThreadCharacteristics_t)(HANDLE AvrtHandle);

typedef BOOL (WINAPI* AvSetMmThreadPriority_t)(
    HANDLE AvrtHandle, PBD::MMCSS::AVRT_PRIORITY Priority);

static HMODULE avrt_dll = NULL;

static AvSetMmThreadCharacteristicsA_t AvSetMmThreadCharacteristicsA = NULL;
static AvRevertMmThreadCharacteristics_t AvRevertMmThreadCharacteristics = NULL;
static AvSetMmThreadPriority_t AvSetMmThreadPriority = NULL;

namespace PBD {

namespace MMCSS {

bool
initialize ()
{
	if (avrt_dll != NULL) return true;

	avrt_dll = LoadLibraryA ("avrt.dll");

	if (avrt_dll == NULL) {
		DEBUG_THREADS ("Unable to load avrt.dll\n");
		return false;
	}
	bool unload_dll = false;

	AvSetMmThreadCharacteristicsA =
	    (AvSetMmThreadCharacteristicsA_t)GetProcAddress (
	        avrt_dll, "AvSetMmThreadCharacteristicsA");

	if (AvSetMmThreadCharacteristicsA == NULL) {
		DEBUG_THREADS ("Unable to resolve AvSetMmThreadCharacteristicsA\n");
		unload_dll = true;
	}

	AvRevertMmThreadCharacteristics =
	    (AvRevertMmThreadCharacteristics_t)GetProcAddress (
	        avrt_dll, "AvRevertMmThreadCharacteristics");

	if (AvRevertMmThreadCharacteristics == NULL) {
		DEBUG_THREADS ("Unable to resolve AvRevertMmThreadCharacteristics\n");
		unload_dll = true;
	}

	AvSetMmThreadPriority = (AvSetMmThreadPriority_t)GetProcAddress (
	    avrt_dll, "AvSetMmThreadPriority");

	if (AvSetMmThreadPriority == NULL) {
		DEBUG_THREADS ("Unable to resolve AvSetMmThreadPriority\n");
		unload_dll = true;
	}

	if (unload_dll) {
		DEBUG_THREADS (
		    "MMCSS Unable to resolve necessary symbols, unloading avrt.dll\n");
		deinitialize ();
	}

	return true;
}

bool
deinitialize ()
{
	if (avrt_dll == NULL) return true;

	if (FreeLibrary (avrt_dll) == 0) {
		DEBUG_THREADS ("Unable to unload avrt.dll\n");
		return false;
	}

	avrt_dll = NULL;

	AvSetMmThreadCharacteristicsA = NULL;
	AvRevertMmThreadCharacteristics = NULL;
	AvSetMmThreadPriority = NULL;

	return true;
}

bool
set_thread_characteristics (const std::string& task_name, HANDLE* task_handle)
{
	if (AvSetMmThreadCharacteristicsA == NULL) return false;

	DWORD task_index_dummy = 0;

	*task_handle = AvSetMmThreadCharacteristicsA(task_name.c_str(), &task_index_dummy);

	if (*task_handle == 0) {
		DEBUG_THREADS (string_compose ("Failed to set Thread Characteristics to %1\n",
		                               task_name));
		DWORD error = GetLastError();

		switch (error) {
		case ERROR_INVALID_TASK_INDEX:
			DEBUG_THREADS("MMCSS: Invalid Task Index\n");
			break;
		case ERROR_INVALID_TASK_NAME:
			DEBUG_THREADS("MMCSS: Invalid Task Name\n");
			break;
		case ERROR_PRIVILEGE_NOT_HELD:
			DEBUG_THREADS("MMCSS: Privilege not held\n");
			break;
		default:
			DEBUG_THREADS("MMCSS: Unknown error setting thread characteristics\n");
			break;
		}
		return false;
	}

	DEBUG_THREADS (
	    string_compose ("Set thread characteristics to %1\n", task_name));

	return true;
}

bool
revert_thread_characteristics (HANDLE task_handle)
{
	if (AvRevertMmThreadCharacteristics == NULL) return false;

	if (AvRevertMmThreadCharacteristics (task_handle) == 0) {
		DEBUG_THREADS ("Failed to set revert thread characteristics\n");
		return false;
	}

	DEBUG_THREADS ("Reverted thread characteristics\n");

	return true;
}

bool
set_thread_priority (HANDLE task_handle, AVRT_PRIORITY priority)
{
	if (AvSetMmThreadPriority == NULL) return false;

	if (AvSetMmThreadPriority (task_handle, priority) == 0) {
		DEBUG_THREADS (
		    string_compose ("Failed to set thread priority %1\n", priority));
		return false;
	}

	DEBUG_THREADS (string_compose ("Set thread priority to %1\n", priority));

	return true;
}

} // namespace MMCSS

} // namespace PBD
