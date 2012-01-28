/*
  Copyright 2010-2011 David Robillard <http://drobilla.net>
  Copyright 2010 Leonard Ritter <paniq@paniq.org>

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
   @file
   C API for the LV2 State extension <http://lv2plug.in/ns/ext/state>.
*/

#ifndef LV2_STATE_H
#define LV2_STATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LV2_STATE_URI "http://lv2plug.in/ns/ext/state"

#define LV2_STATE_INTERFACE_URI LV2_STATE_URI "#Interface"
#define LV2_STATE_PATH_URI      LV2_STATE_URI "#Path"
#define LV2_STATE_MAP_PATH_URI  LV2_STATE_URI "#mapPath"
#define LV2_STATE_MAKE_PATH_URI LV2_STATE_URI "#makePath"

typedef void* LV2_State_Handle;
typedef void* LV2_State_Map_Path_Handle;
typedef void* LV2_State_Make_Path_Handle;

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
	LV2_STATE_IS_POD = 1,

	/**
	   Portable (architecture independent) data.

	   Values with this flag are in a format that is usable on any
	   architecture, i.e. if the value is saved on one machine it can safely be
	   restored on another machine regardless of endianness, alignment, etc.
	*/
	LV2_STATE_IS_PORTABLE = 1 << 1,

	/**
	   Native data.

	   This flag is used by the host to indicate that the saved data is only
	   going to be used locally in the currently running process (e.g. for
	   instance duplication or snapshots), so the plugin should use the most
	   efficient representation possible and not worry about serialisation
	   and portability.
	*/
	LV2_STATE_IS_NATIVE = 1 << 2

} LV2_State_Flags;

/**
   A host-provided function to store a property.
   @param handle Must be the handle passed to LV2_State_Interface.save().
   @param key The key (predicate) to store @c value under (URI mapped integer).
   @param value Pointer to the value (object) to be stored.
   @param size The size of the data at @c value in bytes.
   @param type The type of @c value (URI).
   @param flags LV2_State_Flags for @c value.
   @return 0 on success, otherwise a non-zero error code.

   The host passes a callback of this type to LV2_State_Interface.save(). This callback
   is called repeatedly by the plugin within LV2_State_Interface.save() to store all
   the statements that describe its current state.

   The host MAY fail to store a property if the type is not understood and is
   not LV2_STATE_IS_POD and/or LV2_STATE_IS_PORTABLE. Implementations are
   encouraged to use POD and portable values (e.g. string literals) wherever
   possible, and use common types (e.g. types from
   http://lv2plug.in/ns/ext/atom) regardless, since hosts are likely to already
   contain the necessary implementation.

   Note that @c size MUST be > 0, and @c value MUST point to a valid region of
   memory @c size bytes long (this is required to make restore unambiguous).

   The plugin MUST NOT attempt to use this function outside of the
   LV2_State_Interface.restore() context.
*/
typedef int (*LV2_State_Store_Function)(LV2_State_Handle handle,
                                        uint32_t         key,
                                        const void*      value,
                                        size_t           size,
                                        uint32_t         type,
                                        uint32_t         flags);

/**
   A host-provided function to retrieve a property.
   @param handle Must be the handle passed to
   LV2_State_Interface.restore().
   @param key The key (predicate) of the property to retrieve (URI).
   @param size (Output) If non-NULL, set to the size of the restored value.
   @param type (Output) If non-NULL, set to the type of the restored value.
   @param flags (Output) If non-NULL, set to the LV2_State_Flags for
   the returned value.
   @return A pointer to the restored value (object), or NULL if no value
   has been stored under @c key.

   A callback of this type is passed by the host to
   LV2_State_Interface.restore(). This callback is called repeatedly by the
   plugin within LV2_State_Interface.restore() to retrieve any properties it
   requires to restore its state.

   The returned value MUST remain valid until LV2_State_Interface.restore()
   returns.

   The plugin MUST NOT attempt to use this function, or any value returned from
   it, outside of the LV2_State_Interface.restore() context. Returned values
   MAY be copied for later use if necessary, assuming the plugin knows how to
   do so correctly (e.g. the value is POD, or the plugin understands the type).
*/
typedef const void* (*LV2_State_Retrieve_Function)(LV2_State_Handle handle,
                                                   uint32_t         key,
                                                   size_t*          size,
                                                   uint32_t*        type,
                                                   uint32_t*        flags);

