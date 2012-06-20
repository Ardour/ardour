/* 
    Copyright (C) 2006 Paul Davis

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

    $Id$
*/

#ifndef __pbd_enumwriter_h__
#define __pbd_enumwriter_h__

#include <map>
#include <string>
#include <vector>
#include <exception>
#include <sstream>

namespace PBD {

class unknown_enumeration : public std::exception {
  public:
	unknown_enumeration (std::string const & e) throw() {
		std::stringstream s;
		s << "unknown enumerator " << e << " in PBD::EnumWriter";
		_message = s.str ();
	}

	~unknown_enumeration () throw() {}
	
	virtual const char *what() const throw() {
		return _message.c_str();
	}

private:
	std::string _message;
};

class EnumWriter {
  public:
	static EnumWriter& instance();
	static void destroy();

	void register_distinct (std::string type, std::vector<int>, std::vector<std::string>);
	void register_bits     (std::string type, std::vector<int>, std::vector<std::string>);

	std::string write (std::string type, int value);
	int         read  (std::string type, std::string value);

	void add_to_hack_table (std::string str, std::string hacked_str);

  private:
	EnumWriter ();
	~EnumWriter ();

	struct EnumRegistration {
	    std::vector<int> values;
	    std::vector<std::string> names;
	    bool bitwise;

	    EnumRegistration() {}
	    EnumRegistration (std::vector<int>& v, std::vector<std::string>& s, bool b) 
		    : values (v), names (s), bitwise (b) {}
	};

	typedef std::map<std::string, EnumRegistration> Registry;
	Registry registry;

	std::string write_bits (EnumRegistration&, int value);
	std::string write_distinct (EnumRegistration&, int value);

	int read_bits (EnumRegistration&, std::string value);
	int read_distinct (EnumRegistration&, std::string value);

	static EnumWriter* _instance;
	static std::map<std::string,std::string> hack_table;


        int validate (EnumRegistration& er, int value);
};

}

#define enum_2_string(e) (PBD::EnumWriter::instance().write (typeid(e).name(), e))
#define string_2_enum(str,e) (PBD::EnumWriter::instance().read (typeid(e).name(), (str)))

#endif /*  __pbd_enumwriter_h__ */
