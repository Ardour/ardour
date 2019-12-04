/*
 * Copyright (C) 2014-2017 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2015-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2016 Robin Gareus <robin@gareus.org>
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

#include "bundle_env.h"
#include "pbd/i18n.h"

#include <shlobj.h>
#include <stdlib.h>

#include <iostream>
#include <string>
#include <vector>
#include <fstream>

#include <glibmm.h>
#include "pbd/gstdio_compat.h"

#include <fontconfig/fontconfig.h>

#include "ardour/ardour.h"
#include "ardour/search_paths.h"
#include "ardour/filesystem_paths.h"

#include "pbd/file_utils.h"
#include "pbd/epa.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;

std::string
get_windows_drive_volume_letter()
{
static std::string ret;
char path[PATH_MAX+1];
LPITEMIDLIST pidl = 0;

	if (!ret.length()) {
		if (S_OK == SHGetSpecialFolderLocation (0, CSIDL_WINDOWS, &pidl))
		{
			if (SHGetPathFromIDListA (pidl, path)) {
				path[2] = '\0'; // Gives us just the drive letter and colon
				ret = path;
			}

			CoTaskMemFree (pidl);
		}
		// The above should never fail - but just in case...
		else if (char *env_path = getenv ("windir"))
		{
			strcpy (path, env_path);
			path[2] = '\0'; // Gives us just the drive letter and colon
			ret = path;
		}
	}

	return ret;
}

const string
get_module_folder ()
{
std::string ret;

	// Gives the top-level Ardour installation folder (on Windows)
	// Typically, this will be somehwere like "C:\Program Files"

	gchar* pExeRoot = g_win32_get_package_installation_directory_of_module (0);

	if (0 == pExeRoot) {
		pExeRoot = g_build_filename("C:\\", "Program Files", PROGRAM_NAME, 0);
	}

	if (pExeRoot) {
		gchar  tmp[PATH_MAX+1];
		gchar* p;

		strcpy(tmp, pExeRoot);
		if (0 != (p = strrchr (tmp, G_DIR_SEPARATOR))) {
			*p = '\0';

			if (0 != (p = g_build_filename(tmp, 0))) {
				ret = p;
				g_free (p);
			}
		}

		g_free (pExeRoot);
	}

	return (ret);
}

bool
fixup_config_file (Glib::ustring str_file_to_fix)
{
FILE* fd;
char  buf[4096];
bool  conversion_needed = false;
bool  succeeded = false;

	fstream file_to_fix (fd = g_fopen(str_file_to_fix.c_str(), "r+b"));

	if (file_to_fix.is_open()) {
		vector<std::string> lines;
		std::string line;

		file_to_fix.seekg (0, std::ios::beg);
		file_to_fix.seekp (0, std::ios::beg);

		try {
			while (!file_to_fix.eof() && file_to_fix.getline (buf, sizeof(buf))) {
				line = buf;

				if (!conversion_needed && (std::string::npos != line.find("$(")))
					conversion_needed = true;
				lines.push_back(line);
			}

			if (conversion_needed) {
				bool error = false;
				std::string::size_type token_begin, token_end;
				vector<string>::iterator i;

				for (i = lines.begin(); i != lines.end(); ++i) {
					if (string::npos != (token_begin = i->find("$("))) {
						if (string::npos != (token_end = i->find(")", token_begin))) {
							std::string str_replace_with;
							std::string str_to_replace = i->substr(token_begin, ((token_end+1)-token_begin));

							if (0 == str_to_replace.compare("$(CWD)")) {
								// Replace our token with the current working directory
								if (getcwd(buf, sizeof(buf))) {
									if (buf[strlen(buf)-1] == G_DIR_SEPARATOR)
										buf[strlen(buf)-1] = '\0';
									str_replace_with = buf;

									// Replace the first occurrence of our token with the required string
									i->erase(token_begin, ((token_end+1)-token_begin));
									i->insert(token_begin, str_replace_with);
								} else {
									error = true;
								}
							} else if (0 == str_to_replace.compare("$(WINDRIVE)")){
								// Replace our token with the drive letter (and colon) for the user's Windows volume
								str_replace_with = get_windows_drive_volume_letter();

								// Replace the first occurrence of our token with the required string
								i->erase(token_begin, ((token_end+1)-token_begin));
								i->insert(token_begin, str_replace_with);
							} else if (0 == str_to_replace.compare("$(LOCALCACHEDIR)")){
								// Replace our token with the path to our Ardour cache directory
								str_replace_with = user_cache_directory();

								// Replace the first occurrence of our token with the required string
								i->erase(token_begin, ((token_end+1)-token_begin));
								i->insert(token_begin, str_replace_with);
							} else {
								// Assume that our token represents an environment variable
								std::string envvar_name = str_to_replace.substr(2, str_to_replace.length()-3);

								if (const char *envvar_value = getenv(envvar_name.c_str())) {
									strcpy(buf, envvar_value);
									if (buf[strlen(buf)-1] == G_DIR_SEPARATOR)
										buf[strlen(buf)-1] = '\0';
									str_replace_with = buf;

									// Replace the first occurrence of our token with the required string
									i->erase(token_begin, ((token_end+1)-token_begin));
									i->insert(token_begin, str_replace_with);
								} else {
									error = true;
									cerr << _("ERROR: unknown environment variable") << endl;
								}
							}
						}
					}
				}

				if (!error) {
					file_to_fix.clear ();                  // Clear the EOF flag etc
					file_to_fix.seekg (0, std::ios::beg);  // Seek our 'get' ptr to the file start pos
														   // (our 'put' ptr shouldn't have moved yet).
					chsize(fileno (fd), 0);                // Truncate the file, ready for re-writing

					for (i = lines.begin(); i != lines.end(); ++i) {

						// Write the converted contents to our file
						file_to_fix << (*i).c_str() << endl;
					}

					try {
						file_to_fix.close();
						succeeded = true;
					} catch (...) {}
				}
			} else {
				file_to_fix.close();
				succeeded = true;
			}
		} catch (...) {
			file_to_fix.close();
			succeeded = false;
		}
	} else {
		cerr << _("ERROR: Could not open config file '") << str_file_to_fix << "'" << endl;
	}

	return succeeded;
}

void
fixup_fonts_config ()
{
string fonts_conf_file;

#ifdef DEBUG
	fonts_conf_file = get_module_folder();

	if (!fonts_conf_file.empty()) {
		fonts_conf_file += "\\";
		fonts_conf_file += PROGRAM_NAME;
		fonts_conf_file += PROGRAM_VERSION;
		fonts_conf_file += FONTS_CONF_LOCATION;
#else
	if (PBD::find_file (ARDOUR::ardour_config_search_path(), "fonts.conf", fonts_conf_file)) {
#endif
		Glib::setenv ("FONTCONFIG_FILE", fonts_conf_file, true);

		if (0 == fixup_config_file (fonts_conf_file))
			cerr << _("ERROR: processing error for 'fonts.conf' file") << endl;
	} else {
		cerr << _("ERROR: Malformed module folder (fonts.conf)") << endl;
	}
}

void
fixup_pango_config ()
{
string pango_modules_file;

#if defined(DEBUG) || defined(RDC_BUILD)
	// Make sure we pick up the debuggable DLLs !!!
	pango_modules_file = get_module_folder();

	if (!pango_modules_file.empty()) {
		pango_modules_file += "\\";
		pango_modules_file += PROGRAM_NAME;
		pango_modules_file += PROGRAM_VERSION;
		pango_modules_file += PANGO_CONF_LOCATION;
#if 0
// JE - handy for non-English locale testing (Greek, in this case)
		Glib::ustring pango_modules_path = Glib::locale_to_utf8("C:\\Program Files\\Mixbus3\\etc\\ÄÇÌÇÔÑÇÓ\\pango.modules");
/**/
#else
		Glib::ustring pango_modules_path = pango_modules_file;
