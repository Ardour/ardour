/*
  Copyright 2010-2011 David Robillard <http://drobilla.net>
  Copyright 2010 Leonard Ritter <paniq@paniq.org>

  This header is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This header is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
  License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this header; if not, write to the Free Software Foundation,
  Inc., 59 Temple Place, Suite 330, Boston, MA 01222-1307 USA
*/

/**
   @file
   C API for the LV2 Persist extension <http://lv2plug.in/ns/ext/persist>.
*/

#ifndef LV2_PERSIST_H
#define LV2_PERSIST_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LV2_PERSIST_URI "http://lv2plug.in/ns/ext/persist"

/**
   Flags describing value characteristics.

   These flags are used along with the value's type URI to determine how to
   (de-)serialise the value data, or whether it is even possible to do so.
*/
typedef enum {

	/**
	   Plain Old Data.

	   Values with this flag contain no references to non-persistent or
	   non-global resources (e.g. pointers, handles, local paths, etc.). It is
	   safe to copy POD values with a simple memcpy and store them for use at
	   any time in the future on a machine with a compatible architecture
	   (e.g. the same endianness and alignment).

	   Implementations MUST NOT attempt to copy or serialise a non-POD value if
	   they do not understand its type (and thus know how to correctly do so).
	*/
	LV2_PERSIST_IS_POD = 1,

	/**
	   Portable (architecture independent) data.

	   Values with this flag are in a format that is usable on any
	   architecture, i.e. if the value is saved on one machine it can safely be
	   restored on another machine regardless of endianness, alignment, etc.
	*/
	LV2_PERSIST_IS_PORTABLE = 1 << 1

} LV2_Persist_Flags;

/**
   A host-provided function to store a plugin instance property.
   @param callback_data Must be the callback_data passed to LV2_Persist.save().
   @param key The key (predicate) to store @a value under (URI mapped integer).
   @param value Pointer to the value (object) to be stored.
   @param size The size of the data at @a value in bytes.
   @param type The type of @a value (URI).
   @param flags LV2_Persist_Flags for @a value.
   @return 0 on success, otherwise a non-zero error code.

   The host passes a callback of this type to LV2_Persist.save(). This callback
   is called repeatedly by the plugin within LV2_Persist.save() to store all
   the statements that describe its current state.

   The host MAY fail to store a property if the type is not understood and is
   not LV2_PERSIST_IS_POD and/or LV2_PERSIST_IS_PORTABLE. Implementations are
   encouraged to use POD and portable values (e.g. string literals) wherever
   possible, and use common types (e.g. types from
   http://lv2plug.in/ns/ext/atom) regardless, since hosts are likely to already
   contain the necessary implementation.

   Note that @a size MUST be > 0, and @a value MUST point to a valid region of
   memory @a size bytes long (this is required to make restore unambiguous).

   The plugin MUST NOT attempt to use this function outside of the
   LV2_Persist.restore() context.
*/
typedef int (*LV2_Persist_Store_Function)(
	void*       callback_data,
	uint32_t    key,
	const void* value,
	size_t      size,
	uint32_t    type,
	uint32_t    flags);

/**
   A host-provided function to retrieve a property.
   @param callback_data Must be the callback_data passed to LV2_Persist.restore().
   @param key The key (predicate) of the property to retrieve (URI).
   @param size (Output) If non-NULL, set to the size of the restored value.
   @param type (Output) If non-NULL, set to the type of the restored value.
   @param flags (Output) If non-NULL, set to the LV2_Persist_Flags for
   the returned value.
   @return A pointer to the restored value (object), or NULL if no value
   has been stored under @a key.

   A callback of this type is passed by the host to LV2_Persist.restore(). This
   callback is called repeatedly by the plugin within LV2_Persist.restore() to
   retrieve any properties it requires to restore its state.

   The returned value MUST remain valid until LV2_Persist.restore() returns.

   The plugin MUST NOT attempt to use this function, or any value returned from
   it, outside of the LV2_Persist.restore() context. Returned values MAY be
   copied for later use if necessary, assuming the plugin knows how to do so
   correctly (e.g. the value is POD, or the plugin understands the type).
*/
typedef const void* (*LV2_Persist_Retrieve_Function)(
	void*     callback_data,
	uint32_t  key,
	size_t*   size,
	uint32_t* type,
	uint32_t* flags);

