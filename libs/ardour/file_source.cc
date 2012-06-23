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

#include "pbd/convert.h"
#include "pbd/basename.h"
#include "pbd/stl_delete.h"
#include "pbd/strsplit.h"
#include "pbd/shortpath.h"
#include "pbd/enumwriter.h"
#include "pbd/filesystem.h"

#include <glibmm/miscutils.h>
#include <glibmm/fileutils.h>
#include <glibmm/thread.h>

#include "ardour/data_type.h"
#include "ardour/file_source.h"
#include "ardour/session.h"
#include "ardour/source.h"
#include "ardour/utils.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Glib;

PBD::Signal3<int,std::string,std::string,std::vector<std::string> > FileSource::AmbiguousFileName;

FileSource::FileSource (Session& session, DataType type, const string& path, const string& origin, Source::Flag flag)
	: Source(session, type, path, flag)
	, _path(path)
	, _file_is_new(true)
	, _channel (0)
        , _origin (origin)
        , _open (false)
{
	set_within_session_from_path (path);

        prevent_deletion ();
}

FileSource::FileSource (Session& session, const XMLNode& node, bool /*must_exist*/)
	: Source (session, node)
	, _file_is_new (false)
{
	/* this setting of _path is temporary - we expect derived classes
	   to call ::init() which will actually locate the file
	   and reset _path and _within_session correctly.
	*/

	_path = _name;
	_within_session = true;

        prevent_deletion ();
}

void
FileSource::prevent_deletion ()
{
        /* if this file already exists, it cannot be removed, ever
         */

        if (Glib::file_test (_path, Glib::FILE_TEST_EXISTS)) {
                if (!(_flags & Destructive)) {
                        mark_immutable ();
                } else {
                        _flags = Flag (_flags & ~(Removable|RemovableIfEmpty|RemoveAtDestroy));
                }
        }
}

bool
FileSource::removable () const
{
        bool r = ((_flags & Removable)
                  && ((_flags & RemoveAtDestroy) ||
                      ((_flags & RemovableIfEmpty) && empty() == 0)));

        return r;
}

int
FileSource::init (const string& pathstr, bool must_exist)
{
	_timeline_position = 0;

        if (Stateful::loading_state_version < 3000) {
                if (!find_2X (_session, _type, pathstr, must_exist, _file_is_new, _channel, _path)) {
                        throw MissingSource (pathstr, _type);
                }
        } else {
                if (!find (_session, _type, pathstr, must_exist, _file_is_new, _channel, _path)) {
                        throw MissingSource (pathstr, _type);
                }
        }

	set_within_session_from_path (_path);
	
        _name = Glib::path_get_basename (_path);

	if (_file_is_new && must_exist) {
		return -1;
	}

	return 0;
}

int
FileSource::set_state (const XMLNode& node, int /*version*/)
{
	const XMLProperty* prop;

	if ((prop = node.property (X_("channel"))) != 0) {
		_channel = atoi (prop->value());
	} else {
		_channel = 0;
	}

        if ((prop = node.property (X_("origin"))) != 0) {
                _origin = prop->value();
        }

	return 0;
}

void
FileSource::mark_take (const string& id)
{
	if (writable ()) {
		_take_id = id;
	}
}

