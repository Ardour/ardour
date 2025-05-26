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

#include <ytk/ytk.h>
#include <ydk/gdkkeysyms.h>

#include <Carbon/Carbon.h>

#include <gtkmm2ext/sync-menu.h>


/* TODO
 *
 * - Sync adding/removing/reordering items
 * - Create on demand? (can this be done with gtk+? ie fill in menu
     items when the menu is opened)
 * - Figure out what to do per app/window...
 *
 */

#define IGE_QUARTZ_MENU_CREATOR 'IGEC'
#define IGE_QUARTZ_ITEM_WIDGET  'IWID'


static void   sync_menu_shell (GtkMenuShell *menu_shell,
			       MenuRef       carbon_menu,
			       gboolean      toplevel,
			       gboolean      debug);


/*
 * utility functions
 */

static GtkWidget *
find_menu_label (GtkWidget *widget)
{
  GtkWidget *label = NULL;

  if (GTK_IS_LABEL (widget))
    return widget;

  if (GTK_IS_CONTAINER (widget))
    {
      GList *children;
      GList *l;

      children = gtk_container_get_children (GTK_CONTAINER (widget));

      for (l = children; l; l = l->next)
	{
	  label = find_menu_label (l->data);
	  if (label)
	    break;
	}

      g_list_free (children);
    }

  return label;
}

static const gchar *
get_menu_label_text (GtkWidget  *menu_item,
		     GtkWidget **label)
{
  GtkWidget *my_label;

  my_label = find_menu_label (menu_item);
  if (label)
    *label = my_label;

  if (my_label)
    return gtk_label_get_text (GTK_LABEL (my_label));

  return NULL;
}

static gboolean
accel_find_func (GtkAccelKey *key,
		 GClosure    *closure,
		 gpointer     data)
{
  return (GClosure *) data == closure;
}


/*
 * CarbonMenu functions
 */

typedef struct
{
  MenuRef menu;
} CarbonMenu;

static GQuark carbon_menu_quark = 0;

static CarbonMenu *
carbon_menu_new (void)
{
  return g_slice_new0 (CarbonMenu);
}

static void
carbon_menu_free (CarbonMenu *menu)
{
  g_slice_free (CarbonMenu, menu);
}

static CarbonMenu *
carbon_menu_get (GtkWidget *widget)
{
  return g_object_get_qdata (G_OBJECT (widget), carbon_menu_quark);
}

static void
carbon_menu_connect (GtkWidget *menu,
		     MenuRef    menuRef)
{
  CarbonMenu *carbon_menu = carbon_menu_get (menu);

  if (!carbon_menu)
    {
      carbon_menu = carbon_menu_new ();

      g_object_set_qdata_full (G_OBJECT (menu), carbon_menu_quark,
			       carbon_menu,
			       (GDestroyNotify) carbon_menu_free);
    }

  carbon_menu->menu = menuRef;
}


/*
 * CarbonMenuItem functions
 */

typedef struct
{
  MenuRef        menu;
  MenuItemIndex  index;
  MenuRef        submenu;
  GClosure      *accel_closure;
} CarbonMenuItem;

static GQuark carbon_menu_item_quark = 0;

static CarbonMenuItem *
carbon_menu_item_new (void)
{
  return g_slice_new0 (CarbonMenuItem);
}

static void
carbon_menu_item_free (CarbonMenuItem *menu_item)
{
  if (menu_item->accel_closure)
    g_closure_unref (menu_item->accel_closure);

  g_slice_free (CarbonMenuItem, menu_item);
}

static CarbonMenuItem *
carbon_menu_item_get (GtkWidget *widget)
{
  return g_object_get_qdata (G_OBJECT (widget), carbon_menu_item_quark);
}

