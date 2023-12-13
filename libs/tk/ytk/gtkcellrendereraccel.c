/* gtkcellrendereraccel.h
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
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
#include "gtkintl.h"
#include "gtkaccelgroup.h"
#include "gtkmarshalers.h"
#include "gtkcellrendereraccel.h"
#include "gtklabel.h"
#include "gtkeventbox.h"
#include "gtkmain.h"
#include "gtkprivate.h"
#include "gdk/gdkkeysyms.h"
#include "gtkalias.h"


static void gtk_cell_renderer_accel_get_property (GObject         *object,
                                                  guint            param_id,
                                                  GValue          *value,
                                                  GParamSpec      *pspec);
static void gtk_cell_renderer_accel_set_property (GObject         *object,
                                                  guint            param_id,
                                                  const GValue    *value,
                                                  GParamSpec      *pspec);
static void gtk_cell_renderer_accel_get_size     (GtkCellRenderer *cell,
                                                  GtkWidget       *widget,
                                                  GdkRectangle    *cell_area,
                                                  gint            *x_offset,
                                                  gint            *y_offset,
                                                  gint            *width,
                                                  gint            *height);
static GtkCellEditable *
           gtk_cell_renderer_accel_start_editing (GtkCellRenderer *cell,
                                                  GdkEvent        *event,
                                                  GtkWidget       *widget,
                                                  const gchar     *path,
                                                  GdkRectangle    *background_area,
                                                  GdkRectangle    *cell_area,
                                                  GtkCellRendererState flags);
static gchar *convert_keysym_state_to_string     (GtkCellRendererAccel *accel,
                                                  guint                 keysym,
                                                  GdkModifierType       mask,
                                                  guint                 keycode);

enum {
  ACCEL_EDITED,
  ACCEL_CLEARED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_ACCEL_KEY,
  PROP_ACCEL_MODS,
  PROP_KEYCODE,
  PROP_ACCEL_MODE
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GtkCellRendererAccel, gtk_cell_renderer_accel, GTK_TYPE_CELL_RENDERER_TEXT)

static void
gtk_cell_renderer_accel_init (GtkCellRendererAccel *cell_accel)
{
  gchar *text;

  text = convert_keysym_state_to_string (cell_accel, 0, 0, 0);
  g_object_set (cell_accel, "text", text, NULL);
  g_free (text);
}

static void
gtk_cell_renderer_accel_class_init (GtkCellRendererAccelClass *cell_accel_class)
{
  GObjectClass *object_class;
  GtkCellRendererClass *cell_renderer_class;

  object_class = G_OBJECT_CLASS (cell_accel_class);
  cell_renderer_class = GTK_CELL_RENDERER_CLASS (cell_accel_class);

  object_class->set_property = gtk_cell_renderer_accel_set_property;
  object_class->get_property = gtk_cell_renderer_accel_get_property;

  cell_renderer_class->get_size      = gtk_cell_renderer_accel_get_size;
  cell_renderer_class->start_editing = gtk_cell_renderer_accel_start_editing;

  /**
   * GtkCellRendererAccel:accel-key:
   *
   * The keyval of the accelerator.
   *
   * Since: 2.10
   */
  g_object_class_install_property (object_class,
                                   PROP_ACCEL_KEY,
                                   g_param_spec_uint ("accel-key",
                                                     P_("Accelerator key"),
                                                     P_("The keyval of the accelerator"),
                                                      0,
                                                      G_MAXINT,
                                                      0,
                                                      GTK_PARAM_READWRITE));
  
  /**
   * GtkCellRendererAccel:accel-mods:
   *
   * The modifier mask of the accelerator.
   *
   * Since: 2.10
   */
  g_object_class_install_property (object_class,
                                   PROP_ACCEL_MODS,
                                   g_param_spec_flags ("accel-mods",
                                                       P_("Accelerator modifiers"),
                                                       P_("The modifier mask of the accelerator"),
                                                       GDK_TYPE_MODIFIER_TYPE,
                                                       0,
                                                       GTK_PARAM_READWRITE));

  /**
   * GtkCellRendererAccel:keycode:
   *
   * The hardware keycode of the accelerator. Note that the hardware keycode is
   * only relevant if the key does not have a keyval. Normally, the keyboard
   * configuration should assign keyvals to all keys.
   *
   * Since: 2.10
   */ 
  g_object_class_install_property (object_class,
                                   PROP_KEYCODE,
                                   g_param_spec_uint ("keycode",
                                                      P_("Accelerator keycode"),
                                                      P_("The hardware keycode of the accelerator"),
                                                      0,
                                                      G_MAXINT,
                                                      0,
                                                      GTK_PARAM_READWRITE));

  /**
   * GtkCellRendererAccel:accel-mode:
   *
   * Determines if the edited accelerators are GTK+ accelerators. If
   * they are, consumed modifiers are suppressed, only accelerators
   * accepted by GTK+ are allowed, and the accelerators are rendered
   * in the same way as they are in menus.
   *
   * Since: 2.10
   */
  g_object_class_install_property (object_class,
                                   PROP_ACCEL_MODE,
                                   g_param_spec_enum ("accel-mode",
                                                      P_("Accelerator Mode"),
                                                      P_("The type of accelerators"),
                                                      GTK_TYPE_CELL_RENDERER_ACCEL_MODE,
                                                      GTK_CELL_RENDERER_ACCEL_MODE_GTK,
                                                      GTK_PARAM_READWRITE));
  
  /**
   * GtkCellRendererAccel::accel-edited:
   * @accel: the object reveiving the signal
   * @path_string: the path identifying the row of the edited cell
   * @accel_key: the new accelerator keyval
   * @accel_mods: the new acclerator modifier mask
   * @hardware_keycode: the keycode of the new accelerator
   *
   * Gets emitted when the user has selected a new accelerator.
   *
   * Since: 2.10
   */
  signals[ACCEL_EDITED] = g_signal_new (I_("accel-edited"),
                                        GTK_TYPE_CELL_RENDERER_ACCEL,
                                        G_SIGNAL_RUN_LAST,
                                        G_STRUCT_OFFSET (GtkCellRendererAccelClass, accel_edited),
                                        NULL, NULL,
                                        _gtk_marshal_VOID__STRING_UINT_FLAGS_UINT,
                                        G_TYPE_NONE, 4,
                                        G_TYPE_STRING,
                                        G_TYPE_UINT,
                                        GDK_TYPE_MODIFIER_TYPE,
                                        G_TYPE_UINT);

  /**
   * GtkCellRendererAccel::accel-cleared:
   * @accel: the object reveiving the signal
   * @path_string: the path identifying the row of the edited cell
   *
   * Gets emitted when the user has removed the accelerator.
   *
   * Since: 2.10
   */
  signals[ACCEL_CLEARED] = g_signal_new (I_("accel-cleared"),
                                         GTK_TYPE_CELL_RENDERER_ACCEL,
                                         G_SIGNAL_RUN_LAST,
                                         G_STRUCT_OFFSET (GtkCellRendererAccelClass, accel_cleared),
                                         NULL, NULL,
                                         g_cclosure_marshal_VOID__STRING,
                                         G_TYPE_NONE, 1,
                                         G_TYPE_STRING);
}


