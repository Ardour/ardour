/* gtkcomboboxentry.c
 * Copyright (C) 2002, 2003  Kristian Rietveld <kris@gtk.org>
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

#undef GTK_DISABLE_DEPRECATED

#include "gtkcomboboxentry.h"
#include "gtkcelllayout.h"

#include "gtkentry.h"
#include "gtkcellrenderertext.h"

#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkbuildable.h"
#include "gtkalias.h"

#define GTK_COMBO_BOX_ENTRY_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_COMBO_BOX_ENTRY, GtkComboBoxEntryPrivate))

struct _GtkComboBoxEntryPrivate
{
  GtkCellRenderer *text_renderer;
  gint text_column;
};

static void gtk_combo_box_entry_set_property     (GObject               *object,
                                                  guint                  prop_id,
                                                  const GValue          *value,
                                                  GParamSpec            *pspec);
static void gtk_combo_box_entry_get_property     (GObject               *object,
                                                  guint                  prop_id,
                                                  GValue                *value,
                                                  GParamSpec            *pspec);
static void gtk_combo_box_entry_add              (GtkContainer          *container,
						  GtkWidget             *child);
static void gtk_combo_box_entry_remove           (GtkContainer          *container,
						  GtkWidget             *child);

static gchar *gtk_combo_box_entry_get_active_text (GtkComboBox *combo_box);
static void gtk_combo_box_entry_active_changed   (GtkComboBox           *combo_box,
                                                  gpointer               user_data);
static void gtk_combo_box_entry_contents_changed (GtkEntry              *entry,
                                                  gpointer               user_data);
static gboolean gtk_combo_box_entry_mnemonic_activate (GtkWidget        *entry,
						       gboolean          group_cycling);
static void gtk_combo_box_entry_grab_focus       (GtkWidget *widget);
static void has_frame_changed                    (GtkComboBoxEntry      *entry_box,
						  GParamSpec            *pspec,
						  gpointer               data);
static void gtk_combo_box_entry_buildable_interface_init     (GtkBuildableIface *iface);
static GObject * gtk_combo_box_entry_buildable_get_internal_child (GtkBuildable *buildable,
						     GtkBuilder   *builder,
						     const gchar  *childname);

enum
{
  PROP_0,
  PROP_TEXT_COLUMN
};

G_DEFINE_TYPE_WITH_CODE (GtkComboBoxEntry, gtk_combo_box_entry, GTK_TYPE_COMBO_BOX,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
						gtk_combo_box_entry_buildable_interface_init))

static void
gtk_combo_box_entry_class_init (GtkComboBoxEntryClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;
  GtkComboBoxClass *combo_class;
  
  object_class = (GObjectClass *)klass;
  object_class->set_property = gtk_combo_box_entry_set_property;
  object_class->get_property = gtk_combo_box_entry_get_property;

  widget_class = (GtkWidgetClass *)klass;
  widget_class->mnemonic_activate = gtk_combo_box_entry_mnemonic_activate;
  widget_class->grab_focus = gtk_combo_box_entry_grab_focus;

  container_class = (GtkContainerClass *)klass;
  container_class->add = gtk_combo_box_entry_add;
  container_class->remove = gtk_combo_box_entry_remove;

  combo_class = (GtkComboBoxClass *)klass;
  combo_class->get_active_text = gtk_combo_box_entry_get_active_text;
  
  g_object_class_install_property (object_class,
                                   PROP_TEXT_COLUMN,
                                   g_param_spec_int ("text-column",
                                                     P_("Text Column"),
                                                     P_("A column in the data source model to get the strings from"),
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     GTK_PARAM_READWRITE));

  g_type_class_add_private ((GObjectClass *) klass,
                            sizeof (GtkComboBoxEntryPrivate));
}

static void
gtk_combo_box_entry_init (GtkComboBoxEntry *entry_box)
{
  GtkWidget *entry;

  entry_box->priv = GTK_COMBO_BOX_ENTRY_GET_PRIVATE (entry_box);
  entry_box->priv->text_column = -1;

  entry = gtk_entry_new ();
  gtk_widget_show (entry);
  gtk_container_add (GTK_CONTAINER (entry_box), entry);

  entry_box->priv->text_renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (entry_box),
                              entry_box->priv->text_renderer, TRUE);

  gtk_combo_box_set_active (GTK_COMBO_BOX (entry_box), -1);

  g_signal_connect (entry_box, "changed",
                    G_CALLBACK (gtk_combo_box_entry_active_changed), NULL);
  g_signal_connect (entry_box, "notify::has-frame", G_CALLBACK (has_frame_changed), NULL);
}

static void
gtk_combo_box_entry_buildable_interface_init (GtkBuildableIface *iface)
{
  iface->get_internal_child = gtk_combo_box_entry_buildable_get_internal_child;
}

static GObject *
gtk_combo_box_entry_buildable_get_internal_child (GtkBuildable *buildable,
						  GtkBuilder   *builder,
						  const gchar  *childname)
{
    if (strcmp (childname, "entry") == 0)
      return G_OBJECT (gtk_bin_get_child (GTK_BIN (buildable)));

    return NULL;
}

static void
gtk_combo_box_entry_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GtkComboBoxEntry *entry_box = GTK_COMBO_BOX_ENTRY (object);

  switch (prop_id)
    {
      case PROP_TEXT_COLUMN:
        gtk_combo_box_entry_set_text_column (entry_box,
                                             g_value_get_int (value));
        break;

      default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gtk_combo_box_entry_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GtkComboBoxEntry *entry_box = GTK_COMBO_BOX_ENTRY (object);

  switch (prop_id)
    {
      case PROP_TEXT_COLUMN:
        g_value_set_int (value, entry_box->priv->text_column);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gtk_combo_box_entry_add (GtkContainer *container,
			 GtkWidget    *child)
{
  GtkComboBoxEntry *entry_box = GTK_COMBO_BOX_ENTRY (container);

  if (!GTK_IS_ENTRY (child))
    {
      g_warning ("Attempting to add a widget with type %s to a GtkComboBoxEntry "
		 "(need an instance of GtkEntry or of a subclass)",
                 G_OBJECT_TYPE_NAME (child));
      return;
    }

  GTK_CONTAINER_CLASS (gtk_combo_box_entry_parent_class)->add (container, child);

  /* this flag is a hack to tell the entry to fill its allocation.
   */
  GTK_ENTRY (child)->is_cell_renderer = TRUE;

  g_signal_connect (child, "changed",
		    G_CALLBACK (gtk_combo_box_entry_contents_changed),
		    entry_box);
  has_frame_changed (entry_box, NULL, NULL);
}

