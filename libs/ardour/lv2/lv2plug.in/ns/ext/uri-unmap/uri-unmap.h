/* lv2_uri_unmap.h - C header file for the LV2 URI Unmap extension.
 *
 * Copyright (C) 2010 David Robillard <http://drobilla.net>
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
 * C header for the LV2 URI Map extension <http://lv2plug.in/ns/ext/uri-unmap>.
 */

#ifndef LV2_URI_UNMAP_H
#define LV2_URI_UNMAP_H

#define LV2_URI_UNMAP_URI "http://lv2plug.in/ns/ext/uri-unmap"

#include <stdint.h>


/** Opaque pointer to host data. */
typedef void* LV2_URI_Unmap_Callback_Data;


/** The data field of the LV2_Feature for the URI Unmap extension.
 *
 * To support this extension the host must pass an LV2_Feature struct to the
 * plugin's instantiate method with URI "http://lv2plug.in/ns/ext/uri-unmap"
 * and data pointed to an instance of this struct.
 */
typedef struct {

	/** Opaque pointer to host data.
	 *
	 * The plugin MUST pass this to any call to functions in this struct.
	 * Otherwise, it must not be interpreted in any way.
	 */
	LV2_URI_Unmap_Callback_Data callback_data;

	/** Get the numeric ID of a URI from the host.
	 *
	 * @param callback_data Must be the callback_data member of this struct.
	 * @param map The 'context' used to map this URI.
	 * @param id The URI ID to unmap.
	 * @return The string form of @a id, or NULL on error.
	 *
	 * The @a id MUST be a value previously returned from
	 * LV2_Uri_Map_Feature.uri_to_id.
	 *
	 * The returned string is owned by the host and MUST NOT be freed by
	 * the plugin or stored for a long period of time (e.g. across run
	 * invocations) without copying.
	 *
	 * This function is referentially transparent - any number of calls with
	 * the same arguments is guaranteed to return the same value over the life
	 * of a plugin instance (though the same ID may return different values
	 * with a different map parameter).
	 *
	 * This function may be called from any non-realtime thread, possibly
	 * concurrently (hosts may simply use a mutex to meet these requirements).
	 */
	const char* (*id_to_uri)(LV2_URI_Unmap_Callback_Data callback_data,
	                         const char*                 map,
	                         uint32_t                    id);

} LV2_URI_Unmap_Feature;


#endif /* LV2_URI_UNMAP_H */