/**
 * gtk_cell_renderer_accel_new:
 *
 * Creates a new #GtkCellRendererAccel.
 * 
 * Returns: the new cell renderer
 *
 * Since: 2.10
 */
GtkCellRenderer *
gtk_cell_renderer_accel_new (void)
{
  return g_object_new (GTK_TYPE_CELL_RENDERER_ACCEL, NULL);
}

static gchar *
convert_keysym_state_to_string (GtkCellRendererAccel *accel,
                                guint                 keysym,
                                GdkModifierType       mask,
                                guint                 keycode)
{
  if (keysym == 0 && keycode == 0)
    /* This label is displayed in a treeview cell displaying
     * a disabled accelerator key combination.
     */
    return g_strdup (C_("Accelerator", "Disabled"));
  else 
    {
      if (accel->accel_mode == GTK_CELL_RENDERER_ACCEL_MODE_GTK)
        {
          if (!gtk_accelerator_valid (keysym, mask))
            /* This label is displayed in a treeview cell displaying
             * an accelerator key combination that is not valid according
             * to gtk_accelerator_valid().
             */
            return g_strdup (C_("Accelerator", "Invalid"));

          return gtk_accelerator_get_label (keysym, mask);
        }
      else 
        {
          gchar *name;

          name = gtk_accelerator_get_label (keysym, mask);
          if (name == NULL)
            name = gtk_accelerator_name (keysym, mask);

          if (keysym == 0)
            {
              gchar *tmp;

              tmp = name;
              name = g_strdup_printf ("%s0x%02x", tmp, keycode);
              g_free (tmp);
            }

          return name;
        }
    }
}