int
FileSource::move_to_trash (const string& trash_dir_name)
{
	if (!within_session() || !writable()) {
		return -1;
	}

	/* don't move the file across filesystems, just stick it in the
	   trash_dir_name directory on whichever filesystem it was already on
	*/

        vector<string> v;
	v.push_back (Glib::path_get_dirname (Glib::path_get_dirname (_path)));
        v.push_back (trash_dir_name);
	v.push_back (Glib::path_get_basename (_path));

	string newpath = Glib::build_filename (v);

	/* the new path already exists, try versioning */

	if (Glib::file_test (newpath.c_str(), Glib::FILE_TEST_EXISTS)) {
		char buf[PATH_MAX+1];
		int version = 1;
		string newpath_v;

		snprintf (buf, sizeof (buf), "%s.%d", newpath.c_str(), version);
		newpath_v = buf;

		while (Glib::file_test (newpath_v, Glib::FILE_TEST_EXISTS) && version < 999) {
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

/** Find the actual source file based on \a filename.
 *
 * If the source is within the session tree, \a filename should be a simple filename (no slashes).
 * If the source is external, \a filename should be a full path.
 * In either case, found_path is set to the complete absolute path of the source file.
 * \return true iff the file was found.
 */
bool
FileSource::find (Session& s, DataType type, const string& path, bool must_exist,
		  bool& isnew, uint16_t& /* chan */, string& found_path)
{
	bool ret = false;
        string keeppath;

	isnew = false;

        if (!Glib::path_is_absolute (path)) {
                vector<string> dirs;
                vector<string> hits;
                string fullpath;

                string search_path = s.source_search_path (type);

                if (search_path.length() == 0) {
                        error << _("FileSource: search path not set") << endmsg;
                        goto out;
                }

                split (search_path, dirs, ':');

                hits.clear ();

                for (vector<string>::iterator i = dirs.begin(); i != dirs.end(); ++i) {

                        fullpath = Glib::build_filename (*i, path);

                        if (Glib::file_test (fullpath, Glib::FILE_TEST_EXISTS|Glib::FILE_TEST_IS_REGULAR)) {
                                keeppath = fullpath;
                                hits.push_back (fullpath);
                        }
                }

		/* Remove duplicate inodes from the list of ambiguous files, since if there are symlinks
		   in the session path it is possible to arrive at the same file via more than one path.

		   I suppose this is not necessary on Windows.
		*/

		vector<string> de_duped_hits;

		for (vector<string>::iterator i = hits.begin(); i != hits.end(); ++i) {

			vector<string>::iterator j = i;
			++j;
			
			while (j != hits.end()) {
				if (PBD::sys::equivalent_paths (*i, *j)) {
					/* *i and *j are the same file; break out of the loop early */
					break;
				}

				++j;
			}

			if (j == hits.end ()) {
				de_duped_hits.push_back (*i);
			}
		}

                if (de_duped_hits.size() > 1) {

			/* more than one match: ask the user */

                        int which = FileSource::AmbiguousFileName (path, search_path, de_duped_hits).get_value_or (-1);

                        if (which < 0) {
                                goto out;
                        } else {
                                keeppath = de_duped_hits[which];
                        }

                } else if (de_duped_hits.size() == 0) {

			/* no match: error */

                        if (must_exist) {
                                error << string_compose(
                                        _("Filesource: cannot find required file (%1): while searching %2"),
                                        path, search_path) << endmsg;
                                goto out;
                        } else {
                                isnew = true;
                        }
                } else {

			/* only one match: happy days */
			
			keeppath = de_duped_hits[0];
		}
						  
        } else {
                keeppath = path;
        }

        /* Current find() is unable to parse relative path names to yet non-existant
           sources. QuickFix(tm)
        */
        if (keeppath == "") {
                if (must_exist) {
                        error << "FileSource::find(), keeppath = \"\", but the file must exist" << endl;
                } else {
                        keeppath = path;
                }
        }

        found_path = keeppath;

        ret = true;

  out:
	return ret;
}

/** Find the actual source file based on \a filename.
 *
 * If the source is within the session tree, \a filename should be a simple filename (no slashes).
 * If the source is external, \a filename should be a full path.
 * In either case, found_path is set to the complete absolute path of the source file.
 * \return true iff the file was found.
 */
bool
FileSource::find_2X (Session& s, DataType type, const string& path, bool must_exist,
                     bool& isnew, uint16_t& chan, string& found_path)
{
	string search_path = s.source_search_path (type);

	string pathstr = path;
	string::size_type pos;
	bool ret = false;

	isnew = false;

	if (!Glib::path_is_absolute (pathstr)) {

		/* non-absolute pathname: find pathstr in search path */

		vector<string> dirs;
		int cnt;
		string fullpath;
		string keeppath;

		if (search_path.length() == 0) {
			error << _("FileSource: search path not set") << endmsg;
			goto out;
		}

		split (search_path, dirs, ':');

		cnt = 0;

		for (vector<string>::iterator i = dirs.begin(); i != dirs.end(); ++i) {

                        fullpath = Glib::build_filename (*i, pathstr);

			/* i (paul) made a nasty design error by using ':' as a special character in
			   Ardour 0.99 .. this hack tries to make things sort of work.
			*/

			if ((pos = pathstr.find_last_of (':')) != string::npos) {

				if (Glib::file_test (fullpath, Glib::FILE_TEST_EXISTS|Glib::FILE_TEST_IS_REGULAR)) {

					/* its a real file, no problem */

					keeppath = fullpath;
					++cnt;

				} else {

					if (must_exist) {

						/* might be an older session using file:channel syntax. see if the version
						   without the :suffix exists
						 */

						string shorter = pathstr.substr (0, pos);
                                                fullpath = Glib::build_filename (*i, shorter);

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

		found_path = keeppath;

		ret = true;

	} else {

		/* external files and/or very very old style sessions include full paths */

		/* ugh, handle ':' situation */

		if ((pos = pathstr.find_last_of (':')) != string::npos) {

			string shorter = pathstr.substr (0, pos);

			if (Glib::file_test (shorter, Glib::FILE_TEST_EXISTS|Glib::FILE_TEST_IS_REGULAR)) {
				chan = atoi (pathstr.substr (pos+1));
				pathstr = shorter;
			}
		}

		found_path = pathstr;

		if (!Glib::file_test (pathstr, Glib::FILE_TEST_EXISTS|Glib::FILE_TEST_IS_REGULAR)) {

			/* file does not exist or we cannot read it */

			if (must_exist) {
				error << string_compose(
						_("Filesource: cannot find required file (%1): %2"),
						path, strerror (errno)) << endmsg;
				goto out;
			}

			if (errno != ENOENT) {
				error << string_compose(
						_("Filesource: cannot check for existing file (%1): %2"),
						path, strerror (errno)) << endmsg;
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

out:
	return ret;
}

int
FileSource::set_source_name (const string& newname, bool destructive)
{
	Glib::Mutex::Lock lm (_lock);
	string oldpath = _path;
	string newpath = _session.change_source_path_by_name (oldpath, _name, newname, destructive);

	if (newpath.empty()) {
		error << string_compose (_("programming error: %1"), "cannot generate a changed file path") << endmsg;
		return -1;
	}

	// Test whether newpath exists, if yes notify the user but continue.
	if (Glib::file_test (newpath, Glib::FILE_TEST_EXISTS)) {
		error << string_compose (_("Programming error! %1 tried to rename a file over another file! It's safe to continue working, but please report this to the developers."), PROGRAM_NAME) << endmsg;
		return -1;
	}

        if (::rename (oldpath.c_str(), newpath.c_str()) != 0) {
                error << string_compose (_("cannot rename file %1 to %2 (%3)"), oldpath, newpath, strerror(errno)) << endmsg;
                return -1;
        }

	_name = Glib::path_get_basename (newpath);
	_path = newpath;

	return 0;
}

void
FileSource::mark_immutable ()
{
	/* destructive sources stay writable, and their other flags don't change.  */
	if (!(_flags & Destructive)) {
		_flags = Flag (_flags & ~(Writable|Removable|RemovableIfEmpty|RemoveAtDestroy|CanRename));
	}
}

void
FileSource::mark_nonremovable ()
{
        _flags = Flag (_flags & ~(Removable|RemovableIfEmpty|RemoveAtDestroy));
}

void
FileSource::set_within_session_from_path (const std::string& path)
{
	_within_session = _session.path_is_within_session (path);
}

void
FileSource::set_path (const std::string& newpath)
{
        _path = newpath;
}

void
FileSource::inc_use_count ()
{
        Source::inc_use_count ();
}

