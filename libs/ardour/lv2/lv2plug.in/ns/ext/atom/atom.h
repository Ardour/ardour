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
   @file atom.h C header for the LV2 Atom extension
   <http://lv2plug.in/ns/ext/atom>.
*/

#ifndef LV2_ATOM_H
#define LV2_ATOM_H

#include <stdint.h>
#include <stddef.h>

#define LV2_ATOM_URI "http://lv2plug.in/ns/ext/atom"

#define LV2_ATOM__Atom          LV2_ATOM_URI "#Atom"
#define LV2_ATOM__AtomPort      LV2_ATOM_URI "#AtomPort"
#define LV2_ATOM__AudioFrames   LV2_ATOM_URI "#AudioFrames"
#define LV2_ATOM__Beats         LV2_ATOM_URI "#Beats"
#define LV2_ATOM__Blank         LV2_ATOM_URI "#Blank"
#define LV2_ATOM__Bool          LV2_ATOM_URI "#Bool"
#define LV2_ATOM__Chunk         LV2_ATOM_URI "#Chunk"
#define LV2_ATOM__Double        LV2_ATOM_URI "#Double"
#define LV2_ATOM__Event         LV2_ATOM_URI "#Event"
#define LV2_ATOM__Float         LV2_ATOM_URI "#Float"
#define LV2_ATOM__Int32         LV2_ATOM_URI "#Int32"
#define LV2_ATOM__Int64         LV2_ATOM_URI "#Int64"
#define LV2_ATOM__Literal       LV2_ATOM_URI "#Literal"
#define LV2_ATOM__MessagePort   LV2_ATOM_URI "#MessagePort"
#define LV2_ATOM__Number        LV2_ATOM_URI "#Number"
#define LV2_ATOM__Object        LV2_ATOM_URI "#Object"
#define LV2_ATOM__Path          LV2_ATOM_URI "#Path"
#define LV2_ATOM__Property      LV2_ATOM_URI "#Property"
#define LV2_ATOM__Resource      LV2_ATOM_URI "#Resource"
#define LV2_ATOM__Sequence      LV2_ATOM_URI "#Sequence"
#define LV2_ATOM__String        LV2_ATOM_URI "#String"
#define LV2_ATOM__TimeUnit      LV2_ATOM_URI "#TimeUnit"
#define LV2_ATOM__Tuple         LV2_ATOM_URI "#Tuple"
#define LV2_ATOM__URI           LV2_ATOM_URI "#URI"
#define LV2_ATOM__URID          LV2_ATOM_URI "#URID"
#define LV2_ATOM__ValuePort     LV2_ATOM_URI "#ValuePort"
#define LV2_ATOM__Vector        LV2_ATOM_URI "#Vector"
#define LV2_ATOM__beatTime      LV2_ATOM_URI "#beatTime"
#define LV2_ATOM__bufferType    LV2_ATOM_URI "#bufferType"
#define LV2_ATOM__childType     LV2_ATOM_URI "#childType"
#define LV2_ATOM__eventTransfer LV2_ATOM_URI "#eventTransfer"
#define LV2_ATOM__frameTime     LV2_ATOM_URI "#frameTime"
#define LV2_ATOM__supports      LV2_ATOM_URI "#supports"
#define LV2_ATOM__timeUnit      LV2_ATOM_URI "#timeUnit"

#define LV2_ATOM_REFERENCE_TYPE 0