static void
gtk_cell_renderer_accel_get_property  (GObject    *object,
                                       guint       param_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GtkCellRendererAccel *accel = GTK_CELL_RENDERER_ACCEL (object);

  switch (param_id)
    {
    case PROP_ACCEL_KEY:
      g_value_set_uint (value, accel->accel_key);
      break;

    case PROP_ACCEL_MODS:
      g_value_set_flags (value, accel->accel_mods);
      break;

    case PROP_KEYCODE:
      g_value_set_uint (value, accel->keycode);
      break;

    case PROP_ACCEL_MODE:
      g_value_set_enum (value, accel->accel_mode);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
gtk_cell_renderer_accel_set_property  (GObject      *object,
                                       guint         param_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GtkCellRendererAccel *accel = GTK_CELL_RENDERER_ACCEL (object);
  gboolean changed = FALSE;

  switch (param_id)
    {
    case PROP_ACCEL_KEY:
      {
        guint accel_key = g_value_get_uint (value);

        if (accel->accel_key != accel_key)
          {
            accel->accel_key = accel_key;
            changed = TRUE;
          }
      }
      break;

    case PROP_ACCEL_MODS:
      {
        guint accel_mods = g_value_get_flags (value);

        if (accel->accel_mods != accel_mods)
          {
            accel->accel_mods = accel_mods;
            changed = TRUE;
          }
      }
      break;
    case PROP_KEYCODE:
      {
        guint keycode = g_value_get_uint (value);

        if (accel->keycode != keycode)
          {
            accel->keycode = keycode;
            changed = TRUE;
          }
      }
      break;

    case PROP_ACCEL_MODE:
      accel->accel_mode = g_value_get_enum (value);
      break;
      
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }

  if (changed)
    {
      gchar *text;

      text = convert_keysym_state_to_string (accel, accel->accel_key, accel->accel_mods, accel->keycode);
      g_object_set (accel, "text", text, NULL);
      g_free (text);
    }
}

static void
gtk_cell_renderer_accel_get_size (GtkCellRenderer *cell,
                                  GtkWidget       *widget,
                                  GdkRectangle    *cell_area,
                                  gint            *x_offset,
                                  gint            *y_offset,
                                  gint            *width,
                                  gint            *height)

{
  GtkCellRendererAccel *accel = (GtkCellRendererAccel *) cell;
  GtkRequisition requisition;

  if (accel->sizing_label == NULL)
    accel->sizing_label = gtk_label_new (_("New accelerator..."));

  gtk_widget_size_request (accel->sizing_label, &requisition);

  GTK_CELL_RENDERER_CLASS (gtk_cell_renderer_accel_parent_class)->get_size (cell, widget, cell_area,
                                                                            x_offset, y_offset, width, height);

  /* FIXME: need to take the cell_area et al. into account */
  if (width)
    *width = MAX (*width, requisition.width);
  if (height)
    *height = MAX (*height, requisition.height);
}

static gboolean
grab_key_callback (GtkWidget            *widget,
                   GdkEventKey          *event,
                   GtkCellRendererAccel *accel)
{
  GdkModifierType accel_mods = 0;
  guint accel_key;
  guint keyval;
  gchar *path;
  gboolean edited;
  gboolean cleared;
  GdkModifierType consumed_modifiers;
  GdkDisplay *display;

  display = gtk_widget_get_display (widget);

  if (event->is_modifier)
    return TRUE;

  edited = FALSE;
  cleared = FALSE;

  accel_mods = event->state;

  _gtk_translate_keyboard_accel_state (gdk_keymap_get_for_display (display),
                                       event->hardware_keycode,
                                       event->state,
                                       gtk_accelerator_get_default_mod_mask (),
                                       event->group,
                                       &keyval, NULL, NULL, &consumed_modifiers);

  accel_key = gdk_keyval_to_lower (keyval);
  if (accel_key == GDK_ISO_Left_Tab) 
    accel_key = GDK_Tab;

  accel_mods &= gtk_accelerator_get_default_mod_mask ();

  /* Filter consumed modifiers 
   */
  if (accel->accel_mode == GTK_CELL_RENDERER_ACCEL_MODE_GTK)
    accel_mods &= ~consumed_modifiers;
  
  /* Put shift back if it changed the case of the key, not otherwise.
   */
  if (accel_key != keyval)
    accel_mods |= GDK_SHIFT_MASK;
    
  if (accel_mods == 0)
    {
      switch (keyval)
	{
	case GDK_Escape:
	  goto out; /* cancel */
	case GDK_BackSpace:
	  /* clear the accelerator on Backspace */
	  cleared = TRUE;
	  goto out;
	default:
	  break;
	}
    }

  if (accel->accel_mode == GTK_CELL_RENDERER_ACCEL_MODE_GTK)
    {
      if (!gtk_accelerator_valid (accel_key, accel_mods))
        {
          gtk_widget_error_bell (widget);

          return TRUE;
        }
    }

  edited = TRUE;

 out:
  gtk_grab_remove (accel->grab_widget);
  gdk_display_keyboard_ungrab (display, event->time);
  gdk_display_pointer_ungrab (display, event->time);

  path = g_strdup (g_object_get_data (G_OBJECT (accel->edit_widget), "gtk-cell-renderer-text"));

  gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (accel->edit_widget));
  gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (accel->edit_widget));
  accel->edit_widget = NULL;
  accel->grab_widget = NULL;
  
  if (edited)
    g_signal_emit (accel, signals[ACCEL_EDITED], 0, path, 
                   accel_key, accel_mods, event->hardware_keycode);
  else if (cleared)
    g_signal_emit (accel, signals[ACCEL_CLEARED], 0, path);

  g_free (path);

  return TRUE;
}

