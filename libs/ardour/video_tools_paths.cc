/*
 * Copyright (C) 2018 Robin Gareus <robin@gareus.org>
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

/* This extends ardour/filesystem_paths.cc but requires additional
 * includes, in particular 'rc_configuration.h' which in turn
 * pulls in types.h, which include temporal/bbt_time and evoral
 *
 * filesystem_paths.cc is used by various standalone utils,
 * e.g. the VST scanner and pulling in most of libardour's dependencies
 * there is not reasonable.
 */

#include <string>

#include "pbd/file_utils.h"

#include <glibmm/convert.h>
#include <glibmm/miscutils.h>
#include <glibmm/fileutils.h>

#include "ardour/filesystem_paths.h"
#include "ardour/rc_configuration.h"

#include "pbd/i18n.h"

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#include "shlobj.h"
#include "pbd/windows_special_dirs.h"
#endif

using namespace PBD;

namespace ARDOUR {

bool
ArdourVideoToolPaths::harvid_exe (std::string &harvid_exe)
{

#ifdef PLATFORM_WINDOWS
	std::string reg;
	std::string program_files = PBD::get_win_special_folder_path (CSIDL_PROGRAM_FILES);
#endif

	harvid_exe = "";

	std::string icsd_file_path;
	if (find_file (PBD::Searchpath(Glib::getenv("PATH")), X_("harvid"), icsd_file_path)) {
		harvid_exe = icsd_file_path;
	}
#ifdef PLATFORM_WINDOWS
	else if (PBD::windows_query_registry ("Software\\" PROGRAM_NAME "\\v" PROGRAM_VERSION "\\video", "Install_Dir", reg))
	{
		harvid_exe = g_build_filename(reg.c_str(), "harvid", "harvid.exe", NULL);
	}
	else if (PBD::windows_query_registry ("Software\\RSS\\harvid", "Install_Dir", reg))
	{
		harvid_exe = g_build_filename(reg.c_str(), "harvid.exe", NULL);
	}
	else if (!program_files.empty() && Glib::file_test(g_build_filename(program_files.c_str(), "harvid", "harvid.exe", NULL), Glib::FILE_TEST_EXISTS))
	{
		harvid_exe = g_build_filename(program_files.c_str(), "harvid", "harvid.exe", NULL);
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
	std::string program_files = PBD::get_win_special_folder_path (CSIDL_PROGRAM_FILES);
#endif
	xjadeo_exe = X_("");

	if (getenv("XJREMOTE")) {
		xjadeo_exe = getenv("XJREMOTE");
#ifdef __APPLE__
	} else if (!Config->get_xjadeo_binary().empty()
			&& Glib::file_test (Config->get_xjadeo_binary() + "/Contents/MacOS/xjadeo", Glib::FILE_TEST_EXISTS|Glib::FILE_TEST_IS_EXECUTABLE)) {
		xjadeo_exe = Config->get_xjadeo_binary() + "/Contents/MacOS/xjadeo";
#endif
	} else if (!Config->get_xjadeo_binary().empty()
			&& Glib::file_test (Config->get_xjadeo_binary(), Glib::FILE_TEST_EXISTS|Glib::FILE_TEST_IS_EXECUTABLE)) {
		xjadeo_exe = Config->get_xjadeo_binary();
	} else if (find_file (Searchpath(Glib::getenv("PATH")), X_("xjremote"), xjadeo_file_path)) {
		xjadeo_exe = xjadeo_file_path;
	} else if (find_file (Searchpath(Glib::getenv("PATH")), X_("xjadeo"), xjadeo_file_path)) {
		xjadeo_exe = xjadeo_file_path;
	}
#ifdef __APPLE__
	else if (Glib::file_test(X_("/Applications/Xjadeo.app/Contents/MacOS/xjadeo"), Glib::FILE_TEST_EXISTS|Glib::FILE_TEST_IS_EXECUTABLE)) {
		xjadeo_exe = X_("/Applications/Xjadeo.app/Contents/MacOS/xjadeo");
	}
	else if (Glib::file_test(X_("/Applications/Jadeo.app/Contents/MacOS/Jadeo-bin"), Glib::FILE_TEST_EXISTS|Glib::FILE_TEST_IS_EXECUTABLE)) {
		xjadeo_exe = X_("/Applications/Jadeo.app/Contents/MacOS/Jadeo-bin");
	}
#endif
#ifdef PLATFORM_WINDOWS
	else if (PBD::windows_query_registry ("Software\\" PROGRAM_NAME "\\v" PROGRAM_VERSION "\\video", "Install_Dir", reg))
	{
		xjadeo_exe = std::string(g_build_filename(reg.c_str(), "xjadeo", "xjadeo.exe", NULL));
	}
	else if (PBD::windows_query_registry ("Software\\RSS\\xjadeo", "Install_Dir", reg))
	{
		xjadeo_exe = std::string(g_build_filename(reg.c_str(), "xjadeo.exe", NULL));
	}
	else if (!program_files.empty() && Glib::file_test(g_build_filename(program_files.c_str(), "xjadeo", "xjadeo.exe", NULL), Glib::FILE_TEST_EXISTS))
	{
		xjadeo_exe = std::string(g_build_filename(program_files.c_str(), "xjadeo", "xjadeo.exe", NULL));
	}
	else if (Glib::file_test(X_("C:\\Program Files\\xjadeo\\xjadeo.exe"), Glib::FILE_TEST_EXISTS)) {
		xjadeo_exe = X_("C:\\Program Files\\xjadeo\\xjadeo.exe");
	}
#endif

	return (!xjadeo_exe.empty() && Glib::file_test(xjadeo_exe, Glib::FILE_TEST_EXISTS|Glib::FILE_TEST_IS_EXECUTABLE));
}

bool
ArdourVideoToolPaths::transcoder_exe (std::string &ffmpeg_exe, std::string &ffprobe_exe)
{
#ifdef PLATFORM_WINDOWS
	std::string reg;
	std::string program_files = PBD::get_win_special_folder_path (CSIDL_PROGRAM_FILES);
#endif

	ffmpeg_exe = X_("");
	ffprobe_exe = X_("");

	std::string ff_file_path;
	if (find_file (Searchpath(Glib::getenv("PATH")), X_("ffmpeg_harvid"), ff_file_path)) {
		ffmpeg_exe = ff_file_path;
	}
#ifdef PLATFORM_WINDOWS
	else if (PBD::windows_query_registry ("Software\\" PROGRAM_NAME "\\v" PROGRAM_VERSION "\\video", "Install_Dir", reg))
	{
		ffmpeg_exe = g_build_filename(reg.c_str(), X_("harvid"), X_("ffmpeg.exe"), NULL);
		ffprobe_exe = g_build_filename(reg.c_str(), X_("harvid"), X_("ffprobe.exe"), NULL);
	}
	else if (PBD::windows_query_registry ("Software\\RSS\\harvid", "Install_Dir", reg))
	{
		ffmpeg_exe = g_build_filename(reg.c_str(), X_("ffmpeg.exe"), NULL);
		ffprobe_exe = g_build_filename(reg.c_str(), X_("ffprobe.exe"), NULL);
	}

	if (Glib::file_test(ffmpeg_exe, Glib::FILE_TEST_EXISTS)) {
		;
	}
	else if (!program_files.empty() && Glib::file_test(g_build_filename(program_files.c_str(), "harvid", "ffmpeg.exe", NULL), Glib::FILE_TEST_EXISTS)) {
		ffmpeg_exe = g_build_filename(program_files.c_str(), "harvid", "ffmpeg.exe", NULL);
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
	else if (!program_files.empty() && Glib::file_test(g_build_filename(program_files.c_str(), "harvid", "ffprobe.exe", NULL), Glib::FILE_TEST_EXISTS)) {
		ffprobe_exe = g_build_filename(program_files.c_str(), "harvid", "ffprobe.exe", NULL);
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

} // namespace ARDOUR
