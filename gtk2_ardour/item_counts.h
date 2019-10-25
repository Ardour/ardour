/*
 * Copyright (C) 2014 David Robillard <d@drobilla.net>
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

#ifndef __ardour_item_counts_h__
#define __ardour_item_counts_h__

#include <cstddef>
#include <map>
#include <utility>

#include "ardour/data_type.h"
#include "evoral/Parameter.h"

/** A count of various GUI items.
 *
 * This is used to keep track of 'consumption' of a selection when pasting, but
 * may be useful elsewhere.
 */
class ItemCounts
{
public:
	ItemCounts() : _notes(0) {}

	size_t n_playlists(ARDOUR::DataType t) const { return get_n(t, _playlists); }
	size_t n_regions(ARDOUR::DataType t)   const { return get_n(t, _regions); }
	size_t n_lines(Evoral::Parameter t)    const { return get_n(t, _lines); }
	size_t n_notes()                       const { return _notes; }

	void increase_n_playlists(ARDOUR::DataType t, size_t delta=1) {
		increase_n(t, _playlists, delta);
	}

	void increase_n_regions(ARDOUR::DataType t, size_t delta=1) {
		increase_n(t, _regions, delta);
	}

	void increase_n_lines(Evoral::Parameter t, size_t delta=1) {
		increase_n(t, _lines, delta);
	}

	void increase_n_notes(size_t delta=1) { _notes += delta; }

private:
	template<typename Key>
	size_t
	get_n(const Key& key, const typename std::map<Key, size_t>& counts) const {
		typename std::map<Key, size_t>::const_iterator i = counts.find(key);
		return (i == counts.end()) ? 0 : i->second;
	}

	template<typename Key>
	void
	increase_n(const Key& key, typename std::map<Key, size_t>& counts, size_t delta) {
		typename std::map<Key, size_t>::iterator i = counts.find(key);
		if (i != counts.end()) {
			i->second += delta;
		} else {
			counts.insert(std::make_pair(key, delta));
		}
	}

	std::map<ARDOUR::DataType,  size_t> _playlists;
	std::map<ARDOUR::DataType,  size_t> _regions;
	std::map<Evoral::Parameter, size_t> _lines;
	size_t                              _notes;
};

#endif /* __ardour_item_counts_h__ */
