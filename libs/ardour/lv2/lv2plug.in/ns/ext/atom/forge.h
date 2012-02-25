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
   @file forge.h An API for constructing LV2 atoms.

   This file provides an API for constructing Atoms which makes it relatively
   simple to build nested atoms of arbitrary complexity without requiring
   dynamic memory allocation.

   The API is based on successively appending the appropriate pieces to build a
   complete Atom.  The size of containers is automatically updated.  Functions
   that begin a container return (via their frame argument) a stack frame which
   must be popped when the container is finished.

   All output is written to a user-provided buffer or sink function.  This
   makes it popssible to create create atoms on the stack, on the heap, in LV2
   port buffers, in a ringbuffer, or elsewhere, all using the same API.

   This entire API is realtime safe if used with a buffer or a realtime safe
   sink, except lv2_atom_forge_init() which is only realtime safe if the URI
   map function is.

   Note these functions are all static inline, do not take their address.

   This header is non-normative, it is provided for convenience.
*/

#ifndef LV2_ATOM_FORGE_H
#define LV2_ATOM_FORGE_H

#include <assert.h>

#include "lv2/lv2plug.in/ns/ext/atom/atom.h"
#include "lv2/lv2plug.in/ns/ext/atom/util.h"
#include "lv2/lv2plug.in/ns/ext/urid/urid.h"

