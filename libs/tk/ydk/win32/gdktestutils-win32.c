/* Gtk+ testing utilities
 * Copyright (C) 2007 Imendio AB
 * Authors: Tim Janik
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
#include <gdk/gdktestutils.h>
#include <gdk/gdkkeysyms.h>
#include <win32/gdkwin32.h>
#include "gdkalias.h"

void
gdk_test_render_sync (GdkWindow *window)
{
}

gboolean
gdk_test_simulate_key (GdkWindow      *window,
                       gint            x,
                       gint            y,
                       guint           keyval,
                       GdkModifierType modifiers,
                       GdkEventType    key_pressrelease)
{
  gboolean      success = FALSE;
  GdkKeymapKey *keys    = NULL;
  gint          n_keys  = 0;
  INPUT         ip;
  gint          i;

  g_return_val_if_fail (key_pressrelease == GDK_KEY_PRESS || key_pressrelease == GDK_KEY_RELEASE, FALSE);
  g_return_val_if_fail (window != NULL, FALSE);

  ip.type = INPUT_KEYBOARD;
  ip.ki.wScan = 0;
  ip.ki.time = 0;
  ip.ki.dwExtraInfo = 0;

  switch (key_pressrelease)
    {
    case GDK_KEY_PRESS:
      ip.ki.dwFlags = 0;
      break;
    case GDK_KEY_RELEASE:
      ip.ki.dwFlags = KEYEVENTF_KEYUP;
      break;
    default:
      /* Not a key event. */
      return FALSE;
    }
  if (gdk_keymap_get_entries_for_keyval (gdk_keymap_get_default (), keyval, &keys, &n_keys))
    {
      for (i = 0; i < n_keys; i++)
        {
          if (key_pressrelease == GDK_KEY_PRESS)
            {
              /* AltGr press. */
              if (keys[i].group)
                {
                  /* According to some virtualbox code I found, AltGr is
                   * simulated on win32 with LCtrl+RAlt */
                  ip.ki.wVk = VK_CONTROL;
                  SendInput(1, &ip, sizeof(INPUT));
                  ip.ki.wVk = VK_MENU;
                  SendInput(1, &ip, sizeof(INPUT));
                }
              /* Shift press. */
              if (keys[i].level || (modifiers & GDK_SHIFT_MASK))
                {
                  ip.ki.wVk = VK_SHIFT;
                  SendInput(1, &ip, sizeof(INPUT));
                }
            }

          /* Key pressed/released. */
          ip.ki.wVk = keys[i].keycode;
          SendInput(1, &ip, sizeof(INPUT));

          if (key_pressrelease == GDK_KEY_RELEASE)
            {
              /* Shift release. */
              if (keys[i].level || (modifiers & GDK_SHIFT_MASK))
                {
                  ip.ki.wVk = VK_SHIFT;
                  SendInput(1, &ip, sizeof(INPUT));
                }
              /* AltrGr release. */
              if (keys[i].group)
                {
                  ip.ki.wVk = VK_MENU;
                  SendInput(1, &ip, sizeof(INPUT));
                  ip.ki.wVk = VK_CONTROL;
                  SendInput(1, &ip, sizeof(INPUT));
                }
            }

          /* No need to loop for alternative keycodes. We want only one
           * key generated. */
          success = TRUE;
          break;
        }
      g_free (keys);
    }
  return success;
}

gboolean
gdk_test_simulate_button (GdkWindow      *window,
                          gint            x,
                          gint            y,
                          guint           button, /*1..3*/
                          GdkModifierType modifiers,
                          GdkEventType    button_pressrelease)
{
  g_return_val_if_fail (button_pressrelease == GDK_BUTTON_PRESS || button_pressrelease == GDK_BUTTON_RELEASE, FALSE);
  g_return_val_if_fail (window != NULL, FALSE);

  return FALSE;
}
