// -*- c++ -*-
#ifndef _GLIBMM_MISCUTILS_H
#define _GLIBMM_MISCUTILS_H

/* $Id: miscutils.h 428 2007-07-29 12:43:29Z murrayc $ */

/* Copyright (C) 2002 The gtkmm Development Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <glibmm/arrayhandle.h>
#include <glibmm/ustring.h>


namespace Glib
{

/** @defgroup MiscUtils Miscellaneous Utility Functions
 * Miscellaneous Utility Functions -- a selection of portable utility functions.
 * @{
 */

/** Gets a human-readable name for the application,
 * as set by Glib::set_application_name().
 * This name should be localized if possible, and is intended for display to
 * the user.  Contrast with Glib::get_prgname(), which gets a non-localized
 * name. If Glib::set_application_name() has not been called, returns the
 * result of Glib::get_prgname() (which may be empty if Glib::set_prgname()
 * has also not been called).
 *
 * @return Human-readable application name. May return <tt>""</tt>.
 */
Glib::ustring get_application_name();

/** Sets a human-readable name for the application.
 * This name should be localized if possible, and is intended for display to
 * the user.  Contrast with Glib::set_prgname(), which sets a non-localized
 * name.  Glib::set_prgname() will be called automatically by
 * <tt>gtk_init()</tt>, but Glib::set_application_name() will not.
 *
 * Note that for thread safety reasons, this function can only be called once.
 *
 * The application name will be used in contexts such as error messages,
 * or when displaying an application's name in the task list.
 *
 * @param application_name Localized name of the application.
 */
void set_application_name(const Glib::ustring& application_name);

/** Gets the name of the program.
 * If you are using GDK or GTK+ the program name is set in <tt>gdk_init()</tt>,
 * which is called by <tt>gtk_init()</tt>.  The program name is found by taking
 * the last component of <tt>argv[0]</tt>.
 * @return The name of the program.
 */
std::string get_prgname();

/** Sets the name of the program.
 * @param prgname The name of the program.
 */
void set_prgname(const std::string& prgname);

/** Returns the value of an environment variable. The name and value
 * are in the GLib file name encoding. On Unix, this means the actual
 * bytes which might or might not be in some consistent character set
 * and encoding. On Windows, it is in UTF-8. On Windows, in case the
 * environment variable's value contains references to other
 * environment variables, they are expanded.
 *
 * @param variable The environment variable to get.
 * @retval found <tt>true</tt> Whether the environment variable has been found.
 * @return The value of the environment variable, or <tt>""</tt> if not found.
 */
std::string getenv(const std::string& variable, bool& found);

/** Returns the value of an environment variable. The name and value
 * are in the GLib file name encoding. On Unix, this means the actual
 * bytes which might or might not be in some consistent character set
 * and encoding. On Windows, it is in UTF-8. On Windows, in case the
 * environment variable's value contains references to other
 * environment variables, they are expanded.
 *
 * @param variable The environment variable to get.
 * @return The value of the environment variable, or <tt>""</tt> if not found.
 */
std::string getenv(const std::string& variable);


/** Sets an environment variable. Both the variable's name and value
 * should be in the GLib file name encoding. On Unix, this means that
 * they can be any sequence of bytes. On Windows, they should be in
 * UTF-8.
 *
 * Note that on some systems, when variables are overwritten, the memory 
 * used for the previous variables and its value isn't reclaimed.
 *
 * @param variable The environment variable to set. It must not contain '='.
 * @param value  The value to which the variable should be set.
 * @param overwrite Whether to change the variable if it already exists.
 * @result false if the environment variable couldn't be set.
 */ 
bool setenv(const std::string& variable, const std::string& value, bool overwrite = true);

/** Removes an environment variable from the environment.
 *
 * Note that on some systems, when variables are overwritten, the memory 
 * used for the previous variables and its value isn't reclaimed.
 * Furthermore, this function can't be guaranteed to operate in a 
 * threadsafe way.
 *
 * @param variable: the environment variable to remove. It  must not contain '='.
 **/
void unsetenv(const std::string& variable);

/** Gets the user name of the current user.
 * @return The name of the current user.
 */
std::string get_user_name();

/** Gets the real name of the user.
 * This usually comes from the user's entry in the <tt>passwd</tt> file.
 * @return The user's real name.
 */
std::string get_real_name();

/** Gets the current user's home directory.
 * @return The current user's home directory or an empty string if not defined.
 */
std::string get_home_dir();

/** Gets the directory to use for temporary files.
 * This is found from inspecting the environment variables <tt>TMPDIR</tt>,
 * <tt>TMP</tt>, and <tt>TEMP</tt> in that order.  If none of those are defined
 * <tt>"/tmp"</tt> is returned on UNIX and <tt>"C:\\"</tt> on Windows.
 * @return The directory to use for temporary files.
 */
std::string get_tmp_dir();

/** Gets the current directory.
 * @return The current directory.
 */
std::string get_current_dir();

//TODO: We could create a C++ enum to wrap the C GUserDirectory enum,
//but we would have to either be very careful, or define the enum 
//values in terms of the C enums anyway.
/** Returns the full path of a special directory using its logical id.
 *
 * On Unix this is done using the XDG special user directories.
 *
 * Depending on the platform, the user might be able to change the path
 * of the special directory without requiring the session to restart; GLib
 * will not reflect any change once the special directories are loaded.
 *
 * Return value: the path to the specified special directory.
 * @param directory Te logical id of special directory
 * 
 * @newin2p14
 */
