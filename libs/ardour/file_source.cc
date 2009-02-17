/*
    Copyright (C) 2006-2009 Paul Davis 

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

#include <vector>

#include <sys/time.h>
#include <sys/stat.h>
#include <stdio.h> // for rename(), sigh
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <pbd/convert.h>
#include <pbd/basename.h>
#include <pbd/mountpoint.h>
#include <pbd/stl_delete.h>
#include <pbd/strsplit.h>
#include <pbd/shortpath.h>
#include <pbd/enumwriter.h>

#include <glibmm/miscutils.h>
#include <glibmm/fileutils.h>
#include <glibmm/thread.h>

#include <ardour/file_source.h>
#include <ardour/session.h>
#include <ardour/session_directory.h>
#include <ardour/source_factory.h>
#include <ardour/filename_extensions.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Glib;

static const std::string PATH_SEP = "/"; // I don't do windows

map<DataType, ustring> FileSource::search_paths;

FileSource::FileSource (Session& session, DataType type,
		const ustring& path, bool embedded, Source::Flag flag)
	: Source(session, type, path, flag)
	, _path(path)
	, _file_is_new(true)
	, _channel (0)
	, _is_embedded(embedded)
{
}

FileSource::FileSource (Session& session, const XMLNode& node, bool must_exist)
	: Source(session, node)
	, _file_is_new(false)
{
	_path = _name;
	_is_embedded = (_path.find(PATH_SEP) != string::npos);
}

bool
FileSource::removable () const
{
	return (_flags & Removable)
		&& (   (_flags & RemoveAtDestroy)
			|| ((_flags & RemovableIfEmpty) && length() == 0));
}

int
FileSource::init (const ustring& pathstr, bool must_exist)
{
	_length = 0;
	_timeline_position = 0;

	if (!find (_type, pathstr, must_exist, _file_is_new, _channel)) {
		throw MissingSource ();
	}

	if (_file_is_new && must_exist) {
		return -1;
	}
	
	return 0;
}

int
FileSource::set_state (const XMLNode& node)
{
	const XMLProperty* prop;

	if ((prop = node.property (X_("channel"))) != 0) {
		_channel = atoi (prop->value());
	} else {
		_channel = 0;
	}

	_is_embedded = (_name.find(PATH_SEP) == string::npos);

	return 0;
}

void
FileSource::mark_take (const ustring& id)
{
	if (writable ()) {
		_take_id = id;
	}
}

int
FileSource::move_to_trash (const ustring& trash_dir_name)
{
	if (is_embedded()) {
		cerr << "tried to move an embedded region to trash" << endl;
		return -1;
	}

	if (!writable()) {
		return -1;
	}

	/* don't move the file across filesystems, just stick it in the
	   trash_dir_name directory on whichever filesystem it was already on
	*/
	
	ustring newpath;
	newpath = Glib::path_get_dirname (_path);
	newpath = Glib::path_get_dirname (newpath); 

	newpath += string(PATH_SEP) + trash_dir_name + PATH_SEP;
	newpath += Glib::path_get_basename (_path);

	/* the new path already exists, try versioning */
	if (access (newpath.c_str(), F_OK) == 0) {
		char buf[PATH_MAX+1];
		int version = 1;
		ustring newpath_v;

		snprintf (buf, sizeof (buf), "%s.%d", newpath.c_str(), version);
		newpath_v = buf;

		while (access (newpath_v.c_str(), F_OK) == 0 && version < 999) {
			snprintf (buf, sizeof (buf), "%s.%d", newpath.c_str(), ++version);
			newpath_v = buf;
		}
		
		if (version == 999) {
			PBD::error << string_compose (
					_("there are already 1000 files with names like %1; versioning discontinued"),
					newpath) << endmsg;
		} else {
			newpath = newpath_v;
		}
	}

	if (::rename (_path.c_str(), newpath.c_str()) != 0) {
		PBD::error << string_compose (
				_("cannot rename file source from %1 to %2 (%3)"),
				_path, newpath, strerror (errno)) << endmsg;
		return -1;
	}

	if (move_dependents_to_trash() != 0) {
		/* try to back out */
		rename (newpath.c_str(), _path.c_str());
		return -1;
	}
	    
	_path = newpath;
	
	/* file can not be removed twice, since the operation is not idempotent */
	_flags = Flag (_flags & ~(RemoveAtDestroy|Removable|RemovableIfEmpty));

	return 0;
}

/** Find the actual source file based on \a path.
 * 
 * If the source is embedded, \a path should be a filename (no slashes).
 * If the source is external, \a path should be a full path.
 * In either case, _path is set to the complete absolute path of the source file.
 * \return true iff the file was found.
 */