static void
carbon_menu_item_update_state (CarbonMenuItem *carbon_item,
			       GtkWidget      *widget)
{
  gboolean sensitive;
  gboolean visible;
  UInt32   set_attrs = 0;
  UInt32   clear_attrs = 0;

  g_object_get (widget,
                "sensitive", &sensitive,
                "visible",   &visible,
                NULL);

  if (!sensitive)
    set_attrs |= kMenuItemAttrDisabled;
  else
    clear_attrs |= kMenuItemAttrDisabled;

  if (!visible)
    set_attrs |= kMenuItemAttrHidden;
  else
    clear_attrs |= kMenuItemAttrHidden;

  ChangeMenuItemAttributes (carbon_item->menu, carbon_item->index,
                            set_attrs, clear_attrs);
}

static void
carbon_menu_item_update_active (CarbonMenuItem *carbon_item,
				GtkWidget      *widget)
{
  gboolean active;

  g_object_get (widget,
                "active", &active,
                NULL);

  CheckMenuItem (carbon_item->menu, carbon_item->index,
		 active);
}

static void
carbon_menu_item_update_submenu (CarbonMenuItem *carbon_item,
				 GtkWidget      *widget)
{
  GtkWidget *submenu;

  submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget));

  if (submenu)
    {
      const gchar *label_text;
      CFStringRef  cfstr = NULL;

      label_text = get_menu_label_text (widget, NULL);
      if (label_text)
        cfstr = CFStringCreateWithCString (NULL, label_text,
					   kCFStringEncodingUTF8);

      CreateNewMenu (0, 0, &carbon_item->submenu);
      SetMenuTitleWithCFString (carbon_item->submenu, cfstr);
      SetMenuItemHierarchicalMenu (carbon_item->menu, carbon_item->index,
				   carbon_item->submenu);

      sync_menu_shell (GTK_MENU_SHELL (submenu), carbon_item->submenu, FALSE, FALSE);

      if (cfstr)
	CFRelease (cfstr);
    }
  else
    {
      SetMenuItemHierarchicalMenu (carbon_item->menu, carbon_item->index,
				   NULL);
      carbon_item->submenu = NULL;
    }
}

static void
carbon_menu_item_update_label (CarbonMenuItem *carbon_item,
			       GtkWidget      *widget)
{
  const gchar *label_text;
  CFStringRef  cfstr = NULL;

  label_text = get_menu_label_text (widget, NULL);
  if (label_text)
    cfstr = CFStringCreateWithCString (NULL, label_text,
				       kCFStringEncodingUTF8);

  SetMenuItemTextWithCFString (carbon_item->menu, carbon_item->index,
			       cfstr);

  if (cfstr)
    CFRelease (cfstr);
}

