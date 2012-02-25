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

/**
   @file util.h Helper functions for the LV2 Atom extension.

   Note these functions are all static inline, do not take their address.

   This header is non-normative, it is provided for convenience.
*/

#ifndef LV2_ATOM_UTIL_H
#define LV2_ATOM_UTIL_H

#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#include "lv2/lv2plug.in/ns/ext/atom/atom.h"

#ifdef __cplusplus
extern "C" {
#else
#    include <stdbool.h>
#endif

/** Pad a size to 64 bits. */
static inline uint32_t
lv2_atom_pad_size(uint32_t size)
{
	return (size + 7) & (~7);
}

/** Return the total size of @p atom, including the header. */
static inline uint32_t
lv2_atom_total_size(const LV2_Atom* atom)
{
	return sizeof(LV2_Atom) + atom->size;
}

/** Return true iff @p atom is null. */
static inline bool
lv2_atom_is_null(const LV2_Atom* atom)
{
	return !atom || (atom->type == 0 && atom->size == 0);
}

/** Return true iff @p a is equal to @p b. */
static inline bool
lv2_atom_equals(const LV2_Atom* a, const LV2_Atom* b)
{
	return (a == b) || ((a->type == b->type) &&
	                    (a->size == b->size) &&
	                    !memcmp(a + 1, b + 1, a->size));
}

/**
   @name Sequence Iterator
   @{
*/

/** An iterator over the elements of an LV2_Atom_Sequence. */
typedef LV2_Atom_Event* LV2_Atom_Sequence_Iter;

/** Get an iterator pointing to the first element in a Sequence body. */
static inline LV2_Atom_Sequence_Iter
lv2_sequence_body_begin(const LV2_Atom_Sequence_Body* body)
{
	return (LV2_Atom_Sequence_Iter)(body + 1);
}

/** Get an iterator pointing to the first element in a Sequence. */
static inline LV2_Atom_Sequence_Iter
lv2_sequence_begin(const LV2_Atom_Sequence* seq)
{
	return (LV2_Atom_Sequence_Iter)(seq + 1);
}

/** Return true iff @p i has reached the end of @p body. */
static inline bool
lv2_sequence_body_is_end(const LV2_Atom_Sequence_Body* body,
                         uint32_t                      size,
                         LV2_Atom_Sequence_Iter        i)
{
	return (uint8_t*)i >= ((uint8_t*)body + size);
}

/** Return true iff @p i has reached the end of @p seq. */
static inline bool
lv2_sequence_is_end(const LV2_Atom_Sequence* seq, LV2_Atom_Sequence_Iter i)
{
	return (uint8_t*)i >= ((uint8_t*)seq + sizeof(LV2_Atom) + seq->atom.size);
}

/** Return an iterator to the element following @p i. */
static inline LV2_Atom_Sequence_Iter
lv2_sequence_iter_next(const LV2_Atom_Sequence_Iter i)
{
	return (LV2_Atom_Sequence_Iter)((uint8_t*)i
	                                + sizeof(LV2_Atom_Event)
	                                + lv2_atom_pad_size(i->body.size));
}

/** Return the element pointed to by @p i. */
static inline LV2_Atom_Event*
lv2_sequence_iter_get(LV2_Atom_Sequence_Iter i)
{
	return (LV2_Atom_Event*)i;
}

/**
   A macro for iterating over all events in a Sequence.
   @param sequence The sequence to iterate over
   @param iter The name of the iterator

   This macro is used similarly to a for loop (which it expands to), e.g.:
   @code
   LV2_SEQUENCE_FOREACH(sequence, i) {
       LV2_Atom_Event* ev = lv2_sequence_iter_get(i);
       // Do something with ev here...
   }
   @endcode
*/
#define LV2_SEQUENCE_FOREACH(sequence, iter) \
	for (LV2_Atom_Sequence_Iter (iter) = lv2_sequence_begin(sequence); \
	     !lv2_sequence_is_end(sequence, (iter)); \
	     (iter) = lv2_sequence_iter_next(iter))

/** A version of LV2_SEQUENCE_FOREACH for when only the body is available. */
#define LV2_SEQUENCE_BODY_FOREACH(body, size, iter) \
	for (LV2_Atom_Sequence_Iter (iter) = lv2_sequence_body_begin(body); \
	     !lv2_sequence_body_is_end(body, size, (iter)); \
	     (iter) = lv2_sequence_iter_next(iter))

/**
   @}
   @name Tuple Iterator
   @{
*/

/** An iterator over the elements of an LV2_Atom_Tuple. */
typedef LV2_Atom* LV2_Atom_Tuple_Iter;

/** Get an iterator pointing to the first element in @p tup. */
static inline LV2_Atom_Tuple_Iter
lv2_tuple_begin(const LV2_Atom_Tuple* tup)
{
	return (LV2_Atom_Tuple_Iter)(LV2_ATOM_BODY(tup));
}

/** Return true iff @p i has reached the end of @p body. */
static inline bool
lv2_atom_tuple_body_is_end(const void*         body,
                           uint32_t            size,
                           LV2_Atom_Tuple_Iter i)
{
	return (uint8_t*)i >= ((uint8_t*)body + size);
}

/** Return true iff @p i has reached the end of @p tup. */
static inline bool
lv2_tuple_is_end(const LV2_Atom_Tuple* tup, LV2_Atom_Tuple_Iter i)
{
	return lv2_atom_tuple_body_is_end(LV2_ATOM_BODY(tup), tup->atom.size, i);
}

/** Return an iterator to the element following @p i. */
static inline LV2_Atom_Tuple_Iter
lv2_tuple_iter_next(const LV2_Atom_Tuple_Iter i)
{
	return (LV2_Atom_Tuple_Iter)(
		(uint8_t*)i + sizeof(LV2_Atom) + lv2_atom_pad_size(i->size));
}

/** Return the element pointed to by @p i. */
static inline LV2_Atom*
lv2_tuple_iter_get(LV2_Atom_Tuple_Iter i)
{
	return (LV2_Atom*)i;
}

/**
   A macro for iterating over all properties of a Tuple.
   @param tuple The tuple to iterate over
   @param iter The name of the iterator

   This macro is used similarly to a for loop (which it expands to), e.g.:
   @code
   LV2_TUPLE_FOREACH(tuple, i) {
       LV2_Atom* elem = lv2_tuple_iter_get(i);
       // Do something with elem here...
   }
   @endcode
*/
#define LV2_TUPLE_FOREACH(tuple, iter) \
	for (LV2_Atom_Tuple_Iter (iter) = lv2_tuple_begin(tuple); \
	     !lv2_tuple_is_end(tuple, (iter)); \
	     (iter) = lv2_tuple_iter_next(iter))

/** A version of LV2_TUPLE_FOREACH for when only the body is available. */
#define LV2_TUPLE_BODY_FOREACH(body, size, iter) \
	for (LV2_Atom_Tuple_Iter (iter) = (LV2_Atom_Tuple_Iter)body; \
	     !lv2_atom_tuple_body_is_end(body, size, (iter)); \
	     (iter) = lv2_tuple_iter_next(iter))

/**
   @}
   @name Object Iterator
   @{
*/

/** An iterator over the properties of an LV2_Atom_Object. */
typedef LV2_Atom_Property_Body* LV2_Atom_Object_Iter;

static inline LV2_Atom_Object_Iter
lv2_object_body_begin(const LV2_Atom_Object_Body* body)
{
	return (LV2_Atom_Object_Iter)(body + 1);
}

/** Get an iterator pointing to the first property in @p obj. */
static inline LV2_Atom_Object_Iter
lv2_object_begin(const LV2_Atom_Object* obj)
{
	return (LV2_Atom_Object_Iter)(obj + 1);
}

static inline bool
lv2_atom_object_body_is_end(const LV2_Atom_Object_Body* body,
                            uint32_t                    size,
                            LV2_Atom_Object_Iter        i)
{
	return (uint8_t*)i >= ((uint8_t*)body + size);
}

/** Return true iff @p i has reached the end of @p obj. */
static inline bool
lv2_object_is_end(const LV2_Atom_Object* obj, LV2_Atom_Object_Iter i)
{
	return (uint8_t*)i >= ((uint8_t*)obj + sizeof(LV2_Atom) + obj->atom.size);
}

/** Return an iterator to the property following @p i. */
static inline LV2_Atom_Object_Iter
lv2_object_iter_next(const LV2_Atom_Object_Iter i)
{
	const LV2_Atom* const value = (LV2_Atom*)((uint8_t*)i + sizeof(i));
	return (LV2_Atom_Object_Iter)((uint8_t*)i
	                              + sizeof(LV2_Atom_Property_Body)
	                              + lv2_atom_pad_size(value->size));
}

/** Return the property pointed to by @p i. */
static inline LV2_Atom_Property_Body*
lv2_object_iter_get(LV2_Atom_Object_Iter i)
{
	return (LV2_Atom_Property_Body*)i;
}

/**
   A macro for iterating over all properties of an Object.
   @param object The object to iterate over
   @param iter The name of the iterator

   This macro is used similarly to a for loop (which it expands to), e.g.:
   @code
   LV2_OBJECT_FOREACH(object, i) {
       LV2_Atom_Property_Body* prop = lv2_object_iter_get(i);
       // Do something with prop here...
   }
   @endcode
*/
#define LV2_OBJECT_FOREACH(object, iter) \
	for (LV2_Atom_Object_Iter (iter) = lv2_object_begin(object); \
	     !lv2_object_is_end(object, (iter)); \
	     (iter) = lv2_object_iter_next(iter))

/** A version of LV2_OBJECT_FOREACH for when only the body is available. */
#define LV2_OBJECT_BODY_FOREACH(body, size, iter) \
	for (LV2_Atom_Object_Iter (iter) = lv2_object_body_begin(body); \
	     !lv2_atom_object_body_is_end(body, size, (iter)); \
	     (iter) = lv2_object_iter_next(iter))

/**
   @}
   @name Object Query
   @{
*/

/** A single entry in an Object query. */
typedef struct {
	uint32_t         key;    /**< Key to query (input set by user) */
	const LV2_Atom** value;  /**< Found value (output set by query function) */
} LV2_Atom_Object_Query;

static const LV2_Atom_Object_Query LV2_OBJECT_QUERY_END = { 0, NULL };

/**
   Get an object's values for various keys.

   The value pointer of each item in @p query will be set to the location of
   the corresponding value in @p object.  Every value pointer in @p query MUST
   be initialised to NULL.  This function reads @p object in a single linear
   sweep.  By allocating @p query on the stack, objects can be "queried"
   quickly without allocating any memory.  This function is realtime safe.

   This function can only do "flat" queries, it is not smart enough to match
   variables in nested objects.

   For example:
   @code
   const LV2_Atom* name = NULL;
   const LV2_Atom* age  = NULL;
   LV2_Atom_Object_Query q[] = {
       { urids.eg_name, &name },
       { urids.eg_age,  &age },
       LV2_OBJECT_QUERY_END
   };
   lv2_object_query(obj, q);
   // name and age are now set to the appropriate values in obj, or NULL.
   @endcode
*/
static inline int
lv2_object_query(const LV2_Atom_Object* object, LV2_Atom_Object_Query* query)
{
	int matches   = 0;
	int n_queries = 0;

	/* Count number of query keys so we can short-circuit when done */
	for (LV2_Atom_Object_Query* q = query; q->key; ++q) {
		++n_queries;
	}

	LV2_OBJECT_FOREACH(object, o) {
		const LV2_Atom_Property_Body* prop = lv2_object_iter_get(o);
		for (LV2_Atom_Object_Query* q = query; q->key; ++q) {
			if (q->key == prop->key && !*q->value) {
				*q->value = &prop->value;
				if (++matches == n_queries) {
					return matches;
				}
				break;
			}
		}
	}
	return matches;
}

/**
   Variable argument version of lv2_object_get().

   This is nicer-looking in code, but a bit more error-prone since it is not
   type safe and the argument list must be terminated.

   The arguments should be a series of uint32_t key and const LV2_Atom** value
   pairs, terminated by a zero key.  The value pointers MUST be initialized to
   NULL.  For example:

   @code
   const LV2_Atom* name = NULL;
   const LV2_Atom* age  = NULL;
   lv2_object_get(obj,
                  uris.name_key, &name,
                  uris.age_key,  &age,
                  0);
   @endcode
*/
static inline int
lv2_object_get(const LV2_Atom_Object* object, ...)
{
	int matches   = 0;
	int n_queries = 0;

	/* Count number of keys so we can short-circuit when done */
	va_list args;
	va_start(args, object);
	for (n_queries = 0; va_arg(args, uint32_t); ++n_queries) {
		if (!va_arg(args, const LV2_Atom**)) {
			return -1;
		}
	}
	va_end(args);

	LV2_OBJECT_FOREACH(object, o) {
		const LV2_Atom_Property_Body* prop = lv2_object_iter_get(o);
		va_start(args, object);
		for (int i = 0; i < n_queries; ++i) {
			uint32_t         qkey = va_arg(args, uint32_t);
			const LV2_Atom** qval = va_arg(args, const LV2_Atom**);
			if (qkey == prop->key && !*qval) {
				*qval = &prop->value;
				if (++matches == n_queries) {
					return matches;
				}
				break;
			}
		}
		va_end(args);
	}
	return matches;
}

/**
   @}
*/

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* LV2_ATOM_UTIL_H */
