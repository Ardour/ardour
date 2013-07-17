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

#include <list>
#include <string>
#include <exception>
#include <time.h>
#include "ardour/source.h"

namespace ARDOUR {

class MissingSource : public std::exception
{
  public:
	MissingSource (const std::string& p, DataType t) throw ()
		: path (p), type (t) {}
	~MissingSource() throw() {}

	virtual const char *what() const throw() { return "source file does not exist"; }

	std::string path;
	DataType type;
};

/** A source associated with a file on disk somewhere */
class FileSource : virtual public Source {
public:
	virtual ~FileSource () {}

	virtual const std::string& path() const { return _path; }

	virtual bool safe_file_extension (const std::string& path) const = 0;

	int  move_to_trash (const std::string& trash_dir_name);
	void mark_take (const std::string& id);
        void mark_immutable ();
        void mark_immutable_except_write();
	void mark_nonremovable ();

	const std::string& take_id ()        const { return _take_id; }
	bool                 within_session () const { return _within_session; }
	uint16_t             channel()         const { return _channel; }

	int set_state (const XMLNode&, int version);

	int set_source_name (const std::string& newname, bool destructive);

	static bool find (Session&, DataType type, const std::string& path,
	                  bool must_exist, bool& is_new, uint16_t& chan,
	                  std::string& found_path);

	static bool find_2X (Session&, DataType type, const std::string& path,
	                     bool must_exist, bool& is_new, uint16_t& chan,
	                     std::string& found_path);

	void inc_use_count ();
	bool removable () const;

	const std::string& origin() const { return _origin; }

	virtual void set_path (const std::string&);
	
	static PBD::Signal2<int,std::string,std::vector<std::string> > AmbiguousFileName;

protected:
	FileSource (Session& session, DataType type,
	            const std::string& path,
	            const std::string& origin,
	            Source::Flag flags = Source::Flag(0));

	FileSource (Session& session, const XMLNode& node, bool must_exist);

	virtual int init (const std::string& idstr, bool must_exist);

	virtual int move_dependents_to_trash() { return 0; }
	void set_within_session_from_path (const std::string&);

	std::string _path;
	std::string _take_id;
	bool        _file_is_new;
	uint16_t    _channel;
	bool        _within_session;
	std::string _origin;
	bool        _open;

	void prevent_deletion ();
};

} // namespace ARDOUR

#endif /* __ardour_filesource_h__ */

