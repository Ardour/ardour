/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* GTK - The GIMP Toolkit
 * Copyright (C) Christian Kellner <gicmo@gnome.org>
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include <string.h>

#include "gtkmountoperationprivate.h"
#include "gtkalignment.h"
#include "gtkbox.h"
#include "gtkentry.h"
#include "gtkhbox.h"
#include "gtkintl.h"
#include "gtklabel.h"
#include "gtkvbox.h"
#include "gtkmessagedialog.h"
#include "gtkmisc.h"
#include "gtkmountoperation.h"
#include "gtkprivate.h"
#include "gtkradiobutton.h"
#include "gtkstock.h"
#include "gtktable.h"
#include "gtkwindow.h"
#include "gtktreeview.h"
#include "gtktreeselection.h"
#include "gtkcellrenderertext.h"
#include "gtkcellrendererpixbuf.h"
#include "gtkscrolledwindow.h"
#include "gtkicontheme.h"
#include "gtkimagemenuitem.h"
#include "gtkmain.h"
#include "gtkalias.h"

/**
 * SECTION:filesystem
 * @short_description: Functions for working with GIO
 * @Title: Filesystem utilities
 *
 * The functions and objects described here make working with GTK+ and
 * GIO more convenient.
 *
 * #GtkMountOperation is needed when mounting volumes:
 * It is an implementation of #GMountOperation that can be used with
 * GIO functions for mounting volumes such as
 * g_file_mount_enclosing_volume(), g_file_mount_mountable(),
 * g_volume_mount(), g_mount_unmount_with_operation() and others.
 *
 * When necessary, #GtkMountOperation shows dialogs to ask for
 * passwords, questions or show processes blocking unmount.
 *
 * gtk_show_uri() is a convenient way to launch applications for URIs.
 *
 * Another object that is worth mentioning in this context is
 * #GdkAppLaunchContext, which provides visual feedback when lauching
 * applications.
 */

static void   gtk_mount_operation_finalize     (GObject          *object);
static void   gtk_mount_operation_set_property (GObject          *object,
                                                guint             prop_id,
                                                const GValue     *value,
                                                GParamSpec       *pspec);
static void   gtk_mount_operation_get_property (GObject          *object,
                                                guint             prop_id,
                                                GValue           *value,
                                                GParamSpec       *pspec);

static void   gtk_mount_operation_ask_password (GMountOperation *op,
                                                const char      *message,
                                                const char      *default_user,
                                                const char      *default_domain,
                                                GAskPasswordFlags flags);

static void   gtk_mount_operation_ask_question (GMountOperation *op,
                                                const char      *message,
                                                const char      *choices[]);

static void   gtk_mount_operation_show_processes (GMountOperation *op,
                                                  const char      *message,
                                                  GArray          *processes,
                                                  const char      *choices[]);

static void   gtk_mount_operation_aborted      (GMountOperation *op);

G_DEFINE_TYPE (GtkMountOperation, gtk_mount_operation, G_TYPE_MOUNT_OPERATION);

enum {
  PROP_0,
  PROP_PARENT,
  PROP_IS_SHOWING,
  PROP_SCREEN

};

struct _GtkMountOperationPrivate {
  GtkWindow *parent_window;
  GtkDialog *dialog;
  GdkScreen *screen;

  /* for the ask-password dialog */
  GtkWidget *entry_container;
  GtkWidget *username_entry;
  GtkWidget *domain_entry;
  GtkWidget *password_entry;
  GtkWidget *anonymous_toggle;

  GAskPasswordFlags ask_flags;
  GPasswordSave     password_save;
  gboolean          anonymous;

  /* for the show-processes dialog */
  GtkWidget *process_tree_view;
  GtkListStore *process_list_store;
};

