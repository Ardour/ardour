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
#include <x11/gdkx.h>
#include "gdkalias.h"

#include <X11/Xlib.h>

/**
 * gdk_test_render_sync:
 * @window: a mapped #GdkWindow
 *
 * This function retrieves a pixel from @window to force the windowing
 * system to carry out any pending rendering commands.
 * This function is intended to be used to syncronize with rendering
 * pipelines, to benchmark windowing system rendering operations.
 *
 * Since: 2.14
 **/
void
gdk_test_render_sync (GdkWindow *window)
{
  static GdkImage *p1image = NULL;
  /* syncronize to X drawing queue, see:
   * http://mail.gnome.org/archives/gtk-devel-list/2006-October/msg00103.html
   */
  p1image = gdk_drawable_copy_to_image (window, p1image, 0, 0, 0, 0, 1, 1);
}

/**
 * gdk_test_simulate_key
 * @window: a #GdkWindow to simulate a key event for.
 * @x:      x coordinate within @window for the key event.
 * @y:      y coordinate within @window for the key event.
 * @keyval: A GDK keyboard value.
 * @modifiers: Keyboard modifiers the event is setup with.
 * @key_pressrelease: either %GDK_KEY_PRESS or %GDK_KEY_RELEASE
 *
 * This function is intended to be used in GTK+ test programs.
 * If (@x,@y) are > (-1,-1), it will warp the mouse pointer to
 * the given (@x,@y) corrdinates within @window and simulate a
 * key press or release event.
 *
 * When the mouse pointer is warped to the target location, use
 * of this function outside of test programs that run in their
 * own virtual windowing system (e.g. Xvfb) is not recommended.
 * If (@x,@y) are passed as (-1,-1), the mouse pointer will not
 * be warped and @window origin will be used as mouse pointer
 * location for the event.
 *
 * Also, gtk_test_simulate_key() is a fairly low level function,
 * for most testing purposes, gtk_test_widget_send_key() is the
 * right function to call which will generate a key press event
 * followed by its accompanying key release event.
 *
 * Returns: whether all actions neccessary for a key event simulation 
 *     were carried out successfully.
 *
 * Since: 2.14
 **/
gboolean
gdk_test_simulate_key (GdkWindow      *window,
                       gint            x,
                       gint            y,
                       guint           keyval,
                       GdkModifierType modifiers,
                       GdkEventType    key_pressrelease)
{
  GdkScreen *screen;
  GdkKeymapKey *keys = NULL;
  GdkWindowObject *priv;
  gboolean success;
  gint n_keys = 0;
  XKeyEvent xev = {
    0,  /* type */
    0,  /* serial */
    1,  /* send_event */
  };
  g_return_val_if_fail (key_pressrelease == GDK_KEY_PRESS || key_pressrelease == GDK_KEY_RELEASE, FALSE);
  g_return_val_if_fail (window != NULL, FALSE);
  if (!GDK_WINDOW_IS_MAPPED (window))
    return FALSE;
  screen = gdk_colormap_get_screen (gdk_drawable_get_colormap (window));
  if (x < 0 && y < 0)
    {
      gdk_drawable_get_size (window, &x, &y);
      x /= 2;
      y /= 2;
    }

  priv = (GdkWindowObject *)window;
  /* Convert to impl coordinates */
  x = x + priv->abs_x;
  y = y + priv->abs_y;

  xev.type = key_pressrelease == GDK_KEY_PRESS ? KeyPress : KeyRelease;
  xev.display = GDK_DRAWABLE_XDISPLAY (window);
  xev.window = GDK_WINDOW_XID (window);
  xev.root = RootWindow (xev.display, GDK_SCREEN_XNUMBER (screen));
  xev.subwindow = 0;
  xev.time = 0;
  xev.x = MAX (x, 0);
  xev.y = MAX (y, 0);
  xev.x_root = 0;
  xev.y_root = 0;
  xev.state = modifiers;
  xev.keycode = 0;
  success = gdk_keymap_get_entries_for_keyval (gdk_keymap_get_for_display (gdk_drawable_get_display (window)), keyval, &keys, &n_keys);
  success &= n_keys > 0;
  if (success)
    {
      gint i;
      for (i = 0; i < n_keys; i++)
        if (keys[i].group == 0 && (keys[i].level == 0 || keys[i].level == 1))
          {
            xev.keycode = keys[i].keycode;
            if (keys[i].level == 1)
              {
                /* Assume shift takes us to level 1 */
                xev.state |= GDK_SHIFT_MASK;
              }
            break;
          }
      if (i >= n_keys) /* no match for group==0 and level==0 or 1 */
        xev.keycode = keys[0].keycode;
    }
  g_free (keys);
  if (!success)
    return FALSE;
  gdk_error_trap_push ();
  xev.same_screen = XTranslateCoordinates (xev.display, xev.window, xev.root,
                                           xev.x, xev.y, &xev.x_root, &xev.y_root,
                                           &xev.subwindow);
  if (!xev.subwindow)
    xev.subwindow = xev.window;
  success &= xev.same_screen;
  if (x >= 0 && y >= 0)
    success &= 0 != XWarpPointer (xev.display, None, xev.window, 0, 0, 0, 0, xev.x, xev.y);
  success &= 0 != XSendEvent (xev.display, xev.window, True, key_pressrelease == GDK_KEY_PRESS ? KeyPressMask : KeyReleaseMask, (XEvent*) &xev);
  XSync (xev.display, False);
  success &= 0 == gdk_error_trap_pop();
  return success;
}