/**
   Persist Extension Data.

   When the plugin's extension_data is called with argument LV2_PERSIST_URI,
   the plugin MUST return an LV2_Persist structure, which remains valid for the
   lifetime of the plugin.

   The host can use the contained function pointers to save and restore the
   state of a plugin instance at any time (provided the threading restrictions
   for the given function are met).

   The typical use case is to save the plugin's state when a project is saved,
   and to restore the state when a project has been loaded. Other uses are
   possible (e.g. cloning plugin instances or taking a snapshot of plugin
   state).

   Stored data is only guaranteed to be compatible between instances of plugins
   with the same URI (i.e. if a change to a plugin would cause a fatal error
   when restoring state saved by a previous version of that plugin, the plugin
   URI MUST change just as it must when ports change incompatibly). Plugin
   authors should consider this possibility, and always store sensible data
   with meaningful types to avoid such compatibility issues in the future.
*/
typedef struct _LV2_Persist {

	/**
	   Save plugin state using a host-provided @a store callback.

	   @param instance The instance handle of the plugin.
	   @param store The host-provided store callback.
	   @param callback_data	An opaque pointer to host data, e.g. the map or
	   file where the values are to be stored. If @a store is called,
	   this MUST be passed as its callback_data parameter.

	   The plugin is expected to store everything necessary to completely
	   restore its state later (possibly much later, in a different process, on
	   a completely different machine, etc.)

	   The @a callback_data pointer and @a store function MUST NOT be used
	   beyond the scope of save().

	   This function has its own special threading class: it may not be called
	   concurrently with any "Instantiation" function, but it may be called
	   concurrently with functions in any other class, unless the definition of
	   that class prohibits it (e.g. it may not be called concurrently with a
	   "Discovery" function, but it may be called concurrently with an "Audio"
	   function. The plugin is responsible for any locking or lock-free
	   techniques necessary to make this possible.

	   Note that in the simple case where state is only modified by restore(),
	   there are no synchronization issues since save() is never called
	   concurrently with restore() (though run() may read it during a save).

	   Plugins that dynamically modify state while running, however, must take
	   care to do so in such a way that a concurrent call to save() will save a
	   consistent representation of plugin state for a single instant in time.
	*/
	void (*save)(LV2_Handle                 instance,
	             LV2_Persist_Store_Function store,
	             void*                      callback_data);

	/**
	   Restore plugin state using a host-provided @a retrieve callback.

	   @param instance The instance handle of the plugin.
	   @param retrieve The host-provided retrieve callback.
	   @param callback_data	An opaque pointer to host data, e.g. the map or
	   file from which the values are to be restored. If @a retrieve is
	   called, this MUST be passed as its callback_data parameter.

	   The plugin MAY assume a restored value was set by a previous call to
	   LV2_Persist.save() by a plugin with the same URI.

	   The plugin MUST gracefully fall back to a default value when a value can
	   not be retrieved. This allows the host to reset the plugin state with an
	   empty map.

	   The @a callback_data pointer and @a store function MUST NOT be used
	   beyond the scope of restore().

	   This function is in the "Instantiation" threading class as defined by
	   LV2. This means it MUST NOT be called concurrently with any other
	   function on the same plugin instance.
	*/
	void (*restore)(LV2_Handle                    instance,
	                LV2_Persist_Retrieve_Function retrieve,
	                void*                         callback_data);

} LV2_Persist;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LV2_PERSIST_H */
