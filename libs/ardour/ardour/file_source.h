/*
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_filesource_h__
#define __ardour_filesource_h__

#include <list>
#include <string>
#include <exception>
#include <time.h>
#include "ardour/source.h"

namespace ARDOUR {

class LIBARDOUR_API MissingSource : public std::exception
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
class LIBARDOUR_API FileSource : virtual public Source {
public:
	virtual ~FileSource ();

	const std::string& path() const { return _path; }

	virtual bool safe_file_extension (const std::string& path) const = 0;

	int  move_to_trash (const std::string& trash_dir_name);
	void mark_take (const std::string& id);
        void mark_immutable ();
        void mark_immutable_except_write();
	void mark_nonremovable ();

	bool                 within_session () const { return _within_session; }
	uint16_t             channel()         const { return _channel; }
	float                gain()            const { return _gain; }

	virtual void set_gain (float g, bool temporarily = false) { _gain = g; }

	int set_state (const XMLNode&, int version);

	int set_source_name (const std::string& newname);

	static bool find (Session&, DataType type, const std::string& path,
	                  bool must_exist, bool& is_new, uint16_t& chan,
	                  std::string& found_path);

	static bool find_2X (Session&, DataType type, const std::string& path,
	                     bool must_exist, bool& is_new, uint16_t& chan,
	                     std::string& found_path);

	void inc_use_count ();
	bool removable () const;
        bool is_stub () const;

	const std::string& origin() const { return _origin; }
	void set_origin (std::string const& o) { _origin = o; }

	virtual void set_path (const std::string&);
	void replace_file (const std::string&);

	static PBD::Signal2<int,std::string,std::vector<std::string> > AmbiguousFileName;

	void existence_check ();
	virtual void prevent_deletion ();

	/** Rename the file on disk referenced by this source to \p newname */
	int rename (const std::string& name);

	virtual void close () = 0;

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
	bool        _file_is_new;
	uint16_t    _channel;
	bool        _within_session;
	std::string _origin;
	float       _gain;
};

} // namespace ARDOUR

#endif /* __ardour_filesource_h__ */