#ifdef __cplusplus
extern "C" {
#else
#    include <stdbool.h>
#endif

/** Handle for LV2_Atom_Forge_Sink. */
typedef void* LV2_Atom_Forge_Sink_Handle;

/** Sink function for writing output.  See lv2_atom_forge_set_sink(). */
typedef void* (*LV2_Atom_Forge_Sink)(LV2_Atom_Forge_Sink_Handle handle,
                                     const void*                buf,
                                     uint32_t                   size);

/** A stack frame used for keeping track of nested Atom containers. */
typedef struct _LV2_Atom_Forge_Frame {
	struct _LV2_Atom_Forge_Frame* parent;
	LV2_Atom*                     atom;
} LV2_Atom_Forge_Frame;

/** A "forge" for creating atoms by appending to a buffer. */
typedef struct {
	uint8_t* buf;
	uint32_t offset;
	uint32_t size;

	LV2_Atom_Forge_Sink        sink;
	LV2_Atom_Forge_Sink_Handle handle;

	LV2_Atom_Forge_Frame* stack;

	LV2_URID Blank;
	LV2_URID Bool;
	LV2_URID Double;
	LV2_URID Float;
	LV2_URID Int32;
	LV2_URID Int64;
	LV2_URID Literal;
	LV2_URID Path;
	LV2_URID Property;
	LV2_URID Resource;
	LV2_URID Sequence;
	LV2_URID String;
	LV2_URID Tuple;
	LV2_URID URI;
	LV2_URID URID;
	LV2_URID Vector;
} LV2_Atom_Forge;

/**
   Push a stack frame.
   This is done automatically by container functions (which take a stack frame
   pointer), but may be called by the user to push the top level container when
   writing to an existing Atom.
*/
static inline LV2_Atom*
lv2_atom_forge_push(LV2_Atom_Forge*       forge,
                    LV2_Atom_Forge_Frame* frame,
                    LV2_Atom*             atom)
{
	frame->parent = forge->stack;
	frame->atom   = atom;
	forge->stack  = frame;
	return atom;
}

/** Pop a stack frame.  This must be called when a container is finished. */
static inline void
lv2_atom_forge_pop(LV2_Atom_Forge* forge, LV2_Atom_Forge_Frame* frame)
{
	assert(frame == forge->stack);
	forge->stack = frame->parent;
}

/** Set the output buffer where @p forge will write atoms. */
static inline void
lv2_atom_forge_set_buffer(LV2_Atom_Forge* forge, uint8_t* buf, size_t size)
{
	forge->buf    = buf;
	forge->size   = size;
	forge->offset = 0;
	forge->sink   = NULL;
	forge->handle = NULL;
}

/**
   Set the sink function where @p forge will write output.

   The return value of forge functions is a pointer to the written data, which
   is used for updating parent sizes.  To enable this, the sink function must
   return a valid pointer to a contiguous LV2_Atom header.  For ringbuffers,
   this should be possible as long as the size of the buffer is a multiple of
   sizeof(LV2_Atom), since atoms are always aligned.  When using a ringbuffer,
   the returned pointers may not point to a complete atom (including body).
   The user must take care to only use these return values in a way compatible
   with the sink used.
*/
static inline void
lv2_atom_forge_set_sink(LV2_Atom_Forge*            forge,
                        LV2_Atom_Forge_Sink        sink,
                        LV2_Atom_Forge_Sink_Handle handle)
{
	forge->buf    = NULL;
	forge->size   = forge->offset = 0;
	forge->sink   = sink;
	forge->handle = handle;
}

/**
   Initialise @p forge.

   URIs will be mapped using @p map and stored, a reference to @p map itself is
   not held.
*/
static inline void
lv2_atom_forge_init(LV2_Atom_Forge* forge, LV2_URID_Map* map)
{
	lv2_atom_forge_set_buffer(forge, NULL, 0);
	forge->stack    = NULL;
	forge->Blank    = map->map(map->handle, LV2_ATOM_URI "#Blank");
	forge->Bool     = map->map(map->handle, LV2_ATOM_URI "#Bool");
	forge->Double   = map->map(map->handle, LV2_ATOM_URI "#Double");
	forge->Float    = map->map(map->handle, LV2_ATOM_URI "#Float");
	forge->Int32    = map->map(map->handle, LV2_ATOM_URI "#Int32");
	forge->Int64    = map->map(map->handle, LV2_ATOM_URI "#Int64");
	forge->Literal  = map->map(map->handle, LV2_ATOM_URI "#Literal");
	forge->Path     = map->map(map->handle, LV2_ATOM_URI "#Path");
	forge->Property = map->map(map->handle, LV2_ATOM_URI "#Property");
	forge->Resource = map->map(map->handle, LV2_ATOM_URI "#Resource");
	forge->Sequence = map->map(map->handle, LV2_ATOM_URI "#Sequence");
	forge->String   = map->map(map->handle, LV2_ATOM_URI "#String");
	forge->Tuple    = map->map(map->handle, LV2_ATOM_URI "#Tuple");
	forge->URI      = map->map(map->handle, LV2_ATOM_URI "#URI");
	forge->URID     = map->map(map->handle, LV2_ATOM_URI "#URID");
	forge->Vector   = map->map(map->handle, LV2_ATOM_URI "#Vector");
}

/**
   Write raw output.  This is used internally, but is also useful for writing
   atom types not explicitly supported by the forge API.  Note the caller is
   responsible for ensuring the output is approriately padded.
*/
static inline void*
lv2_atom_forge_raw(LV2_Atom_Forge* forge, const void* data, uint32_t size)
{
	uint8_t* out = NULL;
	if (forge->sink) {
		out = forge->sink(forge->handle, data, size);
	} else {
		out = forge->buf + forge->offset;
		if (forge->offset + size > forge->size) {
			return NULL;
		}
		forge->offset += size;
		memcpy(out, data, size);
	}
	if (out) {
		for (LV2_Atom_Forge_Frame* f = forge->stack; f; f = f->parent) {
			f->atom->size += size;
		}
	}
	return out;
}

/** Pad output accordingly so next write is 64-bit aligned. */
static inline void
lv2_atom_forge_pad(LV2_Atom_Forge* forge, uint32_t written)
{
	const uint64_t pad      = 0;
	const uint32_t pad_size = lv2_atom_pad_size(written) - written;
	lv2_atom_forge_raw(forge, &pad, pad_size);
}

/** Write raw output, padding to 64-bits as necessary. */
static inline void*
lv2_atom_forge_write(LV2_Atom_Forge* forge, const void* data, uint32_t size)
{
	void* out = lv2_atom_forge_raw(forge, data, size);
	if (out) {
		lv2_atom_forge_pad(forge, size);
	}
	return out;
}

/** Write an atom:Atom header. */
static inline LV2_Atom*
lv2_atom_forge_atom(LV2_Atom_Forge* forge, uint32_t size, uint32_t type)
{
	const LV2_Atom a = { size, type };
	return (LV2_Atom*)lv2_atom_forge_raw(forge, &a, sizeof(a));
}

/** Write an atom:Int32. */
static inline LV2_Atom_Int32*
lv2_atom_forge_int32(LV2_Atom_Forge* forge, int32_t val)
{
	const LV2_Atom_Int32 a = { { sizeof(val), forge->Int32 }, val };
	return (LV2_Atom_Int32*)lv2_atom_forge_write(forge, &a, sizeof(a));
}

/** Write an atom:Int64. */
static inline LV2_Atom_Int64*
lv2_atom_forge_int64(LV2_Atom_Forge* forge, int64_t val)
{
	const LV2_Atom_Int64 a = { { sizeof(val), forge->Int64 }, val };
	return (LV2_Atom_Int64*)lv2_atom_forge_write(forge, &a, sizeof(a));
}

/** Write an atom:Float. */
static inline LV2_Atom_Float*
lv2_atom_forge_float(LV2_Atom_Forge* forge, float val)
{
	const LV2_Atom_Float a = { { sizeof(val), forge->Float }, val };
	return (LV2_Atom_Float*)lv2_atom_forge_write(forge, &a, sizeof(a));
}

/** Write an atom:Double. */
static inline LV2_Atom_Double*
lv2_atom_forge_double(LV2_Atom_Forge* forge, double val)
{
	const LV2_Atom_Double a = { { sizeof(val), forge->Double }, val };
	return (LV2_Atom_Double*)lv2_atom_forge_write(
		forge, &a, sizeof(a));
}

/** Write an atom:Bool. */
static inline LV2_Atom_Bool*
lv2_atom_forge_bool(LV2_Atom_Forge* forge, bool val)
{
	const LV2_Atom_Bool a = { { sizeof(val), forge->Bool }, val };
	return (LV2_Atom_Bool*)lv2_atom_forge_write(forge, &a, sizeof(a));
}

/** Write an atom:URID. */
static inline LV2_Atom_URID*
lv2_atom_forge_urid(LV2_Atom_Forge* forge, LV2_URID id)
{
	const LV2_Atom_URID a = { { sizeof(id), forge->URID }, id };
	return (LV2_Atom_URID*)lv2_atom_forge_write(forge, &a, sizeof(a));
}

/** Write a string body.  Used internally. */
static inline uint8_t*
lv2_atom_forge_string_body(LV2_Atom_Forge* forge,
                           const uint8_t*  str,
                           uint32_t        len)
{
	uint8_t* out = NULL;
	if (   (out = lv2_atom_forge_raw(forge, str, len))
	    && (out = lv2_atom_forge_raw(forge, "", 1))) {
		lv2_atom_forge_pad(forge, len + 1);
	}
	return out;
}

/** Write an atom compatible with atom:String.  Used internally. */
static inline LV2_Atom_String*
lv2_atom_forge_typed_string(LV2_Atom_Forge* forge,
                            uint32_t        type,
                            const uint8_t*  str,
                            uint32_t        len)
{
	const LV2_Atom_String a = { { len + 1, type } };
	LV2_Atom_String* out = (LV2_Atom_String*)
		lv2_atom_forge_raw(forge, &a, sizeof(a));
	if (out) {
		if (!lv2_atom_forge_string_body(forge, str, len)) {
			out->atom.size = out->atom.type = 0;
			out = NULL;
		}
	}
	return out;
}

/** Write an atom:String.  Note that @p str need not be NULL terminated. */
static inline LV2_Atom_String*
lv2_atom_forge_string(LV2_Atom_Forge* forge, const uint8_t* str, uint32_t len)
{
	return lv2_atom_forge_typed_string(forge, forge->String, str, len);
}

/**
   Write an atom:URI.  Note that @p uri need not be NULL terminated.
   This does not map the URI, but writes the complete URI string.  To write
   a mapped URI, use lv2_atom_forge_urid().
*/
static inline LV2_Atom_String*
lv2_atom_forge_uri(LV2_Atom_Forge* forge, const uint8_t* uri, uint32_t len)
{
	return lv2_atom_forge_typed_string(forge, forge->URI, uri, len);
}

/** Write an atom:Path.  Note that @p path need not be NULL terminated. */
static inline LV2_Atom_String*
lv2_atom_forge_path(LV2_Atom_Forge* forge, const uint8_t* path, uint32_t len)
{
	return lv2_atom_forge_typed_string(forge, forge->Path, path, len);
}

/** Write an atom:Literal. */
static inline LV2_Atom_Literal*
lv2_atom_forge_literal(LV2_Atom_Forge* forge,
                       const uint8_t*  str,
                       uint32_t        len,
                       uint32_t        datatype,
                       uint32_t        lang)
{
	const LV2_Atom_Literal a = {
		{ sizeof(LV2_Atom_Literal) - sizeof(LV2_Atom) + len + 1,
		  forge->Literal },
		{ datatype,
		  lang }
	};
	LV2_Atom_Literal* out = (LV2_Atom_Literal*)
		lv2_atom_forge_raw(forge, &a, sizeof(a));
	if (out) {
		if (!lv2_atom_forge_string_body(forge, str, len)) {
			out->atom.size = out->atom.type = 0;
			out = NULL;
		}
	}
	return out;
}

/** Write an atom:Vector header, but not the vector body. */
static inline LV2_Atom_Vector*
lv2_atom_forge_vector_head(LV2_Atom_Forge* forge,
                           uint32_t        elem_count,
                           uint32_t        elem_type,
                           uint32_t        elem_size)
{
	const uint32_t size = sizeof(LV2_Atom_Vector) + (elem_size * elem_count);
	const LV2_Atom_Vector a = {
		{ size - sizeof(LV2_Atom), forge->Vector },
		{ elem_count, elem_type }
	};
	return (LV2_Atom_Vector*)lv2_atom_forge_write(forge, &a, sizeof(a));
}

/** Write a complete atom:Vector. */
static inline LV2_Atom_Vector*
lv2_atom_forge_vector(LV2_Atom_Forge* forge,
                      uint32_t        elem_count,
                      uint32_t        elem_type,
                      uint32_t        elem_size,
                      void*           elems)
{
	LV2_Atom_Vector* out = lv2_atom_forge_vector_head(
		forge, elem_count, elem_type, elem_size);
	if (out) {
		lv2_atom_forge_write(forge, elems, elem_size * elem_count);
	}
	return out;
}

/**
   Write the header of an atom:Tuple.

   The passed frame will be initialised to represent this tuple.  To complete
   the tuple, write a sequence of atoms, then pop the frame with
   lv2_atom_forge_pop().

   For example:
   @code
   // Write tuple (1, 2.0)
   LV2_Atom_Forge_Frame frame;
   LV2_Atom* tup = (LV2_Atom*)lv2_atom_forge_tuple(forge, &frame);
   lv2_atom_forge_int32(forge, 1);
   lv2_atom_forge_float(forge, 2.0);
   lv2_atom_forge_pop(forge, &frame);
   @endcode
*/
static inline LV2_Atom_Tuple*
lv2_atom_forge_tuple(LV2_Atom_Forge* forge, LV2_Atom_Forge_Frame* frame)
{
	const LV2_Atom_Tuple a    = { { 0, forge->Tuple } };
	LV2_Atom*            atom = lv2_atom_forge_write(forge, &a, sizeof(a));
	return (LV2_Atom_Tuple*)lv2_atom_forge_push(forge, frame, atom);
}

/**
   Write the header of an atom:Resource.

   The passed frame will be initialised to represent this object.  To complete
   the object, write a sequence of properties, then pop the frame with
   lv2_atom_forge_pop().

   For example:
   @code
   LV2_URID eg_Cat  = map("http://example.org/Cat");
   LV2_URID eg_name = map("http://example.org/name");

   // Write object header
   LV2_Atom_Forge_Frame frame;
   LV2_Atom* obj = (LV2_Atom*)lv2_atom_forge_resource(forge, &frame, 1, eg_Cat);

   // Write property: eg:name = "Hobbes"
   lv2_atom_forge_property_head(forge, eg_name, 0);
   lv2_atom_forge_string(forge, "Hobbes", strlen("Hobbes"));

   // Finish object
   lv2_atom_forge_pop(forge, &frame);
   @endcode
*/
static inline LV2_Atom_Object*
lv2_atom_forge_resource(LV2_Atom_Forge*       forge,
                        LV2_Atom_Forge_Frame* frame,
                        LV2_URID              id,
                        LV2_URID              otype)
{
	const LV2_Atom_Object a = {
		{ sizeof(LV2_Atom_Object) - sizeof(LV2_Atom), forge->Resource },
		{ id, otype }
	};
	LV2_Atom* atom = (LV2_Atom*)lv2_atom_forge_write(forge, &a, sizeof(a));
	return (LV2_Atom_Object*)lv2_atom_forge_push(forge, frame, atom);
}

/**
   The same as lv2_atom_forge_resource(), but for object:Blank.
*/
static inline LV2_Atom_Object*
lv2_atom_forge_blank(LV2_Atom_Forge*       forge,
                     LV2_Atom_Forge_Frame* frame,
                     uint32_t              id,
                     LV2_URID              otype)
{
	const LV2_Atom_Object a = {
		{ sizeof(LV2_Atom_Object) - sizeof(LV2_Atom), forge->Blank },
		{ id, otype }
	};
	LV2_Atom* atom = (LV2_Atom*)lv2_atom_forge_write(forge, &a, sizeof(a));
	return (LV2_Atom_Object*)lv2_atom_forge_push(forge, frame, atom);
}

/**
   Write the header for a property body (likely in an Object).
   See lv2_atom_forge_object() documentation for an example.
*/
static inline LV2_Atom_Property_Body*
lv2_atom_forge_property_head(LV2_Atom_Forge* forge,
                             LV2_URID        key,
                             LV2_URID        context)
{
	const LV2_Atom_Property_Body a = { key, context, { 0, 0 } };
	return (LV2_Atom_Property_Body*)lv2_atom_forge_write(
		forge, &a, 2 * sizeof(uint32_t));
}

/**
   Write the header for a Sequence.
   The size of the returned sequence will be 0, so passing it as the parent
   parameter to other forge methods will do the right thing.
*/
static inline LV2_Atom_Sequence*
lv2_atom_forge_sequence_head(LV2_Atom_Forge*       forge,
                             LV2_Atom_Forge_Frame* frame,
                             uint32_t              unit)
{
	const LV2_Atom_Sequence a = {
		{ sizeof(LV2_Atom_Sequence) - sizeof(LV2_Atom), forge->Sequence },
		{ unit, 0 }
	};
	LV2_Atom* atom = (LV2_Atom*)lv2_atom_forge_write(forge, &a, sizeof(a));
	return (LV2_Atom_Sequence*)lv2_atom_forge_push(forge, frame, atom);
}

/**
   Write the time stamp header of an Event (in a Sequence) in audio frames.
   After this, call the appropriate forge method(s) to write the body, passing
   the same @p parent parameter.  Note the returned LV2_Event is NOT an Atom.
*/
static inline int64_t*
lv2_atom_forge_frame_time(LV2_Atom_Forge* forge, int64_t frames)
{
	return (int64_t*)lv2_atom_forge_write(forge, &frames, sizeof(frames));
}

/**
   Write the time stamp header of an Event (in a Sequence) in beats.
   After this, call the appropriate forge method(s) to write the body, passing
   the same @p parent parameter.  Note the returned LV2_Event is NOT an Atom.
*/
static inline double*
lv2_atom_forge_beat_time(LV2_Atom_Forge* forge, double beats)
{
	return (double*)lv2_atom_forge_write(forge, &beats, sizeof(beats));
}

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* LV2_ATOM_FORGE_H */
