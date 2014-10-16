/*
    Copyright (C) 2009 John Emmas

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifdef COMPILER_MSVC

#include <WTypes.h>

extern "C" WINBASEAPI BOOL WINAPI
CreateHardLinkA( LPCSTR lpFileName,
				 LPCSTR lpExistingFileName,
				 LPSECURITY_ATTRIBUTES lpSecurityAttributes ); // Needs kernel32.lib on anything higher than Win2K

#include <algorithm>
#include <string>
#include <io.h>
#include <math.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <pbd/error.h>
#include <ardourext/misc.h>
#include <ardourext/pthread.h> // Should ensure that we include the right
                               // version - but we'll check anyway, later

#include <glibmm.h>

#define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64

struct timezone
{
	int  tz_minuteswest; /* minutes W of Greenwich */
	int  tz_dsttime;     /* type of dst correction */
};

LIBPBD_API int PBD_APICALLTYPE
gettimeofday(struct timeval *__restrict tv, __timezone_ptr_t tz) // Does this need to be exported ?
{
FILETIME ft;
unsigned __int64 tmpres = 0;
static int tzflag = 0;

	if (NULL != tv)
	{
		GetSystemTimeAsFileTime(&ft);

		tmpres |= ft.dwHighDateTime;
		tmpres <<= 32;
		tmpres |= ft.dwLowDateTime;

		/*converting file time to unix epoch*/
		tmpres /= 10;  /*convert into microseconds*/
		tmpres -= DELTA_EPOCH_IN_MICROSECS;
		tv->tv_sec = (long)(tmpres / 1000000UL);
		tv->tv_usec = (long)(tmpres % 1000000UL);
	}

	if (NULL != tz)
	{
		struct timezone *ptz = static_cast<struct timezone*> (tz);
		if (!tzflag)
		{
			_tzset();
			tzflag++;
		}
		if (ptz)
		{
			ptz->tz_minuteswest = _timezone / 60;
			ptz->tz_dsttime = _daylight;
		}
	}

	return 0;
}

// Define the default comparison operators for Windows (ptw32) 'pthread_t' (not used
// by Ardour AFAIK but would be needed if an array of 'pthread_t' had to be sorted).
#ifndef PTHREAD_H   // Defined by PTW32 (Linux and other versions define _PTHREAD_H)
#error "An incompatible version of 'pthread.h' is #included. Use only the Windows (ptw32) version!"
#else
LIBPBD_API bool operator>  (const pthread_t& lhs, const pthread_t& rhs)
{
	return (std::greater<void*>()(lhs.p, rhs.p));
}

LIBPBD_API bool operator<  (const pthread_t& lhs, const pthread_t& rhs)
{
	return (std::less<void*>()(lhs.p, rhs.p));
}

LIBPBD_API bool operator!= (const pthread_t& lhs, const pthread_t& rhs)
{
	return (std::not_equal_to<void*>()(lhs.p, rhs.p));
}

LIBPBD_API bool operator== (const pthread_t& lhs, const pthread_t& rhs)
{
	return (!(lhs != rhs));
}
#endif

// Functions supplied (later) to std::transform
//***************************************************************
//
//	invert_backslash()
//
// Examines a supplied ASCII character and (if the character is
// a backslash) converts it to a forward slash,
//
//	Returns:
//
//    The supplied character (converted, if it was a backslash)
//
char invert_backslash(char character)
{
	if ('\\' == character)
		character = '/';

	return (character);
}

//***************************************************************
//
//	invert_forwardslash()
//
// Examines a supplied ASCII character and (if the character is
// a forward slash) converts it to a backslash,
//
//	Returns:
//
//    The supplied character (converted, if it was a fwd slash)
//
char invert_forwardslash(char character)
{
	if ('/' == character)
		character = '\\';

	return (character);
}


