/*
    Copyright (C) 2010-2013 Paul Davis
    Author: Robin Gareus <robin@gareus.org>

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
#include <string>
#include <gtkmm.h>

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#include <shlobj.h> // CSIDL_*
#include "pbd/windows_special_dirs.h"
#endif

#include "pbd/file_utils.h"
#include "video_tool_paths.h"
#include "i18n.h"

using namespace PBD;

#ifdef PLATFORM_WINDOWS

static bool
windows_install_dir (const char *regkey, std::string &rv) {
	HKEY key;
	DWORD size = PATH_MAX;
	char tmp[PATH_MAX+1];

	if (   (ERROR_SUCCESS == RegOpenKeyExA (HKEY_LOCAL_MACHINE, regkey, 0, KEY_READ, &key))
	    && (ERROR_SUCCESS == RegQueryValueExA (key, "Install_Dir", 0, NULL, reinterpret_cast<LPBYTE>(tmp), &size))
		 )
	{
		rv = Glib::locale_to_utf8(tmp);
		return true;
	}

	if (   (ERROR_SUCCESS == RegOpenKeyExA (HKEY_LOCAL_MACHINE, regkey, 0, KEY_READ | KEY_WOW64_32KEY, &key))
	    && (ERROR_SUCCESS == RegQueryValueExA (key, "Install_Dir", 0, NULL, reinterpret_cast<LPBYTE>(tmp), &size))
			)
	{
		rv = Glib::locale_to_utf8(tmp);
		return true;
	}

	return false;
}
#endif

bool
ArdourVideoToolPaths::harvid_exe (std::string &harvid_exe)
{

#ifdef PLATFORM_WINDOWS
	std::string reg;
	const char *program_files = PBD::get_win_special_folder (CSIDL_PROGRAM_FILES);
#endif

	harvid_exe = "";

	std::string icsd_file_path;
	if (find_file (PBD::Searchpath(Glib::getenv("PATH")), X_("harvid"), icsd_file_path)) {
		harvid_exe = icsd_file_path;
	}
#ifdef PLATFORM_WINDOWS
	else if ( windows_install_dir("Software\\" PROGRAM_NAME "\\v" PROGRAM_VERSION "\\video", reg))
	{
		harvid_exe = g_build_filename(reg.c_str(), "harvid", "harvid.exe", NULL);
	}
	else if ( windows_install_dir("Software\\RSS\\harvid", reg))
	{
		harvid_exe = g_build_filename(reg.c_str(), "harvid.exe", NULL);
	}
	else if (program_files && Glib::file_test(g_build_filename(program_files, "harvid", "harvid.exe", NULL), Glib::FILE_TEST_EXISTS))
	{
		harvid_exe = g_build_filename(program_files, "harvid", "harvid.exe", NULL);
	}
	else if (Glib::file_test(X_("C:\\Program Files\\harvid\\harvid.exe"), Glib::FILE_TEST_EXISTS)) {
		harvid_exe = X_("C:\\Program Files\\harvid\\harvid.exe");
	}
#endif
	else
	{
		return false;
	}
	return true;
}

bool
ArdourVideoToolPaths::xjadeo_exe (std::string &xjadeo_exe)
{
	std::string xjadeo_file_path;
#ifdef PLATFORM_WINDOWS
	std::string reg;
	const char *program_files = PBD::get_win_special_folder (CSIDL_PROGRAM_FILES);
#endif
	if (getenv("XJREMOTE")) {
		xjadeo_exe = getenv("XJREMOTE");
	} else if (find_file (Searchpath(Glib::getenv("PATH")), X_("xjremote"), xjadeo_file_path)) {
		xjadeo_exe = xjadeo_file_path;
	} else if (find_file (Searchpath(Glib::getenv("PATH")), X_("xjadeo"), xjadeo_file_path)) {
		xjadeo_exe = xjadeo_file_path;
	}
#ifdef __APPLE__
	else if (Glib::file_test(X_("/Applications/Xjadeo.app/Contents/MacOS/xjremote"), Glib::FILE_TEST_EXISTS|Glib::FILE_TEST_IS_EXECUTABLE)) {
		xjadeo_exe = X_("/Applications/Xjadeo.app/Contents/MacOS/xjremote");
	}
	else if (Glib::file_test(X_("/Applications/Jadeo.app/Contents/MacOS/xjremote"), Glib::FILE_TEST_EXISTS|Glib::FILE_TEST_IS_EXECUTABLE)) {
		xjadeo_exe = X_("/Applications/Jadeo.app/Contents/MacOS/xjremote");
	}
#endif
#ifdef PLATFORM_WINDOWS
	else if ( windows_install_dir("Software\\" PROGRAM_NAME "\\v" PROGRAM_VERSION "\\video", reg))
	{
		xjadeo_exe = std::string(g_build_filename(reg.c_str(), "xjadeo", "xjadeo.exe", NULL));
	}
	else if ( windows_install_dir("Software\\RSS\\xjadeo", reg))
	{
		xjadeo_exe = std::string(g_build_filename(reg.c_str(), "xjadeo.exe", NULL));
	}
	else if (program_files && Glib::file_test(g_build_filename(program_files, "xjadeo", "xjadeo.exe", NULL), Glib::FILE_TEST_EXISTS))
	{
		xjadeo_exe = std::string(g_build_filename(program_files, "xjadeo", "xjadeo.exe", NULL));
	}
	else if (Glib::file_test(X_("C:\\Program Files\\xjadeo\\xjadeo.exe"), Glib::FILE_TEST_EXISTS)) {
		xjadeo_exe = X_("C:\\Program Files\\xjadeo\\xjadeo.exe");
	}
#endif
	else  {
		xjadeo_exe = X_("");
		return false;
	}
	return true;
}

bool
ArdourVideoToolPaths::transcoder_exe (std::string &ffmpeg_exe, std::string &ffprobe_exe)
{
#ifdef PLATFORM_WINDOWS
	std::string reg;
	const char *program_files = PBD::get_win_special_folder (CSIDL_PROGRAM_FILES);
#endif

	ffmpeg_exe = X_("");
	ffprobe_exe = X_("");

	std::string ff_file_path;
	if (find_file (Searchpath(Glib::getenv("PATH")), X_("ffmpeg_harvid"), ff_file_path)) {
		ffmpeg_exe = ff_file_path;
	}
#ifdef PLATFORM_WINDOWS
	else if ( windows_install_dir("Software\\" PROGRAM_NAME "\\v" PROGRAM_VERSION "\\video", reg))
	{
		ffmpeg_exe = g_build_filename(reg.c_str(), X_("harvid"), X_("ffmpeg.exe"), NULL);
		ffprobe_exe = g_build_filename(reg.c_str(), X_("harvid"), X_("ffprobe.exe"), NULL);
	}
	else if ( windows_install_dir("Software\\RSS\\harvid", reg))
	{
		ffmpeg_exe = g_build_filename(reg.c_str(), X_("ffmpeg.exe"), NULL);
		ffprobe_exe = g_build_filename(reg.c_str(), X_("ffprobe.exe"), NULL);
	}

	if (Glib::file_test(ffmpeg_exe, Glib::FILE_TEST_EXISTS)) {
		;
	}
	else if (program_files && Glib::file_test(g_build_filename(program_files, "harvid", "ffmpeg.exe", NULL), Glib::FILE_TEST_EXISTS)) {
		ffmpeg_exe = g_build_filename(program_files, "harvid", "ffmpeg.exe", NULL);
	}
	else if (Glib::file_test(X_("C:\\Program Files\\ffmpeg\\ffmpeg.exe"), Glib::FILE_TEST_EXISTS)) {
		ffmpeg_exe = X_("C:\\Program Files\\ffmpeg\\ffmpeg.exe");
	} else {
		ffmpeg_exe = X_("");
	}
#endif

	if (find_file (Searchpath(Glib::getenv("PATH")), X_("ffprobe_harvid"), ff_file_path)) {
		ffprobe_exe = ff_file_path;
	}
#ifdef PLATFORM_WINDOWS
	if (Glib::file_test(ffprobe_exe, Glib::FILE_TEST_EXISTS)) {
		;
	}
	else if (program_files && Glib::file_test(g_build_filename(program_files, "harvid", "ffprobe.exe", NULL), Glib::FILE_TEST_EXISTS)) {
		ffprobe_exe = g_build_filename(program_files, "harvid", "ffprobe.exe", NULL);
	}
	else if (Glib::file_test(X_("C:\\Program Files\\ffmpeg\\ffprobe.exe"), Glib::FILE_TEST_EXISTS)) {
		ffprobe_exe = X_("C:\\Program Files\\ffmpeg\\ffprobe.exe");
	} else {
		ffprobe_exe = X_("");
	}
#endif

	if (ffmpeg_exe.empty() || ffprobe_exe.empty()) {
		return false;
	}
	return true;
}