static void
gtk_mount_operation_class_init (GtkMountOperationClass *klass)
{
  GObjectClass         *object_class = G_OBJECT_CLASS (klass);
  GMountOperationClass *mount_op_class = G_MOUNT_OPERATION_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GtkMountOperationPrivate));

  object_class->finalize     = gtk_mount_operation_finalize;
  object_class->get_property = gtk_mount_operation_get_property;
  object_class->set_property = gtk_mount_operation_set_property;

  mount_op_class->ask_password = gtk_mount_operation_ask_password;
  mount_op_class->ask_question = gtk_mount_operation_ask_question;
  mount_op_class->show_processes = gtk_mount_operation_show_processes;
  mount_op_class->aborted = gtk_mount_operation_aborted;

  g_object_class_install_property (object_class,
                                   PROP_PARENT,
                                   g_param_spec_object ("parent",
                                                        P_("Parent"),
                                                        P_("The parent window"),
                                                        GTK_TYPE_WINDOW,
                                                        GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_IS_SHOWING,
                                   g_param_spec_boolean ("is-showing",
                                                         P_("Is Showing"),
                                                         P_("Are we showing a dialog"),
                                                         FALSE,
                                                         GTK_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_SCREEN,
                                   g_param_spec_object ("screen",
                                                        P_("Screen"),
                                                        P_("The screen where this window will be displayed."),
                                                        GDK_TYPE_SCREEN,
                                                        GTK_PARAM_READWRITE));
}

static void
gtk_mount_operation_init (GtkMountOperation *operation)
{
  operation->priv = G_TYPE_INSTANCE_GET_PRIVATE (operation,
                                                 GTK_TYPE_MOUNT_OPERATION,
                                                 GtkMountOperationPrivate);
}

static void
gtk_mount_operation_finalize (GObject *object)
{
  GtkMountOperation *operation = GTK_MOUNT_OPERATION (object);
  GtkMountOperationPrivate *priv = operation->priv;

  if (priv->parent_window)
    {
      g_signal_handlers_disconnect_by_func (priv->parent_window,
                                            gtk_widget_destroyed,
                                            &priv->parent_window);
      g_object_unref (priv->parent_window);
    }

  if (priv->screen)
    g_object_unref (priv->screen);

  G_OBJECT_CLASS (gtk_mount_operation_parent_class)->finalize (object);
}

static void
gtk_mount_operation_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GtkMountOperation *operation = GTK_MOUNT_OPERATION (object);

  switch (prop_id)
    {
    case PROP_PARENT:
      gtk_mount_operation_set_parent (operation, g_value_get_object (value));
      break;

    case PROP_SCREEN:
      gtk_mount_operation_set_screen (operation, g_value_get_object (value));
      break;

    case PROP_IS_SHOWING:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_mount_operation_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GtkMountOperation *operation = GTK_MOUNT_OPERATION (object);
  GtkMountOperationPrivate *priv = operation->priv;

  switch (prop_id)
    {
    case PROP_PARENT:
      g_value_set_object (value, priv->parent_window);
      break;

    case PROP_IS_SHOWING:
      g_value_set_boolean (value, priv->dialog != NULL);
      break;

    case PROP_SCREEN:
      g_value_set_object (value, priv->screen);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
remember_button_toggled (GtkToggleButton   *button,
                         GtkMountOperation *operation)
{
  GtkMountOperationPrivate *priv = operation->priv;

  if (gtk_toggle_button_get_active (button))
    {
      gpointer data;

      data = g_object_get_data (G_OBJECT (button), "password-save");
      priv->password_save = GPOINTER_TO_INT (data);
    }
}

static void
pw_dialog_got_response (GtkDialog         *dialog,
                        gint               response_id,
                        GtkMountOperation *mount_op)
{
  GtkMountOperationPrivate *priv = mount_op->priv;
  GMountOperation *op = G_MOUNT_OPERATION (mount_op);

  if (response_id == GTK_RESPONSE_OK)
    {
      const char *text;

      if (priv->ask_flags & G_ASK_PASSWORD_ANONYMOUS_SUPPORTED)
        g_mount_operation_set_anonymous (op, priv->anonymous);

      if (priv->username_entry)
        {
          text = gtk_entry_get_text (GTK_ENTRY (priv->username_entry));
          g_mount_operation_set_username (op, text);
        }

      if (priv->domain_entry)
        {
          text = gtk_entry_get_text (GTK_ENTRY (priv->domain_entry));
          g_mount_operation_set_domain (op, text);
        }

      if (priv->password_entry)
        {
          text = gtk_entry_get_text (GTK_ENTRY (priv->password_entry));
          g_mount_operation_set_password (op, text);
        }

      if (priv->ask_flags & G_ASK_PASSWORD_SAVING_SUPPORTED)
        g_mount_operation_set_password_save (op, priv->password_save);

      g_mount_operation_reply (op, G_MOUNT_OPERATION_HANDLED);
    }
  else
    g_mount_operation_reply (op, G_MOUNT_OPERATION_ABORTED);

  priv->dialog = NULL;
  g_object_notify (G_OBJECT (op), "is-showing");
  gtk_widget_destroy (GTK_WIDGET (dialog));
  g_object_unref (op);
}

static gboolean
entry_has_input (GtkWidget *entry_widget)
{
  const char *text;

  if (entry_widget == NULL)
    return TRUE;

  text = gtk_entry_get_text (GTK_ENTRY (entry_widget));

  return text != NULL && text[0] != '\0';
}

static gboolean
pw_dialog_input_is_valid (GtkMountOperation *operation)
{
  GtkMountOperationPrivate *priv = operation->priv;
  gboolean is_valid = TRUE;

  /* We don't require password to be non-empty here
   * since there are situations where it is not needed,
   * see bug 578365.
   * We may add a way for the backend to specify that it
   * definitively needs a password.
   */
  is_valid = entry_has_input (priv->username_entry) &&
             entry_has_input (priv->domain_entry);

  return is_valid;
}

static void
pw_dialog_verify_input (GtkEditable       *editable,
                        GtkMountOperation *operation)
{
  GtkMountOperationPrivate *priv = operation->priv;
  gboolean is_valid;

  is_valid = pw_dialog_input_is_valid (operation);
  gtk_dialog_set_response_sensitive (GTK_DIALOG (priv->dialog),
                                     GTK_RESPONSE_OK,
                                     is_valid);
}

static void
pw_dialog_anonymous_toggled (GtkWidget         *widget,
                             GtkMountOperation *operation)
{
  GtkMountOperationPrivate *priv = operation->priv;
  gboolean is_valid;

  priv->anonymous = widget == priv->anonymous_toggle;

  if (priv->anonymous)
    is_valid = TRUE;
  else
    is_valid = pw_dialog_input_is_valid (operation);

  gtk_widget_set_sensitive (priv->entry_container, priv->anonymous == FALSE);
  gtk_dialog_set_response_sensitive (GTK_DIALOG (priv->dialog),
                                     GTK_RESPONSE_OK,
                                     is_valid);
}


static void
pw_dialog_cycle_focus (GtkWidget         *widget,
                       GtkMountOperation *operation)
{
  GtkMountOperationPrivate *priv;
  GtkWidget *next_widget = NULL;

  priv = operation->priv;

  if (widget == priv->username_entry)
    {
      if (priv->domain_entry != NULL)
        next_widget = priv->domain_entry;
      else if (priv->password_entry != NULL)
        next_widget = priv->password_entry;
    }
  else if (widget == priv->domain_entry && priv->password_entry)
    next_widget = priv->password_entry;

  if (next_widget)
    gtk_widget_grab_focus (next_widget);
  else if (pw_dialog_input_is_valid (operation))
    gtk_window_activate_default (GTK_WINDOW (priv->dialog));
}

static GtkWidget *
table_add_entry (GtkWidget  *table,
                 int         row,
                 const char *label_text,
                 const char *value,
                 gpointer    user_data)
{
  GtkWidget *entry;
  GtkWidget *label;

  label = gtk_label_new_with_mnemonic (label_text);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

  entry = gtk_entry_new ();

  if (value)
    gtk_entry_set_text (GTK_ENTRY (entry), value);

  gtk_table_attach (GTK_TABLE (table), label,
                    0, 1, row, row + 1,
                    GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_table_attach_defaults (GTK_TABLE (table), entry,
                             1, 2, row, row + 1);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);

  g_signal_connect (entry, "changed",
                    G_CALLBACK (pw_dialog_verify_input), user_data);

  g_signal_connect (entry, "activate",
                    G_CALLBACK (pw_dialog_cycle_focus), user_data);

  return entry;
}

static void
gtk_mount_operation_ask_password (GMountOperation   *mount_op,
                                  const char        *message,
                                  const char        *default_user,
                                  const char        *default_domain,
                                  GAskPasswordFlags  flags)
{
  GtkMountOperation *operation;
  GtkMountOperationPrivate *priv;
  GtkWidget *widget;
  GtkDialog *dialog;
  GtkWindow *window;
  GtkWidget *entry_container;
  GtkWidget *hbox, *main_vbox, *vbox, *icon;
  GtkWidget *table;
  GtkWidget *message_label;
  gboolean   can_anonymous;
  guint      rows;
  const gchar *secondary;

  operation = GTK_MOUNT_OPERATION (mount_op);
  priv = operation->priv;

  priv->ask_flags = flags;

  widget = gtk_dialog_new ();
  dialog = GTK_DIALOG (widget);
  window = GTK_WINDOW (widget);

  priv->dialog = dialog;

  /* Set the dialog up with HIG properties */
  gtk_dialog_set_has_separator (dialog, FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
  gtk_box_set_spacing (GTK_BOX (dialog->vbox), 2); /* 2 * 5 + 2 = 12 */
  gtk_container_set_border_width (GTK_CONTAINER (dialog->action_area), 5);
  gtk_box_set_spacing (GTK_BOX (dialog->action_area), 6);

  gtk_window_set_resizable (window, FALSE);
  gtk_window_set_title (window, "");
  gtk_window_set_icon_name (window, GTK_STOCK_DIALOG_AUTHENTICATION);

  gtk_dialog_add_buttons (dialog,
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                          _("Co_nnect"), GTK_RESPONSE_OK,
                          NULL);
  gtk_dialog_set_default_response (dialog, GTK_RESPONSE_OK);

  gtk_dialog_set_alternative_button_order (dialog,
                                           GTK_RESPONSE_OK,
                                           GTK_RESPONSE_CANCEL,
                                           -1);

  /* Build contents */
  hbox = gtk_hbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
  gtk_box_pack_start (GTK_BOX (dialog->vbox), hbox, TRUE, TRUE, 0);

  icon = gtk_image_new_from_stock (GTK_STOCK_DIALOG_AUTHENTICATION,
                                   GTK_ICON_SIZE_DIALOG);

  gtk_misc_set_alignment (GTK_MISC (icon), 0.5, 0.0);
  gtk_box_pack_start (GTK_BOX (hbox), icon, FALSE, FALSE, 0);

  main_vbox = gtk_vbox_new (FALSE, 18);
  gtk_box_pack_start (GTK_BOX (hbox), main_vbox, TRUE, TRUE, 0);

  secondary = strstr (message, "\n");
  if (secondary != NULL)
    {
      gchar *s;
      gchar *primary;

      primary = g_strndup (message, secondary - message + 1);
      s = g_strdup_printf ("<big><b>%s</b></big>%s", primary, secondary);

      message_label = gtk_label_new (NULL);
      gtk_label_set_markup (GTK_LABEL (message_label), s);
      gtk_misc_set_alignment (GTK_MISC (message_label), 0.0, 0.5);
      gtk_label_set_line_wrap (GTK_LABEL (message_label), TRUE);
      gtk_box_pack_start (GTK_BOX (main_vbox), GTK_WIDGET (message_label),
                          FALSE, TRUE, 0);

      g_free (s);
      g_free (primary);
    }
  else
    {
      message_label = gtk_label_new (message);
      gtk_misc_set_alignment (GTK_MISC (message_label), 0.0, 0.5);
      gtk_label_set_line_wrap (GTK_LABEL (message_label), TRUE);
      gtk_box_pack_start (GTK_BOX (main_vbox), GTK_WIDGET (message_label),
                          FALSE, FALSE, 0);
    }

  vbox = gtk_vbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (main_vbox), vbox, FALSE, FALSE, 0);

  can_anonymous = flags & G_ASK_PASSWORD_ANONYMOUS_SUPPORTED;

  priv->anonymous_toggle = NULL;
  if (can_anonymous)
    {
      GtkWidget *anon_box;
      GtkWidget *choice;
      GSList    *group;

      anon_box = gtk_vbox_new (FALSE, 6);
      gtk_box_pack_start (GTK_BOX (vbox), anon_box,
                          FALSE, FALSE, 0);

      choice = gtk_radio_button_new_with_mnemonic (NULL, _("Connect _anonymously"));
      gtk_box_pack_start (GTK_BOX (anon_box),
                          choice,
                          FALSE, FALSE, 0);
      g_signal_connect (choice, "toggled",
                        G_CALLBACK (pw_dialog_anonymous_toggled), operation);
      priv->anonymous_toggle = choice;

      group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (choice));
      choice = gtk_radio_button_new_with_mnemonic (group, _("Connect as u_ser:"));
      gtk_box_pack_start (GTK_BOX (anon_box),
                          choice,
                          FALSE, FALSE, 0);
      g_signal_connect (choice, "toggled",
                        G_CALLBACK (pw_dialog_anonymous_toggled), operation);
    }

  rows = 0;

  if (flags & G_ASK_PASSWORD_NEED_PASSWORD)
    rows++;

  if (flags & G_ASK_PASSWORD_NEED_USERNAME)
    rows++;

  if (flags &G_ASK_PASSWORD_NEED_DOMAIN)
    rows++;

  /* The table that holds the entries */
  entry_container = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);

  gtk_alignment_set_padding (GTK_ALIGNMENT (entry_container),
                             0, 0, can_anonymous ? 12 : 0, 0);

  gtk_box_pack_start (GTK_BOX (vbox), entry_container,
                      FALSE, FALSE, 0);
  priv->entry_container = entry_container;

  table = gtk_table_new (rows, 2, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 12);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_container_add (GTK_CONTAINER (entry_container), table);

  rows = 0;

  priv->username_entry = NULL;
  if (flags & G_ASK_PASSWORD_NEED_USERNAME)
    priv->username_entry = table_add_entry (table, rows++, _("_Username:"),
                                            default_user, operation);

  priv->domain_entry = NULL;
  if (flags & G_ASK_PASSWORD_NEED_DOMAIN)
    priv->domain_entry = table_add_entry (table, rows++, _("_Domain:"),
                                          default_domain, operation);

  priv->password_entry = NULL;
  if (flags & G_ASK_PASSWORD_NEED_PASSWORD)
    {
      priv->password_entry = table_add_entry (table, rows++, _("_Password:"),
                                              NULL, operation);
      gtk_entry_set_visibility (GTK_ENTRY (priv->password_entry), FALSE);
    }

   if (flags & G_ASK_PASSWORD_SAVING_SUPPORTED)
    {
      GtkWidget    *choice;
      GtkWidget    *remember_box;
      GSList       *group;
      GPasswordSave password_save;

      remember_box = gtk_vbox_new (FALSE, 6);
      gtk_box_pack_start (GTK_BOX (vbox), remember_box,
                          FALSE, FALSE, 0);

      password_save = g_mount_operation_get_password_save (mount_op);
      
      choice = gtk_radio_button_new_with_mnemonic (NULL, _("Forget password _immediately"));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (choice),
                                    password_save == G_PASSWORD_SAVE_NEVER);
      g_object_set_data (G_OBJECT (choice), "password-save",
                         GINT_TO_POINTER (G_PASSWORD_SAVE_NEVER));
      g_signal_connect (choice, "toggled",
                        G_CALLBACK (remember_button_toggled), operation);
      gtk_box_pack_start (GTK_BOX (remember_box), choice, FALSE, FALSE, 0);

      group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (choice));
      choice = gtk_radio_button_new_with_mnemonic (group, _("Remember password until you _logout"));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (choice),
                                    password_save == G_PASSWORD_SAVE_FOR_SESSION);
      g_object_set_data (G_OBJECT (choice), "password-save",
                         GINT_TO_POINTER (G_PASSWORD_SAVE_FOR_SESSION));
      g_signal_connect (choice, "toggled",
                        G_CALLBACK (remember_button_toggled), operation);
      gtk_box_pack_start (GTK_BOX (remember_box), choice, FALSE, FALSE, 0);

      group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (choice));
      choice = gtk_radio_button_new_with_mnemonic (group, _("Remember _forever"));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (choice),
                                    password_save == G_PASSWORD_SAVE_PERMANENTLY);
      g_object_set_data (G_OBJECT (choice), "password-save",
                         GINT_TO_POINTER (G_PASSWORD_SAVE_PERMANENTLY));
      g_signal_connect (choice, "toggled",
                        G_CALLBACK (remember_button_toggled), operation);
      gtk_box_pack_start (GTK_BOX (remember_box), choice, FALSE, FALSE, 0);
    }

  g_signal_connect (G_OBJECT (dialog), "response",
                    G_CALLBACK (pw_dialog_got_response), operation);

  if (can_anonymous)
    {
      /* The anonymous option will be active by default,
       * ensure the toggled signal is emitted for it.
       */
      gtk_toggle_button_toggled (GTK_TOGGLE_BUTTON (priv->anonymous_toggle));
    }
  else if (! pw_dialog_input_is_valid (operation))
    gtk_dialog_set_response_sensitive (dialog, GTK_RESPONSE_OK, FALSE);

  g_object_notify (G_OBJECT (operation), "is-showing");

  if (priv->parent_window)
    {
      gtk_window_set_transient_for (window, priv->parent_window);
      gtk_window_set_modal (window, TRUE);
    }
  else if (priv->screen)
    gtk_window_set_screen (GTK_WINDOW (dialog), priv->screen);

  gtk_widget_show_all (GTK_WIDGET (dialog));

  g_object_ref (operation);
}