static void
gtk_combo_box_entry_remove (GtkContainer *container,
			    GtkWidget    *child)
{
  if (child && child == GTK_BIN (container)->child)
    {
      g_signal_handlers_disconnect_by_func (child,
					    gtk_combo_box_entry_contents_changed,
					    container);
      GTK_ENTRY (child)->is_cell_renderer = FALSE;
    }

  GTK_CONTAINER_CLASS (gtk_combo_box_entry_parent_class)->remove (container, child);
}

static void
gtk_combo_box_entry_active_changed (GtkComboBox *combo_box,
                                    gpointer     user_data)
{
  GtkComboBoxEntry *entry_box = GTK_COMBO_BOX_ENTRY (combo_box);
  GtkTreeModel *model;
  GtkTreeIter iter;
  gchar *str = NULL;

  if (gtk_combo_box_get_active_iter (combo_box, &iter))
    {
      GtkEntry *entry = GTK_ENTRY (GTK_BIN (combo_box)->child);

      if (entry)
	{
	  g_signal_handlers_block_by_func (entry,
					   gtk_combo_box_entry_contents_changed,
					   combo_box);

	  model = gtk_combo_box_get_model (combo_box);

	  gtk_tree_model_get (model, &iter, 
			      entry_box->priv->text_column, &str, 
			      -1);
	  gtk_entry_set_text (entry, str);
	  g_free (str);

	  g_signal_handlers_unblock_by_func (entry,
					     gtk_combo_box_entry_contents_changed,
					     combo_box);
	}
    }
}

static void 
has_frame_changed (GtkComboBoxEntry *entry_box,
		   GParamSpec       *pspec,
		   gpointer          data)
{
  if (GTK_BIN (entry_box)->child)
    {
      gboolean has_frame;
  
      g_object_get (entry_box, "has-frame", &has_frame, NULL);

      gtk_entry_set_has_frame (GTK_ENTRY (GTK_BIN (entry_box)->child), has_frame);
    }
}

static void
gtk_combo_box_entry_contents_changed (GtkEntry *entry,
                                      gpointer  user_data)
{
  GtkComboBox *combo_box = GTK_COMBO_BOX (user_data);

  /*
   *  Fixes regression reported in bug #574059. The old functionality relied on
   *  bug #572478.  As a bugfix, we now emit the "changed" signal ourselves
   *  when the selection was already set to -1. 
   */
  if (gtk_combo_box_get_active(combo_box) == -1)
    g_signal_emit_by_name (combo_box, "changed");
  else 
    gtk_combo_box_set_active (combo_box, -1);
}

/* public API */

/**
 * gtk_combo_box_entry_new:
 *
 * Creates a new #GtkComboBoxEntry which has a #GtkEntry as child. After
 * construction, you should set a model using gtk_combo_box_set_model() and a
 * text column using gtk_combo_box_entry_set_text_column().
 *
 * Return value: A new #GtkComboBoxEntry.
 *
 * Since: 2.4
 *
 * Deprecated: 2.24: Use gtk_combo_box_new_with_entry() instead
 */