static void
carbon_menu_item_update_accelerator (CarbonMenuItem *carbon_item,
				     GtkWidget      *widget)
{
  GtkWidget *label;

  get_menu_label_text (widget, &label);

  if (GTK_IS_ACCEL_LABEL (label) &&
      GTK_ACCEL_LABEL (label)->accel_closure)
    {
      GtkAccelKey *key;

      key = gtk_accel_group_find (GTK_ACCEL_LABEL (label)->accel_group,
				  accel_find_func,
				  GTK_ACCEL_LABEL (label)->accel_closure);

      if (key            &&
	  key->accel_key &&
	  key->accel_flags & GTK_ACCEL_VISIBLE)
	{
	  GdkDisplay      *display = gtk_widget_get_display (widget);
	  GdkKeymap       *keymap  = gdk_keymap_get_for_display (display);
	  GdkKeymapKey    *keys;
	  gint             n_keys;
	  gint             use_command;
	  gboolean         add_modifiers = FALSE;
	  UInt8            modifiers = 0; /* implies Command key */

	  if (gdk_keymap_get_entries_for_keyval (keymap, key->accel_key,
						 &keys, &n_keys) == 0)
            {
		  gint realkey = -1;

		  switch (key->accel_key) {
		  case GDK_rightarrow:
		  case GDK_Right:
			  realkey = kRightArrowCharCode;
			  break;
		  case GDK_leftarrow:
		  case GDK_Left:
			  realkey = kLeftArrowCharCode;
			  break;
		  case GDK_uparrow:
		  case GDK_Up:
			  realkey = kUpArrowCharCode;
			  break;
		  case GDK_downarrow:
		  case GDK_Down:
			  realkey = kDownArrowCharCode;
			  break;
		  default:
			  break;
		  }

		  if (realkey != -1) {
			  SetMenuItemCommandKey (carbon_item->menu, carbon_item->index,
						 false, realkey);
			  add_modifiers = TRUE;
		  }

	    } else {
                 SetMenuItemCommandKey (carbon_item->menu, carbon_item->index, true, keys[0].keycode);
                 if (keys[0].level == 1) {
			 /* regular key, but it needs shift to make it work */
			 modifiers |= kMenuShiftModifier;
		 }

		 g_free (keys);
		 add_modifiers = TRUE;
	    }

	  if (add_modifiers)
	    {
  	     UInt8 modifiers = 0; /* implies Command key */

	      use_command = 0;

	      if (key->accel_mods)
		{
		  if (key->accel_mods & GDK_SHIFT_MASK) {
		    modifiers |= kMenuShiftModifier;
		  }

		  /* gdk/quartz maps Alt/Option to Mod1 */

		  if (key->accel_mods & (GDK_MOD1_MASK)) {
		    modifiers |= kMenuOptionModifier;
		  }

		  if (key->accel_mods & GDK_CONTROL_MASK) {
		    modifiers |= kMenuControlModifier;
		  }

		  /* gdk/quartz maps Command to Meta */

		  if (key->accel_mods & GDK_META_MASK) {
			  use_command = 1;
		  }
		}

	      if (!use_command)
  	        modifiers |= kMenuNoCommandModifier;

	      SetMenuItemModifiers (carbon_item->menu, carbon_item->index,
				    modifiers);

	      return;
	    }
	}
    }

  /*  otherwise, clear the menu shortcut  */
  SetMenuItemModifiers (carbon_item->menu, carbon_item->index,
			kMenuNoModifiers | kMenuNoCommandModifier);
  ChangeMenuItemAttributes (carbon_item->menu, carbon_item->index,
			    0, kMenuItemAttrUseVirtualKey);
  SetMenuItemCommandKey (carbon_item->menu, carbon_item->index,
			 false, 0);
}

static void
carbon_menu_item_accel_changed (GtkAccelGroup   *accel_group,
				guint            keyval,
				GdkModifierType  modifier,
				GClosure        *accel_closure,
				GtkWidget       *widget)
{
  CarbonMenuItem *carbon_item = carbon_menu_item_get (widget);
  GtkWidget      *label;

  get_menu_label_text (widget, &label);

  if (GTK_IS_ACCEL_LABEL (label) &&
      GTK_ACCEL_LABEL (label)->accel_closure == accel_closure)
    carbon_menu_item_update_accelerator (carbon_item, widget);
}

static void
carbon_menu_item_update_accel_closure (CarbonMenuItem *carbon_item,
				       GtkWidget      *widget)
{
  GtkAccelGroup *group;
  GtkWidget     *label;

  get_menu_label_text (widget, &label);

  if (carbon_item->accel_closure)
    {
      group = gtk_accel_group_from_accel_closure (carbon_item->accel_closure);

      g_signal_handlers_disconnect_by_func (group,
					    carbon_menu_item_accel_changed,
					    widget);

      g_closure_unref (carbon_item->accel_closure);
      carbon_item->accel_closure = NULL;
    }

  if (GTK_IS_ACCEL_LABEL (label))
    carbon_item->accel_closure = GTK_ACCEL_LABEL (label)->accel_closure;

  if (carbon_item->accel_closure)
    {
      g_closure_ref (carbon_item->accel_closure);

      group = gtk_accel_group_from_accel_closure (carbon_item->accel_closure);

      g_signal_connect_object (group, "accel-changed",
			       G_CALLBACK (carbon_menu_item_accel_changed),
			       widget, 0);
    }

  carbon_menu_item_update_accelerator (carbon_item, widget);
}

