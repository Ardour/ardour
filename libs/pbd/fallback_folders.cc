/*
    Copyright (C) 2008 John Emmas

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

#include <pbd/fallback_folders.h>
#include <glib.h>
#include <glibmm.h>
#include <string.h>



#ifdef PLATFORM_WINDOWS // Would not be relevant for Cygwin!!
#include <shlobj.h>
#include <winreg.h>

//***************************************************************
//
//	get_win_special_folder()
//
//  Gets the full path name that corresponds of one of the Windows
//  special folders, such as "My Documents" and the like. The input
//  parameter must be one of the corresponding CSIDL values, such
//  as CSIDL_SYSTEM etc.
//  
//	Returns:
//
//    On Success: A pointer to a newly allocated string containing
//                the name of the special folder (must later be freed).
//    On Failure: NULL
//
gchar *
get_win_special_folder (int csidl)
{
wchar_t path[PATH_MAX+1];
HRESULT hr;
LPITEMIDLIST pidl = 0;
gchar *retval = 0;

	if (S_OK == (hr = SHGetSpecialFolderLocation (0, csidl, &pidl)))
	{
		if (SHGetPathFromIDListW (pidl, path))
			retval = g_utf16_to_utf8 ((const gunichar2*)path, -1, 0, 0, 0);
		CoTaskMemFree (pidl);
	}

	return retval;
}
#endif // PLATFORM_WINDOWS

namespace PBD {

static  gchar **fallback_folders = 0;

//***************************************************************
//
//	get_platform_fallback_folders()
//
//  Returns an array of folders to fall back to if the folders
//  weren't named at build time and subsequently couldn't be found
//  in the user's environment. This might not be needed any more
//  because the function 'fixup_bundle_environment()' (in the
//  gtk2_ardour branch) now explicitly sets up any environment
//  paths that the program will need at run time. However, having
//  the folders here might help us to simplify the above function
//  which would be useful (currently, there are different versions
//  of 'fixup_bundle_environment()' for each supported platform).
//  Twelve fallback folders are currently catered for, corresponding to:-
//
//      LOCALEDIR
//      GTK_DIR
//      CONFIG_DIR
//      ARDOUR_DIR
//      MODULE_DIR
//      DATA_DIR
//      ICONS_DIR
//      PIXMAPS_DIR
//      CONTROL_SURFACES_DIR
//      VAMP_DIR
//      LADSPA_PATH - note that there's only one entry in the path
//      VST_PATH - note that there may only be one entry in the path
//
//	Returns:
//
//    On Success: A pointer to an array containing the above dirs.
//    On Failure: NULL
//
#ifdef PLATFORM_WINDOWS // Would not be relevant for Cygwin!!

static gchar**
get_platform_fallback_folders ()
{
gchar **fallback_dir_vector = 0;
const   gchar  *pUsrHome    = 0; // Do not free !!

	if (!fallback_folders)
	{
		GArray *pFallbackDirs;
		gchar *pAppData   = 0;
		gchar *pMyAppData = 0;
		gchar *pExeRoot   = 0;
		gchar *pPersonal  = 0;

		pFallbackDirs = g_array_new (TRUE, TRUE, sizeof (char *));

		if (pFallbackDirs)
		{
			/* Get the path for the user's personal folder */
			gchar *pPersonalTemp = get_win_special_folder (CSIDL_PERSONAL);

			/* and the path for the user's personal application data */
			gchar *pMyAppDataTemp = get_win_special_folder (CSIDL_LOCAL_APPDATA);

			/* and the path for common application data ("Documents and Settings\All Users\Application Data") */
			gchar *pAppDataTemp = get_win_special_folder (CSIDL_COMMON_APPDATA);

			if (0 == pAppDataTemp)
				pAppData = g_build_filename("C:\\", "Documents and Settings", "All Users", "Application Data", PROGRAM_NAME, "local", 0);
			else
			{
				pAppData = g_build_filename(pAppDataTemp, PROGRAM_NAME, "local", 0);
				g_free (pAppDataTemp);
			}

			if (0 == pMyAppDataTemp)
			{
				pMyAppData = g_build_filename(g_get_home_dir(), "Application Data", "local", 0);
			}
			else
			{
				pMyAppData = g_build_filename(pMyAppDataTemp, 0);
				g_free (pMyAppDataTemp);
			}

			if (0 == pPersonalTemp)
				pPersonal = g_build_filename(g_get_home_dir(), 0);
			else
			{
				pPersonal = g_build_filename(pPersonalTemp, 0);
				g_free (pPersonalTemp);
			}

			/* Get the path to the running application */
			pExeRoot = g_win32_get_package_installation_directory_of_module (0);

			if (0 == pExeRoot)
			{
				pExeRoot = g_build_filename("C:\\", "Program Files", PROGRAM_NAME, 0);
			}

			if ((pExeRoot) && (pAppData) && (pMyAppData) && (pPersonal))
			{
				gchar  tmp[PATH_MAX+1];
				gchar* p;

				// Build our LOCALEDIR entry
				if (0 != (p = g_build_filename(pAppData, "share", "locale", 0)))
				{
					g_array_append_val (pFallbackDirs, p);

					// Build our GTK_DIR entry
					if (0 != (p = g_build_filename(pPersonal, ".gtk-2.0", 0)))
					{
						g_array_append_val (pFallbackDirs, p);

						// Build our CONFIG_DIR entry
						if (0 != (p = g_build_filename(pAppData, "etc", 0)))
						{
							g_array_append_val (pFallbackDirs, p);

							// Build our ARDOUR_DIR entry
							p = g_build_filename(pMyAppData, PROGRAM_NAME, 0);

							if (0 != p)
							{
								g_array_append_val (pFallbackDirs, p);

								// Build our MODULE_DIR entry
								strcpy(tmp, pExeRoot);
								if (0 != (p = strrchr (tmp, G_DIR_SEPARATOR)))
								{
									*p = '\0';

									if (0 != (p = g_build_filename(tmp, 0)))
									{
										g_array_append_val (pFallbackDirs, p);

										// Build our DATA_DIR entry
										if (0 != (p = g_build_filename(pAppData, "share", 0)))
										{
											g_array_append_val (pFallbackDirs, p);

											// Build our ICONS_DIR entry
											if (0 != (p = g_build_filename(pAppData, "share", "icons", 0)))
											{
												g_array_append_val (pFallbackDirs, p);

												// Build our PIXMAPS_DIR entry
												if (0 != (p = g_build_filename(pAppData, "share", "pixmaps", 0)))
												{
													g_array_append_val (pFallbackDirs, p);

													// Build our CONTROL_SURFACES_DIR entry
													if (0 != (p = g_build_filename(pExeRoot, "bin", "surfaces", 0)))
													{
														g_array_append_val (pFallbackDirs, p);

														// Build our VAMP_DIR entry
														p = g_build_filename(pExeRoot, "bin", "vamp", 0);
														if (p)
															g_array_append_val (pFallbackDirs, p);
														else
															g_array_append_val (pFallbackDirs, "");

														// Next, build our LADSPA_PATH entry
														p = g_build_filename(pExeRoot, "bin", "plugins", 0);
														if (p)
															g_array_append_val (pFallbackDirs, p);
														else
															g_array_append_val (pFallbackDirs, "");

														// And finally, build our VST_PATH entry
														DWORD dwType = REG_SZ;  HKEY hKey;
														DWORD dwSize = PATH_MAX;  p = 0;
														if (ERROR_SUCCESS == RegOpenKeyExA (HKEY_CURRENT_USER, "Software\\VST", 0, KEY_READ, &hKey))
														{
															// Look for the user's VST Registry entry
															if (ERROR_SUCCESS == RegQueryValueExA (hKey, "VSTPluginsPath", 0, &dwType, (LPBYTE)tmp, &dwSize))
																p = g_build_filename (Glib::locale_to_utf8(tmp).c_str(), 0);

															RegCloseKey (hKey);
														}

														if (p == 0)
															if (ERROR_SUCCESS == RegOpenKeyExA (HKEY_LOCAL_MACHINE, "Software\\VST", 0, KEY_READ, &hKey))
															{
																// Look for a global VST Registry entry
																if (ERROR_SUCCESS == RegQueryValueExA (hKey, "VSTPluginsPath", 0, &dwType, (LPBYTE)tmp, &dwSize))
																	p = g_build_filename (Glib::locale_to_utf8(tmp).c_str(), 0);

																RegCloseKey (hKey);
															}

														if (p == 0)
														{
															gchar *pVSTx86 = 0;
															gchar *pProgFilesX86 = get_win_special_folder (CSIDL_PROGRAM_FILESX86);

															if (pProgFilesX86)
															{
																// Look for a VST folder under C:\Program Files (x86)
																if (pVSTx86 = g_build_filename (pProgFilesX86, "Steinberg", "VSTPlugins", 0))
																{
																	if (Glib::file_test (pVSTx86, Glib::FILE_TEST_EXISTS))
																		if (Glib::file_test (pVSTx86, Glib::FILE_TEST_IS_DIR))
																			p = g_build_filename (pVSTx86, 0);

																	g_free (pVSTx86);
																}

																g_free (pProgFilesX86);
															}

															if (p == 0)
															{
																// Look for a VST folder under C:\Program Files
																gchar *pVST = 0;
																gchar *pProgFiles = get_win_special_folder (CSIDL_PROGRAM_FILES);

																if (pProgFiles)
																{
																	if (pVST = g_build_filename (pProgFiles, "Steinberg", "VSTPlugins", 0))
																	{
																		if (Glib::file_test (pVST, Glib::FILE_TEST_EXISTS))
																			if (Glib::file_test (pVST, Glib::FILE_TEST_IS_DIR))
																				p = g_build_filename (pVST, 0);

																		g_free (pVST);
																	}

																	g_free (pProgFiles);
																}
															}
														}

														if (p == 0)
														{
															// If all else failed, assume the plugins are under "My Documents"
															pUsrHome = g_get_user_special_dir (G_USER_DIRECTORY_DOCUMENTS);
															if (pUsrHome)
																p = g_build_filename (pUsrHome, "Plugins", "VST", 0);
															else
															{
																pUsrHome = g_build_filename(g_get_home_dir(), "My Documents", 0);
																if (pUsrHome)
																	p = g_build_filename (pUsrHome, "Plugins", "VST", 0);
															}
														}
														else
														{
															gchar* q = 0;

															// Concatenate the registry path with the user's personal path
															pUsrHome = g_get_user_special_dir (G_USER_DIRECTORY_DOCUMENTS);

															if (pUsrHome)
															{
																q = p;
																p = g_build_path (";", q, g_build_filename(pUsrHome, "Plugins", "VST", 0), 0);
															}
															else
															{
																pUsrHome = g_build_filename(g_get_home_dir(), "My Documents", 0);
																if (pUsrHome)
																{
																	q = p;
																	p = g_build_path (";", q, g_build_filename (pUsrHome, "Plugins", "VST", 0), 0);
																}
															}
														}

														if (p) //VST
															g_array_append_val (pFallbackDirs, p);
														else
															g_array_append_val (pFallbackDirs, "");

														// BUNDLED_LV2
														p = g_build_filename(pExeRoot, "bin", "lv2", 0);
														if (p)
															g_array_append_val (pFallbackDirs, p);
														else
															g_array_append_val (pFallbackDirs, "");
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			
				g_free (pAppData);
				g_free (pMyAppData);
				g_free (pExeRoot);
				g_free (pPersonal);
			}

			fallback_dir_vector = fallback_folders = (gchar **) g_array_free (pFallbackDirs, FALSE);
		}
	}
	else
		fallback_dir_vector = fallback_folders;

	return (fallback_dir_vector);
}

#else
// Assume Linux, Cygwin or OS-X. Note that in all 3 cases we only
// need to cater for unbundled releases (those built by a user from
// source). Bundled releases of Ardour and Mixbus now specifically
// write their folders and paths to the user's environment at startup.
// See the function 'fixup_bundle_environment()'.

static gchar**
get_platform_fallback_folders ()
{
gchar **fallback_dir_vector = 0;
gchar  *pUsrHome            = 0;

	if (!fallback_folders)
	{
		GArray *pFallbackDirs;
		gchar *pAppData  = 0;
		gchar *pExeRoot  = 0;
		gchar *pPersonal = 0;

		pFallbackDirs = g_array_new (TRUE, TRUE, sizeof (char *));

		if (pFallbackDirs)
		{
			pAppData  = g_build_filename("/usr", "local", 0);
			pExeRoot  = g_build_filename("/usr", "local", "lib", "ardour2", 0);
			pPersonal = g_build_filename(g_get_home_dir(), 0);

			if ((pExeRoot) && (pAppData) && (pPersonal))
			{
				gchar  tmp[PATH_MAX+1];
				gchar* p;

				// Build our LOCALEDIR entry
				if (0 != (p = g_build_filename(pAppData, "share", "locale", 0)))
				{
					g_array_append_val (pFallbackDirs, p);

					// Build our GTK_DIR entry
					if (0 != (p = g_build_filename(pPersonal, ".gtk-2.0", 0)))
					{
						g_array_append_val (pFallbackDirs, p);

						// Build our CONFIG_DIR entry
						if (0 != (p = g_build_filename(pAppData, "etc", 0)))
						{
							g_array_append_val (pFallbackDirs, p);

							// Build our ARDOUR_DIR entry
							p = ""; // Empty string (temporary)
							if (0 != p)
							{
								g_array_append_val (pFallbackDirs, p);

								// Build our MODULE_DIR entry
								strcpy(tmp, pExeRoot);
								if (0 != (p = strrchr (tmp, G_DIR_SEPARATOR)))
								{
									*p = '\0';

									if (0 != (p = g_build_filename(tmp, 0)))
									{
										g_array_append_val (pFallbackDirs, p);

										// Build our DATA_DIR entry
										if (0 != (p = g_build_filename(pAppData, "share", 0)))
										{
											g_array_append_val (pFallbackDirs, p);

											// Build our ICONS_DIR entry (re-use 'tmp')
											strcpy(tmp, "/usr/local/share/ardour2");
											if (0 != (p = g_build_filename(tmp, "icons", 0)))
											{
												g_array_append_val (pFallbackDirs, p);

												// Build our PIXMAPS_DIR entry
												if (0 != (p = g_build_filename(tmp, "pixmaps", 0)))
												{
													g_array_append_val (pFallbackDirs, p);

													// Build our CONTROL_SURFACES_DIR entry
													if (0 != (p = g_build_filename(pExeRoot, "surfaces", 0)))
													{
														g_array_append_val (pFallbackDirs, p);

														// Build our VAMP_DIR entry
														p = g_build_filename(pExeRoot, "vamp", 0);
														if (p)
															g_array_append_val (pFallbackDirs, p);

														// Next, build our LADSPA_PATH entry
														p = g_build_filename(Glib::path_get_dirname(pExeRoot).c_str(), "plugins", 0);
														if (p)
															g_array_append_val (pFallbackDirs, p);

														// And finally, build our VST_PATH entry
														if (g_getenv("HOME"))
															p = g_build_filename(g_getenv("HOME"), "VST", "plugins", 0);
														else
															p = g_build_filename(g_get_home_dir(), "VST", "plugins", 0);

														if (p)
															g_array_append_val (pFallbackDirs, p);
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			
				g_free (pAppData);
				g_free (pExeRoot);
				g_free (pPersonal);
			}

			fallback_dir_vector = fallback_folders = (gchar **) g_array_free (pFallbackDirs, FALSE);
		}
	}
	else
		fallback_dir_vector = fallback_folders;

	if (pUsrHome)
		g_free (pUsrHome);

	return (fallback_dir_vector);
}
#endif


//***************************************************************
//
//	get_platform_fallback_folder()
//
//  Returns a const gchar* which points to a string describing
//  the full path to the Ardour fallback folder corresponding to
//  the supplied index. See 'get_platform_fallback_folders()' for a
//  complete list of the supported index enumerations. Calling this
//  function will initialize the fallback folder array if it wasn't
//  already initiaized. The array should then (eventually) be freed
//  using 'free_platform_fallback_folders()'.
//
//	Returns:
//
//    On Success: A pointer to the path string contained at the
//                relevant index.
//    On Failure: NULL
//
LIBPBD_API G_CONST_RETURN gchar* PBD_APICALLTYPE
get_platform_fallback_folder (PBD::fallback_folder_t index)
{
	if ((index >= 0) && (index < FALLBACK_FOLDER_MAX))
		return ((G_CONST_RETURN gchar *)get_platform_fallback_folders ()[index]);
	else
		return (G_CONST_RETURN gchar *) 0;
}


//***************************************************************
//
//	alloc_platform_fallback_folders()
//
//  Calls 'get_platform_fallback_folders()' to ensure that memory
//  for the fallback folder array is already allocated before the
//  array gets used. It doesn't cause any problems if the array gets
//  used prior to calling this function (since the memory will get
//  allocated anyway, on fist usage). Either way however, the momory
//  must later be freed using 'free_platform_fallback_folders()'.
//
//	Returns:
//
//    The value obtained from 'get_platform_fallback_folders()'
//
LIBPBD_API G_CONST_RETURN gchar* G_CONST_RETURN * PBD_APICALLTYPE
alloc_platform_fallback_folders ()
{
	return ((G_CONST_RETURN gchar* G_CONST_RETURN *)get_platform_fallback_folders ());
}


//***************************************************************
//
//	free_platform_fallback_folders()
//
//  Frees the memory that was previously allocated for the Ardour
//  fallback folder array.
//
//	Returns:
//
//    NONE.
//
LIBPBD_API void PBD_APICALLTYPE
free_platform_fallback_folders ()
{
int index = FOLDER_LOCALE;

	if (fallback_folders)
	{
		gchar *p = get_platform_fallback_folders()[(fallback_folder_t)index++];

		while (index < (FALLBACK_FOLDER_MAX+1)) {
			if (p)
				g_free (p);

			if (index < FALLBACK_FOLDER_MAX)
				p = get_platform_fallback_folders()[(fallback_folder_t)index++];
			else
				break;
		}

		fallback_folders = 0;
	}
}

}  // namespace PBD