static void
question_dialog_button_clicked (GtkDialog       *dialog,
                                gint             button_number,
                                GMountOperation *op)
{
  GtkMountOperationPrivate *priv;
  GtkMountOperation *operation;

  operation = GTK_MOUNT_OPERATION (op);
  priv = operation->priv;

  if (button_number >= 0)
    {
      g_mount_operation_set_choice (op, button_number);
      g_mount_operation_reply (op, G_MOUNT_OPERATION_HANDLED);
    }
  else
    g_mount_operation_reply (op, G_MOUNT_OPERATION_ABORTED);

  priv->dialog = NULL;
  g_object_notify (G_OBJECT (operation), "is-showing");
  gtk_widget_destroy (GTK_WIDGET (dialog));
  g_object_unref (op);
}

static void
gtk_mount_operation_ask_question (GMountOperation *op,
                                  const char      *message,
                                  const char      *choices[])
{
  GtkMountOperationPrivate *priv;
  GtkWidget  *dialog;
  const char *secondary = NULL;
  char       *primary;
  int        count, len = 0;

  g_return_if_fail (GTK_IS_MOUNT_OPERATION (op));
  g_return_if_fail (message != NULL);
  g_return_if_fail (choices != NULL);

  priv = GTK_MOUNT_OPERATION (op)->priv;

  primary = strstr (message, "\n");
  if (primary)
    {
      secondary = primary + 1;
      primary = g_strndup (message, primary - message);
    }

  dialog = gtk_message_dialog_new (priv->parent_window, 0,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_NONE, "%s",
                                   primary != NULL ? primary : message);
  g_free (primary);

  if (secondary)
    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                              "%s", secondary);

  /* First count the items in the list then
   * add the buttons in reverse order */

  while (choices[len] != NULL)
    len++;

  for (count = len - 1; count >= 0; count--)
    gtk_dialog_add_button (GTK_DIALOG (dialog), choices[count], count);

  g_signal_connect (G_OBJECT (dialog), "response",
                    G_CALLBACK (question_dialog_button_clicked), op);

  priv->dialog = GTK_DIALOG (dialog);
  g_object_notify (G_OBJECT (op), "is-showing");

  if (priv->parent_window == NULL && priv->screen)
    gtk_window_set_screen (GTK_WINDOW (dialog), priv->screen);

  gtk_widget_show (dialog);
  g_object_ref (op);
}