/**
   State Extension Data.

   When the plugin's extension_data is called with argument LV2_STATE_URI,
   the plugin MUST return an LV2_State structure, which remains valid for the
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
typedef struct _LV2_State_Interface {

	/**
	   Save plugin state using a host-provided @c store callback.

	   @param instance The instance handle of the plugin.
	   @param store The host-provided store callback.
	   @param handle An opaque pointer to host data, e.g. the map or
	   file where the values are to be stored. If @c store is called, this MUST
	   be passed as its handle parameter.
	   @param flags Flags describing desires properties of this save.  The
	   plugin SHOULD use these values to determine the most appropriate and/or
	   efficient serialisation, but is not required to do so.
	   @param features Extensible parameter for passing any additional
	   features to be used for this save.

	   The plugin is expected to store everything necessary to completely
	   restore its state later (possibly much later, in a different process, on
	   a completely different machine, etc.)

	   The @c handle pointer and @c store function MUST NOT be used
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
	             LV2_State_Store_Function   store,
	             LV2_State_Handle           handle,
	             uint32_t                   flags,
	             const LV2_Feature *const * features);

	/**
	   Restore plugin state using a host-provided @c retrieve callback.

	   @param instance The instance handle of the plugin.
	   @param retrieve The host-provided retrieve callback.
	   @param handle An opaque pointer to host data, e.g. the map or
	   file from which the values are to be restored. If @c retrieve is
	   called, this MUST be passed as its handle parameter.
	   @param flags Currently unused.
	   @param features Extensible parameter for passing any additional
	   features to be used for this restore.

	   The plugin MAY assume a restored value was set by a previous call to
	   LV2_State_Interface.save() by a plugin with the same URI.

	   The plugin MUST gracefully fall back to a default value when a value can
	   not be retrieved. This allows the host to reset the plugin state with an
	   empty map.

	   The @c handle pointer and @c store function MUST NOT be used
	   beyond the scope of restore().

	   This function is in the "Instantiation" threading class as defined by
	   LV2. This means it MUST NOT be called concurrently with any other
	   function on the same plugin instance.
	*/
	void (*restore)(LV2_Handle                  instance,
	                LV2_State_Retrieve_Function retrieve,
	                LV2_State_Handle            handle,
	                uint32_t                    flags,
	                const LV2_Feature *const *  features);

} LV2_State_Interface;

/**
   Feature data for state:mapPath (LV2_STATE_MAP_PATH_URI).
*/
typedef struct {

	/**
	   Opaque host data.
	*/
	LV2_State_Map_Path_Handle handle;

	/**
	   Map an absolute path to an abstract path for use in plugin state.
	   @param handle MUST be the @a handle member of this struct.
	   @param absolute_path The absolute path of a file.
	   @return An abstract path suitable for use in plugin state.

	   The plugin MUST use this function to map any paths that will be stored
	   in files in plugin state.  The returned value is an abstract path which
	   MAY not be an actual file system path; @ref absolute_path MUST be used
	   to map it to an actual path in order to use the file.

	   Hosts MAY map paths in any way (e.g. by creating symbolic links within
	   the plugin's state directory or storing a list of referenced files
	   elsewhere).  Plugins MUST NOT make any assumptions about abstract paths
	   except that they can be mapped back to an absolute path using @ref
	   absolute_path.

	   This function may only be called within the context of
	   LV2_State_Interface.save() or LV2_State_Interface.restore().  The caller
	   is responsible for freeing the returned value.
	*/
	char* (*abstract_path)(LV2_State_Map_Path_Handle handle,
	                       const char*               absolute_path);

	/**
	   Map an abstract path from plugin state to an absolute path.
	   @param handle MUST be the @a handle member of this struct.
	   @param abstract_path An abstract path (e.g. a path from plugin state).
	   @return An absolute file system path.

	   Since abstract paths are not necessarily actual file paths (or at least
	   not necessarily absolute paths), this function MUST be used in order to
	   actually open or otherwise use the file referred to by an abstract path.

	   This function may only be called within the context of
	   LV2_State_Interface.save() or LV2_State_Interface.restore().  The caller
	   is responsible for freeing the returned value.
	*/
	char* (*absolute_path)(LV2_State_Map_Path_Handle handle,
	                       const char*               abstract_path);

} LV2_State_Map_Path;

/**
   Feature data for state:makePath (@ref LV2_STATE_MAKE_PATH_URI).
*/
typedef struct {

	/**
	   Opaque host data.
	*/
	LV2_State_Make_Path_Handle handle;

	/**
	   Return a path the plugin may use to create a new file.
	   @param handle MUST be the @a handle member of this struct.
	   @param path The path of the new file relative to a namespace unique
	   to this plugin instance.
	   @return The absolute path to use for the new file.

	   This function can be used by plugins to create files and directories,
	   either at state saving time (if this feature is passed to
	   LV2_State_Interface.save()) or any time (if this feature is passed to
	   LV2_Descriptor.instantiate()).

	   The host must do whatever is necessary for the plugin to be able to
	   create a file at the returned path (e.g. using fopen), including
	   creating any leading directories.

	   If this function is passed to LV2_Descriptor.instantiate(), it may be
	   called from any non-realtime context.  If it is passed to
	   LV2_State_Interface.save(), it may only be called within the dynamic
	   scope of that function call.

	   The caller is responsible for freeing the returned value with free().
	*/
	char* (*path)(LV2_State_Make_Path_Handle handle,
	              const char*                path);

} LV2_State_Make_Path;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LV2_STATE_H */