//***************************************************************
//
//	pread()
//
// Emulates pread() using _lseek()/_read()/_lseek().
//
//	Returns:
//
//    On Success: The number of bytes read from the file
//    On Failure: -1
//
LIBPBD_API ssize_t PBD_APICALLTYPE
pread(int handle, void *buf, size_t nbytes, off_t offset)
{
int old_errno;
ssize_t ret;

	off_t old_offset = _tell(handle);

	if (0 > old_offset)
		ret = (-1);
	else
	{
		_lseek(handle, offset, SEEK_SET);
		ret = _read(handle, buf, nbytes);

		old_errno = errno;
		_lseek(handle, old_offset, SEEK_SET);
		errno = old_errno;
	}

	return (ret);
}


//***************************************************************
//
//	pwrite()
//
// Emulates pwrite() using _lseek()/_write()/_lseek().
//
//	Returns:
//
//    On Success: The number of bytes written to the file
//    On Failure: -1
//
LIBPBD_API ssize_t PBD_APICALLTYPE
pwrite(int handle, const void *buf, size_t nbytes, off_t offset)
{
int old_errno;
ssize_t ret;

	off_t old_offset = _lseek(handle, offset, SEEK_SET);

	if (0 > old_offset)
		ret = (-1);
	else
	{
		ret = _write(handle, buf, nbytes);

		old_errno = errno;
		_lseek(handle, old_offset, SEEK_SET);
		errno = old_errno;
	}

	return (ret);
}

//***************************************************************
//
//	round()
//
// Emulates round() using floor().
//
//	Returns:
//
//    On Success: The largest integer that is less than or
//                equal to 'x'.
//    On Failure: None
//
LIBPBD_API double PBD_APICALLTYPE
round(double x)
{
	return (floor(x));
}