std::string get_user_special_dir(GUserDirectory directory);

/** Returns a base directory in which to access application data such as icons
 * that is customized for a particular user.
 *
 * On UNIX platforms this is determined using the mechanisms described in the
 * XDG Base Directory Specification
 *
 * @newin2p14
 */
std::string get_user_data_dir();

/** Returns a base directory in which to store user-specific application
 * configuration information such as user preferences and settings.
 *
 * On UNIX platforms this is determined using the mechanisms described in the
 * XDG Base Directory Specification
 *
 * @newin2p14
 */
std::string get_user_config_dir();

/** Returns a base directory in which to store non-essential, cached data
 * specific to particular user.
 *
 * On UNIX platforms this is determined using the mechanisms described in the
 * XDG Base Directory Specification
 *
 * @newin2p14
 */
std::string get_user_cache_dir();

/** Returns @c true if the given @a filename is an absolute file name, i.e.\ it
 * contains a full path from the root directory such as <tt>"/usr/local"</tt>
 * on UNIX or <tt>"C:\\windows"</tt> on Windows systems.
 * @param filename A file name.
 * @return Whether @a filename is an absolute path.
 */
bool path_is_absolute(const std::string& filename);

/** Returns the remaining part of @a filename after the root component,
 * i.e.\ after the <tt>"/"</tt> on UNIX or <tt>"C:\\"</tt> on Windows.
 * If @a filename is not an absolute path, <tt>""</tt> will be returned.
 * @param filename A file name.
 * @return The file name without the root component, or <tt>""</tt>.
 */
std::string path_skip_root(const std::string& filename);

/** Gets the name of the file without any leading directory components.
 * @param filename The name of the file.
 * @return The name of the file without any leading directory components.
 */
std::string path_get_basename(const std::string& filename);

/** Gets the directory components of a file name.
 * If the file name has no directory components <tt>"."</tt> is returned.
 * @param filename The name of the file.
 * @return The directory components of the file.
 */
std::string path_get_dirname(const std::string& filename);

/** Creates a filename from a series of elements using the correct
 * separator for filenames.
 * This function behaves identically to Glib::build_path(G_DIR_SEPARATOR_S,
 * elements).  No attempt is made to force the resulting filename to be an
 * absolute path.  If the first element is a relative path, the result will
 * be a relative path.
 * @param elements A container holding the elements of the path to build.
 *   Any STL compatible container type is accepted.
 * @return The resulting path.
 */
std::string build_filename(const Glib::ArrayHandle<std::string>& elements);

/** Creates a filename from two elements using the correct separator for filenames.
 * No attempt is made to force the resulting filename to be an absolute path.
 * If the first element is a relative path, the result will be a relative path.
 * @param elem1 First path element.
 * @param elem2 Second path element.
 * @return The resulting path.
 */
std::string build_filename(const std::string& elem1, const std::string& elem2);

/** Creates a path from a series of elements using @a separator as the
 * separator between elements.
 *
 * At the boundary between two elements, any trailing occurrences of
 * @a separator in the first element, or leading occurrences of @a separator
 * in the second element are removed and exactly one copy of the separator is
 * inserted.
 *
 * Empty elements are ignored.
 *
 * The number of leading copies of the separator on the result is
 * the same as the number of leading copies of the separator on
 * the first non-empty element.
 *
 * The number of trailing copies of the separator on the result is the same
 * as the number of trailing copies of the separator on the last non-empty
 * element. (Determination of the number of trailing copies is done without
 * stripping leading copies, so if the separator is <tt>"ABA"</tt>,
 * <tt>"ABABA"</tt> has 1 trailing copy.)
 *
 * However, if there is only a single non-empty element, and there
 * are no characters in that element not part of the leading or
 * trailing separators, then the result is exactly the original value
 * of that element.
 *
 * Other than for determination of the number of leading and trailing
 * copies of the separator, elements consisting only of copies
 * of the separator are ignored.
 *                                                                             
 * @param separator A string used to separate the elements of the path.
 * @param elements A container holding the elements of the path to build.
 *   Any STL compatible container type is accepted.
 * @return The resulting path.
 */
std::string build_path(const std::string& separator,
                       const Glib::ArrayHandle<std::string>& elements);

/** Locates the first executable named @a program in the user's path, in the
 * same way that <tt>execvp()</tt> would locate it.
 * Returns a string with the absolute path name, or <tt>""</tt> if the program
 * is not found in the path.  If @a program is already an absolute path,
 * returns a copy of @a program if @a program exists and is executable, and
 * <tt>""</tt> otherwise.
 *
 * On Windows, if @a program does not have a file type suffix, tries to append
 * the suffixes in the <tt>PATHEXT</tt> environment variable (if that doesn't
 * exist, the suffixes .com, .exe, and .bat) in turn, and then look for the
 * resulting file name in the same way as CreateProcess() would.  This means
 * first in the directory where the program was loaded from, then in the
 * current directory, then in the Windows 32-bit system directory, then in the
 * Windows directory, and finally in the directories in the <tt>PATH</tt>
 * environment variable.  If the program is found, the return value contains
 * the full name including the type suffix.
 *
 * @param program A program name.
 * @return An absolute path, or <tt>""</tt>.
 */
std::string find_program_in_path(const std::string& program);

/** @} group MiscUtils */

} // namespace Glib


#endif /* _GLIBMM_FILEUTILS_H */

