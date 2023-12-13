/* ATK -  Accessibility Toolkit
 * Copyright (c) 2011 SUSE LINUX Products GmbH, Nuernberg, Germany.
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

#include "atkwindow.h"
#include "atkmarshal.h"

/**
 * SECTION:atkwindow
 * @Short_description: The ATK Interface provided by UI components that represent a top-level window.
 * @Title: AtkWindow
 * @See_also: #AtkObject
 *
 * #AtkWindow should be implemented by the UI elements that represent
 * a top-level window, such as the main window of an application or
 * dialog.
 *
 */

enum {
  ACTIVATE,
  CREATE,
  DEACTIVATE,
  DESTROY,
  MAXIMIZE,
  MINIMIZE,
  MOVE,
  RESIZE,
  RESTORE,
  LAST_SIGNAL
};

static guint atk_window_signals[LAST_SIGNAL] = { 0 };

static guint
atk_window_add_signal (const gchar *name)
{
  return g_signal_new (name,
		       ATK_TYPE_WINDOW,
		       G_SIGNAL_RUN_LAST,
		       0,
		       (GSignalAccumulator) NULL, NULL,
		       g_cclosure_marshal_VOID__VOID,
		       G_TYPE_NONE,
		       0);
}

typedef AtkWindowIface AtkWindowInterface;
G_DEFINE_INTERFACE (AtkWindow, atk_window, ATK_TYPE_OBJECT)

static void
atk_window_default_init (AtkWindowIface *iface)
{
  static gboolean initialized = FALSE;

  if (!initialized)
    {
      /**
       * AtkWindow::activate:
       * @object: the object which received the signal
       *
       * The signal #AtkWindow::activate is emitted when a window
       * becomes the active window of the application or session.
       *
       * Since: 2.2
       */
      atk_window_signals[ACTIVATE] = atk_window_add_signal ("activate");
      /**
       * AtkWindow::create:
       * @object: the object which received the signal
       *
       * The signal #AtkWindow::create is emitted when a new window
       * is created.
       *
       * Since: 2.2
       */
      atk_window_signals[CREATE] = atk_window_add_signal ("create");
      /**
       * AtkWindow::deactivate:
       * @object: the object which received the signal
       *
       * The signal #AtkWindow::deactivate is emitted when a window is
       * no longer the active window of the application or session.
       *
       * Since: 2.2
       */
      atk_window_signals[DEACTIVATE] = atk_window_add_signal ("deactivate");
      /**
       * AtkWindow::destroy:
       * @object: the object which received the signal
       *
       * The signal #AtkWindow::destroy is emitted when a window is
       * destroyed.
       *
       * Since: 2.2
       */
      atk_window_signals[DESTROY] = atk_window_add_signal ("destroy");
      /**
       * AtkWindow::maximize:
       * @object: the object which received the signal
       *
       * The signal #AtkWindow::maximize is emitted when a window
       * is maximized.
       *
       * Since: 2.2
       */
      atk_window_signals[MAXIMIZE] = atk_window_add_signal ("maximize");
      /**
       * AtkWindow::minimize:
       * @object: the object which received the signal
       *
       * The signal #AtkWindow::minimize is emitted when a window
       * is minimized.
       *
       * Since: 2.2
       */
      atk_window_signals[MINIMIZE] = atk_window_add_signal ("minimize");
      /**
       * AtkWindow::move:
       * @object: the object which received the signal
       *
       * The signal #AtkWindow::move is emitted when a window
       * is moved.
       *
       * Since: 2.2
       */
      atk_window_signals[MOVE] = atk_window_add_signal ("move");
      /**
       * AtkWindow::resize:
       * @object: the object which received the signal
       *
       * The signal #AtkWindow::resize is emitted when a window
       * is resized.
       *
       * Since: 2.2
       */
      atk_window_signals[RESIZE] = atk_window_add_signal ("resize");
      /**
       * AtkWindow::restore:
       * @object: the object which received the signal
       *
       * The signal #AtkWindow::restore is emitted when a window
       * is restored.
       *
       * Since: 2.2
       */
      atk_window_signals[RESTORE] = atk_window_add_signal ("restore");

      initialized = TRUE;
    }
}
