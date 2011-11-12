/*
  Copyright (C) 2011 Paul Davis
  Author: David Robillard

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

#ifndef __ardour_lv2_state_h__
#define __ardour_lv2_state_h__

#include <stdint.h>
#include <stdlib.h>

#include <map>
#include <string>

#include "pbd/error.h"

#include "ardour/uri_map.h"
#include "lv2/lv2plug.in/ns/ext/state/state.h"
#include "rdff.h"

namespace ARDOUR {

struct LV2State {
	LV2State(URIMap& map) : uri_map(map) {}

	struct Value {
		inline Value(uint32_t k, const void* v, size_t s, uint32_t t, uint32_t f)
			: key(k), value(v), size(s), type(t), flags(f)
		{}

		const uint32_t key;
		const void*    value;
		const size_t   size;
		const uint32_t type;
		const uint32_t flags;
	};

	typedef std::map<uint32_t, std::string> URIs;
	typedef std::map<uint32_t, Value>       Values;

	uint32_t file_id_to_runtime_id(uint32_t file_id) const {
		URIs::const_iterator i = uris.find(file_id);
		if (i == uris.end()) {
			PBD::error << "LV2 state refers to undefined URI ID" << endmsg;
			return 0;
		}
		return uri_map.uri_to_id(NULL, i->second.c_str());
	}

	int add_uri(uint32_t file_id, const char* str) {
		// TODO: check for clashes (invalid file)
		uris.insert(std::make_pair(file_id, str));
		return 0;
	}

	int add_value(uint32_t    file_key,
	              const void* value,
	              size_t      size,
	              uint32_t    file_type,
	              uint32_t    flags) {
		const uint32_t key  = file_id_to_runtime_id(file_key);
		const uint32_t type = file_id_to_runtime_id(file_type);
		if (!key || !type) {
			return 1;
		}

		Values::const_iterator i = values.find(key);
		if (i != values.end()) {
			PBD::error << "LV2 state contains duplicate keys" << endmsg;
			return 1;
		} else {
			void* value_copy = malloc(size);
			memcpy(value_copy, value, size); // FIXME: leak
			values.insert(
				std::make_pair(key,
				               Value(key, value_copy, size, type, flags)));
			return 0;
		}
	}

	void read(RDFF file) {
		RDFFChunk* chunk = (RDFFChunk*)malloc(sizeof(RDFFChunk));
		chunk->size = 0;
		while (!rdff_read_chunk(file, &chunk)) {
			if (rdff_chunk_is_uri(chunk)) {
				RDFFURIChunk* body = (RDFFURIChunk*)chunk->data;
				add_uri(body->id, body->uri);
			} else if (rdff_chunk_is_triple(chunk)) {
				RDFFTripleChunk* body = (RDFFTripleChunk*)chunk->data;
				add_value(body->predicate,
				          body->object,
				          body->object_size,
				          body->object_type,
				          LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE);
			}
		}
		free(chunk);
	}

	void write(RDFF file) {
		// Write all referenced URIs to state file
		for (URIs::const_iterator i = uris.begin(); i != uris.end(); ++i) {
			rdff_write_uri(file,
			               i->first,
			               i->second.length(),
			               i->second.c_str());
		}

		// Write all values to state file
		for (Values::const_iterator i = values.begin(); i != values.end(); ++i) {
			const uint32_t         key = i->first;
			const LV2State::Value& val = i->second;
			rdff_write_triple(file,
			                  0,
			                  key,
			                  val.type,
			                  val.size,
			                  val.value);
		}
	}

	URIMap& uri_map;
	URIs    uris;
	Values  values;
};

} // namespace ARDOUR

#endif /* __ardour_lv2_state_h__ */