static void
show_processes_button_clicked (GtkDialog       *dialog,
                               gint             button_number,
                               GMountOperation *op)
{
  GtkMountOperationPrivate *priv;
  GtkMountOperation *operation;

  operation = GTK_MOUNT_OPERATION (op);
  priv = operation->priv;

  if (button_number >= 0)
    {
      g_mount_operation_set_choice (op, button_number);
      g_mount_operation_reply (op, G_MOUNT_OPERATION_HANDLED);
    }
  else
    g_mount_operation_reply (op, G_MOUNT_OPERATION_ABORTED);

  priv->dialog = NULL;
  g_object_notify (G_OBJECT (operation), "is-showing");
  gtk_widget_destroy (GTK_WIDGET (dialog));
  g_object_unref (op);
}

static gint
pid_equal (gconstpointer a,
           gconstpointer b)
{
  GPid pa, pb;

  pa = *((GPid *) a);
  pb = *((GPid *) b);

  return GPOINTER_TO_INT(pb) - GPOINTER_TO_INT(pa);
}

static void
diff_sorted_arrays (GArray         *array1,
                    GArray         *array2,
                    GCompareFunc   compare,
                    GArray         *added_indices,
                    GArray         *removed_indices)
{
  gint order;
  guint n1, n2;
  guint elem_size;

  n1 = n2 = 0;

  elem_size = g_array_get_element_size (array1);
  g_assert (elem_size == g_array_get_element_size (array2));

  while (n1 < array1->len && n2 < array2->len)
    {
      order = (*compare) (((const char*) array1->data) + n1 * elem_size,
                          ((const char*) array2->data) + n2 * elem_size);
      if (order < 0)
        {
          g_array_append_val (removed_indices, n1);
          n1++;
        }
      else if (order > 0)
        {
          g_array_append_val (added_indices, n2);
          n2++;
        }
      else
        { /* same item */
          n1++;
          n2++;
        }
    }

  while (n1 < array1->len)
    {
      g_array_append_val (removed_indices, n1);
      n1++;
    }
  while (n2 < array2->len)
    {
      g_array_append_val (added_indices, n2);
      n2++;
    }
}


