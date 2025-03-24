/* GTK+ Integration for the Mac OS X Menubar.
 *
 * Copyright (C) 2007 Pioneer Research Center USA, Inc.
 * Copyright (C) 2007 Imendio AB
 *
 * For further information, see:
 * http://developer.imendio.com/projects/gtk-macosx/menubar
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; version 2.1
 * of the License.
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

#ifndef __IGE_MAC_MENU_H__
#define __IGE_MAC_MENU_H__

#include <ytk/ytk.h>

#include "gtkmm2ext/visibility.h"

G_BEGIN_DECLS

typedef struct _IgeMacMenuGroup IgeMacMenuGroup;

LIBGTKMM2EXT_API void              ige_mac_menu_set_menu_bar       (GtkMenuShell    *menu_shell);
LIBGTKMM2EXT_API void              ige_mac_menu_set_quit_menu_item (GtkMenuItem     *menu_item);

LIBGTKMM2EXT_API IgeMacMenuGroup * ige_mac_menu_add_app_menu_group (void);
LIBGTKMM2EXT_API void              ige_mac_menu_add_app_menu_item  (IgeMacMenuGroup *group,
								    GtkMenuItem     *menu_item,
								    const gchar     *label);

G_END_DECLS

#endif /* __IGE_MAC_MENU_H__ */
