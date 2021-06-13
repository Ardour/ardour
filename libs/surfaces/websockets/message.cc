/*
 * Copyright (C) 2020 Luciano Iam <oss@lucianoiam.com>
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

#ifndef NDEBUG
#include <iostream>
#endif

#include <boost/lexical_cast.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <sstream>

#include "message.h"
#include "json.h"

// JSON does not support Infinity or NaN
#define XSTR(s) STR (s)
#define STR(s) #s
#define JSON_INF 1.0e+128
#define JSON_INF_STR XSTR (JSON_INF)

using namespace ArdourSurface;

namespace pt = boost::property_tree;

NodeStateMessage::NodeStateMessage (const NodeState& state)
    : _valid (true)
    , _state (state)
{
	_write = state.n_val () > 0;
}

NodeStateMessage::NodeStateMessage (void* buf, size_t len)
    : _valid (false)
    , _write (false)
{
	try {
		std::string s (static_cast<char*> (buf), len);

		std::istringstream is (s);
		pt::ptree          root;
		pt::read_json (is, root);

		_state = NodeState (root.get<std::string> ("node"));

		pt::ptree addr = root.get_child ("addr", pt::ptree ());

		for (pt::ptree::iterator it = addr.begin (); it != addr.end (); ++it) {
			// throws if datatype not uint32_t
			_state.add_addr (boost::lexical_cast<uint32_t> (it->second.data ()));
		}

		pt::ptree val = root.get_child ("val", pt::ptree ());

		for (pt::ptree::iterator it = val.begin (); it != val.end (); ++it) {
			std::string val = it->second.data ();

			try {
				_state.add_val (boost::lexical_cast<int> (val));
			} catch (const boost::bad_lexical_cast&) {
				try {
					double d = boost::lexical_cast<double> (val);
					if (d >= JSON_INF) {
						d = std::numeric_limits<double>::infinity ();
					} else if (d <= -JSON_INF) {
						d = -std::numeric_limits<double>::infinity ();
					}
					_state.add_val (d);
				} catch (const boost::bad_lexical_cast&) {
					if (val == "false") {
						_state.add_val (false);
					} else if (val == "true") {
						_state.add_val (true);
					} else {
						_state.add_val (val);
					}
				}
			}
		}

		if (_state.n_val () > 0) {
			_write = true;
		}

		_valid = true;

	} catch (const std::exception& exc) {
#ifndef NDEBUG
		std::cerr << "cannot parse message - " << exc.what () << std::endl;
#endif
	}
}

size_t
NodeStateMessage::serialize (void* buf, size_t len) const
{
	// boost json writes all values as strings, we do not want that

	if (len == 0) {
		return -1;
	}

	std::stringstream ss;

	ss << "{\"node\":\"" << _state.node () << "\"";

	int n_addr = _state.n_addr ();

	if (n_addr > 0) {
		ss << ",\"addr\":[";

		for (int i = 0; i < n_addr; i++) {
			if (i > 0) {
				ss << ',';
			}

			ss << _state.nth_addr (i);
		}

		ss << "]";
	}

	int n_val = _state.n_val ();

	if (n_val > 0) {
		ss << ",\"val\":[";

		for (int i = 0; i < n_val; i++) {
			if (i > 0) {
				ss << ',';
			}

			TypedValue val = _state.nth_val (i);

			switch (val.type ()) {
				case TypedValue::Empty:
					ss << "null";
					break;
				case TypedValue::Bool:
					ss << (static_cast<bool> (val) ? "true" : "false");
					break;
				case TypedValue::Int:
					ss << static_cast<int> (val);
					break;
				case TypedValue::Double: {
					double d = static_cast<double> (val);
					if (d == std::numeric_limits<double>::infinity ()) {
						ss << JSON_INF_STR;
					} else if (d == -std::numeric_limits<double>::infinity ()) {
						ss << "-" JSON_INF_STR;
					} else {
						ss << d;
					}
					break;
				}
				case TypedValue::String:
					ss << '"' << WebSocketsJSON::escape (static_cast<std::string> (val)) << '"';
					break;
				default:
					break;
			}
		}

		ss << "]";
	}

	ss << '}';

	std::string s     = ss.str ();
	const char* cs    = s.c_str ();
	size_t      cs_sz = strlen (cs);

	if (len < cs_sz) {
		return -1;
	}

	memcpy (buf, cs, cs_sz);

	return cs_sz;
}