static void
add_pid_to_process_list_store (GtkMountOperation              *mount_operation,
                               GtkMountOperationLookupContext *lookup_context,
                               GtkListStore                   *list_store,
                               GPid                            pid)
{
  gchar *command_line;
  gchar *name;
  GdkPixbuf *pixbuf;
  gchar *markup;
  GtkTreeIter iter;

  name = NULL;
  pixbuf = NULL;
  command_line = NULL;
  _gtk_mount_operation_lookup_info (lookup_context,
                                    pid,
                                    24,
                                    &name,
                                    &command_line,
                                    &pixbuf);

  if (name == NULL)
    name = g_strdup_printf (_("Unknown Application (PID %d)"), pid);

  if (command_line == NULL)
    command_line = g_strdup ("");

  if (pixbuf == NULL)
    {
      GtkIconTheme *theme;
      theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (mount_operation->priv->dialog)));
      pixbuf = gtk_icon_theme_load_icon (theme,
                                         "application-x-executable",
                                         24,
                                         0,
                                         NULL);
    }

  markup = g_strdup_printf ("<b>%s</b>\n"
                            "<small>%s</small>",
                            name,
                            command_line);

  gtk_list_store_append (list_store, &iter);
  gtk_list_store_set (list_store, &iter,
                      0, pixbuf,
                      1, markup,
                      2, pid,
                      -1);

  if (pixbuf != NULL)
    g_object_unref (pixbuf);
  g_free (markup);
  g_free (name);
  g_free (command_line);
}

