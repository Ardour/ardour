/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2006 Tim Janik
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <string.h>
#include <glib.h>
#include "gdksettings.c"

int
main (int   argc,
      char *argv[])
{
  guint i, accu = 0;

  for (i = 0; i < GDK_SETTINGS_N_ELEMENTS(); i++)
    {
      if (gdk_settings_map[i].xsettings_offset != accu)
        g_error ("settings_map[%u].xsettings_offset != %u\n", i, accu);
      accu += strlen (gdk_settings_names + accu) + 1;
      if (gdk_settings_map[i].gdk_offset != accu)
        g_error ("settings_map[%u].gdk_offset != %u\n", i, accu);
      accu += strlen (gdk_settings_names + accu) + 1;
      // g_print ("%u) ok.\n", i);
    }

  g_print ("checksettings: all ok.\n");

  return 0;
}
