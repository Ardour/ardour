/* ATK -  Accessibility Toolkit
 *
 * Copyright (C) 2014 Igalia, S.L.
 *
 * Author: Alejandro Piñeiro Iglesias <apinheiro@igalia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <string.h>
#include <locale.h>

#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include "atkprivate.h"

#ifdef G_OS_WIN32

#define STRICT
#include <windows.h>
#undef STRICT

static HMODULE atk_dll;

BOOL WINAPI
DllMain (HINSTANCE hinstDLL,
         DWORD     fdwReason,
         LPVOID    lpvReserved)
{
  switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
      atk_dll = (HMODULE) hinstDLL;
      break;
    }

  return TRUE;
}

static const char *
get_atk_locale_dir (void)
{
  static gchar *atk_localedir = NULL;

  if (!atk_localedir)
    {
      const gchar *p;
      gchar *root, *temp;
      
      /* ATK_LOCALEDIR might end in either /lib/locale or
       * /share/locale. Scan for that slash.
       */
      p = ATK_LOCALEDIR + strlen (ATK_LOCALEDIR);
      while (*--p != '/')
	;
      while (*--p != '/')
	;

      root = g_win32_get_package_installation_directory_of_module (atk_dll);
      temp = g_build_filename (root, p, NULL);
      g_free (root);

      /* atk_localedir is passed to bindtextdomain() which isn't
       * UTF-8-aware.
       */
      atk_localedir = g_win32_locale_filename_from_utf8 (temp);
      g_free (temp);
    }
  return atk_localedir;
}

#undef ATK_LOCALEDIR

#define ATK_LOCALEDIR get_atk_locale_dir()

#endif

void
_gettext_initialization (void)
{
#ifdef ENABLE_NLS
  static gboolean gettext_initialized = FALSE;

  if (!gettext_initialized)
    {
      const char *dir = g_getenv ("ATK_ALT_LOCALEDIR");

      gettext_initialized = TRUE;
      if (dir == NULL)
        dir = ATK_LOCALEDIR;

      bindtextdomain (GETTEXT_PACKAGE, dir);
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
      bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif
    }
#endif
}

/*
 * Compacts a name. For example: to get "accelerator label" instead of
 * "accelerator-label"
 */
void
_compact_name (gchar *name)
{
  gchar *p = name;

  while (*p)
    {
      if (*p == '-')
        *p = ' ';
      p++;
    }
}