static void
remove_pid_from_process_list_store (GtkMountOperation *mount_operation,
                                    GtkListStore      *list_store,
                                    GPid               pid)
{
  GtkTreeIter iter;
  GPid pid_of_item;

  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (list_store), &iter))
    {
      do
        {
          gtk_tree_model_get (GTK_TREE_MODEL (list_store),
                              &iter,
                              2, &pid_of_item,
                              -1);

          if (pid_of_item == pid)
            {
              gtk_list_store_remove (list_store, &iter);
              break;
            }
        }
      while (gtk_tree_model_iter_next (GTK_TREE_MODEL (list_store), &iter));
    }
}


static void
update_process_list_store (GtkMountOperation *mount_operation,
                           GtkListStore      *list_store,
                           GArray            *processes)
{
  guint n;
  GtkMountOperationLookupContext *lookup_context;
  GArray *current_pids;
  GArray *pid_indices_to_add;
  GArray *pid_indices_to_remove;
  GtkTreeIter iter;
  GPid pid;

  /* Just removing all items and adding new ones will screw up the
   * focus handling in the treeview - so compute the delta, and add/remove
   * items as appropriate
   */
  current_pids = g_array_new (FALSE, FALSE, sizeof (GPid));
  pid_indices_to_add = g_array_new (FALSE, FALSE, sizeof (gint));
  pid_indices_to_remove = g_array_new (FALSE, FALSE, sizeof (gint));

  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (list_store), &iter))
    {
      do
        {
          gtk_tree_model_get (GTK_TREE_MODEL (list_store),
                              &iter,
                              2, &pid,
                              -1);

          g_array_append_val (current_pids, pid);
        }
      while (gtk_tree_model_iter_next (GTK_TREE_MODEL (list_store), &iter));
    }

  g_array_sort (current_pids, pid_equal);
  g_array_sort (processes, pid_equal);

  diff_sorted_arrays (current_pids, processes, pid_equal, pid_indices_to_add, pid_indices_to_remove);

  for (n = 0; n < pid_indices_to_remove->len; n++)
    {
      pid = g_array_index (current_pids, GPid, n);
      remove_pid_from_process_list_store (mount_operation, list_store, pid);
    }

  if (pid_indices_to_add->len > 0)
    {
      lookup_context = _gtk_mount_operation_lookup_context_get (gtk_widget_get_display (mount_operation->priv->process_tree_view));
      for (n = 0; n < pid_indices_to_add->len; n++)
        {
          pid = g_array_index (processes, GPid, n);
          add_pid_to_process_list_store (mount_operation, lookup_context, list_store, pid);
        }
      _gtk_mount_operation_lookup_context_free (lookup_context);
    }

  /* select the first item, if we went from a zero to a non-zero amount of processes */
  if (current_pids->len == 0 && pid_indices_to_add->len > 0)
    {
      if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (list_store), &iter))
        {
          GtkTreeSelection *tree_selection;
          tree_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (mount_operation->priv->process_tree_view));
          gtk_tree_selection_select_iter (tree_selection, &iter);
        }
    }

  g_array_unref (current_pids);
  g_array_unref (pid_indices_to_add);
  g_array_unref (pid_indices_to_remove);
}

static void
on_end_process_activated (GtkMenuItem *item,
                          gpointer user_data)
{
  GtkMountOperation *op = GTK_MOUNT_OPERATION (user_data);
  GtkTreeSelection *selection;
  GtkTreeIter iter;
  GPid pid_to_kill;
  GError *error;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (op->priv->process_tree_view));

  if (!gtk_tree_selection_get_selected (selection,
                                        NULL,
                                        &iter))
    goto out;

  gtk_tree_model_get (GTK_TREE_MODEL (op->priv->process_list_store),
                      &iter,
                      2, &pid_to_kill,
                      -1);

  /* TODO: We might want to either
   *
   *       - Be smart about things and send SIGKILL rather than SIGTERM if
   *         this is the second time the user requests killing a process
   *
   *       - Or, easier (but worse user experience), offer both "End Process"
   *         and "Terminate Process" options
   *
   *      But that's not how things work right now....
   */
  error = NULL;
  if (!_gtk_mount_operation_kill_process (pid_to_kill, &error))
    {
      GtkWidget *dialog;
      gint response;

      /* Use GTK_DIALOG_DESTROY_WITH_PARENT here since the parent dialog can be
       * indeed be destroyed via the GMountOperation::abort signal... for example,
       * this is triggered if the user yanks the device while we are showing
       * the dialog...
       */
      dialog = gtk_message_dialog_new (GTK_WINDOW (op->priv->dialog),
                                       GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       _("Unable to end process"));
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                "%s",
                                                error->message);

      gtk_widget_show_all (dialog);
      response = gtk_dialog_run (GTK_DIALOG (dialog));

      /* GTK_RESPONSE_NONE means the dialog were programmatically destroy, e.g. that
       * GTK_DIALOG_DESTROY_WITH_PARENT kicked in - so it would trigger a warning to
       * destroy the dialog in that case
       */
      if (response != GTK_RESPONSE_NONE)
        gtk_widget_destroy (dialog);

      g_error_free (error);
    }

 out:
  ;
}

