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

#ifndef __ardour_filesource_h__
#define __ardour_filesource_h__

#include <exception>
#include <time.h>
#include "ardour/source.h"

namespace ARDOUR {

class MissingSource : public std::exception {
public:
	virtual const char *what() const throw() { return "source file does not exist"; }
};

/** A source associated with a file on disk somewhere */
class FileSource : virtual public Source {
public:
	const Glib::ustring& path() const { return _path; }

	virtual bool safe_file_extension (const Glib::ustring& path) const = 0;

	int  move_to_trash (const Glib::ustring& trash_dir_name);
	void mark_take (const Glib::ustring& id);
	void mark_immutable ();

	const Glib::ustring& take_id ()     const { return _take_id; }
	bool                 is_embedded () const { return _is_embedded; }
	uint16_t             channel()      const { return _channel; }

	int set_state (const XMLNode&, int version = 3000);

	int set_source_name (const Glib::ustring& newname, bool destructive);

	static void set_search_path (DataType type, const Glib::ustring& path);

	static bool find (DataType type, const Glib::ustring& path,
			bool must_exist, bool& is_new, uint16_t& chan,
			Glib::ustring& found_path);

protected:
	FileSource (Session& session, DataType type,
			const Glib::ustring& path, bool embedded,
			Source::Flag flags = Source::Flag(0));

	FileSource (Session& session, const XMLNode& node, bool must_exist);

	virtual int init (const Glib::ustring& idstr, bool must_exist);

	virtual int move_dependents_to_trash() { return 0; }

	bool removable () const;

	Glib::ustring _path;
	Glib::ustring _take_id;
	bool          _file_is_new;
	uint16_t      _channel;
	bool          _is_embedded;

	static std::map<DataType, Glib::ustring> search_paths;
};

} // namespace ARDOUR

#endif /* __ardour_filesource_h__ */