namespace PBD {

//***************************************************************
//
//	TestForMinimumSpecOS()
//
// Tests the user's OS to see if it is Win2K or later (could be
// expanded quite easily to accommodate other OS's)
//
//	Returns:
//
//    On Success: TRUE (if the user's OS matches the minimum spec)
//    On Failure: FALSE otherwise
//
LIBPBD_API bool PBD_APICALLTYPE
TestForMinimumSpecOS(char *revision /* currently ignored */)
{
bool bRet = true;
#ifdef PLATFORM_WINDOWS
	bRet = false;
	HINSTANCE hKernelDll = (HINSTANCE)dlopen("kernel32.dll", RTLD_NOW);

	if (hKernelDll)
	{
		// 'CreateHardLink()' is only available from Win2K onwards.
		if (NULL != dlsym(hKernelDll, "CreateHardLinkA"))
			bRet = true;

		dlclose(hKernelDll);
	}
#endif
	// Other OS's could be accommodated here

	return (bRet);
}


//***************************************************************
//
//	realpath()
//
// Emulates POSIX realpath() using Win32 _fullpath().
//
//	Returns:
//
//    On Success: A pointer to the resolved (absolute) path
//    On Failure: NULL
//
LIBPBD_API char* PBD_APICALLTYPE
realpath (const char *original_path, char resolved_path[_MAX_PATH+1])
{
char *pRet = NULL;
bool bIsSymLink = 0; // We'll probably need to test the incoming path
                     // to find out if it points to a Windows shortcut
                     // (or a hard link) and set this appropriately.
	if (bIsSymLink)
	{
		// At the moment I'm not sure if Windows '_fullpath()' is directly
		// equivalent to POSIX 'realpath()' - in as much as the latter will
		// resolve the supplied path if it happens to point to a symbolic
		// link ('_fullpath()' probably DOESN'T do this but I'm not really
		// sure if Ardour needs such functionality anyway). Therefore we'll
		// possibly need to add that functionality here at a later date.
	}
	else
	{
		char temp[(MAX_PATH+1)*6]; // Allow for maximum length of a path in UTF8 characters

		// POSIX 'realpath()' requires that the buffer size is at
		// least PATH_MAX+1, so assume that the user knew this !!
		pRet = _fullpath(temp, Glib::locale_from_utf8(original_path).c_str(), _MAX_PATH);
		if (NULL != pRet)
			strcpy(resolved_path, Glib::locale_to_utf8(temp).c_str());
	}

	return (pRet);
}


//***************************************************************
//
//	opendir()
//
// Creates a pointer to a DIR structure, appropriately filled in
// and ready to begin a directory search iteration.
//
//	Returns:
//
//    On Success: Pointer to a (heap based) DIR structure
//    On Failure: NULL
//
LIBPBD_API DIR* PBD_APICALLTYPE
opendir (const char *szPath)
{
wchar_t wpath[PATH_MAX+1];
unsigned int rc;
DIR *pDir = 0;

	errno = 0;

	if (!szPath)
		errno = EFAULT;

	if ((!errno) && ('\0' == szPath[0]))
		errno = ENOTDIR;

	// Determine if the given path really is a directory

	if (!errno)
		if (0 == MultiByteToWideChar (CP_UTF8, 0, (LPCSTR)szPath, -1, (LPWSTR)wpath, sizeof(wpath)))
			errno = EFAULT;

	if ((!errno) && ((rc = GetFileAttributesW(wpath)) == -1))
		errno = ENOENT;

	if ((!errno) && (!(rc & FILE_ATTRIBUTE_DIRECTORY)))
		// Error. Entry exists but not a directory. */
		errno = ENOTDIR;

	if (!errno)
	{
		// Allocate enough memory to store DIR structure, plus
		// the complete directory path originally supplied.
		pDir = (DIR *)malloc(sizeof(DIR) + strlen(szPath) + strlen("\\") + strlen ("*"));

		if (!pDir)
		{
			// Error - out of memory
			errno = ENOMEM;
		}
	}

	if (!errno)
	{
		// Create the search expression
		strcpy(pDir->dd_name, szPath);

		// Add a backslash if the path doesn't already end with one
		if (pDir->dd_name[0] != '\0' &&
			pDir->dd_name[strlen(pDir->dd_name) - 1] != '/' &&
			pDir->dd_name[strlen(pDir->dd_name) - 1] != '\\')
		{
			strcat (pDir->dd_name, "\\");
		}

		// Add the search pattern
		strcat(pDir->dd_name, "*");

		// Initialize handle to -1 so that a premature closedir()
		// doesn't try to call _findclose() on it.
		pDir->dd_handle = (-1);

		// Initialize the status
		pDir->dd_stat = 0;

		// Initialize the dirent structure. 'ino' and 'reclen' are invalid under Win32
		// and 'name' simply points at the appropriate part of the findfirst_t struct.
		pDir->dd_dir.d_ino = 0;
		pDir->dd_dir.d_reclen = 0;
		pDir->dd_dir.d_namlen = 0;
		strcpy(pDir->dd_dir.d_name, pDir->dd_dta.name);

		return (pDir);  // Succeeded
	}

	if (pDir)
		free (pDir);
	return (DIR *) 0; // Failed
}


//***************************************************************
//
//	readdir()
//
// Return a pointer to a dirent struct, filled with information
// about the next entry in the directory.
//
//	Returns:
//
//    On Success: A pointer to the supplied DIR's 'dirent' struct
//    On Failure: NULL
//
LIBPBD_API struct dirent* PBD_APICALLTYPE
readdir (DIR* pDir)
{
int old_errno = 0;
errno = 0;

	// Check for valid DIR struct
	if (!pDir)
		errno = EFAULT;

	if ((strcmp(pDir->dd_dir.d_name, pDir->dd_dta.name)) && (!errno))
		// The structure does not seem to be set up correctly
		errno = EINVAL;
	else
	{
		if (pDir->dd_stat < 0)
		{
			// We have already returned all files in this directory
			// (or the structure has an invalid dd_stat).
			return (struct dirent *)0;
		}
		else if (pDir->dd_stat == 0)
		{
			// We haven't started the search yet.
			// Start the search
			pDir->dd_handle = _findfirst (Glib::locale_from_utf8(pDir->dd_name).c_str(), &(pDir->dd_dta));

			if (pDir->dd_handle == -1)
				// The directory is empty
				pDir->dd_stat = -1;
			else
				pDir->dd_stat = 1;
		}
		else
		{
			// Do not return ENOENT on last file in directory
			old_errno = errno;

			// Get the next search entry
			if (_findnext (pDir->dd_handle, &(pDir->dd_dta)))
			{
				// We are off the end or otherwise error
				errno = old_errno;
				_findclose (pDir->dd_handle);
				pDir->dd_handle = -1;
				pDir->dd_stat = -1;
			}
			else
				// Update to indicate the correct status number
				pDir->dd_stat++;
		}

		if (pDir->dd_stat > 0)
		{
			// We successfully got an entry. Details about the file are
			// already appropriately filled in except for the length of
			// file name.
			strcpy(pDir->dd_dir.d_name, pDir->dd_dta.name);
			pDir->dd_dir.d_namlen = strlen (pDir->dd_dir.d_name);
			return (&pDir->dd_dir); // Succeeded
		}
	}

	return (struct dirent *) 0; // Failed
}


//***************************************************************
//
//	closedir()
//
// Frees the resources allocated by opendir().
//
//	Returns:
//
//    On Success: 0
//    On Failure: -1
//
LIBPBD_API int PBD_APICALLTYPE
closedir (DIR *pDir)
{
int rc = 0;

	errno = 0;

	if (!pDir)
		errno = EFAULT;
	else
	{
		if ((-1) != pDir->dd_handle)
			rc = _findclose (pDir->dd_handle);

		// Free the DIR structure
		free (pDir);

		return rc; // Succeeded
	}

	return (-1); // Failed
}

//***************************************************************
//
//	mkstemp()
//
// Emulates Linux mkstemp() using Win32 _mktemp() and _open() etc.
//
//	Returns:
//
//    On Success: A file descriptor for the opened file.
//    On Failure: (-1)
//
LIBPBD_API int PBD_APICALLTYPE
mkstemp (char *template_name)
{
int ret = (-1);
char *szFileName;
char szTempPath[PATH_MAX+100]; // Just ensure we have plenty of buffer space

	if (NULL != (szFileName = _mktemp(template_name)))
	{
		if (0 != ::GetTempPathA(sizeof(szTempPath), szTempPath))
		{
			strcat(szTempPath, szFileName);
			ret = _open(szTempPath, (_O_CREAT|_O_BINARY|_O_TEMPORARY|_O_RDWR|_O_TRUNC), _S_IWRITE);
		}
	}

	return (ret);
}


//***************************************************************
//
//	ntfs_link()
//
// Emulates Linux link() using Win32 CreateHardLink()(NTFS only).
//
//	Returns:
//
//    On Success: Non-zero.
//    On Failure: Zero (call 'GetLastError()' to retrieve info)
//
LIBPBD_API int PBD_APICALLTYPE
ntfs_link (const char *existing_filepath, const char *link_filepath)
{
int ret = 1; // 'ERROR_INVALID_FUNCTION'
bool bValidPath = false;

	// Make sure we've been sent a valid input string
	if (existing_filepath && link_filepath)
	{
		std::string strRoot = existing_filepath;

		if ((1 < strRoot.length()) && ('\\' == existing_filepath[0]) && ('\\' == existing_filepath[1]))
		{
			int slashcnt = 0;

			// We've been sent a network path. Convert backslashes to forward slashes temporarily.
			std::transform(strRoot.begin(), strRoot.end(), strRoot.begin(), invert_backslash);

			// Now, if there are less than four slashes, add a fourth one or abort
			std::string::iterator iter = strRoot.begin();
			while ((slashcnt < 4) && (iter != strRoot.end()))
			{
				if ('/' == (*iter))
					slashcnt++;

				++iter;
			}

			if (slashcnt > 2)
			{
				// If only 3 slashes were counted, add a trailing slash
				if (slashcnt == 3)
					strRoot += '/';

				// Now find the position of the fourth slash
				iter = strRoot.begin();
				int charcnt = 0;
				for (slashcnt=0; slashcnt<4;)
				{
					charcnt++;

					if ('/' == (*iter))
						slashcnt++;

					if (++iter == strRoot.end())
						break;
				}

				strRoot.resize(charcnt);
				bValidPath = true;
			}
		}
		else
		{
			// Assume a standard Windows style path
			if (1 < strRoot.length() && (':' == existing_filepath[1]))
			{
				// Convert backslashes to forward slashes temporarily.
				std::transform(strRoot.begin(), strRoot.end(), strRoot.begin(), invert_backslash);

				if (2 == strRoot.length())
					strRoot += '/';

				if ('/' == strRoot[2])
				{
					strRoot.resize(3);
					bValidPath = true;
				}
			}
		}

		if (bValidPath)
		{
			char szFileSystemType[_MAX_PATH+1];

			// Restore the original backslashes
			std::transform(strRoot.begin(), strRoot.end(), strRoot.begin(), invert_forwardslash);

			// Windows only supports hard links for the NTFS filing
			// system, so let's make sure that's what we're using!!
			if (::GetVolumeInformationA(strRoot.c_str(), NULL, 0, NULL, NULL, NULL, szFileSystemType, _MAX_PATH+1))
			{
				std::string strRootFileSystemType = szFileSystemType;
				std::transform(strRootFileSystemType.begin(), strRootFileSystemType.end(), strRootFileSystemType.begin(), ::toupper);
#if (_WIN32_WINNT >= 0x0500)
				if (0 == strRootFileSystemType.compare("NTFS"))
				{
					if (TestForMinimumSpecOS()) // Hard links were only available from Win2K onwards
						if (0 == CreateHardLinkA(link_filepath, existing_filepath, NULL))
						{	// Note that the above API call cannot create a link to a directory, so
							// should we also be checking that the supplied path was actually a file?
							ret = GetLastError();
						}
						else
							SetLastError(ret = 0); // 'NO_ERROR'
				}
				else
				{
					ret = 4300; // 'ERROR_INVALID_MEDIA'
				}
#endif
			}
		}
		else
			ret = 123; // 'ERROR_INVALID_NAME'
	}
	else
		ret = 161; // 'ERROR_BAD_PATHNAME'

	if (ret)
	{
		SetLastError(ret);
		return (-1);
	}
	else
		return (0);
}


//***************************************************************
//
//	ntfs_unlink()
//
// Emulates Linux unlink() using Win32 DeleteFile()(NTFS only).
//
//	Returns:
//
//    On Success: Non-zero.
//    On Failure: Zero (call 'GetLastError()' to retrieve info)
//
LIBPBD_API int PBD_APICALLTYPE
ntfs_unlink (const char *link_filepath)
{
int ret = 1; // 'ERROR_INVALID_FUNCTION'
bool bValidPath = false;

	// Make sure we've been sent a valid input string
	if (link_filepath)
	{
		std::string strRoot = link_filepath;

		if ((1 < strRoot.length()) && ('\\' == link_filepath[0]) && ('\\' == link_filepath[1]))
		{
			int slashcnt = 0;

			// We've been sent a network path. Convert backslashes to forward slashes temporarily.
			std::transform(strRoot.begin(), strRoot.end(), strRoot.begin(), invert_backslash);

			// Now, if there are less than four slashes, add a fourth one or abort
			std::string::iterator iter = strRoot.begin();
			while ((slashcnt < 4) && (iter != strRoot.end()))
			{
				if ('/' == (*iter))
					slashcnt++;

				++iter;
			}

			if (slashcnt > 2)
			{
				// If only 3 slashes were counted, add a trailing slash
				if (slashcnt == 3)
					strRoot += '/';

				// Now find the position of the fourth slash
				iter = strRoot.begin();
				int charcnt = 0;
				for (slashcnt=0; slashcnt<4;)
				{
					charcnt++;

					if ('/' == (*iter))
						slashcnt++;

					if (++iter == strRoot.end())
						break;
				}

				strRoot.resize(charcnt);
				bValidPath = true;
			}
		}
		else
		{
			// Assume a standard Windows style path
			if (1 < strRoot.length() && (':' == link_filepath[1]))
			{
				// Convert backslashes to forward slashes temporarily.
				std::transform(strRoot.begin(), strRoot.end(), strRoot.begin(), invert_backslash);

				if (2 == strRoot.length())
					strRoot += '/';

				if ('/' == strRoot[2])
				{
					strRoot.resize(3);
					bValidPath = true;
				}
			}
		}

		if (bValidPath)
		{
			char szFileSystemType[_MAX_PATH+1];

			// Restore the original backslashes
			std::transform(strRoot.begin(), strRoot.end(), strRoot.begin(), invert_forwardslash);

			// Windows only supports hard links for the NTFS filing
			// system, so let's make sure that's what we're using!!
			if (::GetVolumeInformationA(strRoot.c_str(), NULL, 0, NULL, NULL, NULL, szFileSystemType, _MAX_PATH+1))
			{
				std::string strRootFileSystemType = szFileSystemType;
				std::transform(strRootFileSystemType.begin(), strRootFileSystemType.end(), strRootFileSystemType.begin(), ::toupper);
#if (_WIN32_WINNT >= 0x0500)
				if (0 == strRootFileSystemType.compare("NTFS"))
					if (TestForMinimumSpecOS()) // Hard links were only available from Win2K onwards
						if (0 == DeleteFileA(link_filepath))
							ret = GetLastError();
						else
							ret = 0; // 'NO_ERROR'
#endif
			}
		}
		else
			ret = 123; // 'ERROR_INVALID_NAME'
	}
	else
		ret = 161; // 'ERROR_BAD_PATHNAME'

	if (ret)
	{
		SetLastError(ret);
		return (-1);
	}
	else
		return (0);
}

}  // namespace PBD