/**
 * gdk_test_simulate_button
 * @window: a #GdkWindow to simulate a button event for.
 * @x:      x coordinate within @window for the button event.
 * @y:      y coordinate within @window for the button event.
 * @button: Number of the pointer button for the event, usually 1, 2 or 3.
 * @modifiers: Keyboard modifiers the event is setup with.
 * @button_pressrelease: either %GDK_BUTTON_PRESS or %GDK_BUTTON_RELEASE
 *
 * This function is intended to be used in GTK+ test programs.
 * It will warp the mouse pointer to the given (@x,@y) corrdinates
 * within @window and simulate a button press or release event.
 * Because the mouse pointer needs to be warped to the target
 * location, use of this function outside of test programs that
 * run in their own virtual windowing system (e.g. Xvfb) is not
 * recommended.
 *
 * Also, gtk_test_simulate_button() is a fairly low level function,
 * for most testing purposes, gtk_test_widget_click() is the right
 * function to call which will generate a button press event followed
 * by its accompanying button release event.
 *
 * Returns: whether all actions neccessary for a button event simulation 
 *     were carried out successfully.
 *
 * Since: 2.14
 **/
gboolean
gdk_test_simulate_button (GdkWindow      *window,
                          gint            x,
                          gint            y,
                          guint           button, /*1..3*/
                          GdkModifierType modifiers,
                          GdkEventType    button_pressrelease)
{
  GdkScreen *screen;
  XButtonEvent xev = {
    0,  /* type */
    0,  /* serial */
    1,  /* send_event */
  };
  gboolean success;
  GdkWindowObject *priv;

  g_return_val_if_fail (button_pressrelease == GDK_BUTTON_PRESS || button_pressrelease == GDK_BUTTON_RELEASE, FALSE);
  g_return_val_if_fail (window != NULL, FALSE);

  if (!GDK_WINDOW_IS_MAPPED (window))
    return FALSE;
  screen = gdk_colormap_get_screen (gdk_drawable_get_colormap (window));
  if (x < 0 && y < 0)
    {
      gdk_drawable_get_size (window, &x, &y);
      x /= 2;
      y /= 2;
    }

  priv = (GdkWindowObject *)window;
  /* Convert to impl coordinates */
  x = x + priv->abs_x;
  y = y + priv->abs_y;

  xev.type = button_pressrelease == GDK_BUTTON_PRESS ? ButtonPress : ButtonRelease;
  xev.display = GDK_DRAWABLE_XDISPLAY (window);
  xev.window = GDK_WINDOW_XID (window);
  xev.root = RootWindow (xev.display, GDK_SCREEN_XNUMBER (screen));
  xev.subwindow = 0;
  xev.time = 0;
  xev.x = x;
  xev.y = y;
  xev.x_root = 0;
  xev.y_root = 0;
  xev.state = modifiers;
  xev.button = button;
  gdk_error_trap_push ();
  xev.same_screen = XTranslateCoordinates (xev.display, xev.window, xev.root,
                                           xev.x, xev.y, &xev.x_root, &xev.y_root,
                                           &xev.subwindow);
  if (!xev.subwindow)
    xev.subwindow = xev.window;
  success = xev.same_screen;
  success &= 0 != XWarpPointer (xev.display, None, xev.window, 0, 0, 0, 0, xev.x, xev.y);
  success &= 0 != XSendEvent (xev.display, xev.window, True, button_pressrelease == GDK_BUTTON_PRESS ? ButtonPressMask : ButtonReleaseMask, (XEvent*) &xev);
  XSync (xev.display, False);
  success &= 0 == gdk_error_trap_pop();
  return success;
}

#define __GDK_TEST_UTILS_X11_C__
#include "gdkaliasdef.c"