static void
carbon_menu_item_notify (GObject        *object,
			 GParamSpec     *pspec,
			 CarbonMenuItem *carbon_item)
{
  if (!strcmp (pspec->name, "sensitive") ||
      !strcmp (pspec->name, "visible"))
    {
      carbon_menu_item_update_state (carbon_item, GTK_WIDGET (object));
    }
  else if (!strcmp (pspec->name, "active"))
    {
      carbon_menu_item_update_active (carbon_item, GTK_WIDGET (object));
    }
  else if (!strcmp (pspec->name, "submenu"))
    {
      carbon_menu_item_update_submenu (carbon_item, GTK_WIDGET (object));
    }
}

static void
carbon_menu_item_notify_label (GObject    *object,
			       GParamSpec *pspec,
			       gpointer    data)
{
  CarbonMenuItem *carbon_item = carbon_menu_item_get (GTK_WIDGET (object));

  if (!strcmp (pspec->name, "label"))
    {
      carbon_menu_item_update_label (carbon_item,
				     GTK_WIDGET (object));
    }
  else if (!strcmp (pspec->name, "accel-closure"))
    {
      carbon_menu_item_update_accel_closure (carbon_item,
					     GTK_WIDGET (object));
    }
}

static CarbonMenuItem *
carbon_menu_item_connect (GtkWidget     *menu_item,
			  GtkWidget     *label,
			  MenuRef        menu,
			  MenuItemIndex  index)
{
  CarbonMenuItem *carbon_item = carbon_menu_item_get (menu_item);

  if (!carbon_item)
    {
      carbon_item = carbon_menu_item_new ();

      g_object_set_qdata_full (G_OBJECT (menu_item), carbon_menu_item_quark,
			       carbon_item,
			       (GDestroyNotify) carbon_menu_item_free);

      g_signal_connect (menu_item, "notify",
                        G_CALLBACK (carbon_menu_item_notify),
                        carbon_item);

      if (label)
	g_signal_connect_swapped (label, "notify::label",
				  G_CALLBACK (carbon_menu_item_notify_label),
				  menu_item);
    }

  carbon_item->menu  = menu;
  carbon_item->index = index;

  return carbon_item;
}


/*
 * carbon event handler
 */

static int _in_carbon_menu_event_handler = 0;

int
gdk_quartz_in_carbon_menu_event_handler ()
{
	return _in_carbon_menu_event_handler;
}

static gboolean
dummy_gtk_menu_item_activate (gpointer *arg)
{
	gtk_menu_item_activate (GTK_MENU_ITEM(arg));
	return FALSE;
}