//***************************************************************
//
//	dlopen()
//
// Emulates POSIX dlopen() using Win32 LoadLibrary().
//
//	Returns:
//
//    On Success: A handle to the opened DLL
//    On Failure: NULL
//
LIBPBD_API void* PBD_APICALLTYPE
dlopen (const char *file_name, int mode)
{
	// Note that 'mode' is ignored in Win32
	return(::LoadLibraryA(Glib::locale_from_utf8(file_name).c_str()));
}


//***************************************************************
//
//	dlclose()
//
// Emulates POSIX dlclose() using Win32 FreeLibrary().
//
//	Returns:
//
//    On Success: A non-zero number
//    On Failure: 0
//
LIBPBD_API int PBD_APICALLTYPE
dlclose (void *handle)
{
	return (::FreeLibrary((HMODULE)handle));
}


//***************************************************************
//
//	dlsym()
//
// Emulates POSIX dlsym() using Win32 GetProcAddress().
//
//	Returns:
//
//    On Success: A pointer to the found function or symbol
//    On Failure: NULL
//
LIBPBD_API void* PBD_APICALLTYPE
dlsym (void *handle, const char *symbol_name)
{
	// First test for RTLD_DEFAULT and RTLD_NEXT
	if ((handle == 0/*RTLD_DEFAULT*/) || (handle == ((void *) -1L)/*RTLD_NEXT*/))
	{
		return 0; // Not yet supported for Win32
	}
	else
		return (::GetProcAddress((HMODULE)handle, symbol_name));
}

#define LOCAL_ERROR_BUF_SIZE 1024
static char szLastWinError[LOCAL_ERROR_BUF_SIZE];
//***************************************************************
//
//	dlerror()
//
// Emulates POSIX dlerror() using Win32 GetLastError().
//
//	Returns:
//
//    On Success: The translated message corresponding to the
//                last error
//    On Failure: NULL (if the last error was ERROR_SUCCESS)
//
LIBPBD_API char* PBD_APICALLTYPE
dlerror ()
{
	DWORD dwLastErrorId = GetLastError();
	if (ERROR_SUCCESS == dwLastErrorId)
		return 0;
	else
	{
		if (0 == FormatMessage(
					FORMAT_MESSAGE_FROM_SYSTEM,
					NULL,
					dwLastErrorId,
					0,
					szLastWinError,
					LOCAL_ERROR_BUF_SIZE,
					0))
		{
			sprintf(szLastWinError, "Could not decipher the previous error message");
		}

		// POSIX dlerror() seems to reset the
		// error system, so emulate that here
		SetLastError(ERROR_SUCCESS);
	}

	return(szLastWinError);
}

#endif  // COMPILER_MSVC
