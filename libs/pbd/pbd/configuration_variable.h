/*
 * Copyright (C) 1999-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __libpbd_configuration_variable_h__
#define __libpbd_configuration_variable_h__

#include <string>

#include "pbd/xml++.h"
#include "pbd/string_convert.h"
#include "pbd/libpbd_visibility.h"

namespace PBD {

class LIBPBD_API ConfigVariableBase {
  public:

	ConfigVariableBase (std::string str) : _name (str) {}
	virtual ~ConfigVariableBase() {}

	std::string name () const { return _name; }
	void add_to_node (XMLNode&);
	bool set_from_node (XMLNode const &);

	virtual std::string get_as_string () const = 0;
	virtual void set_from_string (std::string const &) = 0;

  protected:
	std::string _name;

	void notify ();
	void miss ();
};

template<class T>
class /*LIBPBD_API*/ ConfigVariable : public ConfigVariableBase
{
  public:

	ConfigVariable (std::string str) : ConfigVariableBase (str) {}
	ConfigVariable (std::string str, T val) : ConfigVariableBase (str), value (val) {}

	T get() const {
		return value;
	}

	std::string get_as_string () const {
		return to_string<T>(value);
	}

	virtual bool set (T val) {
		if (val == value) {
			miss ();
			return false;
		}
		value = val;
		notify ();
		return true;
	}

	virtual void set_from_string (std::string const & s) {
		value = string_to<T>(s);
	}

  protected:
	virtual T get_for_save() { return value; }
	T value;
};

/** Specialisation of ConfigVariable for std::string to cope with whitespace properly */
template<>
class /*LIBPBD_API*/ ConfigVariable<std::string> : public ConfigVariableBase
{
  public:

	ConfigVariable (std::string str) : ConfigVariableBase (str) {}
	ConfigVariable (std::string str, std::string val) : ConfigVariableBase (str), value (val) {}

	std::string get() const {
		return value;
	}

	std::string get_as_string () const {
		return value;
	}

	virtual bool set (std::string val) {
		if (val == value) {
			miss ();
			return false;
		}
		value = val;
		notify ();
		return true;
	}

	virtual void set_from_string (std::string const & s) {
		value = s;
	}

  protected:
	virtual std::string get_for_save() { return value; }
	std::string value;
};

template<class T>
class /*LIBPBD_API*/ ConfigVariableWithMutation : public ConfigVariable<T>
{
  public:
	ConfigVariableWithMutation (std::string name, T val, T (*m)(T))
		: ConfigVariable<T> (name, m (val)), unmutated_value (val), mutator (m) {}

	bool set (T val) {
		if (unmutated_value != val) {
			unmutated_value = val;
			return ConfigVariable<T>::set (mutator (val));
		}
		return false;
	}

	void set_from_string (std::string const & s) {
		set (string_to<T>(s));
	}

  protected:
	virtual T get_for_save() { return unmutated_value; }
	T unmutated_value;
	T (*mutator)(T);
};

template<>
class /*LIBPBD_API*/ ConfigVariableWithMutation<std::string> : public ConfigVariable<std::string>
{
  public:
	ConfigVariableWithMutation (std::string name, std::string val, std::string (*m)(std::string))
		: ConfigVariable<std::string> (name, val), mutator (m) {}

	bool set (std::string val) {
		if (unmutated_value != val) {
			unmutated_value = val;
			return ConfigVariable<std::string>::set (mutator (val));
		}
		return false;
	}

	void set_from_string (std::string const & s) {
		set (s);
	}

  protected:
	virtual std::string get_for_save() { return unmutated_value; }
	std::string unmutated_value;
	std::string (*mutator)(std::string);
};

}

#endif /* __libpbd_configuration_variable_h__ */
