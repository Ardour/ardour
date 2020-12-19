/*
 * Copyright (C) 2014-2015 David Robillard <d@drobilla.net>
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

#ifndef __ardour_variant_h__
#define __ardour_variant_h__

#include <stdint.h>
#include <limits.h>

#include <algorithm>
#include <stdexcept>

#include "ardour/libardour_visibility.h"
#include "temporal/beats.h"
#include "pbd/compose.h"

namespace ARDOUR {

/** A value with dynamic type (tagged union). */
class LIBARDOUR_API Variant
{
public:
	enum Type {
		NOTHING, ///< Nothing (void)
		BEATS,   ///< Beats+ticks
		BOOL,    ///< Boolean
		DOUBLE,  ///< C double (64-bit IEEE-754)
		FLOAT,   ///< C float (32-bit IEEE-754)
		INT,     ///< Signed 32-bit int
		LONG,    ///< Signed 64-bit int
		PATH,    ///< File path string
		STRING,  ///< Raw string (no semantics)
		URI      ///< URI string
	};

	Variant() : _type(NOTHING) { _long = 0; }

	explicit Variant(bool    value) : _type(BOOL)    { _bool   = value; }
	explicit Variant(double  value) : _type(DOUBLE)  { _double = value; }
	explicit Variant(float   value) : _type(FLOAT)   { _float  = value; }
	explicit Variant(int32_t value) : _type(INT)     { _int    = value; }
	explicit Variant(int64_t value) : _type(LONG)    { _long   = value; }

	explicit Variant(const Temporal::Beats& beats)
		: _type(BEATS)
		, _beats(beats)
	{}

	/** Make a variant of a specific string type (string types only) */
	Variant(Type type, const std::string& value)
		: _type(type)
		, _string(value)
	{}

	/** Make a numeric variant from a double (numeric types only).
	 *
	 * If conversion is impossible, the variant will have type NOTHING.
	 */
	Variant(Type type, double value)
		: _type(type)
	{
		switch (type) {
		case BOOL:
			_bool = value != 0.0;
			break;
		case DOUBLE:
			_double = (double)value;
			break;
		case FLOAT:
			_float = (float)value;
			break;
		case INT:
			_int = (int32_t)lrint(std::max((double)INT32_MIN,
			                               std::min(value, (double)INT32_MAX)));
			break;
		case LONG:
			_long = (int64_t)lrint(std::max((double)INT64_MIN,
			                                std::min(value, (double)INT64_MAX)));
			break;
		case BEATS:
			_beats = Temporal::Beats::from_double (value);
			break;
		default:
			_type = NOTHING;
			_long = 0;
		}
	}

	/** Convert a numeric variant to a double. */
	double to_double() const {
		switch (_type) {
		case BOOL:   return _bool;
		case DOUBLE: return _double;
		case FLOAT:  return _float;
		case INT:    return _int;
		case LONG:   return _long;
		case BEATS:  return Temporal::DoubleableBeats (_beats).to_double();
		default:     return 0.0;
		}
	}

	bool   get_bool()   const { ensure_type(BOOL);   return _bool;   }
	double get_double() const { ensure_type(DOUBLE); return _double; }
	float  get_float()  const { ensure_type(FLOAT);  return _float;  }
	int    get_int()    const { ensure_type(INT);    return _int;    }
	long   get_long()   const { ensure_type(LONG);   return _long;   }

	bool   operator==(bool v)   const { return _type == BOOL   && _bool == v; }
	double operator==(double v) const { return _type == DOUBLE && _double == v; }
	float  operator==(float v)  const { return _type == FLOAT  && _float == v; }
	int    operator==(int v)    const { return _type == INT    && _int == v; }
	long   operator==(long v)   const { return _type == LONG   && _long == v; }

	Variant& operator=(bool v)   { _type = BOOL;   _bool = v;   return *this; }
	Variant& operator=(double v) { _type = DOUBLE; _double = v; return *this; }
	Variant& operator=(float v)  { _type = FLOAT;  _float = v;  return *this; }
	Variant& operator=(int v)    { _type = INT;    _int = v;    return *this; }
	Variant& operator=(long v)   { _type = LONG;   _long = v;   return *this; }

	const std::string& get_path()   const { ensure_type(PATH);   return _string; }
	const std::string& get_string() const { ensure_type(STRING); return _string; }
	const std::string& get_uri()    const { ensure_type(URI);    return _string; }

	bool operator==(const Variant& v) const {
		if (_type != v._type) {
			return false;
		}

		switch (_type) {
		case NOTHING: return true;
		case BEATS:   return _beats  == v._beats;
		case BOOL:    return _bool   == v._bool;
		case DOUBLE:  return _double == v._double;
		case FLOAT:   return _float  == v._float;
		case INT:     return _int    == v._int;
		case LONG:    return _long   == v._long;
		case PATH:
		case STRING:
		case URI:     return _string == v._string;
		}

		return false;
	}

	bool operator==(const Temporal::Beats& v) const {
		return _type == BEATS && _beats == v;
	}

	bool operator!() const { return _type == NOTHING; }

	Variant& operator=(Temporal::Beats v) {
		_type  = BEATS;
		_beats = v;
		return *this;
	}

	const Temporal::Beats& get_beats() const {
		ensure_type(BEATS); return _beats;
	}

	Type type() const { return _type; }

	static bool type_is_numeric(Type type) {
		switch (type) {
		case BOOL: case DOUBLE: case FLOAT: case INT: case LONG: case BEATS:
			return true;
		default:
			return false;
		}
	}

private:
	static const char* type_name(const Type type) {
		static const char* names[] = {
			"bool", "double", "float", "int", "long", "path", "string", "uri"
		};

		return names[type];
	}

	void ensure_type(const Type type) const {
		if (_type != type) {
			throw std::domain_error(
				string_compose("get_%1 called on %2 variant",
				               type_name(type), type_name(_type)));
		}
	}

	Type          _type;    ///< Type tag
	std::string   _string;  ///< PATH, STRING, URI
	Temporal::Beats _beats;   ///< BEATS

	// Union of all primitive numeric types
	union {
		bool    _bool;
		double  _double;
		float   _float;
		int32_t _int;
		int64_t _long;
	};
};

} // namespace ARDOUR

#endif // __ardour_variant_h__