static OSStatus
menu_event_handler_func (EventHandlerCallRef  event_handler_call_ref,
			 EventRef             event_ref,
			 void                *data)
{
  UInt32  event_class = GetEventClass (event_ref);
  UInt32  event_kind = GetEventKind (event_ref);
  MenuRef menu_ref;
  OSStatus ret;

  _in_carbon_menu_event_handler = 1;

  switch (event_class)
    {
    case kEventClassCommand:
      /* This is called when activating (is that the right GTK+ term?)
       * a menu item.
       */
      if (event_kind == kEventCommandProcess)
	{
	  HICommand command;
	  OSStatus  err;

	  /*g_printerr ("Menu: kEventClassCommand/kEventCommandProcess\n");*/

	  err = GetEventParameter (event_ref, kEventParamDirectObject,
				   typeHICommand, 0,
				   sizeof (command), 0, &command);

	  if (err == noErr)
	    {
	      GtkWidget *widget = NULL;

	      /* Get any GtkWidget associated with the item. */
	      err = GetMenuItemProperty (command.menu.menuRef,
					 command.menu.menuItemIndex,
					 IGE_QUARTZ_MENU_CREATOR,
					 IGE_QUARTZ_ITEM_WIDGET,
					 sizeof (widget), 0, &widget);
	      if (err == noErr && GTK_IS_WIDGET (widget))
		{
		  g_idle_add ((GSourceFunc) dummy_gtk_menu_item_activate, widget);
		  // gtk_menu_item_activate (GTK_MENU_ITEM (widget));
		  _in_carbon_menu_event_handler = 0;
		  return noErr;
		}
	    }
	}
      break;

    case kEventClassMenu:
      GetEventParameter (event_ref,
			 kEventParamDirectObject,
			 typeMenuRef,
			 NULL,
			 sizeof (menu_ref),
			 NULL,
			 &menu_ref);

      switch (event_kind)
	{
	case kEventMenuTargetItem:
	  /* This is called when an item is selected (what is the
	   * GTK+ term? prelight?)
	   */
	  /*g_printerr ("kEventClassMenu/kEventMenuTargetItem\n");*/
	  break;

	case kEventMenuOpening:
	  /* Is it possible to dynamically build the menu here? We
	   * can at least set visibility/sensitivity.
	   */
	  /*g_printerr ("kEventClassMenu/kEventMenuOpening\n");*/
	  break;

	case kEventMenuClosed:
	  /*g_printerr ("kEventClassMenu/kEventMenuClosed\n");*/
	  break;

	default:
	  break;
	}

      break;

    default:
      break;
    }

  ret = CallNextEventHandler (event_handler_call_ref, event_ref);
  _in_carbon_menu_event_handler = 0;
  return ret;
}

static void
setup_menu_event_handler (void)
{
  EventHandlerUPP menu_event_handler_upp;
  EventHandlerRef menu_event_handler_ref;
  const EventTypeSpec menu_events[] = {
    { kEventClassCommand, kEventCommandProcess },
    { kEventClassMenu, kEventMenuTargetItem },
    { kEventClassMenu, kEventMenuOpening },
    { kEventClassMenu, kEventMenuClosed }
  };

  /* FIXME: We might have to install one per window? */

  menu_event_handler_upp = NewEventHandlerUPP (menu_event_handler_func);
  InstallEventHandler (GetApplicationEventTarget (), menu_event_handler_upp,
		       GetEventTypeCount (menu_events), menu_events, 0,
		       &menu_event_handler_ref);

#if 0
  /* FIXME: Remove the handler with: */
  RemoveEventHandler(menu_event_handler_ref);
  DisposeEventHandlerUPP(menu_event_handler_upp);
#endif
}

