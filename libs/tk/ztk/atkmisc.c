/* ATK -  Accessibility Toolkit
 * Copyright 2007 Sun Microsystems Inc.
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

#include "atkmisc.h"

/**
 * SECTION:atkmisc
 * @Short_description: A set of ATK utility functions for thread locking
 * @Title:AtkMisc
 *
 * A set of utility functions for thread locking. This interface and
 * all his related methods are deprecated since 2.12.
 */

static void atk_misc_class_init (AtkMiscClass *klass);

GType
atk_misc_get_type (void)
{
  static GType type = 0;

  if (!type)
    {
      static const GTypeInfo typeInfo =
      {
        sizeof (AtkMiscClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) atk_misc_class_init,
        (GClassFinalizeFunc) NULL,
        NULL,
        sizeof (AtkMisc),
        0,
        (GInstanceInitFunc) NULL,
      } ;
      type = g_type_register_static (G_TYPE_OBJECT, "AtkMisc", &typeInfo, 0) ;
    }
  return type;
}

static void
atk_misc_class_init (AtkMiscClass *klass)
{
  klass->threads_enter = NULL;
  klass->threads_leave = NULL;
}

/**
 * atk_misc_threads_enter:
 * @misc: an AtkMisc instance for this application. 
 *
 * Take the thread mutex for the GUI toolkit, 
 * if one exists. 
 * (This method is implemented by the toolkit ATK implementation layer;
 *  for instance, for GTK+, GAIL implements this via GDK_THREADS_ENTER).
 *
 * Deprecated: Since 2.12.
 *
 * Since: 1.13
 *
 **/
void
atk_misc_threads_enter (AtkMisc *misc)
{
  AtkMiscClass *klass;

  if (misc == NULL)
    return;

  klass = ATK_MISC_GET_CLASS (misc);

  if (klass->threads_enter)
    {
      klass->threads_enter (misc);
    }
}

/**
 * atk_misc_threads_leave:
 * @misc: an AtkMisc instance for this application. 
 *
 * Release the thread mutex for the GUI toolkit, 
 * if one exists. This method, and atk_misc_threads_enter, 
 * are needed in some situations by threaded application code which 
 * services ATK requests, since fulfilling ATK requests often
 * requires calling into the GUI toolkit.  If a long-running or
 * potentially blocking call takes place inside such a block, it should
 * be bracketed by atk_misc_threads_leave/atk_misc_threads_enter calls.
 * (This method is implemented by the toolkit ATK implementation layer;
 *  for instance, for GTK+, GAIL implements this via GDK_THREADS_LEAVE).
 *
 * Deprecated: Since 2.12.
 *
 * Since: 1.13
 *
 **/
void
atk_misc_threads_leave (AtkMisc *misc)
{
  AtkMiscClass *klass;

  if (misc == NULL)
    return;

  klass = ATK_MISC_GET_CLASS (misc);

  if (klass->threads_leave)
    {
      klass->threads_leave (misc);
    }
}

AtkMisc *atk_misc_instance = NULL;

/**
 * atk_misc_get_instance:
 *
 * Obtain the singleton instance of AtkMisc for this application.
 * 
 * Since: 1.13
 *
 * Deprecated: Since 2.12.
 *
 * Returns: The singleton instance of AtkMisc for this application.
 *
 **/
const AtkMisc *
atk_misc_get_instance (void)
{
  return atk_misc_instance;
}
