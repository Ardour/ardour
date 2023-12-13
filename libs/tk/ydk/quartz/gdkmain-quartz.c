/* gdkmain-quartz.c
 *
 * Copyright (C) 2005 Imendio AB
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

#include "config.h"
#include <dlfcn.h>

#include "gdk.h"
#include <ApplicationServices/ApplicationServices.h>

const GOptionEntry _gdk_windowing_args[] = {
  { NULL }
};

void
_gdk_windowing_init (void)
{
  ProcessSerialNumber psn = { 0, kCurrentProcess };
  void (*_gtk_quartz_framework_init_ptr) (void);

  /* Make the current process a foreground application, i.e. an app
   * with a user interface, in case we're not running from a .app bundle
   */
  TransformProcessType (&psn, kProcessTransformToForegroundApplication);

  /* Initialize GTK+ framework if there is one. */
  _gtk_quartz_framework_init_ptr = dlsym (RTLD_DEFAULT, "_gtk_quartz_framework_init");
  if (_gtk_quartz_framework_init_ptr)
    _gtk_quartz_framework_init_ptr ();
}

void
gdk_error_trap_push (void)
{
}

gint
gdk_error_trap_pop (void)
{
  return 0;
}

gchar *
gdk_get_display (void)
{
  return g_strdup (gdk_display_get_name (gdk_display_get_default ()));
}

void
gdk_notify_startup_complete (void)
{
  /* FIXME: Implement? */
}

void
gdk_notify_startup_complete_with_id (const gchar* startup_id)
{
  /* FIXME: Implement? */
}

void          
gdk_window_set_startup_id (GdkWindow   *window,
			   const gchar *startup_id)
{
  /* FIXME: Implement? */
}

void
_gdk_windowing_display_set_sm_client_id (GdkDisplay  *display,
					 const gchar *sm_client_id)
{
}

void
gdk_set_use_xshm (gboolean use_xshm)
{
  /* Always on, since we're always on the local machine */
}

gboolean
gdk_get_use_xshm (void)
{
  return TRUE;
}

