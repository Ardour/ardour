/*
  Copyright 2010-2011 David Robillard <d@drobilla.net>

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the
  Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
  OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
  ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
  OTHER DEALINGS IN THE SOFTWARE.
*/

/**
   @file files.h
   C API for the LV2 Files extension <http://lv2plug.in/ns/ext/files>.
*/

#ifndef LV2_FILES_H
#define LV2_FILES_H

#ifdef __cplusplus
extern "C" {
#endif

#define LV2_FILES_URI                  "http://lv2plug.in/ns/ext/files"
#define LV2_FILES_PATH_SUPPORT_URI     LV2_FILES_URI "#pathSupport"
#define LV2_FILES_NEW_FILE_SUPPORT_URI LV2_FILES_URI "#newFileSupport"

typedef void* LV2_Files_Host_Data;

/**
   files:pathSupport feature struct.
   
   To support this feature, the host MUST pass an LV2_Feature struct with @a
   URI @ref LV2_FILES_PATH_SUPPORT_URI and @a data pointed to an instance of
   this struct.
*/
typedef struct {

	/**
	   Opaque host data.
	*/
	LV2_Files_Host_Data host_data;

	/**
	   Map an absolute path to an abstract path for use in plugin state.
	   @param host_data MUST be the @a host_data member of this struct.
	   @param absolute_path The absolute path of a file.
	   @return An abstract path suitable for use in plugin state.

	   The plugin MUST use this function to map any paths that will be stored
	   in plugin state.  The returned value is an abstract path which MAY not
	   be an actual file system path; @ref absolute_path MUST be used to map it
	   to an actual path in order to use the file.

	   Hosts MAY map paths in any way (e.g. by creating symbolic links within
	   the plugin's state directory or storing a list of referenced files for
	   later export).  Plugins MUST NOT make any assumptions about abstract
	   paths except that they can be mapped to an absolute path using @ref
	   absolute_path.  Particularly when restoring from state, this absolute
	   path MAY not be the same as the original absolute path, but the host
	   MUST guarantee it refers to a file with contents equivalent to the
	   original.

	   This function may only be called within the context of
	   LV2_Persist.save() or LV2_Persist.restore().  The caller is responsible
	   for freeing the returned value.
	*/
	char* (*abstract_path)(LV2_Files_Host_Data host_data,
	                       const char*         absolute_path);

	/**
	   Map an abstract path from plugin state to an absolute path.
	   @param host_data MUST be the @a host_data member of this struct.
	   @param abstract_path An abstract path (e.g. a path from plugin state).
	   @return An absolute file system path.

	   Since abstract paths are not necessarily actual file paths (or at least
	   not necessarily absolute paths), this function MUST be used in order to
	   actually open or otherwise use the file referred to by an abstract path.

	   This function may only be called within the context of
	   LV2_Persist.save() or LV2_Persist.restore().  The caller is responsible
	   for freeing the returned value.
	*/
	char* (*absolute_path)(LV2_Files_Host_Data host_data,
	                       const char*         abstract_path);

} LV2_Files_Path_Support;

/**
   files:newFileSupport feature struct.
   
   To support this feature, the host MUST pass an LV2_Feature struct with @a
   URI @ref LV2_FILES_NEW_FILE_SUPPORT_URI and @a data pointed to an instance
   of this struct.
*/
typedef struct {

	/**
	   Opaque host data.
	*/
	LV2_Files_Host_Data host_data;

	/**
	   Return an absolute path the plugin may use to create a new file.
	   @param host_data MUST be the @a host_data member of this struct.
	   @param relative_path The relative path of the file.
	   @return The absolute path to use for the new file.

	   The plugin can assume @a relative_path is relative to a namespace
	   dedicated to that plugin instance; hosts MUST ensure this, e.g. by
	   giving each plugin instance its own state directory.  The returned path
	   is absolute and thus suitable for creating and using a file, but NOT
	   suitable for storing in plugin state (it MUST be mapped to an abstract
	   path using @ref LV2_Files_Path_Support::abstract_path to do so).

	   This function may be called from any non-realtime context.  The caller
	   is responsible for freeing the returned value.
	*/
	char* (*new_file_path)(LV2_Files_Host_Data host_data,
	                       const char*         relative_path);

} LV2_Files_New_File_Support;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LV2_FILES_H */
