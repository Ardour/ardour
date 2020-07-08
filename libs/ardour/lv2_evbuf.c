/*
  Copyright 2008-2012 David Robillard <http://drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "lv2/lv2plug.in/ns/ext/atom/atom.h"
#include "lv2/lv2plug.in/ns/ext/event/event.h"

#include "lv2_evbuf.h"

struct LV2_Evbuf_Impl {
	uint32_t          capacity;
	uint32_t          atom_Chunk;
	uint32_t          atom_Sequence;
	LV2_Atom_Sequence atom;
};

static inline uint32_t
lv2_evbuf_pad_size(uint32_t size)
{
	return (size + 7) & (~7);
}

LV2_Evbuf*
lv2_evbuf_new(uint32_t       capacity,
              uint32_t       atom_Chunk,
              uint32_t       atom_Sequence)
{
	// FIXME: memory must be 64-bit aligned
	LV2_Evbuf* evbuf = (LV2_Evbuf*)malloc(
		sizeof(LV2_Evbuf) + sizeof(LV2_Atom_Sequence) + capacity);
	evbuf->capacity      = capacity;
	evbuf->atom_Chunk    = atom_Chunk;
	evbuf->atom_Sequence = atom_Sequence;
	lv2_evbuf_reset(evbuf, true);
	return evbuf;
}

void
lv2_evbuf_free(LV2_Evbuf* evbuf)
{
	free(evbuf);
}

void
lv2_evbuf_reset(LV2_Evbuf* evbuf, bool input)
{
	if (input) {
		evbuf->atom.atom.size = sizeof(LV2_Atom_Sequence_Body);
		evbuf->atom.atom.type = evbuf->atom_Sequence;
	} else {
		evbuf->atom.atom.size = evbuf->capacity;
		evbuf->atom.atom.type = evbuf->atom_Chunk;
	}
}

uint32_t
lv2_evbuf_get_size(LV2_Evbuf* evbuf)
{
	assert(evbuf->atom.atom.type != evbuf->atom_Sequence
	       || evbuf->atom.atom.size >= sizeof(LV2_Atom_Sequence_Body));

	return evbuf->atom.atom.type == evbuf->atom_Sequence
		? evbuf->atom.atom.size - sizeof(LV2_Atom_Sequence_Body)
		: 0;
}

uint32_t
lv2_evbuf_get_capacity(LV2_Evbuf* evbuf)
{
	return evbuf->capacity;
}

void*
lv2_evbuf_get_buffer(LV2_Evbuf* evbuf)
{
	return &evbuf->atom;
}

LV2_Evbuf_Iterator
lv2_evbuf_begin(LV2_Evbuf* evbuf)
{
	LV2_Evbuf_Iterator iter = { evbuf, 0 };
	return iter;
}

LV2_Evbuf_Iterator
lv2_evbuf_end(LV2_Evbuf* evbuf)
{
	const uint32_t           size = lv2_evbuf_get_size(evbuf);
	const LV2_Evbuf_Iterator iter = { evbuf, lv2_evbuf_pad_size(size) };
	return iter;
}

bool
lv2_evbuf_is_valid(LV2_Evbuf_Iterator iter)
{
	return iter.offset < lv2_evbuf_get_size(iter.evbuf);
}

LV2_Evbuf_Iterator
lv2_evbuf_next(LV2_Evbuf_Iterator iter)
{
	if (!lv2_evbuf_is_valid(iter)) {
		return iter;
	}

	LV2_Evbuf* evbuf  = iter.evbuf;
	uint32_t   offset = iter.offset;
	uint32_t   size   =
	    ((LV2_Atom_Event*)((uintptr_t)(
	         (char*)LV2_ATOM_CONTENTS(LV2_Atom_Sequence, &evbuf->atom) +
	         offset)))
	        ->body.size;

	offset += lv2_evbuf_pad_size(sizeof(LV2_Atom_Event) + size);

	LV2_Evbuf_Iterator next = { evbuf, offset };
	return next;
}

bool
lv2_evbuf_get(LV2_Evbuf_Iterator iter,
              uint32_t*          samples,
              uint32_t*          subframes,
              uint32_t*          type,
              uint32_t*          size,
              uint8_t**          data)
{
	*samples = *subframes = *type = *size = 0;
	*data = NULL;

	if (!lv2_evbuf_is_valid(iter)) {
		return false;
	}

	LV2_Atom_Sequence* aseq = (LV2_Atom_Sequence*)&iter.evbuf->atom;
	LV2_Atom_Event*    aev  = (LV2_Atom_Event*)((uintptr_t)(
	    (char*)LV2_ATOM_CONTENTS(LV2_Atom_Sequence, aseq) + iter.offset));

	*samples    = aev->time.frames;
	*subframes = 0;
	*type      = aev->body.type;
	*size      = aev->body.size;
	*data      = (uint8_t*)LV2_ATOM_BODY(&aev->body);

	return true;
}

bool
lv2_evbuf_write(LV2_Evbuf_Iterator* iter,
                uint32_t            samples,
                uint32_t            subframes,
                uint32_t            type,
                uint32_t            size,
                const uint8_t*      data)
{
	LV2_Atom_Sequence* aseq = (LV2_Atom_Sequence*)&iter->evbuf->atom;

	if (iter->evbuf->capacity - sizeof(LV2_Atom) - aseq->atom.size
	    < sizeof(LV2_Atom_Event) + size) {
		return false;
	}

	LV2_Atom_Event* aev = (LV2_Atom_Event*)((uintptr_t)(
	    (char*)LV2_ATOM_CONTENTS(LV2_Atom_Sequence, aseq) + iter->offset));

	aev->time.frames = samples;
	aev->body.type   = type;
	aev->body.size   = size;
	memcpy(LV2_ATOM_BODY(&aev->body), data, size);

	size             = lv2_evbuf_pad_size(sizeof(LV2_Atom_Event) + size);
	aseq->atom.size += size;
	iter->offset    += size;

	return true;
}
