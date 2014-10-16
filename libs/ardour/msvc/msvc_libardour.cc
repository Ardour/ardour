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

#if (defined(PLATFORM_WINDOWS) && !defined(COMPILER_CYGWIN))
#include <shlobj.h>
#include <glibmm.h>
#ifdef COMPILER_MSVC
#pragma warning(disable:4996)
#endif
#else
#include <glib.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <ardour/msvc_libardour.h>

namespace ARDOUR {

//***************************************************************
//
// placeholder_for_non_msvc_specific_function()
//
// Description
//
//	Returns:
//
//    On Success:
//
//    On Failure:
//
/* LIBARDOUR_API char* LIBARDOUR_APICALLTYPE
   placeholder_for_non_msvc_specific_function()
{
char *pRet = buffer;

	return (pRet);
}
*/

}  // namespace ARDOUR

#ifdef COMPILER_MSVC

#include <errno.h>

namespace ARDOUR {

//***************************************************************
//
//	symlink()
//
// Emulates POSIX symlink() but creates a Windows shortcut. To
// create a Windows shortcut the supplied shortcut name must end
// in ".lnk"
// Note that you can only create a shortcut in a folder for which
// you have appropriate access rights. Note also that the folder
// must already exist. If it doesn't exist or if you don't have
// sufficient access rights to it, symlink() will generate an
// error (in common with its POSIX counterpart).
//
//	Returns:
//
//    On Success: Zero
//    On Failure: -1 ('errno' will contain the specific error)
//
LIBARDOUR_API int LIBARDOUR_APICALLTYPE
symlink(const char *dest, const char *shortcut, const char *working_directory /*= NULL */)
{
IShellLinkA  *pISL = NULL;    
IPersistFile *ppf  = NULL;
int           ret  = (-1);

	if ((NULL == dest) || (NULL == shortcut) || (strlen(shortcut) < 5) || (strlen(dest) == 0))
		_set_errno(EINVAL);
	else if ((strlen(shortcut) > _MAX_PATH) || (strlen(dest) > _MAX_PATH))
		_set_errno(ENAMETOOLONG);
	else if (Glib::file_test(shortcut, Glib::FILE_TEST_EXISTS))
		_set_errno(EEXIST);
	else
	{
		HRESULT hRet = 0;

		if (SUCCEEDED (hRet = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**)&pISL)))
		{
			if (SUCCEEDED (pISL->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf)))
			{
				char  sc_path_lower_case[_MAX_PATH];
				WCHAR shortcut_path[_MAX_PATH];

				// Fail if the path isn't a shortcut
				strcpy(sc_path_lower_case, shortcut);
				strlwr(sc_path_lower_case);
				const char *p = strlen(sc_path_lower_case) + sc_path_lower_case - 4;

				if (0 == strcmp(p, ".lnk"))
				{
					HRESULT hr;

					// We're apparently been given valid Windows shortcut name
					MultiByteToWideChar (CP_ACP, MB_PRECOMPOSED, shortcut, -1, shortcut_path, _MAX_PATH);

					// Create the shortcut
					if (FAILED (hr = ppf->Load(shortcut_path, STGM_CREATE|STGM_READWRITE|STGM_SHARE_EXCLUSIVE)))
						hr = ppf->Save(shortcut_path, TRUE);

					if (S_OK == hr)
					{
						// Set its target path
						if (S_OK == pISL->SetPath(dest))
						{
							// Set its working directory
							if (working_directory)
								p = working_directory;
							else
								p = "";

							if (S_OK == pISL->SetWorkingDirectory(p))
							{
								// Set its 'Show' command
								if (S_OK == pISL->SetShowCmd(SW_SHOWNORMAL))
								{
									// And finally, set its icon to the same file as the target.
									// For the time being, don't fail if the target has no icon.
					                if (Glib::file_test(dest, Glib::FILE_TEST_IS_DIR))
										pISL->SetIconLocation("%SystemRoot%\\system32\\shell32.dll", 1);
									else
										pISL->SetIconLocation(dest, 0);

									if (S_OK == ppf->Save(shortcut_path, FALSE))
									{
										Sleep(1500);

										ret = 0;
										// _set_errno(0);
									}
									else
										_set_errno(EACCES);
								}
								else
									_set_errno(EACCES);
							}
							else
								_set_errno(EACCES);
						}
						else
							_set_errno(EACCES);
					}
					else
						_set_errno(EBADF);
				}
				else
					_set_errno(EACCES);
			}
			else
				_set_errno(EBADF);
		}
		else
		{
			if (E_POINTER == hRet)
				_set_errno(EINVAL);
			else
				_set_errno(EIO);
		}
	}

	return (ret);
}


//***************************************************************
//
//	readlink()
//
// Emulates POSIX readlink() but using Windows shortcuts
// Doesn't (currently) resolve shortcuts to shortcuts. This would
// be quite simple to incorporate but we'd need to check for
// recursion (i.e. a shortcut that points to an earlier shortcut
// in the same chain).
//
//	Returns:
//
//    On Success: Zero
//    On Failure: -1 ('errno' will contain the specific error)
//
LIBARDOUR_API int LIBARDOUR_APICALLTYPE
readlink(const char *__restrict shortcut, char *__restrict buf, size_t bufsize)
{
IShellLinkA  *pISL = NULL;    
IPersistFile *ppf  = NULL;
int           ret  = (-1);

	if ((NULL == shortcut) || (NULL == buf) || (strlen(shortcut) < 5) || (bufsize == 0))
		_set_errno(EINVAL);
	else if ((bufsize > _MAX_PATH) || (strlen(shortcut) > _MAX_PATH))
		_set_errno(ENAMETOOLONG);
	else
	{
		HRESULT hRet = 0;

		if (SUCCEEDED (hRet = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**)&pISL)))
		{
			if (SUCCEEDED (pISL->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf)))
			{
				char  target_path[_MAX_PATH];
				WCHAR shortcut_path[_MAX_PATH];

				// Fail if the path isn't a shortcut
				strcpy(target_path, shortcut); // Use 'target_path' temporarily
				strlwr(target_path);
				const char *p = strlen(target_path) + target_path - 4;

				if (0 == strcmp(p, ".lnk"))
				{
					// We're apparently pointing to a valid Windows shortcut
					MultiByteToWideChar (CP_ACP, MB_PRECOMPOSED, shortcut, -1, shortcut_path, _MAX_PATH);

					// Load the shortcut into our persistent file
					if (SUCCEEDED (ppf->Load(shortcut_path, 0)))
					{
						// Read the target information from the shortcut object
						if (S_OK == (pISL->GetPath (target_path, _MAX_PATH, NULL, SLGP_UNCPRIORITY)))
						{
							strncpy(buf, target_path, bufsize); 
							ret = ((ret = strlen(buf)) > bufsize) ? bufsize : ret;
							// _set_errno(0);
						}
						else
							_set_errno(EACCES);
					}
					else
						_set_errno(EBADF);
				}
				else
					_set_errno(EINVAL);
			}
			else
				_set_errno(EBADF);
		}
		else
		{
			if (E_POINTER == hRet)
				_set_errno(EINVAL);
			else
				_set_errno(EIO);
		}

		if (ppf)
			ppf->Release();

		if (pISL)
			pISL->Release();
	}

	return (ret);
}

}  // namespace ARDOUR

#endif  // COMPILER_MSVC