bool
FileSource::find (DataType type, const ustring& path, bool must_exist, bool& isnew, uint16_t& chan)
{
	Glib::ustring search_path = search_paths[type];

	ustring pathstr = path;
	ustring::size_type pos;
	bool ret = false;

	isnew = false;

	if (pathstr[0] != '/') {

		/* non-absolute pathname: find pathstr in search path */

		vector<ustring> dirs;
		int cnt;
		ustring fullpath;
		ustring keeppath;

		if (search_path.length() == 0) {
			error << _("FileSource: search path not set") << endmsg;
			goto out;
		}

		split (search_path, dirs, ':');

		cnt = 0;
		
		for (vector<ustring>::iterator i = dirs.begin(); i != dirs.end(); ++i) {
			fullpath = *i;
			if (fullpath[fullpath.length()-1] != '/') {
				fullpath += '/';
			}

			fullpath += pathstr;

			/* i (paul) made a nasty design error by using ':' as a special character in
			   Ardour 0.99 .. this hack tries to make things sort of work.
			*/
			
			if ((pos = pathstr.find_last_of (':')) != ustring::npos) {
				
				if (Glib::file_test (fullpath, Glib::FILE_TEST_EXISTS|Glib::FILE_TEST_IS_REGULAR)) {

					/* its a real file, no problem */
					
					keeppath = fullpath;
					++cnt;

				} else {
					
					if (must_exist) {
						
						/* might be an older session using file:channel syntax. see if the version
						   without the :suffix exists
						 */
						
						ustring shorter = pathstr.substr (0, pos);
						fullpath = *i;

						if (fullpath[fullpath.length()-1] != '/') {
							fullpath += '/';
						}

						fullpath += shorter;

						if (Glib::file_test (pathstr, Glib::FILE_TEST_EXISTS|Glib::FILE_TEST_IS_REGULAR)) {
							chan = atoi (pathstr.substr (pos+1));
							pathstr = shorter;
							keeppath = fullpath;
							++cnt;
						} 
						
					} else {
						
						/* new derived file (e.g. for timefx) being created in a newer session */
						
					}
				}

			} else {

				if (Glib::file_test (fullpath, Glib::FILE_TEST_EXISTS|Glib::FILE_TEST_IS_REGULAR)) {
					keeppath = fullpath;
					++cnt;
				} 
			}
		}

		if (cnt > 1) {

			error << string_compose (
					_("FileSource: \"%1\" is ambigous when searching %2\n\t"),
					pathstr, search_path) << endmsg;
			goto out;

		} else if (cnt == 0) {

			if (must_exist) {
				error << string_compose(
						_("Filesource: cannot find required file (%1): while searching %2"),
						pathstr, search_path) << endmsg;
				goto out;
			} else {
				isnew = true;
			}
		}

		/* Current find() is unable to parse relative path names to yet non-existant
		   sources. QuickFix(tm) */
		if (keeppath == "") {
			if (must_exist) {
				error << "FileSource::find(), keeppath = \"\", but the file must exist" << endl;
			} else {
				keeppath = pathstr;
			}
		}

		_path = keeppath;
		
		ret = true;

	} else {
		
		/* external files and/or very very old style sessions include full paths */

		/* ugh, handle ':' situation */

		if ((pos = pathstr.find_last_of (':')) != ustring::npos) {
			
			ustring shorter = pathstr.substr (0, pos);

			if (Glib::file_test (shorter, Glib::FILE_TEST_EXISTS|Glib::FILE_TEST_IS_REGULAR)) {
				chan = atoi (pathstr.substr (pos+1));
				pathstr = shorter;
			}
		}
		
		_path = pathstr;

		if (!Glib::file_test (pathstr, Glib::FILE_TEST_EXISTS|Glib::FILE_TEST_IS_REGULAR)) {

			/* file does not exist or we cannot read it */
			
			if (must_exist) {
				error << string_compose(
						_("Filesource: cannot find required file (%1): %2"),
						_path, strerror (errno)) << endmsg;
				goto out;
			}
			
			if (errno != ENOENT) {
				error << string_compose(
						_("Filesource: cannot check for existing file (%1): %2"),
						_path, strerror (errno)) << endmsg;
				goto out;
			}
			
			/* a new file */
			isnew = true;
			ret = true;

		} else {
			
			/* already exists */
			ret = true;
		}
	}
	
	if (_is_embedded) {
		_name = Glib::path_get_basename (_name);
	}
	
out:
	return ret;
}

int
FileSource::set_source_name (const ustring& newname, bool destructive)
{
	Glib::Mutex::Lock lm (_lock);
	ustring oldpath = _path;
	ustring newpath = Session::change_source_path_by_name (oldpath, _name, newname, destructive);
	
	if (newpath.empty()) {
		error << string_compose (_("programming error: %1"), "cannot generate a changed file path") << endmsg;
		return -1;
	}

	// Test whether newpath exists, if yes notify the user but continue. 
	if (access(newpath.c_str(),F_OK) == 0) {
		error << _("Programming error! Ardour tried to rename a file over another file! It's safe to continue working, but please report this to the developers.") << endmsg;
		return -1;
	}

	if (rename (oldpath.c_str(), newpath.c_str()) != 0) {
		error << string_compose (_("cannot rename audio file %1 to %2"), _name, newpath) << endmsg;
		return -1;
	}

	_name = Glib::path_get_basename (newpath);
	_path = newpath;

	return 0;
}

void
FileSource::set_search_path (DataType type, const ustring& p)
{
	search_paths[type] = p;
}

void
FileSource::mark_immutable ()
{
	/* destructive sources stay writable, and their other flags don't change.  */
	if (!(_flags & Destructive)) {
		_flags = Flag (_flags & ~(Writable|Removable|RemovableIfEmpty|RemoveAtDestroy|CanRename));
	}
}

