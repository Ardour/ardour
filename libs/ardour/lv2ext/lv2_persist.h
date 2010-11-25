/* lv2_persist.h - C header file for the LV2 Persist extension.
 * Copyright (C) 2010 Leonard Ritter <paniq@paniq.org>
 *
 * This header is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This header is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this header; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 01222-1307 USA
 */

/** @file
 * C header for the LV2 Persist extension <http://lv2plug.in/ns/ext/persist>.
 */

#ifndef LV2_PERSIST_H
#define LV2_PERSIST_H

#ifdef __cplusplus
extern "C" {
#endif

#define LV2_PERSIST_URI "http://lv2plug.in/ns/ext/persist"

/** A host-provided function to store a value under a given key.
 * @param callback_data Must be the callback_data passed to LV2_Persist.save().
 * @param key The URI key (predicate) under which the value is to be stored.
 * @param value Pointer to the value (object) to be stored.
 * @param size The size of the data at @a value in bytes.
 * @param type The type of @a value, as a URI mapped to an integer.
 *
 * The host passes a callback of this type to LV2_Persist.save().  This
 * callback is called repeatedly by the plugin within LV2_Persist.save() to
 * store all the key/value records that describe its current state.
 *
 * Unless @a type is 0, @a value is guaranteed to be POD (i.e. a region
 * of memory that does not contain pointers and can safely be copied
 * and persisted indefinitely with a simple memcpy).  If @a type is 0,
 * then @a value is a reference, as defined by the LV2 Atom extension
 * <http://lv2plug.in/ns/ext/atom/>.  Hosts are not required to support
 * references: a plugin MUST NOT expect a host to persist references unless
 * the host supports the feature <http://lv2plug.in/ns/ext/atom#blobSupport>.
 * Plugins SHOULD express their state entirely with POD values.
 *
 * Note that @a size MUST be > 0, and @a value MUST point to a valid region of
 * memory @a size bytes long (this is required to make restore unambiguous).
 *
 * The plugin MUST NOT attempt to use this function outside of the
 * LV2_Persist.restore() context.
 */
typedef void (*LV2_Persist_Store_Function)(
	void*       callback_data,
	const char* key,
	const void* value,
	size_t      size,
	uint32_t    type);

/** A host-provided function to retrieve a value under a given key.
 * @param callback_data Must be the callback_data passed to LV2_Persist.restore().
 * @param key The URI key (predicate) under which a value has been stored.
 * @param size (Output) If non-NULL, set to the size of the restored value.
 * @param type (Output) If non-NULL, set to the type of the restored value.
 * @return A pointer to the restored value (object), or NULL if no value
 *         has been stored under @a key.
 *
 * A callback of this type is passed by the host to LV2_Persist.restore().  This
 * callback is called repeatedly by the plugin within LV2_Persist.restore() to
 * retrieve the values of any keys it requires to restore its state.
 *
 * The returned value MUST remain valid until LV2_Persist.restore() returns.
 *
 * The plugin MUST NOT attempt to use this function, or any value returned from
 * it, outside of the LV2_Persist.restore() context.  Returned values MAY be
 * copied for later use if necessary.
 */
typedef const void* (*LV2_Persist_Retrieve_Function)(
	void*       callback_data,
	const char* key,
	size_t*     size,
	uint32_t*   type);

/** When the plugin's extension_data is called with argument LV2_PERSIST_URI,
 * the plugin MUST return an LV2_Persist structure, which remains valid for
 * the lifetime of the plugin.
 *
 * The host can use the contained function pointers to save and restore the
 * state of a plugin instance at any time (provided the threading restrictions
 * for the given function are met).
 *
 * The typical use case is to save the plugin's state when a project is
 * saved, and to restore the state when a project has been loaded.  Other
 * uses are possible (e.g. cloning plugin instances or taking a snapshot
 * of plugin state).
 *
 * Stored data is only guaranteed to be compatible between instances of plugins
 * with the same URI (i.e. if a change to a plugin would cause a fatal error
 * when restoring state saved by a previous version of that plugin, the plugin
 * URI must change just as it must when a plugin's ports change).  Plugin
 * authors should consider this possibility, and always store sensible data
 * with meaningful types to avoid such compatibility issues in the future.
 */
typedef struct _LV2_Persist {
	
	/** Save plugin state using a host-provided @a store callback.
	 *
	 * @param instance The instance handle of the plugin.
	 * @param store The host-provided store callback.
	 * @param callback_data	An opaque pointer to host data, e.g. the map or
	 *        file where the values are to be stored.  If @a store is called,
	 *        this MUST be passed as its callback_data parameter.
	 *
	 * The plugin is expected to store everything necessary to completely
	 * restore its state later (possibly much later, in a different
	 * process, on a completely different machine, etc.)
	 *
	 * The @a callback_data pointer and @a store function MUST NOT be
	 * used beyond the scope of save().
	 *
	 * This function has its own special threading class: it may not be
	 * called concurrently with any "Instantiation" function, but it
	 * may be called concurrently with functions in any other class,
	 * unless the definition of that class prohibits it (e.g. it may
	 * not be called concurrently with a "Discovery" function, but it
	 * may be called concurrently with an "Audio" function.  The plugin
	 * is responsible for any locking or lock-free techniques necessary
	 * to make this possible.
	 *
	 * Note that in the simple case where state is only modified by
	 * restore(), there are no synchronization issues since save() is
	 * never called concurrently with restore() (though run() may read
	 * it during a save).
	 *
	 * Plugins that dynamically modify state while running, however,
	 * must take care to do so in such a way that a concurrent call to
	 * save() will save a consistent representation of plugin state for a
	 * single instant in time.  The simplest way to do this is to modify a
	 * copy of the state map and atomically swap a pointer to the entire
	 * map once the changes are complete (for very large state maps,
	 * a purely functional map data structure may be more appropriate
	 * since a complete copy is not necessary).
	 */
	void (*save)(LV2_Handle                 instance,
	             LV2_Persist_Store_Function store,
	             void*                      callback_data);

	/** Restore plugin state using a host-provided @a retrieve callback.
	 *
	 * @param instance The instance handle of the plugin.
	 * @param retrieve The host-provided retrieve callback.
	 * @param callback_data	An opaque pointer to host data, e.g. the map or
	 *        file from which the values are to be restored.  If @a retrieve is
	 *        called, this MUST be passed as its callback_data parameter.
	 *
	 * The plugin MAY assume a restored value was set by a previous call to
	 * LV2_Persist.save() by a plugin with the same URI.
	 *
	 * The plugin MUST gracefully fall back to a default value when a
	 * value can not be retrieved.	This allows the host to reset the
	 * plugin state with an empty map.
	 *
	 * The @a callback_data pointer and @a store function MUST NOT be used
	 * beyond the scope of restore().
	 *
	 * This function is in the "Instantiation" threading class as defined
	 * by LV2.  This means it MUST NOT be called concurrently with any other
	 * function on the same plugin instance.
	 */
	void (*restore)(LV2_Handle                    instance,
	                LV2_Persist_Retrieve_Function retrieve,
	                void*                         callback_data);

} LV2_Persist;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LV2_PERSIST_H */