#ifdef __cplusplus
extern "C" {
#endif

/** This expression will fail to compile if double does not fit in 64 bits. */
typedef char lv2_atom_assert_double_fits_in_64_bits[
	((sizeof(double) <= sizeof(uint64_t)) * 2) - 1];

/**
   Return a pointer to the contents of an Atom.  The "contents" of an atom
   is the data past the complete type-specific header.
   @param type The type of the atom, e.g. LV2_Atom_String.
   @param atom A variable-sized atom.
*/
#define LV2_ATOM_CONTENTS(type, atom) \
	((void*)((uint8_t*)(atom) + sizeof(type)))

/**
   Return a pointer to the body of an Atom.  The "body" of an atom is the
   data just past the LV2_Atom head (i.e. the same offset for all types).
*/
#define LV2_ATOM_BODY(atom) LV2_ATOM_CONTENTS(LV2_Atom, atom)

/** The header of an atom:Atom. */
typedef struct {
	uint32_t size;  /**< Size in bytes, not including type and size. */
	uint32_t type;  /**< Type of this atom (mapped URI). */
} LV2_Atom;

/** A chunk of memory that may be uninitialized or contain an Atom. */
typedef struct {
	LV2_Atom atom;  /**< Atom header. */
	LV2_Atom body;  /**< Body atom header. */
} LV2_Atom_Chunk;

/** An atom:Int32 or atom:Bool.  May be cast to LV2_Atom. */
typedef struct {
	LV2_Atom atom;  /**< Atom header. */
	int32_t  body;  /**< Integer value. */
} LV2_Atom_Int32;

/** An atom:Int64.  May be cast to LV2_Atom. */
typedef struct {
	LV2_Atom atom;  /**< Atom header. */
	int64_t  body;  /**< Integer value. */
} LV2_Atom_Int64;

/** An atom:Float.  May be cast to LV2_Atom. */
typedef struct {
	LV2_Atom atom;  /**< Atom header. */
	float    body;  /**< Floating point value. */
} LV2_Atom_Float;

/** An atom:Double.  May be cast to LV2_Atom. */
typedef struct {
	LV2_Atom atom;  /**< Atom header. */
	double   body;  /**< Floating point value. */
} LV2_Atom_Double;

/** An atom:Bool.  May be cast to LV2_Atom. */
typedef LV2_Atom_Int32 LV2_Atom_Bool;

/** An atom:URID.  May be cast to LV2_Atom. */
typedef struct {
	LV2_Atom atom;  /**< Atom header. */
	uint32_t body;  /**< URID. */
} LV2_Atom_URID;

/** An atom:String.  May be cast to LV2_Atom. */
typedef struct {
	LV2_Atom atom;  /**< Atom header. */
	/* Contents (a null-terminated UTF-8 string) follow here. */
} LV2_Atom_String;

/** The body of an atom:Literal. */
typedef struct {
	uint32_t datatype;  /**< Datatype URID. */
	uint32_t lang;      /**< Language URID. */
	/* Contents (a null-terminated UTF-8 string) follow here. */
} LV2_Atom_Literal_Body;

/** An atom:Literal.  May be cast to LV2_Atom. */
typedef struct {
	LV2_Atom              atom;  /**< Atom header. */
	LV2_Atom_Literal_Body body;  /**< Body. */
} LV2_Atom_Literal;

/** An atom:Tuple.  May be cast to LV2_Atom. */
typedef struct {
	LV2_Atom atom;  /**< Atom header. */
	/* Contents (a series of complete atoms) follow here. */
} LV2_Atom_Tuple;

/** The body of an atom:Vector. */
typedef struct {
	uint32_t elem_count;  /**< The number of elements in the vector */
	uint32_t elem_type;   /**< The type of each element in the vector */
	/* Contents (a series of packed atom bodies) follow here. */
} LV2_Atom_Vector_Body;

/** An atom:Vector.  May be cast to LV2_Atom. */
typedef struct {
	LV2_Atom             atom;  /**< Atom header. */
	LV2_Atom_Vector_Body body;  /**< Body. */
} LV2_Atom_Vector;

/** The body of an atom:Property (e.g. in an atom:Object). */
typedef struct {
	uint32_t key;      /**< Key (predicate) (mapped URI). */
	uint32_t context;  /**< Context URID (may be, and generally is, 0). */
	LV2_Atom value;    /**< Value atom header. */
	/* Value atom body follows here. */
} LV2_Atom_Property_Body;

/** An atom:Property.  May be cast to LV2_Atom. */
typedef struct {
	LV2_Atom               atom;  /**< Atom header. */
	LV2_Atom_Property_Body body;  /**< Body. */
} LV2_Atom_Property;

/** The body of an atom:Object. May be cast to LV2_Atom. */
typedef struct {
	uint32_t id;     /**< URID (atom:Resource) or blank ID (atom:Blank). */
	uint32_t otype;  /**< Type URID (same as rdf:type, for fast dispatch). */
	/* Contents (a series of property bodies) follow here. */
} LV2_Atom_Object_Body;

/** An atom:Object.  May be cast to LV2_Atom. */
typedef struct {
	LV2_Atom             atom;  /**< Atom header. */
	LV2_Atom_Object_Body body;  /**< Body. */
} LV2_Atom_Object;

/** The header of an atom:Event.  Note this type is NOT an LV2_Atom. */
typedef struct {
	/** Time stamp.  Which type is valid is determined by context. */
	union {
		int64_t frames;  /**< Time in audio frames. */
		double  beats;   /**< Time in beats. */
	} time;
	LV2_Atom body;  /**< Event body atom header. */
	/* Body atom contents follow here. */
} LV2_Atom_Event;

/**
   The body of an atom:Sequence (a sequence of events).

   The unit field is either a URID that described an appropriate time stamp
   type, or may be 0 where a default stamp type is known.  For
   LV2_Descriptor::run(), the default stamp type is atom:AudioFrames, i.e.
   LV2_Atom_Audio_Time.

   The contents of a sequence is a series of LV2_Atom_Event, each aligned
   to 64-bits, e.g.:
   <pre>
   | Event 1 (size 6)                              | Event 2
   |       |       |       |       |       |       |       |       |
   | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | |
   |FRAMES |SUBFRMS|TYPE   |SIZE   |DATADATADATAPAD|FRAMES |SUBFRMS|...
   </pre>
*/
typedef struct {
	uint32_t unit;  /**< URID of unit of event time stamps. */
	uint32_t pad;   /**< Currently unused. */
	/* Contents (a series of events) follow here. */
} LV2_Atom_Sequence_Body;

/** An atom:Sequence. */
typedef struct {
	LV2_Atom              atom;  /**< Atom header. */
	LV2_Atom_Literal_Body body;  /**< Body. */
} LV2_Atom_Sequence;

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* LV2_ATOM_H */