static void
ungrab_stuff (GtkWidget            *widget,
              GtkCellRendererAccel *accel)
{
  GdkDisplay *display = gtk_widget_get_display (widget);

  gtk_grab_remove (accel->grab_widget);
  gdk_display_keyboard_ungrab (display, GDK_CURRENT_TIME);
  gdk_display_pointer_ungrab (display, GDK_CURRENT_TIME);

  g_signal_handlers_disconnect_by_func (G_OBJECT (accel->grab_widget),
                                        G_CALLBACK (grab_key_callback),
                                        accel);
}

static void
_gtk_cell_editable_event_box_start_editing (GtkCellEditable *cell_editable,
                                            GdkEvent        *event)
{
  /* do nothing, because we are pointless */
}

static void
_gtk_cell_editable_event_box_cell_editable_init (GtkCellEditableIface *iface)
{
  iface->start_editing = _gtk_cell_editable_event_box_start_editing;
}

typedef struct _GtkCellEditableEventBox GtkCellEditableEventBox;
typedef         GtkEventBoxClass        GtkCellEditableEventBoxClass;

struct _GtkCellEditableEventBox
{
  GtkEventBox box;
  gboolean editing_canceled;
};

G_DEFINE_TYPE_WITH_CODE (GtkCellEditableEventBox, _gtk_cell_editable_event_box, GTK_TYPE_EVENT_BOX, { \
    G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_EDITABLE, _gtk_cell_editable_event_box_cell_editable_init)   \
      })