static void
sync_menu_shell (GtkMenuShell *menu_shell,
                 MenuRef       carbon_menu,
		 gboolean      toplevel,
		 gboolean      debug)
{
  GList         *children;
  GList         *l;
  MenuItemIndex  carbon_index = 1;

  if (debug)
    g_printerr ("%s: syncing shell %p\n", G_STRFUNC, menu_shell);

  carbon_menu_connect (GTK_WIDGET (menu_shell), carbon_menu);

  children = gtk_container_get_children (GTK_CONTAINER (menu_shell));

  for (l = children; l; l = l->next)
    {
      GtkWidget      *menu_item = l->data;
      CarbonMenuItem *carbon_item;

      if (GTK_IS_TEAROFF_MENU_ITEM (menu_item))
	continue;

      if (toplevel && g_object_get_data (G_OBJECT (menu_item),
					 "gtk-empty-menu-item"))
	continue;

      carbon_item = carbon_menu_item_get (menu_item);

      if (debug)
	g_printerr ("%s: carbon_item %d for menu_item %d (%s, %s)\n",
		    G_STRFUNC, carbon_item ? carbon_item->index : -1,
		    carbon_index, get_menu_label_text (menu_item, NULL),
		    g_type_name (G_TYPE_FROM_INSTANCE (menu_item)));

      if (carbon_item && carbon_item->index != carbon_index)
	{
	  if (debug)
	    g_printerr ("%s:   -> not matching, deleting\n", G_STRFUNC);

	  DeleteMenuItem (carbon_item->menu, carbon_index);
	  carbon_item = NULL;
	}

      if (!carbon_item)
	{
	  GtkWidget          *label      = NULL;
	  const gchar        *label_text;
	  CFStringRef         cfstr      = NULL;
	  MenuItemAttributes  attributes = 0;

	  if (debug)
	    g_printerr ("%s:   -> creating new\n", G_STRFUNC);

	  label_text = get_menu_label_text (menu_item, &label);
	  if (label_text)
	    cfstr = CFStringCreateWithCString (NULL, label_text,
					       kCFStringEncodingUTF8);

	  if (GTK_IS_SEPARATOR_MENU_ITEM (menu_item))
	    attributes |= kMenuItemAttrSeparator;

	  if (!GTK_WIDGET_IS_SENSITIVE (menu_item))
	    attributes |= kMenuItemAttrDisabled;

	  if (!GTK_WIDGET_VISIBLE (menu_item))
	    attributes |= kMenuItemAttrHidden;

	  InsertMenuItemTextWithCFString (carbon_menu, cfstr,
					  carbon_index - 1,
					  attributes, 0);
	  SetMenuItemProperty (carbon_menu, carbon_index,
			       IGE_QUARTZ_MENU_CREATOR,
			       IGE_QUARTZ_ITEM_WIDGET,
			       sizeof (menu_item), &menu_item);

	  if (cfstr)
	    CFRelease (cfstr);

	  carbon_item = carbon_menu_item_connect (menu_item, label,
						  carbon_menu,
						  carbon_index);

	  if (GTK_IS_CHECK_MENU_ITEM (menu_item))
	    carbon_menu_item_update_active (carbon_item, menu_item);

	  carbon_menu_item_update_accel_closure (carbon_item, menu_item);

	  if (gtk_menu_item_get_submenu (GTK_MENU_ITEM (menu_item)))
	    carbon_menu_item_update_submenu (carbon_item, menu_item);
	}

      carbon_index++;
    }

  g_list_free (children);
}


static gulong emission_hook_id = 0;

static gboolean
parent_set_emission_hook (GSignalInvocationHint *ihint,
			  guint                  n_param_values,
			  const GValue          *param_values,
			  gpointer               data)
{
  GtkWidget *instance = g_value_get_object (param_values);

  if (GTK_IS_MENU_ITEM (instance))
    {
      GtkWidget *previous_parent = g_value_get_object (param_values + 1);
      GtkWidget *menu_shell      = NULL;

      if (GTK_IS_MENU_SHELL (previous_parent))
	{
	  menu_shell = previous_parent;
        }
      else if (GTK_IS_MENU_SHELL (instance->parent))
	{
	  menu_shell = instance->parent;
	}

      if (menu_shell)
        {
	  CarbonMenu *carbon_menu = carbon_menu_get (menu_shell);

	  if (carbon_menu)
	    {
#if 0
	      g_printerr ("%s: item %s %p (%s, %s)\n", G_STRFUNC,
			  previous_parent ? "removed from" : "added to",
			  menu_shell,
			  get_menu_label_text (instance, NULL),
			  g_type_name (G_TYPE_FROM_INSTANCE (instance)));
#endif

	      sync_menu_shell (GTK_MENU_SHELL (menu_shell),
			       carbon_menu->menu,
			       carbon_menu->menu == (MenuRef) data,
			       FALSE);
	    }
        }
    }

  return TRUE;
}

static void
parent_set_emission_hook_remove (GtkWidget *widget,
				 gpointer   data)
{
  g_signal_remove_emission_hook (g_signal_lookup ("parent-set",
						  GTK_TYPE_WIDGET),
				 emission_hook_id);
}


/*
 * public functions
 */