#endif
		pango_modules_path.resize (pango_modules_path.size()-14); // Remove "/pango.modules" from the end
#else
	if (PBD::find_file (ARDOUR::ardour_config_search_path(), "pango.modules", pango_modules_file)) {

		Glib::ustring pango_modules_path = pango_modules_file;
		pango_modules_path.resize (pango_modules_path.size()-14); // Remove "/pango.modules" from the end
#endif
		// Set an environment variable so we can find our pango modules. Note
		// that this requires a modified version of libpango (pango-utils.c)
		Glib::setenv ("PANGO_MODULE_PATH", Glib::filename_from_utf8(pango_modules_path), true);

		if (0 == fixup_config_file (pango_modules_file))
			cerr << _("ERROR: processing error for 'pango.modules' file") << endl;
	} else {
		cerr << _("ERROR: Malformed module folder (pango.modules)") << endl;
	}
}

void
fixup_pixbuf_loaders_config ()
{
string gdk_pixbuf_loaders_file;

#if defined(DEBUG) || defined(RDC_BUILD)
	// Make sure we pick up the debuggable DLLs !!!
	gdk_pixbuf_loaders_file = get_module_folder();

	if (!gdk_pixbuf_loaders_file.empty()) {
		gdk_pixbuf_loaders_file += "\\";
		gdk_pixbuf_loaders_file += PROGRAM_NAME;
		gdk_pixbuf_loaders_file += PROGRAM_VERSION;
		gdk_pixbuf_loaders_file += PIXBUFLOADERS_CONF_LOCATION;
#else
	if (PBD::find_file (ARDOUR::ardour_config_search_path(), "gdk-pixbuf.loaders", gdk_pixbuf_loaders_file)) {
#endif
		// Set an environment variable so we can find our pixbuf modules.
		Glib::setenv ("GDK_PIXBUF_MODULE_FILE", Glib::filename_from_utf8(gdk_pixbuf_loaders_file), true);

		if (0 == fixup_config_file (gdk_pixbuf_loaders_file))
			cerr << _("ERROR: processing error for 'gdk-pixbuf.loaders' file") << endl;
	} else {
		cerr << _("ERROR: Malformed module folder (gdk-pixbuf.loaders)") << endl;
	}
}

void
fixup_clearlooks_config ()
{
string clearlooks_la_file;

#if defined(DEBUG) || defined(RDC_BUILD)
	// Make sure we pick up the debuggable DLLs !!!
	clearlooks_la_file = get_module_folder();

	if (!clearlooks_la_file.empty()) {
		clearlooks_la_file += "\\";
		clearlooks_la_file += PROGRAM_NAME;
		clearlooks_la_file += PROGRAM_VERSION;
		clearlooks_la_file += CLEARLOOKS_CONF_LOCATION;
#else
	if (PBD::find_file (ARDOUR::ardour_config_search_path(), "libclearlooks.la", clearlooks_la_file)) {
#endif
		// Set an environment variable so we can find our clearlooks engine.
		// Note that this requires a modified version of libgtk (gtkthemes.c)
		Glib::setenv ("GTK_THEME_ENGINE_FILE", Glib::filename_from_utf8(clearlooks_la_file).c_str(), true);

		if (0 == fixup_config_file (clearlooks_la_file))
			cerr << _("ERROR: processing error for 'clearlooks.la' file") << endl;
	} else {
		cerr << _("ERROR: Malformed module folder (clearlooks.la)") << endl;
	}
}

void
fixup_bundle_environment (int argc, char* argv[], string & localedir)
{
	std::string exec_path = argv[0];
	std::string dir_path  = Glib::path_get_dirname (exec_path);

	// Make sure that our runtime CWD is set to Mixbus's install
	// folder, regardless of where the caller's CWD was set to.
	g_chdir (dir_path.c_str());

	EnvironmentalProtectionAgency::set_global_epa (new EnvironmentalProtectionAgency (true));

	// Now set 'dir_path' so we can append some relative paths
	dir_path = Glib::path_get_dirname (dir_path);

	std::string path;
	const  char *cstr;

	// First, set up 'ARDOUR_DLL_PATH'
	path  = dir_path;
	path += "\\lib\\ardour3\\surfaces;";
	path += dir_path;
	path += "\\lib\\ardour3\\panners;";
	path += dir_path;
	path += "\\lib\\ardour3\\backends;";
	path += dir_path;
	path += "\\bin";
	Glib::setenv ("ARDOUR_DLL_PATH", path, true);


	// Next, set up 'ARDOUR_DATA_PATH'
	path  = get_module_folder() + "\\";
	path += PROGRAM_NAME;
	path += PROGRAM_VERSION;
	path += "\\share";
	Glib::setenv ("ARDOUR_DATA_PATH", path, true);


	// Next, set up 'ARDOUR_CONFIG_PATH'
#ifdef _WIN64
	path = user_config_directory() + "\\win64;";
#else
	path = user_config_directory() + "\\win32;";
#endif
	Glib::setenv ("ARDOUR_CONFIG_PATH", path, true);


	// Next, set up 'ARDOUR_INSTANT_XML_PATH'
	path = user_config_directory();
	Glib::setenv ("ARDOUR_INSTANT_XML_PATH", path, true);


	// Next, set up 'LADSPA_PATH'
	path = ladspa_search_path().to_string();
	Glib::setenv ("LADSPA_PATH", path, true);


	// Next, set up 'SUIL_MODULE_DIR'
	Glib::setenv ("SUIL_MODULE_DIR", Glib::build_filename(ardour_dll_directory(), "suil"), true);


	// Next, set up 'VAMP_PATH'
	cstr = getenv ("VAMP_PATH");
	if (cstr) {
		path = cstr;
		path += G_SEARCHPATH_SEPARATOR;
	} else {
		path = "";
	}
	path += get_module_folder() + "\\";
	path += PROGRAM_NAME;
	path += PROGRAM_VERSION;
	path += "\\bin\\vamp";
	path += G_SEARCHPATH_SEPARATOR;
	path += "%ProgramFiles%\\Vamp Plugins";
	Glib::setenv ("VAMP_PATH", path, true);


	// Next, set up 'ARDOUR_CONTROL_SURFACE_PATH'
	cstr = getenv ("ARDOUR_CONTROL_SURFACE_PATH");
	if (cstr) {
		path = cstr;
		path += G_SEARCHPATH_SEPARATOR;
	} else {
		path = "";
	}
	path += control_protocol_search_path().to_string();
	Glib::setenv ("ARDOUR_CONTROL_SURFACE_PATH", path, true);


	// Next, set up 'GTK_LOCALEDIR'
	if (ARDOUR::translations_are_enabled ()) {
		path = windows_search_path().to_string();
		path += "\\locale";
		Glib::setenv ("GTK_LOCALEDIR", path, true);

		// and return the same path to our caller
		localedir = path;
	}


	// Next, set up 'GTK_PATH'
	cstr = getenv ("GTK_PATH");
	if (cstr) {
		path = cstr;
		path += G_SEARCHPATH_SEPARATOR;
	} else {
		path = "";
	}
	path += user_config_directory();
	path += "\\.gtk-2.0";
	Glib::setenv ("GTK_PATH", path, true);


	// Unset GTK2_RC_FILES so that we only load the RC files that we define
	Glib::unsetenv ("GTK2_RC_FILES");


	// and set a '$HOME' environment variable. This variable changes the value returned
	// by 'g_get_home_dir()' so to prevent that function from unexpectedly changing its
	// mind, we'll set '$HOME' to whatever 'g_get_home_dir()' is already returning!!
	if (NULL == getenv("HOME")) {
		Glib::setenv ("HOME", Glib::locale_from_utf8(g_get_home_dir()), true);
	}

	fixup_fonts_config();
	fixup_clearlooks_config();

#ifdef DLL_PIXBUF_LOADERS
	fixup_pixbuf_loaders_config();
#endif
#ifdef DLL_PANGO_MODULES
	fixup_pango_config();
#endif
}


void load_custom_fonts()
{
	FcConfig* config = FcInitLoadConfigAndFonts();

	std::string font_file;

	if (!find_file (ardour_data_search_path(), "ArdourMono.ttf", font_file)) {
		cerr << _("Cannot find ArdourMono TrueType font") << endl;
	} else {
		FcBool ret = FcConfigAppFontAddFile(config, reinterpret_cast<const FcChar8*>(font_file.c_str()));
		if (ret == FcFalse) {
			cerr << _("Cannot load ArdourMono TrueType font.") << endl;
		}
	}

	if (!find_file (ardour_data_search_path(), "ArdourSans.ttf", font_file)) {
		cerr << _("Cannot find ArdourSans TrueType font") << endl;
	} else {
		FcBool ret = FcConfigAppFontAddFile(config, reinterpret_cast<const FcChar8*>(font_file.c_str()));
		if (ret == FcFalse) {
			cerr << _("Cannot load ArdourSans TrueType font.") << endl;
		}
	}

	if (FcFalse == FcConfigSetCurrent(config)) {
		cerr << _("Failed to set fontconfig configuration.") << endl;
	}
}