enum {
  PROP_ZERO,
  PROP_EDITING_CANCELED
};

static void
gtk_cell_editable_event_box_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  GtkCellEditableEventBox *box = (GtkCellEditableEventBox*)object;

  switch (prop_id)
    {
    case PROP_EDITING_CANCELED:
      box->editing_canceled = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_cell_editable_event_box_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  GtkCellEditableEventBox *box = (GtkCellEditableEventBox*)object;

  switch (prop_id)
    {
    case PROP_EDITING_CANCELED:
      g_value_set_boolean (value, box->editing_canceled);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
_gtk_cell_editable_event_box_class_init (GtkCellEditableEventBoxClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->set_property = gtk_cell_editable_event_box_set_property;
  gobject_class->get_property = gtk_cell_editable_event_box_get_property;

  g_object_class_override_property (gobject_class,
                                    PROP_EDITING_CANCELED,
                                    "editing-canceled");
}

static void
_gtk_cell_editable_event_box_init (GtkCellEditableEventBox *box)
{
}

static GtkCellEditable *
gtk_cell_renderer_accel_start_editing (GtkCellRenderer      *cell,
                                       GdkEvent             *event,
                                       GtkWidget            *widget,
                                       const gchar          *path,
                                       GdkRectangle         *background_area,
                                       GdkRectangle         *cell_area,
                                       GtkCellRendererState  flags)
{
  GtkCellRendererText *celltext;
  GtkCellRendererAccel *accel;
  GtkWidget *label;
  GtkWidget *eventbox;
  
  celltext = GTK_CELL_RENDERER_TEXT (cell);
  accel = GTK_CELL_RENDERER_ACCEL (cell);

  /* If the cell isn't editable we return NULL. */
  if (celltext->editable == FALSE)
    return NULL;

  g_return_val_if_fail (widget->window != NULL, NULL);
  
  if (gdk_keyboard_grab (widget->window, FALSE,
                         gdk_event_get_time (event)) != GDK_GRAB_SUCCESS)
    return NULL;

  if (gdk_pointer_grab (widget->window, FALSE,
                        GDK_BUTTON_PRESS_MASK,
                        NULL, NULL,
                        gdk_event_get_time (event)) != GDK_GRAB_SUCCESS)
    {
      gdk_display_keyboard_ungrab (gtk_widget_get_display (widget),
                                   gdk_event_get_time (event));
      return NULL;
    }
  
  accel->grab_widget = widget;

  g_signal_connect (G_OBJECT (widget), "key-press-event",
                    G_CALLBACK (grab_key_callback),
                    accel);

  eventbox = g_object_new (_gtk_cell_editable_event_box_get_type (), NULL);
  accel->edit_widget = eventbox;
  g_object_add_weak_pointer (G_OBJECT (accel->edit_widget),
                             (gpointer) &accel->edit_widget);
  
  label = gtk_label_new (NULL);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  
  gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL,
                        &widget->style->bg[GTK_STATE_SELECTED]);

  gtk_widget_modify_fg (label, GTK_STATE_NORMAL,
                        &widget->style->fg[GTK_STATE_SELECTED]);
  
  /* This label is displayed in a treeview cell displaying
   * an accelerator when the cell is clicked to change the 
   * acelerator.
   */
  gtk_label_set_text (GTK_LABEL (label), _("New accelerator..."));

  gtk_container_add (GTK_CONTAINER (eventbox), label);
  
  g_object_set_data_full (G_OBJECT (accel->edit_widget), "gtk-cell-renderer-text",
                          g_strdup (path), g_free);
  
  gtk_widget_show_all (accel->edit_widget);

  gtk_grab_add (accel->grab_widget);

  g_signal_connect (G_OBJECT (accel->edit_widget), "unrealize",
                    G_CALLBACK (ungrab_stuff), accel);
  
  return GTK_CELL_EDITABLE (accel->edit_widget);
}


#define __GTK_CELL_RENDERER_ACCEL_C__
#include "gtkaliasdef.c"