static gboolean
do_popup_menu_for_process_tree_view (GtkWidget         *widget,
                                     GdkEventButton    *event,
                                     GtkMountOperation *op)
{
  GtkWidget *menu;
  GtkWidget *item;
  gint button;
  gint event_time;
  gboolean popped_up_menu;

  popped_up_menu = FALSE;

  menu = gtk_menu_new ();

  item = gtk_image_menu_item_new_with_mnemonic (_("_End Process"));
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
                                 gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU));
  g_signal_connect (item, "activate",
                    G_CALLBACK (on_end_process_activated),
                    op);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show_all (menu);

  if (event != NULL)
    {
      GtkTreePath *path;
      GtkTreeSelection *selection;

      if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (op->priv->process_tree_view),
                                         (gint) event->x,
                                         (gint) event->y,
                                         &path,
                                         NULL,
                                         NULL,
                                         NULL))
        {
          selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (op->priv->process_tree_view));
          gtk_tree_selection_select_path (selection, path);
          gtk_tree_path_free (path);
        }
      else
        {
          /* don't popup a menu if the user right-clicked in an area with no rows */
          goto out;
        }

      button = event->button;
      event_time = event->time;
    }
  else
    {
      button = 0;
      event_time = gtk_get_current_event_time ();
    }

  gtk_menu_popup (GTK_MENU (menu),
                  NULL,
                  widget,
                  NULL,
                  NULL,
                  button,
                  event_time);

  popped_up_menu = TRUE;

 out:
  return popped_up_menu;
}

static gboolean
on_popup_menu_for_process_tree_view (GtkWidget *widget,
                                     gpointer   user_data)
{
  GtkMountOperation *op = GTK_MOUNT_OPERATION (user_data);
  return do_popup_menu_for_process_tree_view (widget, NULL, op);
}

static gboolean
on_button_press_event_for_process_tree_view (GtkWidget      *widget,
                                             GdkEventButton *event,
                                             gpointer        user_data)
{
  GtkMountOperation *op = GTK_MOUNT_OPERATION (user_data);
  gboolean ret;

  ret = FALSE;

  if (_gtk_button_event_triggers_context_menu (event))
    {
      ret = do_popup_menu_for_process_tree_view (widget, event, op);
    }

  return ret;
}

static void
create_show_processes_dialog (GMountOperation *op,
                              const char      *message,
                              const char      *choices[])
{
  GtkMountOperationPrivate *priv;
  GtkWidget  *dialog;
  const char *secondary = NULL;
  char       *primary;
  int        count, len = 0;
  GtkWidget *label;
  GtkWidget *tree_view;
  GtkWidget *scrolled_window;
  GtkWidget *vbox;
  GtkWidget *content_area;
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;
  GtkListStore *list_store;
  gchar *s;

  priv = GTK_MOUNT_OPERATION (op)->priv;

  primary = strstr (message, "\n");
  if (primary)
    {
      secondary = primary + 1;
      primary = g_strndup (message, primary - message);
    }

  dialog = gtk_dialog_new ();

  if (priv->parent_window != NULL)
    gtk_window_set_transient_for (GTK_WINDOW (dialog), priv->parent_window);
  gtk_window_set_title (GTK_WINDOW (dialog), "");
  gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  vbox = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
  gtk_box_pack_start (GTK_BOX (content_area), vbox, TRUE, TRUE, 0);

  if (secondary != NULL)
    {
      s = g_strdup_printf ("<big><b>%s</b></big>\n\n%s", primary, secondary);
    }
  else
    {
      s = g_strdup_printf ("%s", primary);
    }
  g_free (primary);
  label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (label), s);
  g_free (s);
  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);

  /* First count the items in the list then
   * add the buttons in reverse order */

  while (choices[len] != NULL)
    len++;

  for (count = len - 1; count >= 0; count--)
    gtk_dialog_add_button (GTK_DIALOG (dialog), choices[count], count);

  g_signal_connect (G_OBJECT (dialog), "response",
                    G_CALLBACK (show_processes_button_clicked), op);

  priv->dialog = GTK_DIALOG (dialog);
  g_object_notify (G_OBJECT (op), "is-showing");

  if (priv->parent_window == NULL && priv->screen)
    gtk_window_set_screen (GTK_WINDOW (dialog), priv->screen);

  tree_view = gtk_tree_view_new ();
  /* TODO: should use EM's when gtk+ RI patches land */
  gtk_widget_set_size_request (tree_view,
                               300,
                               120);

  column = gtk_tree_view_column_new ();
  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "pixbuf", 0,
                                       NULL);
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer,
                "ellipsize", PANGO_ELLIPSIZE_MIDDLE,
                "ellipsize-set", TRUE,
                NULL);
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "markup", 1,
                                       NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tree_view), FALSE);


  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);

  gtk_container_add (GTK_CONTAINER (scrolled_window), tree_view);
  gtk_box_pack_start (GTK_BOX (vbox), scrolled_window, TRUE, TRUE, 0);

  g_signal_connect (tree_view, "popup-menu",
                    G_CALLBACK (on_popup_menu_for_process_tree_view),
                    op);
  g_signal_connect (tree_view, "button-press-event",
                    G_CALLBACK (on_button_press_event_for_process_tree_view),
                    op);

  list_store = gtk_list_store_new (3,
                                   GDK_TYPE_PIXBUF,
                                   G_TYPE_STRING,
                                   G_TYPE_INT);

  gtk_tree_view_set_model (GTK_TREE_VIEW (tree_view), GTK_TREE_MODEL (list_store));

  priv->process_list_store = list_store;
  priv->process_tree_view = tree_view;
  /* set pointers to NULL when dialog goes away */
  g_object_add_weak_pointer (G_OBJECT (list_store), (gpointer *) &priv->process_list_store);
  g_object_add_weak_pointer (G_OBJECT (tree_view), (gpointer *) &priv->process_tree_view);

  g_object_unref (list_store);

  gtk_widget_show_all (dialog);
  g_object_ref (op);
}

