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

#ifndef __ardour_paste_context_h__
#define __ardour_paste_context_h__

#include "item_counts.h"

class PasteContext
{
public:
	PasteContext(unsigned count, float times, ItemCounts counts, bool greedy)
		: count(count)
		, times(times)
		, counts(counts)
		, greedy(greedy)
	{}

	unsigned   count;   ///< Number of previous pastes to the same position
	float      times;   ///< Number of times to paste
	ItemCounts counts;  ///< Count of consumed selection items
	bool       greedy;  ///< If true, greedily steal items that don't match
};

#endif /* __ardour_paste_context_h__ */