GtkWidget *
gtk_combo_box_entry_new (void)
{
  return g_object_new (gtk_combo_box_entry_get_type (), NULL);
}

/**
 * gtk_combo_box_entry_new_with_model:
 * @model: A #GtkTreeModel.
 * @text_column: A column in @model to get the strings from.
 *
 * Creates a new #GtkComboBoxEntry which has a #GtkEntry as child and a list
 * of strings as popup. You can get the #GtkEntry from a #GtkComboBoxEntry
 * using GTK_ENTRY (GTK_BIN (combo_box_entry)->child). To add and remove
 * strings from the list, just modify @model using its data manipulation
 * API.
 *
 * Return value: A new #GtkComboBoxEntry.
 *
 * Since: 2.4
 *
 * Deprecated: 2.24: Use gtk_combo_box_new_with_model_and_entry() instead
 */
GtkWidget *
gtk_combo_box_entry_new_with_model (GtkTreeModel *model,
                                    gint          text_column)
{
  GtkWidget *ret;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (model), NULL);
  g_return_val_if_fail (text_column >= 0, NULL);
  g_return_val_if_fail (text_column < gtk_tree_model_get_n_columns (model), NULL);

  ret = g_object_new (gtk_combo_box_entry_get_type (),
                      "model", model,
                      "text-column", text_column,
                      NULL);

  return ret;
}

/**
 * gtk_combo_box_entry_set_text_column:
 * @entry_box: A #GtkComboBoxEntry.
 * @text_column: A column in @model to get the strings from.
 *
 * Sets the model column which @entry_box should use to get strings from
 * to be @text_column.
 *
 * Since: 2.4
 *
 * Deprecated: 2.24: Use gtk_combo_box_set_entry_text_column() instead
 */
void
gtk_combo_box_entry_set_text_column (GtkComboBoxEntry *entry_box,
                                     gint              text_column)
{
  GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (entry_box));

  g_return_if_fail (text_column >= 0);
  g_return_if_fail (model == NULL || text_column < gtk_tree_model_get_n_columns (model));

  entry_box->priv->text_column = text_column;

  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (entry_box),
                                  entry_box->priv->text_renderer,
                                  "text", text_column,
                                  NULL);
}

/**
 * gtk_combo_box_entry_get_text_column:
 * @entry_box: A #GtkComboBoxEntry.
 *
 * Returns the column which @entry_box is using to get the strings from.
 *
 * Return value: A column in the data source model of @entry_box.
 *
 * Since: 2.4
 *
 * Deprecated: 2.24: Use gtk_combo_box_get_entry_text_column() instead
 */
gint
gtk_combo_box_entry_get_text_column (GtkComboBoxEntry *entry_box)
{
  g_return_val_if_fail (GTK_IS_COMBO_BOX_ENTRY (entry_box), 0);

  return entry_box->priv->text_column;
}

static gboolean
gtk_combo_box_entry_mnemonic_activate (GtkWidget *widget,
				       gboolean   group_cycling)
{
  GtkBin *entry_box = GTK_BIN (widget);

  if (entry_box->child)
    gtk_widget_grab_focus (entry_box->child);

  return TRUE;
}

static void
gtk_combo_box_entry_grab_focus (GtkWidget *widget)
{
  GtkBin *entry_box = GTK_BIN (widget);

  if (entry_box->child)
    gtk_widget_grab_focus (entry_box->child);
}



/* convenience API for simple text combos */

/**
 * gtk_combo_box_entry_new_text:
 *
 * Convenience function which constructs a new editable text combo box, which 
 * is a #GtkComboBoxEntry just displaying strings. If you use this function to
 * create a text combo box, you should only manipulate its data source with
 * the following convenience functions: gtk_combo_box_append_text(),
 * gtk_combo_box_insert_text(), gtk_combo_box_prepend_text() and
 * gtk_combo_box_remove_text().
 *
 * Return value: A new text #GtkComboBoxEntry.
 *
 * Since: 2.4
 */
GtkWidget *
gtk_combo_box_entry_new_text (void)
{
  GtkWidget *entry_box;
  GtkListStore *store;

  store = gtk_list_store_new (1, G_TYPE_STRING);
  entry_box = gtk_combo_box_entry_new_with_model (GTK_TREE_MODEL (store), 0);
  g_object_unref (store);

  return entry_box;
}

static gchar *
gtk_combo_box_entry_get_active_text (GtkComboBox *combo_box)
{
  GtkBin *combo = GTK_BIN (combo_box);

  if (combo->child)
    return g_strdup (gtk_entry_get_text (GTK_ENTRY (combo->child)));

  return NULL;
}

#define __GTK_COMBO_BOX_ENTRY_C__
#include "gtkaliasdef.c"
