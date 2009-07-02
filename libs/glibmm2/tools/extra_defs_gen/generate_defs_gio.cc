/* generate_defs_gio.cc
 *
 * Copyright (C) 2007 The gtkmm Development Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "generate_extra_defs.h"
#include <iostream>
#include <gio/gio.h>

int main (int argc, char** argv)
{
  g_type_init ();

  std::cout << get_defs(G_TYPE_ASYNC_RESULT)
            << get_defs(G_TYPE_CANCELLABLE)
            << get_defs(G_TYPE_BUFFERED_INPUT_STREAM)
            << get_defs(G_TYPE_BUFFERED_OUTPUT_STREAM)
            << get_defs(G_TYPE_DATA_INPUT_STREAM)
            << get_defs(G_TYPE_DATA_OUTPUT_STREAM)
            << get_defs(G_TYPE_DRIVE)
            << get_defs(G_TYPE_FILE)
            << get_defs(G_TYPE_FILE_ENUMERATOR)
            << get_defs(G_TYPE_FILE_INFO)
            << get_defs(G_TYPE_FILE_ICON)
            << get_defs(G_TYPE_FILE_MONITOR)
            << get_defs(G_TYPE_FILENAME_COMPLETER)
//            << get_defs(G_TYPE_FILE_ATTRIBUTE_INFO_LIST)
//            << get_defs(G_TYPE_FILE_ATTRIBUTE_MATCHER)
            << get_defs(G_TYPE_FILE_INPUT_STREAM)
            << get_defs(G_TYPE_FILE_OUTPUT_STREAM)
            << get_defs(G_TYPE_FILTER_INPUT_STREAM)
            << get_defs(G_TYPE_FILTER_OUTPUT_STREAM)

            << get_defs(G_TYPE_INPUT_STREAM)
            << get_defs(G_TYPE_LOADABLE_ICON)
            << get_defs(G_TYPE_MEMORY_INPUT_STREAM)
            << get_defs(G_TYPE_MEMORY_OUTPUT_STREAM)
            << get_defs(G_TYPE_MOUNT)
            << get_defs(G_TYPE_MOUNT_OPERATION)
            << get_defs(G_TYPE_SEEKABLE)
            << get_defs(G_TYPE_SIMPLE_ASYNC_RESULT)
            << get_defs(G_TYPE_THEMED_ICON)

            //TODO: This causes a g_warning:
            //GLib-GObject-CRITICAL **: g_param_spec_pool_list: assertion `pool != NULL' failed"
            << get_defs(G_TYPE_VOLUME)

            << get_defs(G_TYPE_VOLUME_MONITOR)

            << std::endl;
  
  return 0;
}