static void
gtk_mount_operation_show_processes (GMountOperation *op,
                                    const char      *message,
                                    GArray          *processes,
                                    const char      *choices[])
{
  GtkMountOperationPrivate *priv;

  g_return_if_fail (GTK_IS_MOUNT_OPERATION (op));
  g_return_if_fail (message != NULL);
  g_return_if_fail (processes != NULL);
  g_return_if_fail (choices != NULL);

  priv = GTK_MOUNT_OPERATION (op)->priv;

  if (priv->process_list_store == NULL)
    {
      /* need to create the dialog */
      create_show_processes_dialog (op, message, choices);
    }

  /* otherwise, we're showing the dialog, assume messages+choices hasn't changed */

  update_process_list_store (GTK_MOUNT_OPERATION (op),
                             priv->process_list_store,
                             processes);
}

static void
gtk_mount_operation_aborted (GMountOperation *op)
{
  GtkMountOperationPrivate *priv;

  priv = GTK_MOUNT_OPERATION (op)->priv;

  if (priv->dialog != NULL)
    {
      gtk_widget_destroy (GTK_WIDGET (priv->dialog));
      priv->dialog = NULL;
      g_object_notify (G_OBJECT (op), "is-showing");
      g_object_unref (op);
    }
}

/**
 * gtk_mount_operation_new:
 * @parent: (allow-none): transient parent of the window, or %NULL
 *
 * Creates a new #GtkMountOperation
 *
 * Returns: a new #GtkMountOperation
 *
 * Since: 2.14
 */
GMountOperation *
gtk_mount_operation_new (GtkWindow *parent)
{
  GMountOperation *mount_operation;

  mount_operation = g_object_new (GTK_TYPE_MOUNT_OPERATION,
                                  "parent", parent, NULL);

  return mount_operation;
}

/**
 * gtk_mount_operation_is_showing:
 * @op: a #GtkMountOperation
 *
 * Returns whether the #GtkMountOperation is currently displaying
 * a window.
 *
 * Returns: %TRUE if @op is currently displaying a window
 *
 * Since: 2.14
 */
gboolean
gtk_mount_operation_is_showing (GtkMountOperation *op)
{
  g_return_val_if_fail (GTK_IS_MOUNT_OPERATION (op), FALSE);

  return op->priv->dialog != NULL;
}

/**
 * gtk_mount_operation_set_parent:
 * @op: a #GtkMountOperation
 * @parent: (allow-none): transient parent of the window, or %NULL
 *
 * Sets the transient parent for windows shown by the
 * #GtkMountOperation.
 *
 * Since: 2.14
 */
void
gtk_mount_operation_set_parent (GtkMountOperation *op,
                                GtkWindow         *parent)
{
  GtkMountOperationPrivate *priv;

  g_return_if_fail (GTK_IS_MOUNT_OPERATION (op));
  g_return_if_fail (parent == NULL || GTK_IS_WINDOW (parent));

  priv = op->priv;

  if (priv->parent_window == parent)
    return;

  if (priv->parent_window)
    {
      g_signal_handlers_disconnect_by_func (priv->parent_window,
                                            gtk_widget_destroyed,
                                            &priv->parent_window);
      g_object_unref (priv->parent_window);
    }
  priv->parent_window = parent;
  if (priv->parent_window)
    {
      g_object_ref (priv->parent_window);
      g_signal_connect (priv->parent_window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed),
                        &priv->parent_window);
    }

  if (priv->dialog)
    gtk_window_set_transient_for (GTK_WINDOW (priv->dialog), priv->parent_window);

  g_object_notify (G_OBJECT (op), "parent");
}

/**
 * gtk_mount_operation_get_parent:
 * @op: a #GtkMountOperation
 *
 * Gets the transient parent used by the #GtkMountOperation
 *
 * Returns: (transfer none): the transient parent for windows shown by @op
 *
 * Since: 2.14
 */
GtkWindow *
gtk_mount_operation_get_parent (GtkMountOperation *op)
{
  g_return_val_if_fail (GTK_IS_MOUNT_OPERATION (op), NULL);

  return op->priv->parent_window;
}

/**
 * gtk_mount_operation_set_screen:
 * @op: a #GtkMountOperation
 * @screen: a #GdkScreen
 *
 * Sets the screen to show windows of the #GtkMountOperation on.
 *
 * Since: 2.14
 */
void
gtk_mount_operation_set_screen (GtkMountOperation *op,
                                GdkScreen         *screen)
{
  GtkMountOperationPrivate *priv;

  g_return_if_fail (GTK_IS_MOUNT_OPERATION (op));
  g_return_if_fail (GDK_IS_SCREEN (screen));

  priv = op->priv;

  if (priv->screen == screen)
    return;

  if (priv->screen)
    g_object_unref (priv->screen);

  priv->screen = g_object_ref (screen);

  if (priv->dialog)
    gtk_window_set_screen (GTK_WINDOW (priv->dialog), screen);

  g_object_notify (G_OBJECT (op), "screen");
}

/**
 * gtk_mount_operation_get_screen:
 * @op: a #GtkMountOperation
 *
 * Gets the screen on which windows of the #GtkMountOperation
 * will be shown.
 *
 * Returns: (transfer none): the screen on which windows of @op are shown
 *
 * Since: 2.14
 */
GdkScreen *
gtk_mount_operation_get_screen (GtkMountOperation *op)
{
  GtkMountOperationPrivate *priv;

  g_return_val_if_fail (GTK_IS_MOUNT_OPERATION (op), NULL);

  priv = op->priv;

  if (priv->dialog)
    return gtk_window_get_screen (GTK_WINDOW (priv->dialog));
  else if (priv->parent_window)
    return gtk_window_get_screen (GTK_WINDOW (priv->parent_window));
  else if (priv->screen)
    return priv->screen;
  else
    return gdk_screen_get_default ();
}

#define __GTK_MOUNT_OPERATION_C__
#include "gtkaliasdef.c"
