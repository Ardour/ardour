/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2001-2007 Sun Microsystems, Inc.  All rights reserved.
 * (Brian Cameron, Dmitriy Demin, James Cheng, Padraig O'Briain)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2007.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include <stdlib.h>
#include <dlfcn.h>

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#if defined(HAVE_SYS_SYSTEMINFO_H)
#include <sys/systeminfo.h>
#elif defined(HAVE_SYS_SYSINFO_H)
#include <sys/sysinfo.h>
#endif

#include "gdkmedialib.h"

typedef char *      (*ml_version)         (void);

static ml_version                 medialib_version = mlib_version;

gboolean 
_gdk_use_medialib (void)
{
  char *mlib_version_string;
  char sys_info[257];
  long count;

  /*
   * Sun mediaLib(tm) support.
   *
   *   http://www.sun.com/processors/vis/mlib.html
   *
   */
  if (getenv ("GDK_DISABLE_MEDIALIB"))
    return FALSE;

  /*
   * The imaging functions we want to use were added in mediaLib version 2.
   * So turn off mediaLib support if the user has an older version.
   * mlib_version returns a string in this format:
   *
   * mediaLib:0210:20011101:v8plusa
   * ^^^^^^^^ ^^^^ ^^^^^^^^ ^^^^^^^
   * libname  vers build    ISALIST identifier
   *               date     (in this case sparcv8plus+vis)
   *
   * The first 2 digits of the version are the major version.  The 3rd digit
   * is the minor version, and the 4th digit is the micro version. So the
   * above string corresponds to version 2.1.0.In the following test we only
   * care about the major version.
   */
   mlib_version_string = medialib_version ();

   count = sysinfo (SI_ARCHITECTURE, &sys_info[0], 257);
             
   if (count != -1)
     {
       if (strcmp (sys_info, "i386") == 0)
         {
           char *mlib_target_isa = &mlib_version_string[23];
  
           /*
            * For x86 processors mediaLib generic C implementation
            * does not give any performance advantage so disable it.
            */
           if (strncmp (mlib_target_isa, "sse", 3) != 0)
             {
               return FALSE;
             }

           /*
            * For x86 processors use of libumem conflicts with
            * mediaLib, so avoid using it.
            */
           if (dlsym (RTLD_PROBE,   "umem_alloc") != NULL)
             {
               return FALSE;
             }
         }
     }
   else
     {
       /* Failed to get system architecture, disable mediaLib */
       return FALSE;
     }

  return TRUE;
}