void
ige_mac_menu_set_menu_bar (GtkMenuShell *menu_shell)
{
  MenuRef carbon_menubar;

  g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));

  if (carbon_menu_quark == 0)
    carbon_menu_quark = g_quark_from_static_string ("CarbonMenu");

  if (carbon_menu_item_quark == 0)
    carbon_menu_item_quark = g_quark_from_static_string ("CarbonMenuItem");

  CreateNewMenu (0 /*id*/, 0 /*options*/, &carbon_menubar);
  SetRootMenu (carbon_menubar);

  setup_menu_event_handler ();

  emission_hook_id =
    g_signal_add_emission_hook (g_signal_lookup ("parent-set",
						 GTK_TYPE_WIDGET),
				0,
				parent_set_emission_hook,
				carbon_menubar, NULL);

  g_signal_connect (menu_shell, "destroy",
		    G_CALLBACK (parent_set_emission_hook_remove),
		    NULL);

  sync_menu_shell (menu_shell, carbon_menubar, TRUE, FALSE);
}

void
ige_mac_menu_set_quit_menu_item (GtkMenuItem *menu_item)
{
  MenuRef       appmenu;
  MenuItemIndex index;

  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  if (GetIndMenuItemWithCommandID (NULL, kHICommandQuit, 1,
                                   &appmenu, &index) == noErr)
    {
      SetMenuItemCommandID (appmenu, index, 0);
      SetMenuItemProperty (appmenu, index,
                           IGE_QUARTZ_MENU_CREATOR,
                           IGE_QUARTZ_ITEM_WIDGET,
                           sizeof (menu_item), &menu_item);

      gtk_widget_hide (GTK_WIDGET (menu_item));
    }
}


struct _IgeMacMenuGroup
{
  GList *items;
};

static GList *app_menu_groups = NULL;

IgeMacMenuGroup *
ige_mac_menu_add_app_menu_group (void)
{
  IgeMacMenuGroup *group = g_slice_new0 (IgeMacMenuGroup);

  app_menu_groups = g_list_append (app_menu_groups, group);

  return group;
}

void
ige_mac_menu_add_app_menu_item (IgeMacMenuGroup *group,
				GtkMenuItem     *menu_item,
				const gchar     *label)
{
  MenuRef  appmenu;
  GList   *list;
  gint     index = 0;

  g_return_if_fail (group != NULL);
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  if (GetIndMenuItemWithCommandID (NULL, kHICommandHide, 1,
                                   &appmenu, NULL) != noErr)
    {
      g_warning ("%s: retrieving app menu failed",
		 G_STRFUNC);
      return;
    }

  for (list = app_menu_groups; list; list = g_list_next (list))
    {
      IgeMacMenuGroup *list_group = list->data;

      index += g_list_length (list_group->items);

      /*  adjust index for the separator between groups, but not
       *  before the first group
       */
      if (list_group->items && list->prev)
	index++;

      if (group == list_group)
	{
	  CFStringRef cfstr;

	  /*  add a separator before adding the first item, but not
	   *  for the first group
	   */
	  if (!group->items && list->prev)
	    {
	      InsertMenuItemTextWithCFString (appmenu, NULL, index,
					      kMenuItemAttrSeparator, 0);
	      index++;
	    }

	  if (!label)
	    label = get_menu_label_text (GTK_WIDGET (menu_item), NULL);

	  cfstr = CFStringCreateWithCString (NULL, label,
					     kCFStringEncodingUTF8);

	  InsertMenuItemTextWithCFString (appmenu, cfstr, index, 0, 0);
	  SetMenuItemProperty (appmenu, index + 1,
			       IGE_QUARTZ_MENU_CREATOR,
			       IGE_QUARTZ_ITEM_WIDGET,
			       sizeof (menu_item), &menu_item);

	  CFRelease (cfstr);

	  gtk_widget_hide (GTK_WIDGET (menu_item));

	  group->items = g_list_append (group->items, menu_item);

	  return;
	}
    }

  if (!list)
    g_warning ("%s: app menu group %p does not exist",
	       G_STRFUNC, group);
}
