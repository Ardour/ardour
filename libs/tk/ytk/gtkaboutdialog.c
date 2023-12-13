/* GTK - The GIMP Toolkit
 * Copyright (C) 2001 CodeFactory AB
 * Copyright (C) 2001, 2002 Anders Carlsson
 * Copyright (C) 2003, 2004 Matthias Clasen <mclasen@redhat.com>
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

/*
 * Author: Anders Carlsson <andersca@gnome.org>
 *
 * Modified by the GTK+ Team and others 1997-2004.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include <string.h>

#include <gdk/gdkkeysyms.h>

#include "gtkaboutdialog.h"
#include "gtkbutton.h"
#include "gtkbbox.h"
#include "gtkdialog.h"
#include "gtkhbox.h"
#include "gtkimage.h"
#include "gtklabel.h"
#include "gtklinkbutton.h"
#include "gtkmarshalers.h"
#include "gtknotebook.h"
#include "gtkscrolledwindow.h"
#include "gtkstock.h"
#include "gtktextview.h"
#include "gtkvbox.h"
#include "gtkiconfactory.h"
#include "gtkshow.h"
#include "gtkmain.h"
#include "gtkmessagedialog.h"
#include "gtkprivate.h"
#include "gtkintl.h"

#include "gtkalias.h"


/**
 * SECTION:gtkaboutdialog
 * @Short_description: Display information about an application
 * @Title: GtkAboutDialog
 * @See_also:#GTK_STOCK_ABOUT
 *
 * The #GtkAboutDialog offers a simple way to display information about
 * a program like its logo, name, copyright, website and license. It is
 * also possible to give credits to the authors, documenters, translators
 * and artists who have worked on the program. An about dialog is typically
 * opened when the user selects the <literal>About</literal> option from
 * the <literal>Help</literal> menu. All parts of the dialog are optional.
 *
 * About dialog often contain links and email addresses. #GtkAboutDialog
 * supports this by offering global hooks, which are called when the user
 * clicks on a link or email address, see gtk_about_dialog_set_email_hook()
 * and gtk_about_dialog_set_url_hook(). Email addresses in the
 * authors, documenters and artists properties are recognized by looking for
 * <literal>&lt;user@<!-- -->host&gt;</literal>, URLs are
 * recognized by looking for <literal>http://url</literal>, with
 * <literal>url</literal> extending to the next space, tab or line break.
 *
 * <para id="gtk-about-dialog-hook-setup">
 * Since 2.18 #GtkAboutDialog provides default website and email hooks that
 * use gtk_show_uri().
 * </para>
 *
 * If you want provide your own hooks overriding the default ones, it is
 * important to do so before setting the website and email URL properties,
 * like this:
 * <informalexample><programlisting>
 * gtk_about_dialog_set_url_hook (GTK_ABOUT_DIALOG (dialog), launch_url, NULL, NULL);
 * gtk_about_dialog_set_website (GTK_ABOUT_DIALOG (dialog), app_url);
 * </programlisting></informalexample>
 * To disable the default hooks, you can pass %NULL as the hook func. Then,
 * the #GtkAboutDialog widget will not display the website or the
 * email addresses as clickable.
 *
 * To make constructing a #GtkAboutDialog as convenient as possible, you can
 * use the function gtk_show_about_dialog() which constructs and shows a dialog
 * and keeps it around so that it can be shown again.
 *
 * Note that GTK+ sets a default title of <literal>_("About &percnt;s")</literal>
 * on the dialog window (where &percnt;s is replaced by the name of the
 * application, but in order to ensure proper translation of the title,
 * applications should set the title property explicitly when constructing
 * a #GtkAboutDialog, as shown in the following example:
 * <informalexample><programlisting>
 * gtk_show_about_dialog (NULL,
 *                        "program-name", "ExampleCode",
 *                        "logo", example_logo,
 *                        "title" _("About ExampleCode"),
 *                        NULL);
 * </programlisting></informalexample>
 * Note that prior to GTK+ 2.12, the #GtkAboutDialog:program-name property
 * was called "name". This was changed to avoid the conflict with the
 * #GtkWidget:name property.
 */


static GdkColor default_link_color = { 0, 0, 0, 0xeeee };
static GdkColor default_visited_link_color = { 0, 0x5555, 0x1a1a, 0x8b8b };

typedef struct _GtkAboutDialogPrivate GtkAboutDialogPrivate;
struct _GtkAboutDialogPrivate
{
  gchar *name;
  gchar *version;
  gchar *copyright;
  gchar *comments;
  gchar *website_url;
  gchar *website_text;
  gchar *translator_credits;
  gchar *license;

  gchar **authors;
  gchar **documenters;
  gchar **artists;

  GtkWidget *logo_image;
  GtkWidget *name_label;
  GtkWidget *comments_label;
  GtkWidget *copyright_label;
  GtkWidget *website_label;

  GtkWidget *credits_button;
  GtkWidget *credits_dialog;
  GtkWidget *license_button;
  GtkWidget *license_dialog;

  GdkCursor *hand_cursor;
  GdkCursor *regular_cursor;

  GSList *visited_links;

  guint hovering_over_link : 1;
  guint wrap_license : 1;
};

#define GTK_ABOUT_DIALOG_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_ABOUT_DIALOG, GtkAboutDialogPrivate))


enum
{
  PROP_0,
  PROP_NAME,
  PROP_VERSION,
  PROP_COPYRIGHT,
  PROP_COMMENTS,
  PROP_WEBSITE,
  PROP_WEBSITE_LABEL,
  PROP_LICENSE,
  PROP_AUTHORS,
  PROP_DOCUMENTERS,
  PROP_TRANSLATOR_CREDITS,
  PROP_ARTISTS,
  PROP_LOGO,
  PROP_LOGO_ICON_NAME,
  PROP_WRAP_LICENSE
};

static void                 gtk_about_dialog_finalize       (GObject            *object);
static void                 gtk_about_dialog_get_property   (GObject            *object,
                                                             guint               prop_id,
                                                             GValue             *value,
                                                             GParamSpec         *pspec);
static void                 gtk_about_dialog_set_property   (GObject            *object,
                                                             guint               prop_id,
                                                             const GValue       *value,
                                                             GParamSpec         *pspec);
static void                 gtk_about_dialog_show           (GtkWidget          *widge);
static void                 update_name_version             (GtkAboutDialog     *about);
static GtkIconSet *         icon_set_new_from_pixbufs       (GList              *pixbufs);
static void                 follow_if_link                  (GtkAboutDialog     *about,
                                                             GtkTextView        *text_view,
                                                             GtkTextIter        *iter);
static void                 set_cursor_if_appropriate       (GtkAboutDialog     *about,
                                                             GtkTextView        *text_view,
                                                             gint                x,
                                                             gint                y);
static void                 display_credits_dialog          (GtkWidget          *button,
                                                             gpointer            data);
static void                 display_license_dialog          (GtkWidget          *button,
                                                             gpointer            data);
static void                 close_cb                        (GtkAboutDialog     *about);
static gboolean             gtk_about_dialog_activate_link  (GtkAboutDialog     *about,
                                                             const gchar        *uri);

static void                 default_url_hook                (GtkAboutDialog     *about,
                                                             const gchar        *uri,
                                                             gpointer            user_data);
static void                 default_email_hook              (GtkAboutDialog     *about,
                                                             const gchar        *email_address,
                                                             gpointer            user_data);

static gboolean activate_email_hook_set = FALSE;
static GtkAboutDialogActivateLinkFunc activate_email_hook = NULL;
static gpointer activate_email_hook_data = NULL;
static GDestroyNotify activate_email_hook_destroy = NULL;

static gboolean activate_url_hook_set = FALSE;
static GtkAboutDialogActivateLinkFunc activate_url_hook = NULL;
static gpointer activate_url_hook_data = NULL;
static GDestroyNotify activate_url_hook_destroy = NULL;

static void
default_url_hook (GtkAboutDialog *about,
                  const gchar    *uri,
                  gpointer        user_data)
{
  GdkScreen *screen;
  GError *error = NULL;

  screen = gtk_widget_get_screen (GTK_WIDGET (about));

  if (!gtk_show_uri (screen, uri, gtk_get_current_event_time (), &error))
    {
      GtkWidget *dialog;

      dialog = gtk_message_dialog_new (GTK_WINDOW (about),
                                       GTK_DIALOG_DESTROY_WITH_PARENT |
                                       GTK_DIALOG_MODAL,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       "%s", _("Could not show link"));
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                "%s", error->message);
      g_error_free (error);

      g_signal_connect (dialog, "response",
                        G_CALLBACK (gtk_widget_destroy), NULL);

      gtk_window_present (GTK_WINDOW (dialog));
    }
}

static void
default_email_hook (GtkAboutDialog *about,
                    const gchar    *email_address,
                    gpointer        user_data)
{
  char *escaped, *uri;

  escaped = g_uri_escape_string (email_address, NULL, FALSE);
  uri = g_strdup_printf ("mailto:%s", escaped);
  g_free (escaped);

  default_url_hook (about, uri, user_data);
  g_free (uri);
}

enum {
  ACTIVATE_LINK,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GtkAboutDialog, gtk_about_dialog, GTK_TYPE_DIALOG)

static void
gtk_about_dialog_class_init (GtkAboutDialogClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GObjectClass *)klass;
  widget_class = (GtkWidgetClass *)klass;

  object_class->set_property = gtk_about_dialog_set_property;
  object_class->get_property = gtk_about_dialog_get_property;

  object_class->finalize = gtk_about_dialog_finalize;

  widget_class->show = gtk_about_dialog_show;

  klass->activate_link = gtk_about_dialog_activate_link;

  /**
   * GtkAboutDialog::activate-link:
   * @label: The object on which the signal was emitted
   * @uri: the URI that is activated
   *
   * The signal which gets emitted to activate a URI.
   * Applications may connect to it to override the default behaviour,
   * which is to call gtk_show_uri().
   *
   * Returns: %TRUE if the link has been activated
   *
   * Since: 2.24
   */
  signals[ACTIVATE_LINK] =
    g_signal_new ("activate-link",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkAboutDialogClass, activate_link),
                  _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__STRING,
                  G_TYPE_BOOLEAN, 1, G_TYPE_STRING);

  /**
   * GtkAboutDialog:program-name:
   *
   * The name of the program.
   * If this is not set, it defaults to g_get_application_name().
   *
   * Since: 2.12
   */
  g_object_class_install_property (object_class,
                                   PROP_NAME,
                                   g_param_spec_string ("program-name",
                                                        P_("Program name"),
                                                        P_("The name of the program. If this is not set, it defaults to g_get_application_name()"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkAboutDialog:version:
   *
   * The version of the program.
   *
   * Since: 2.6
   */
  g_object_class_install_property (object_class,
                                   PROP_VERSION,
                                   g_param_spec_string ("version",
                                                        P_("Program version"),
                                                        P_("The version of the program"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkAboutDialog:copyright:
   *
   * Copyright information for the program.
   *
   * Since: 2.6
   */
  g_object_class_install_property (object_class,
                                   PROP_COPYRIGHT,
                                   g_param_spec_string ("copyright",
                                                        P_("Copyright string"),
                                                        P_("Copyright information for the program"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));
        

  /**
   * GtkAboutDialog:comments:
   *
   * Comments about the program. This string is displayed in a label
   * in the main dialog, thus it should be a short explanation of
   * the main purpose of the program, not a detailed list of features.
   *
   * Since: 2.6
   */
  g_object_class_install_property (object_class,
                                   PROP_COMMENTS,
                                   g_param_spec_string ("comments",
                                                        P_("Comments string"),
                                                        P_("Comments about the program"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkAboutDialog:license:
   *
   * The license of the program. This string is displayed in a
   * text view in a secondary dialog, therefore it is fine to use
   * a long multi-paragraph text. Note that the text is only wrapped
   * in the text view if the "wrap-license" property is set to %TRUE;
   * otherwise the text itself must contain the intended linebreaks.
   *
   * Since: 2.6
   */
  g_object_class_install_property (object_class,
                                   PROP_LICENSE,
                                   g_param_spec_string ("license",
                                                        _("License"),
                                                        _("The license of the program"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkAboutDialog:website:
   *
   * The URL for the link to the website of the program.
   * This should be a string starting with "http://.
   *
   * Since: 2.6
   */
  g_object_class_install_property (object_class,
                                   PROP_WEBSITE,
                                   g_param_spec_string ("website",
                                                        P_("Website URL"),
                                                        P_("The URL for the link to the website of the program"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkAboutDialog:website-label:
   *
   * The label for the link to the website of the program. If this is not set,
   * it defaults to the URL specified in the #GtkAboutDialog:website property.
   *
   * Since: 2.6
   */
  g_object_class_install_property (object_class,
                                   PROP_WEBSITE_LABEL,
                                   g_param_spec_string ("website-label",
                                                        P_("Website label"),
                                                        P_("The label for the link to the website of the program. If this is not set, it defaults to the URL"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkAboutDialog:authors:
   *
   * The authors of the program, as a %NULL-terminated array of strings.
   * Each string may contain email addresses and URLs, which will be displayed
   * as links, see the introduction for more details.
   *
   * Since: 2.6
   */
  g_object_class_install_property (object_class,
                                   PROP_AUTHORS,
                                   g_param_spec_boxed ("authors",
                                                       P_("Authors"),
                                                       P_("List of authors of the program"),
                                                       G_TYPE_STRV,
                                                       GTK_PARAM_READWRITE));

  /**
   * GtkAboutDialog:documenters:
   *
   * The people documenting the program, as a %NULL-terminated array of strings.
   * Each string may contain email addresses and URLs, which will be displayed
   * as links, see the introduction for more details.
   *
   * Since: 2.6
   */
  g_object_class_install_property (object_class,
                                   PROP_DOCUMENTERS,
                                   g_param_spec_boxed ("documenters",
                                                       P_("Documenters"),
                                                       P_("List of people documenting the program"),
                                                       G_TYPE_STRV,
                                                       GTK_PARAM_READWRITE));

  /**
   * GtkAboutDialog:artists:
   *
   * The people who contributed artwork to the program, as a %NULL-terminated
   * array of strings. Each string may contain email addresses and URLs, which
   * will be displayed as links, see the introduction for more details.
   *
   * Since: 2.6
   */
  g_object_class_install_property (object_class,
                                   PROP_ARTISTS,
                                   g_param_spec_boxed ("artists",
                                                       P_("Artists"),
                                                       P_("List of people who have contributed artwork to the program"),
                                                       G_TYPE_STRV,
                                                       GTK_PARAM_READWRITE));


  /**
   * GtkAboutDialog:translator-credits:
   *
   * Credits to the translators. This string should be marked as translatable.
   * The string may contain email addresses and URLs, which will be displayed
   * as links, see the introduction for more details.
   *
   * Since: 2.6
   */
  g_object_class_install_property (object_class,
                                   PROP_TRANSLATOR_CREDITS,
                                   g_param_spec_string ("translator-credits",
                                                        P_("Translator credits"),
                                                        P_("Credits to the translators. This string should be marked as translatable"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));
        
  /**
   * GtkAboutDialog:logo:
   *
   * A logo for the about box. If this is not set, it defaults to
   * gtk_window_get_default_icon_list().
   *
   * Since: 2.6
   */
  g_object_class_install_property (object_class,
                                   PROP_LOGO,
                                   g_param_spec_object ("logo",
                                                        P_("Logo"),
                                                        P_("A logo for the about box. If this is not set, it defaults to gtk_window_get_default_icon_list()"),
                                                        GDK_TYPE_PIXBUF,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkAboutDialog:logo-icon-name:
   *
   * A named icon to use as the logo for the about box. This property
   * overrides the #GtkAboutDialog:logo property.
   *
   * Since: 2.6
   */
  g_object_class_install_property (object_class,
                                   PROP_LOGO_ICON_NAME,
                                   g_param_spec_string ("logo-icon-name",
                                                        P_("Logo Icon Name"),
                                                        P_("A named icon to use as the logo for the about box."),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));
  /**
   * GtkAboutDialog:wrap-license:
   *
   * Whether to wrap the text in the license dialog.
   *
   * Since: 2.8
   */
  g_object_class_install_property (object_class,
                                   PROP_WRAP_LICENSE,
                                   g_param_spec_boolean ("wrap-license",
                                                         P_("Wrap license"),
                                                         P_("Whether to wrap the license text."),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));


  g_type_class_add_private (object_class, sizeof (GtkAboutDialogPrivate));
}

static gboolean
emit_activate_link (GtkAboutDialog *about,
                    const gchar    *uri)
{
  gboolean handled = FALSE;

  g_signal_emit (about, signals[ACTIVATE_LINK], 0, uri, &handled);

  return TRUE;
}

static void
gtk_about_dialog_init (GtkAboutDialog *about)
{
  GtkDialog *dialog = GTK_DIALOG (about);
  GtkAboutDialogPrivate *priv;
  GtkWidget *vbox, *hbox, *button, *close_button, *image;

  /* Data */
  priv = GTK_ABOUT_DIALOG_GET_PRIVATE (about);
  about->private_data = priv;

  priv->name = NULL;
  priv->version = NULL;
  priv->copyright = NULL;
  priv->comments = NULL;
  priv->website_url = NULL;
  priv->website_text = NULL;
  priv->translator_credits = NULL;
  priv->license = NULL;
  priv->authors = NULL;
  priv->documenters = NULL;
  priv->artists = NULL;

  priv->hand_cursor = gdk_cursor_new (GDK_HAND2);
  priv->regular_cursor = gdk_cursor_new (GDK_XTERM);
  priv->hovering_over_link = FALSE;
  priv->wrap_license = FALSE;

  gtk_dialog_set_has_separator (dialog, FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
  gtk_box_set_spacing (GTK_BOX (dialog->vbox), 2); /* 2 * 5 + 2 = 12 */
  gtk_container_set_border_width (GTK_CONTAINER (dialog->action_area), 5);

  /* Widgets */
  gtk_widget_push_composite_child ();

  vbox = gtk_vbox_new (FALSE, 8);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
  gtk_box_pack_start (GTK_BOX (dialog->vbox), vbox, TRUE, TRUE, 0);

  priv->logo_image = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (vbox), priv->logo_image, FALSE, FALSE, 0);

  priv->name_label = gtk_label_new (NULL);
  gtk_label_set_selectable (GTK_LABEL (priv->name_label), TRUE);
  gtk_label_set_justify (GTK_LABEL (priv->name_label), GTK_JUSTIFY_CENTER);
  gtk_box_pack_start (GTK_BOX (vbox), priv->name_label, FALSE, FALSE, 0);

  priv->comments_label = gtk_label_new (NULL);
  gtk_label_set_selectable (GTK_LABEL (priv->comments_label), TRUE);
  gtk_label_set_justify (GTK_LABEL (priv->comments_label), GTK_JUSTIFY_CENTER);
  gtk_label_set_line_wrap (GTK_LABEL (priv->comments_label), TRUE);
  gtk_box_pack_start (GTK_BOX (vbox), priv->comments_label, FALSE, FALSE, 0);

  priv->copyright_label = gtk_label_new (NULL);
  gtk_label_set_selectable (GTK_LABEL (priv->copyright_label), TRUE);
  gtk_label_set_justify (GTK_LABEL (priv->copyright_label), GTK_JUSTIFY_CENTER);
  gtk_box_pack_start (GTK_BOX (vbox), priv->copyright_label, FALSE, FALSE, 0);

  hbox = gtk_hbox_new (TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, FALSE, 0);

  priv->website_label = button = gtk_label_new ("");
  gtk_widget_set_no_show_all (button, TRUE);
  gtk_label_set_selectable (GTK_LABEL (button), TRUE);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  g_signal_connect_swapped (button, "activate-link",
                            G_CALLBACK (emit_activate_link), about);

  gtk_widget_show (vbox);
  gtk_widget_show (priv->logo_image);
  gtk_widget_show (priv->name_label);
  gtk_widget_show (hbox);

  /* Add the close button */
  close_button = gtk_dialog_add_button (GTK_DIALOG (about), GTK_STOCK_CLOSE,
                                        GTK_RESPONSE_CANCEL);
  gtk_dialog_set_default_response (GTK_DIALOG (about), GTK_RESPONSE_CANCEL);

  /* Add the credits button */
  button = gtk_button_new_with_mnemonic (_("C_redits"));
  gtk_widget_set_can_default (button, TRUE);
  image = gtk_image_new_from_stock (GTK_STOCK_ABOUT, GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image (GTK_BUTTON (button), image);
  gtk_widget_set_no_show_all (button, TRUE);
  gtk_box_pack_end (GTK_BOX (GTK_DIALOG (about)->action_area),
                    button, FALSE, TRUE, 0);
  gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (GTK_DIALOG (about)->action_area), button, TRUE);
  g_signal_connect (button, "clicked",
                    G_CALLBACK (display_credits_dialog), about);
  priv->credits_button = button;
  priv->credits_dialog = NULL;

  /* Add the license button */
  button = gtk_button_new_from_stock (_("_License"));
  gtk_widget_set_can_default (button, TRUE);
  gtk_widget_set_no_show_all (button, TRUE);
  gtk_box_pack_end (GTK_BOX (GTK_DIALOG (about)->action_area),
                    button, FALSE, TRUE, 0);
  gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (GTK_DIALOG (about)->action_area), button, TRUE);
  g_signal_connect (button, "clicked",
                    G_CALLBACK (display_license_dialog), about);
  priv->license_button = button;
  priv->license_dialog = NULL;

  gtk_window_set_resizable (GTK_WINDOW (about), FALSE);

  gtk_widget_pop_composite_child ();

  gtk_widget_grab_default (close_button);
  gtk_widget_grab_focus (close_button);

  /* force defaults */
  gtk_about_dialog_set_program_name (about, NULL);
  gtk_about_dialog_set_logo (about, NULL);
}

static void
gtk_about_dialog_finalize (GObject *object)
{
  GtkAboutDialog *about = GTK_ABOUT_DIALOG (object);
  GtkAboutDialogPrivate *priv = (GtkAboutDialogPrivate *)about->private_data;

  g_free (priv->name);
  g_free (priv->version);
  g_free (priv->copyright);
  g_free (priv->comments);
  g_free (priv->license);
  g_free (priv->website_url);
  g_free (priv->website_text);
  g_free (priv->translator_credits);

  g_strfreev (priv->authors);
  g_strfreev (priv->documenters);
  g_strfreev (priv->artists);

  g_slist_foreach (priv->visited_links, (GFunc)g_free, NULL);
  g_slist_free (priv->visited_links);

  gdk_cursor_unref (priv->hand_cursor);
  gdk_cursor_unref (priv->regular_cursor);

  G_OBJECT_CLASS (gtk_about_dialog_parent_class)->finalize (object);
}

static void
gtk_about_dialog_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkAboutDialog *about = GTK_ABOUT_DIALOG (object);
  GtkAboutDialogPrivate *priv = (GtkAboutDialogPrivate *)about->private_data;

  switch (prop_id)
    {
    case PROP_NAME:
      gtk_about_dialog_set_program_name (about, g_value_get_string (value));
      break;
    case PROP_VERSION:
      gtk_about_dialog_set_version (about, g_value_get_string (value));
      break;
    case PROP_COMMENTS:
      gtk_about_dialog_set_comments (about, g_value_get_string (value));
      break;
    case PROP_WEBSITE:
      gtk_about_dialog_set_website (about, g_value_get_string (value));
      break;
    case PROP_WEBSITE_LABEL:
      gtk_about_dialog_set_website_label (about, g_value_get_string (value));
      break;
    case PROP_LICENSE:
      gtk_about_dialog_set_license (about, g_value_get_string (value));
      break;
    case PROP_COPYRIGHT:
      gtk_about_dialog_set_copyright (about, g_value_get_string (value));
      break;
    case PROP_LOGO:
      gtk_about_dialog_set_logo (about, g_value_get_object (value));
      break;
    case PROP_AUTHORS:
      gtk_about_dialog_set_authors (about, (const gchar**)g_value_get_boxed (value));
      break;
    case PROP_DOCUMENTERS:
      gtk_about_dialog_set_documenters (about, (const gchar**)g_value_get_boxed (value));
      break;
    case PROP_ARTISTS:
      gtk_about_dialog_set_artists (about, (const gchar**)g_value_get_boxed (value));
      break;
    case PROP_TRANSLATOR_CREDITS:
      gtk_about_dialog_set_translator_credits (about, g_value_get_string (value));
      break;
    case PROP_LOGO_ICON_NAME:
      gtk_about_dialog_set_logo_icon_name (about, g_value_get_string (value));
      break;
    case PROP_WRAP_LICENSE:
      priv->wrap_license = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_about_dialog_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GtkAboutDialog *about = GTK_ABOUT_DIALOG (object);
  GtkAboutDialogPrivate *priv = (GtkAboutDialogPrivate *)about->private_data;

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, priv->name);
      break;
    case PROP_VERSION:
      g_value_set_string (value, priv->version);
      break;
    case PROP_COPYRIGHT:
      g_value_set_string (value, priv->copyright);
      break;
    case PROP_COMMENTS:
      g_value_set_string (value, priv->comments);
      break;
    case PROP_WEBSITE:
      g_value_set_string (value, priv->website_url);
      break;
    case PROP_WEBSITE_LABEL:
      g_value_set_string (value, priv->website_text);
      break;
    case PROP_LICENSE:
      g_value_set_string (value, priv->license);
      break;
    case PROP_TRANSLATOR_CREDITS:
      g_value_set_string (value, priv->translator_credits);
      break;
    case PROP_AUTHORS:
      g_value_set_boxed (value, priv->authors);
      break;
    case PROP_DOCUMENTERS:
      g_value_set_boxed (value, priv->documenters);
      break;
    case PROP_ARTISTS:
      g_value_set_boxed (value, priv->artists);
      break;
    case PROP_LOGO:
      if (gtk_image_get_storage_type (GTK_IMAGE (priv->logo_image)) == GTK_IMAGE_PIXBUF)
        g_value_set_object (value, gtk_image_get_pixbuf (GTK_IMAGE (priv->logo_image)));
      else
        g_value_set_object (value, NULL);
      break;
    case PROP_LOGO_ICON_NAME:
      if (gtk_image_get_storage_type (GTK_IMAGE (priv->logo_image)) == GTK_IMAGE_ICON_NAME)
        {
          const gchar *icon_name;

          gtk_image_get_icon_name (GTK_IMAGE (priv->logo_image), &icon_name, NULL);
          g_value_set_string (value, icon_name);
        }
      else
        g_value_set_string (value, NULL);
      break;
    case PROP_WRAP_LICENSE:
      g_value_set_boolean (value, priv->wrap_license);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gboolean
gtk_about_dialog_activate_link (GtkAboutDialog *about,
                                const gchar    *uri)
{
  if (g_str_has_prefix (uri, "mailto:"))
    {
      gchar *email;

      email = g_uri_unescape_string (uri + strlen ("mailto:"), NULL);

      if (activate_email_hook_set)
        activate_email_hook (about, email, activate_email_hook_data);
      else
        default_email_hook (about, email, NULL);

      g_free (email);
    }
  else
    {
      if (activate_url_hook_set)
        activate_url_hook (about, uri, activate_url_hook_data);
      else
        default_url_hook (about, uri, NULL);
    }

  return TRUE;
}

static void
update_website (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv = (GtkAboutDialogPrivate *)about->private_data;

  gtk_widget_show (priv->website_label);

  if (priv->website_url && (!activate_url_hook_set || activate_url_hook != NULL))
    {
      gchar *markup;

      if (priv->website_text)
        {
          gchar *escaped;

          escaped = g_markup_escape_text (priv->website_text, -1);
          markup = g_strdup_printf ("<a href=\"%s\">%s</a>",
                                    priv->website_url, escaped);
          g_free (escaped);
        }
      else
        {
          markup = g_strdup_printf ("<a href=\"%s\">%s</a>",
                                    priv->website_url, priv->website_url);
        }

      gtk_label_set_markup (GTK_LABEL (priv->website_label), markup);
      g_free (markup);
    }
  else
    {
      if (priv->website_url)
        gtk_label_set_text (GTK_LABEL (priv->website_label), priv->website_url);
      else if (priv->website_text)
        gtk_label_set_text (GTK_LABEL (priv->website_label), priv->website_text);
      else
        gtk_widget_hide (priv->website_label);
    }
}

static void
gtk_about_dialog_show (GtkWidget *widget)
{
  update_website (GTK_ABOUT_DIALOG (widget));

  GTK_WIDGET_CLASS (gtk_about_dialog_parent_class)->show (widget);
}

/**
 * gtk_about_dialog_get_name:
 * @about: a #GtkAboutDialog
 *
 * Returns the program name displayed in the about dialog.
 *
 * Return value: The program name. The string is owned by the about
 *  dialog and must not be modified.
 *
 * Since: 2.6
 *
 * Deprecated: 2.12: Use gtk_about_dialog_get_program_name() instead.
 */
const gchar *
gtk_about_dialog_get_name (GtkAboutDialog *about)
{
  return gtk_about_dialog_get_program_name (about);
}

/**
 * gtk_about_dialog_get_program_name:
 * @about: a #GtkAboutDialog
 *
 * Returns the program name displayed in the about dialog.
 *
 * Return value: The program name. The string is owned by the about
 *  dialog and must not be modified.
 *
 * Since: 2.12
 */
const gchar *
gtk_about_dialog_get_program_name (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;

  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  priv = (GtkAboutDialogPrivate *)about->private_data;

  return priv->name;
}

static void
update_name_version (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;
  gchar *title_string, *name_string;

  priv = (GtkAboutDialogPrivate *)about->private_data;

  title_string = g_strdup_printf (_("About %s"), priv->name);
  gtk_window_set_title (GTK_WINDOW (about), title_string);
  g_free (title_string);

  if (priv->version != NULL)
    name_string = g_markup_printf_escaped ("<span size=\"xx-large\" weight=\"bold\">%s %s</span>",
                                             priv->name, priv->version);
  else
    name_string = g_markup_printf_escaped ("<span size=\"xx-large\" weight=\"bold\">%s</span>",
                                           priv->name);

  gtk_label_set_markup (GTK_LABEL (priv->name_label), name_string);

  g_free (name_string);
}

/**
 * gtk_about_dialog_set_name:
 * @about: a #GtkAboutDialog
 * @name: (allow-none): the program name
 *
 * Sets the name to display in the about dialog.
 * If this is not set, it defaults to g_get_application_name().
 *
 * Since: 2.6
 *
 * Deprecated: 2.12: Use gtk_about_dialog_set_program_name() instead.
 */
void
gtk_about_dialog_set_name (GtkAboutDialog *about,
                           const gchar    *name)
{
    gtk_about_dialog_set_program_name (about, name);
}

/**
 * gtk_about_dialog_set_program_name:
 * @about: a #GtkAboutDialog
 * @name: the program name
 *
 * Sets the name to display in the about dialog.
 * If this is not set, it defaults to g_get_application_name().
 *
 * Since: 2.12
 */
void
gtk_about_dialog_set_program_name (GtkAboutDialog *about,
                                   const gchar    *name)
{
  GtkAboutDialogPrivate *priv;
  gchar *tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = (GtkAboutDialogPrivate *)about->private_data;
  tmp = priv->name;
  priv->name = g_strdup (name ? name : g_get_application_name ());
  g_free (tmp);

  update_name_version (about);

  g_object_notify (G_OBJECT (about), "program-name");
}


/**
 * gtk_about_dialog_get_version:
 * @about: a #GtkAboutDialog
 *
 * Returns the version string.
 *
 * Return value: The version string. The string is owned by the about
 *  dialog and must not be modified.
 *
 * Since: 2.6
 */
const gchar *
gtk_about_dialog_get_version (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;

  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  priv = (GtkAboutDialogPrivate *)about->private_data;

  return priv->version;
}

/**
 * gtk_about_dialog_set_version:
 * @about: a #GtkAboutDialog
 * @version: (allow-none): the version string
 *
 * Sets the version string to display in the about dialog.
 *
 * Since: 2.6
 */
void
gtk_about_dialog_set_version (GtkAboutDialog *about,
                              const gchar    *version)
{
  GtkAboutDialogPrivate *priv;
  gchar *tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = (GtkAboutDialogPrivate *)about->private_data;

  tmp = priv->version;
  priv->version = g_strdup (version);
  g_free (tmp);

  update_name_version (about);

  g_object_notify (G_OBJECT (about), "version");
}

/**
 * gtk_about_dialog_get_copyright:
 * @about: a #GtkAboutDialog
 *
 * Returns the copyright string.
 *
 * Return value: The copyright string. The string is owned by the about
 *  dialog and must not be modified.
 *
 * Since: 2.6
 */
const gchar *
gtk_about_dialog_get_copyright (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;

  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  priv = (GtkAboutDialogPrivate *)about->private_data;

  return priv->copyright;
}

/**
 * gtk_about_dialog_set_copyright:
 * @about: a #GtkAboutDialog
 * @copyright: (allow-none) the copyright string
 *
 * Sets the copyright string to display in the about dialog.
 * This should be a short string of one or two lines.
 *
 * Since: 2.6
 */
void
gtk_about_dialog_set_copyright (GtkAboutDialog *about,
                                const gchar    *copyright)
{
  GtkAboutDialogPrivate *priv;
  gchar *copyright_string, *tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = (GtkAboutDialogPrivate *)about->private_data;

  tmp = priv->copyright;
  priv->copyright = g_strdup (copyright);
  g_free (tmp);

  if (priv->copyright != NULL)
    {
      copyright_string = g_markup_printf_escaped ("<span size=\"small\">%s</span>",
                                                  priv->copyright);
      gtk_label_set_markup (GTK_LABEL (priv->copyright_label), copyright_string);
      g_free (copyright_string);

      gtk_widget_show (priv->copyright_label);
    }
  else
    gtk_widget_hide (priv->copyright_label);

  g_object_notify (G_OBJECT (about), "copyright");
}

/**
 * gtk_about_dialog_get_comments:
 * @about: a #GtkAboutDialog
 *
 * Returns the comments string.
 *
 * Return value: The comments. The string is owned by the about
 *  dialog and must not be modified.
 *
 * Since: 2.6
 */
const gchar *
gtk_about_dialog_get_comments (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;

  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  priv = (GtkAboutDialogPrivate *)about->private_data;

  return priv->comments;
}

/**
 * gtk_about_dialog_set_comments:
 * @about: a #GtkAboutDialog
 * @comments: (allow-none): a comments string
 *
 * Sets the comments string to display in the about dialog.
 * This should be a short string of one or two lines.
 *
 * Since: 2.6
 */
void
gtk_about_dialog_set_comments (GtkAboutDialog *about,
                               const gchar    *comments)
{
  GtkAboutDialogPrivate *priv;
  gchar *tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = (GtkAboutDialogPrivate *)about->private_data;

  tmp = priv->comments;
  if (comments)
    {
      priv->comments = g_strdup (comments);
      gtk_label_set_text (GTK_LABEL (priv->comments_label), priv->comments);
      gtk_widget_show (priv->comments_label);
    }
  else
    {
      priv->comments = NULL;
      gtk_widget_hide (priv->comments_label);
    }
  g_free (tmp);

  g_object_notify (G_OBJECT (about), "comments");
}

/**
 * gtk_about_dialog_get_license:
 * @about: a #GtkAboutDialog
 *
 * Returns the license information.
 *
 * Return value: The license information. The string is owned by the about
 *  dialog and must not be modified.
 *
 * Since: 2.6
 */
const gchar *
gtk_about_dialog_get_license (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;

  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  priv = (GtkAboutDialogPrivate *)about->private_data;

  return priv->license;
}

/**
 * gtk_about_dialog_set_license:
 * @about: a #GtkAboutDialog
 * @license: (allow-none): the license information or %NULL
 *
 * Sets the license information to be displayed in the secondary
 * license dialog. If @license is %NULL, the license button is
 * hidden.
 *
 * Since: 2.6
 */
void
gtk_about_dialog_set_license (GtkAboutDialog *about,
                              const gchar    *license)
{
  GtkAboutDialogPrivate *priv;
  gchar *tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = (GtkAboutDialogPrivate *)about->private_data;

  tmp = priv->license;
  if (license)
    {
      priv->license = g_strdup (license);
      gtk_widget_show (priv->license_button);
    }
  else
    {
      priv->license = NULL;
      gtk_widget_hide (priv->license_button);
    }
  g_free (tmp);

  g_object_notify (G_OBJECT (about), "license");
}

/**
 * gtk_about_dialog_get_wrap_license:
 * @about: a #GtkAboutDialog
 *
 * Returns whether the license text in @about is
 * automatically wrapped.
 *
 * Returns: %TRUE if the license text is wrapped
 *
 * Since: 2.8
 */
gboolean
gtk_about_dialog_get_wrap_license (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;

  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), FALSE);

  priv = (GtkAboutDialogPrivate *)about->private_data;

  return priv->wrap_license;
}

/**
 * gtk_about_dialog_set_wrap_license:
 * @about: a #GtkAboutDialog
 * @wrap_license: whether to wrap the license
 *
 * Sets whether the license text in @about is
 * automatically wrapped.
 *
 * Since: 2.8
 */
void
gtk_about_dialog_set_wrap_license (GtkAboutDialog *about,
                                   gboolean        wrap_license)
{
  GtkAboutDialogPrivate *priv;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = (GtkAboutDialogPrivate *)about->private_data;

  wrap_license = wrap_license != FALSE;

  if (priv->wrap_license != wrap_license)
    {
       priv->wrap_license = wrap_license;

       g_object_notify (G_OBJECT (about), "wrap-license");
    }
}

/**
 * gtk_about_dialog_get_website:
 * @about: a #GtkAboutDialog
 *
 * Returns the website URL.
 *
 * Return value: The website URL. The string is owned by the about
 *  dialog and must not be modified.
 *
 * Since: 2.6
 */
const gchar *
gtk_about_dialog_get_website (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;

  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  priv = (GtkAboutDialogPrivate *)about->private_data;

  return priv->website_url;
}

/**
 * gtk_about_dialog_set_website:
 * @about: a #GtkAboutDialog
 * @website: (allow-none): a URL string starting with "http://"
 *
 * Sets the URL to use for the website link.
 *
 * Note that that the hook functions need to be set up
 * before calling this function.
 *
 * Since: 2.6
 */
void
gtk_about_dialog_set_website (GtkAboutDialog *about,
                              const gchar    *website)
{
  GtkAboutDialogPrivate *priv;
  gchar *tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = (GtkAboutDialogPrivate *)about->private_data;

  tmp = priv->website_url;
  priv->website_url = g_strdup (website);
  g_free (tmp);

  update_website (about);

  g_object_notify (G_OBJECT (about), "website");
}

/**
 * gtk_about_dialog_get_website_label:
 * @about: a #GtkAboutDialog
 *
 * Returns the label used for the website link.
 *
 * Return value: The label used for the website link. The string is
 *     owned by the about dialog and must not be modified.
 *
 * Since: 2.6
 */
const gchar *
gtk_about_dialog_get_website_label (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;

  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  priv = (GtkAboutDialogPrivate *)about->private_data;

  return priv->website_text;
}

/**
 * gtk_about_dialog_set_website_label:
 * @about: a #GtkAboutDialog
 * @website_label: the label used for the website link
 *
 * Sets the label to be used for the website link.
 * It defaults to the website URL.
 *
 * Since: 2.6
 */
void
gtk_about_dialog_set_website_label (GtkAboutDialog *about,
                                    const gchar    *website_label)
{
  GtkAboutDialogPrivate *priv;
  gchar *tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = (GtkAboutDialogPrivate *)about->private_data;

  tmp = priv->website_text;
  priv->website_text = g_strdup (website_label);
  g_free (tmp);

  update_website (about);

  g_object_notify (G_OBJECT (about), "website-label");
}

/**
 * gtk_about_dialog_get_authors:
 * @about: a #GtkAboutDialog
 *
 * Returns the string which are displayed in the authors tab
 * of the secondary credits dialog.
 *
 * Return value: (array zero-terminated=1) (transfer none): A
 *  %NULL-terminated string array containing the authors. The array is
 *  owned by the about dialog and must not be modified.
 *
 * Since: 2.6
 */
const gchar * const *
gtk_about_dialog_get_authors (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;

  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  priv = (GtkAboutDialogPrivate *)about->private_data;

  return (const gchar * const *) priv->authors;
}

static void
update_credits_button_visibility (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv = about->private_data;
  gboolean show;

  show = priv->authors != NULL ||
         priv->documenters != NULL ||
         priv->artists != NULL ||
         (priv->translator_credits != NULL &&
          strcmp (priv->translator_credits, "translator_credits") &&
          strcmp (priv->translator_credits, "translator-credits"));
  if (show)
    gtk_widget_show (priv->credits_button);
  else
    gtk_widget_hide (priv->credits_button);
}

/**
 * gtk_about_dialog_set_authors:
 * @about: a #GtkAboutDialog
 * @authors: a %NULL-terminated array of strings
 *
 * Sets the strings which are displayed in the authors tab
 * of the secondary credits dialog.
 *
 * Since: 2.6
 */
void
gtk_about_dialog_set_authors (GtkAboutDialog  *about,
                              const gchar    **authors)
{
  GtkAboutDialogPrivate *priv;
  gchar **tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = (GtkAboutDialogPrivate *)about->private_data;

  tmp = priv->authors;
  priv->authors = g_strdupv ((gchar **)authors);
  g_strfreev (tmp);

  update_credits_button_visibility (about);

  g_object_notify (G_OBJECT (about), "authors");
}

/**
 * gtk_about_dialog_get_documenters:
 * @about: a #GtkAboutDialog
 *
 * Returns the string which are displayed in the documenters
 * tab of the secondary credits dialog.
 *
 * Return value: (array zero-terminated=1) (transfer none): A
 *  %NULL-terminated string array containing the documenters. The
 *  array is owned by the about dialog and must not be modified.
 *
 * Since: 2.6
 */
const gchar * const *
gtk_about_dialog_get_documenters (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;

  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  priv = (GtkAboutDialogPrivate *)about->private_data;

  return (const gchar * const *)priv->documenters;
}

/**
 * gtk_about_dialog_set_documenters:
 * @about: a #GtkAboutDialog
 * @documenters: a %NULL-terminated array of strings
 *
 * Sets the strings which are displayed in the documenters tab
 * of the secondary credits dialog.
 *
 * Since: 2.6
 */
void
gtk_about_dialog_set_documenters (GtkAboutDialog *about,
                                  const gchar   **documenters)
{
  GtkAboutDialogPrivate *priv;
  gchar **tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = (GtkAboutDialogPrivate *)about->private_data;

  tmp = priv->documenters;
  priv->documenters = g_strdupv ((gchar **)documenters);
  g_strfreev (tmp);

  update_credits_button_visibility (about);

  g_object_notify (G_OBJECT (about), "documenters");
}

/**
 * gtk_about_dialog_get_artists:
 * @about: a #GtkAboutDialog
 *
 * Returns the string which are displayed in the artists tab
 * of the secondary credits dialog.
 *
 * Return value: (array zero-terminated=1) (transfer none): A
 *  %NULL-terminated string array containing the artists. The array is
 *  owned by the about dialog and must not be modified.
 *
 * Since: 2.6
 */
const gchar * const *
gtk_about_dialog_get_artists (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;

  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  priv = (GtkAboutDialogPrivate *)about->private_data;

  return (const gchar * const *)priv->artists;
}

/**
 * gtk_about_dialog_set_artists:
 * @about: a #GtkAboutDialog
 * @artists: a %NULL-terminated array of strings
 *
 * Sets the strings which are displayed in the artists tab
 * of the secondary credits dialog.
 *
 * Since: 2.6
 */
void
gtk_about_dialog_set_artists (GtkAboutDialog *about,
                              const gchar   **artists)
{
  GtkAboutDialogPrivate *priv;
  gchar **tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = (GtkAboutDialogPrivate *)about->private_data;

  tmp = priv->artists;
  priv->artists = g_strdupv ((gchar **)artists);
  g_strfreev (tmp);

  update_credits_button_visibility (about);

  g_object_notify (G_OBJECT (about), "artists");
}

/**
 * gtk_about_dialog_get_translator_credits:
 * @about: a #GtkAboutDialog
 *
 * Returns the translator credits string which is displayed
 * in the translators tab of the secondary credits dialog.
 *
 * Return value: The translator credits string. The string is
 *   owned by the about dialog and must not be modified.
 *
 * Since: 2.6
 */
const gchar *
gtk_about_dialog_get_translator_credits (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;

  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  priv = (GtkAboutDialogPrivate *)about->private_data;

  return priv->translator_credits;
}

/**
 * gtk_about_dialog_set_translator_credits:
 * @about: a #GtkAboutDialog
 * @translator_credits: (allow-none): the translator credits
 *
 * Sets the translator credits string which is displayed in
 * the translators tab of the secondary credits dialog.
 *
 * The intended use for this string is to display the translator
 * of the language which is currently used in the user interface.
 * Using gettext(), a simple way to achieve that is to mark the
 * string for translation:
 * |[
 *  gtk_about_dialog_set_translator_credits (about, _("translator-credits"));
 * ]|
 * It is a good idea to use the customary msgid "translator-credits" for this
 * purpose, since translators will already know the purpose of that msgid, and
 * since #GtkAboutDialog will detect if "translator-credits" is untranslated
 * and hide the tab.
 *
 * Since: 2.6
 */
void
gtk_about_dialog_set_translator_credits (GtkAboutDialog *about,
                                         const gchar    *translator_credits)
{
  GtkAboutDialogPrivate *priv;
  gchar *tmp;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = (GtkAboutDialogPrivate *)about->private_data;

  tmp = priv->translator_credits;
  priv->translator_credits = g_strdup (translator_credits);
  g_free (tmp);

  update_credits_button_visibility (about);

  g_object_notify (G_OBJECT (about), "translator-credits");
}

/**
 * gtk_about_dialog_get_logo:
 * @about: a #GtkAboutDialog
 *
 * Returns the pixbuf displayed as logo in the about dialog.
 *
 * Return value: (transfer none): the pixbuf displayed as logo. The
 *   pixbuf is owned by the about dialog. If you want to keep a
 *   reference to it, you have to call g_object_ref() on it.
 *
 * Since: 2.6
 */
GdkPixbuf *
gtk_about_dialog_get_logo (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;

  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  priv = (GtkAboutDialogPrivate *)about->private_data;

  if (gtk_image_get_storage_type (GTK_IMAGE (priv->logo_image)) == GTK_IMAGE_PIXBUF)
    return gtk_image_get_pixbuf (GTK_IMAGE (priv->logo_image));
  else
    return NULL;
}

static GtkIconSet *
icon_set_new_from_pixbufs (GList *pixbufs)
{
  GtkIconSet *icon_set = gtk_icon_set_new ();

  for (; pixbufs; pixbufs = pixbufs->next)
    {
      GdkPixbuf *pixbuf = GDK_PIXBUF (pixbufs->data);

      GtkIconSource *icon_source = gtk_icon_source_new ();
      gtk_icon_source_set_pixbuf (icon_source, pixbuf);
      gtk_icon_set_add_source (icon_set, icon_source);
      gtk_icon_source_free (icon_source);
    }

  return icon_set;
}

/**
 * gtk_about_dialog_set_logo:
 * @about: a #GtkAboutDialog
 * @logo: (allow-none): a #GdkPixbuf, or %NULL
 *
 * Sets the pixbuf to be displayed as logo in the about dialog.
 * If it is %NULL, the default window icon set with
 * gtk_window_set_default_icon() will be used.
 *
 * Since: 2.6
 */
void
gtk_about_dialog_set_logo (GtkAboutDialog *about,
                           GdkPixbuf      *logo)
{
  GtkAboutDialogPrivate *priv;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = (GtkAboutDialogPrivate *)about->private_data;

  g_object_freeze_notify (G_OBJECT (about));

  if (gtk_image_get_storage_type (GTK_IMAGE (priv->logo_image)) == GTK_IMAGE_ICON_NAME)
    g_object_notify (G_OBJECT (about), "logo-icon-name");

  if (logo != NULL)
    gtk_image_set_from_pixbuf (GTK_IMAGE (priv->logo_image), logo);
  else
    {
      GList *pixbufs = gtk_window_get_default_icon_list ();

      if (pixbufs != NULL)
        {
          GtkIconSet *icon_set = icon_set_new_from_pixbufs (pixbufs);

          gtk_image_set_from_icon_set (GTK_IMAGE (priv->logo_image),
                                       icon_set, GTK_ICON_SIZE_DIALOG);

          gtk_icon_set_unref (icon_set);
          g_list_free (pixbufs);
        }
    }

  g_object_notify (G_OBJECT (about), "logo");

  g_object_thaw_notify (G_OBJECT (about));
}

/**
 * gtk_about_dialog_get_logo_icon_name:
 * @about: a #GtkAboutDialog
 *
 * Returns the icon name displayed as logo in the about dialog.
 *
 * Return value: the icon name displayed as logo. The string is
 *   owned by the dialog. If you want to keep a reference
 *   to it, you have to call g_strdup() on it.
 *
 * Since: 2.6
 */
const gchar *
gtk_about_dialog_get_logo_icon_name (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv;
  const gchar *icon_name = NULL;

  g_return_val_if_fail (GTK_IS_ABOUT_DIALOG (about), NULL);

  priv = (GtkAboutDialogPrivate *)about->private_data;

  if (gtk_image_get_storage_type (GTK_IMAGE (priv->logo_image)) == GTK_IMAGE_ICON_NAME)
    gtk_image_get_icon_name (GTK_IMAGE (priv->logo_image), &icon_name, NULL);

  return icon_name;
}

/**
 * gtk_about_dialog_set_logo_icon_name:
 * @about: a #GtkAboutDialog
 * @icon_name: (allow-none): an icon name, or %NULL
 *
 * Sets the pixbuf to be displayed as logo in the about dialog.
 * If it is %NULL, the default window icon set with
 * gtk_window_set_default_icon() will be used.
 *
 * Since: 2.6
 */
void
gtk_about_dialog_set_logo_icon_name (GtkAboutDialog *about,
                                     const gchar    *icon_name)
{
  GtkAboutDialogPrivate *priv;

  g_return_if_fail (GTK_IS_ABOUT_DIALOG (about));

  priv = (GtkAboutDialogPrivate *)about->private_data;

  g_object_freeze_notify (G_OBJECT (about));

  if (gtk_image_get_storage_type (GTK_IMAGE (priv->logo_image)) == GTK_IMAGE_PIXBUF)
    g_object_notify (G_OBJECT (about), "logo");

  gtk_image_set_from_icon_name (GTK_IMAGE (priv->logo_image), icon_name,
                                GTK_ICON_SIZE_DIALOG);
  g_object_notify (G_OBJECT (about), "logo-icon-name");

  g_object_thaw_notify (G_OBJECT (about));
}

static void
follow_if_link (GtkAboutDialog *about,
                GtkTextView    *text_view,
                GtkTextIter    *iter)
{
  GSList *tags = NULL, *tagp = NULL;
  GtkAboutDialogPrivate *priv = (GtkAboutDialogPrivate *)about->private_data;
  gchar *uri = NULL;

  tags = gtk_text_iter_get_tags (iter);
  for (tagp = tags; tagp != NULL && !uri; tagp = tagp->next)
    {
      GtkTextTag *tag = tagp->data;

      uri = g_object_get_data (G_OBJECT (tag), "uri");
      if (uri)
        emit_activate_link (about, uri);

      if (uri && !g_slist_find_custom (priv->visited_links, uri, (GCompareFunc)strcmp))
        {
          GdkColor *style_visited_link_color;
          GdkColor color;

          gtk_widget_ensure_style (GTK_WIDGET (about));
          gtk_widget_style_get (GTK_WIDGET (about),
                                "visited-link-color", &style_visited_link_color,
                                NULL);
          if (style_visited_link_color)
            {
              color = *style_visited_link_color;
              gdk_color_free (style_visited_link_color);
            }
          else
            color = default_visited_link_color;

          g_object_set (G_OBJECT (tag), "foreground-gdk", &color, NULL);

          priv->visited_links = g_slist_prepend (priv->visited_links, g_strdup (uri));
        }
    }

  if (tags)
    g_slist_free (tags);
}

static gboolean
text_view_key_press_event (GtkWidget      *text_view,
                           GdkEventKey    *event,
                           GtkAboutDialog *about)
{
  GtkTextIter iter;
  GtkTextBuffer *buffer;

  switch (event->keyval)
    {
      case GDK_Return:
      case GDK_ISO_Enter:
      case GDK_KP_Enter:
        buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
        gtk_text_buffer_get_iter_at_mark (buffer, &iter,
                                          gtk_text_buffer_get_insert (buffer));
        follow_if_link (about, GTK_TEXT_VIEW (text_view), &iter);
        break;

      default:
        break;
    }

  return FALSE;
}

static gboolean
text_view_event_after (GtkWidget      *text_view,
                       GdkEvent       *event,
                       GtkAboutDialog *about)
{
  GtkTextIter start, end, iter;
  GtkTextBuffer *buffer;
  GdkEventButton *button_event;
  gint x, y;

  if (event->type != GDK_BUTTON_RELEASE)
    return FALSE;

  button_event = (GdkEventButton *)event;

  if (button_event->button != 1)
    return FALSE;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));

  /* we shouldn't follow a link if the user has selected something */
  gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
  if (gtk_text_iter_get_offset (&start) != gtk_text_iter_get_offset (&end))
    return FALSE;

  gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (text_view),
                                         GTK_TEXT_WINDOW_WIDGET,
                                         button_event->x, button_event->y, &x, &y);

  gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (text_view), &iter, x, y);

  follow_if_link (about, GTK_TEXT_VIEW (text_view), &iter);

  return FALSE;
}

static void
set_cursor_if_appropriate (GtkAboutDialog *about,
                           GtkTextView    *text_view,
                           gint            x,
                           gint            y)
{
  GtkAboutDialogPrivate *priv = (GtkAboutDialogPrivate *)about->private_data;
  GSList *tags = NULL, *tagp = NULL;
  GtkTextIter iter;
  gboolean hovering_over_link = FALSE;

  gtk_text_view_get_iter_at_location (text_view, &iter, x, y);

  tags = gtk_text_iter_get_tags (&iter);
  for (tagp = tags;  tagp != NULL;  tagp = tagp->next)
    {
      GtkTextTag *tag = tagp->data;
      gchar *uri = g_object_get_data (G_OBJECT (tag), "uri");

      if (uri != NULL)
        {
          hovering_over_link = TRUE;
          break;
        }
    }

  if (hovering_over_link != priv->hovering_over_link)
    {
      priv->hovering_over_link = hovering_over_link;

      if (hovering_over_link)
        gdk_window_set_cursor (gtk_text_view_get_window (text_view, GTK_TEXT_WINDOW_TEXT), priv->hand_cursor);
      else
        gdk_window_set_cursor (gtk_text_view_get_window (text_view, GTK_TEXT_WINDOW_TEXT), priv->regular_cursor);
    }

  if (tags)
    g_slist_free (tags);
}

static gboolean
text_view_motion_notify_event (GtkWidget *text_view,
                               GdkEventMotion *event,
                               GtkAboutDialog *about)
{
  gint x, y;

  gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (text_view),
                                         GTK_TEXT_WINDOW_WIDGET,
                                         event->x, event->y, &x, &y);

  set_cursor_if_appropriate (about, GTK_TEXT_VIEW (text_view), x, y);

  gdk_event_request_motions (event);

  return FALSE;
}


static gboolean
text_view_visibility_notify_event (GtkWidget          *text_view,
                                   GdkEventVisibility *event,
                                   GtkAboutDialog     *about)
{
  gint wx, wy, bx, by;

  gdk_window_get_pointer (text_view->window, &wx, &wy, NULL);

  gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (text_view),
                                         GTK_TEXT_WINDOW_WIDGET,
                                         wx, wy, &bx, &by);

  set_cursor_if_appropriate (about, GTK_TEXT_VIEW (text_view), bx, by);

  return FALSE;
}

static GtkWidget *
text_view_new (GtkAboutDialog  *about,
               GtkWidget       *dialog,
               gchar          **strings,
               GtkWrapMode      wrap_mode)
{
  gchar **p;
  gchar *q0, *q1, *q2, *r1, *r2;
  GtkWidget *view;
  GtkTextView *text_view;
  GtkTextBuffer *buffer;
  GdkColor *style_link_color;
  GdkColor *style_visited_link_color;
  GdkColor color;
  GdkColor link_color;
  GdkColor visited_link_color;
  GtkAboutDialogPrivate *priv = (GtkAboutDialogPrivate *)about->private_data;

  gtk_widget_ensure_style (GTK_WIDGET (about));
  gtk_widget_style_get (GTK_WIDGET (about),
                        "link-color", &style_link_color,
                        "visited-link-color", &style_visited_link_color,
                        NULL);
  if (style_link_color)
    {
      link_color = *style_link_color;
      gdk_color_free (style_link_color);
    }
  else
    link_color = default_link_color;

  if (style_visited_link_color)
    {
      visited_link_color = *style_visited_link_color;
      gdk_color_free (style_visited_link_color);
    }
  else
    visited_link_color = default_visited_link_color;

  view = gtk_text_view_new ();
  text_view = GTK_TEXT_VIEW (view);
  buffer = gtk_text_view_get_buffer (text_view);
  gtk_text_view_set_cursor_visible (text_view, FALSE);
  gtk_text_view_set_editable (text_view, FALSE);
  gtk_text_view_set_wrap_mode (text_view, wrap_mode);

  gtk_text_view_set_left_margin (text_view, 8);
  gtk_text_view_set_right_margin (text_view, 8);

  g_signal_connect (view, "key-press-event",
                    G_CALLBACK (text_view_key_press_event), about);
  g_signal_connect (view, "event-after",
                    G_CALLBACK (text_view_event_after), about);
  g_signal_connect (view, "motion-notify-event",
                    G_CALLBACK (text_view_motion_notify_event), about);
  g_signal_connect (view, "visibility-notify-event",
                    G_CALLBACK (text_view_visibility_notify_event), about);

  if (strings == NULL)
    {
      gtk_widget_hide (view);
      return view;
    }

  for (p = strings; *p; p++)
    {
      q0  = *p;
      while (*q0)
        {
          q1 = strchr (q0, '<');
          q2 = q1 ? strchr (q1, '>') : NULL;
          r1 = strstr (q0, "http://");
          if (r1)
            {
              r2 = strpbrk (r1, " \n\t");
              if (!r2)
                r2 = strchr (r1, '\0');
            }
          else
            r2 = NULL;

          if (r1 && r2 && (!q1 || !q2 || (r1 < q1)))
            {
              q1 = r1;
              q2 = r2;
            }

          if (q1 && q2)
            {
              GtkTextIter end;
              gchar *link;
              gchar *uri;
              const gchar *link_type;
              GtkTextTag *tag;

              if (*q1 == '<')
                {
                  gtk_text_buffer_insert_at_cursor (buffer, q0, (q1 - q0) + 1);
                  gtk_text_buffer_get_end_iter (buffer, &end);
                  q1++;
                  link_type = "email";
                }
              else
                {
                  gtk_text_buffer_insert_at_cursor (buffer, q0, q1 - q0);
                  gtk_text_buffer_get_end_iter (buffer, &end);
                  link_type = "uri";
                }

              q0 = q2;

              link = g_strndup (q1, q2 - q1);

              if (g_slist_find_custom (priv->visited_links, link, (GCompareFunc)strcmp))
                color = visited_link_color;
              else
                color = link_color;

              tag = gtk_text_buffer_create_tag (buffer, NULL,
                                                "foreground-gdk", &color,
                                                "underline", PANGO_UNDERLINE_SINGLE,
                                                NULL);
              if (strcmp (link_type, "email") == 0)
                {
                  gchar *escaped;

                  escaped = g_uri_escape_string (link, NULL, FALSE);
                  uri = g_strconcat ("mailto:", escaped, NULL);
                  g_free (escaped);
                }
              else
                {
                  uri = g_strdup (link);
                }
              g_object_set_data_full (G_OBJECT (tag), I_("uri"), uri, g_free);
              gtk_text_buffer_insert_with_tags (buffer, &end, link, -1, tag, NULL);

              g_free (link);
            }
          else
            {
              gtk_text_buffer_insert_at_cursor (buffer, q0, -1);
              break;
            }
        }

      if (p[1])
        gtk_text_buffer_insert_at_cursor (buffer, "\n", 1);
    }

  gtk_widget_show (view);
  return view;
}

static void
add_credits_page (GtkAboutDialog *about,
                  GtkWidget      *credits_dialog,
                  GtkWidget      *notebook,
                  gchar          *title,
                  gchar         **people)
{
  GtkWidget *sw, *view;

  view = text_view_new (about, credits_dialog, people, GTK_WRAP_NONE);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
                                       GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  gtk_container_add (GTK_CONTAINER (sw), view);

  gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
                            sw, gtk_label_new (title));
}

static void
display_credits_dialog (GtkWidget *button,
                        gpointer   data)
{
  GtkAboutDialog *about = (GtkAboutDialog *)data;
  GtkAboutDialogPrivate *priv = (GtkAboutDialogPrivate *)about->private_data;
  GtkWidget *dialog, *notebook;
  GtkDialog *credits_dialog;

  if (priv->credits_dialog != NULL)
    {
      gtk_window_present (GTK_WINDOW (priv->credits_dialog));
      return;
    }

  dialog = gtk_dialog_new_with_buttons (_("Credits"),
                                        GTK_WINDOW (about),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_STOCK_CLOSE, GTK_RESPONSE_CANCEL,
                                        NULL);
  credits_dialog = GTK_DIALOG (dialog);
  gtk_dialog_set_has_separator (credits_dialog, FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (credits_dialog), 5);
  gtk_box_set_spacing (GTK_BOX (credits_dialog->vbox), 2); /* 2 * 5 + 2 = 12 */
  gtk_container_set_border_width (GTK_CONTAINER (credits_dialog->action_area), 5);

  priv->credits_dialog = dialog;
  gtk_window_set_default_size (GTK_WINDOW (dialog), 360, 260);
  gtk_dialog_set_default_response (credits_dialog, GTK_RESPONSE_CANCEL);

  gtk_window_set_modal (GTK_WINDOW (dialog),
                        gtk_window_get_modal (GTK_WINDOW (about)));

  g_signal_connect (dialog, "response",
                    G_CALLBACK (gtk_widget_destroy), dialog);
  g_signal_connect (dialog, "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &(priv->credits_dialog));

  notebook = gtk_notebook_new ();
  gtk_container_set_border_width (GTK_CONTAINER (notebook), 5);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), notebook, TRUE, TRUE, 0);

  if (priv->authors != NULL)
    add_credits_page (about, dialog, notebook, _("Written by"), priv->authors);

  if (priv->documenters != NULL)
    add_credits_page (about, dialog, notebook, _("Documented by"), priv->documenters);

  /* Don't show an untranslated gettext msgid */
  if (priv->translator_credits != NULL &&
      strcmp (priv->translator_credits, "translator_credits") != 0 &&
      strcmp (priv->translator_credits, "translator-credits") != 0)
    {
      gchar *translators[2];

      translators[0] = priv->translator_credits;
      translators[1] = NULL;

      add_credits_page (about, dialog, notebook, _("Translated by"), translators);
    }

  if (priv->artists != NULL)
    add_credits_page (about, dialog, notebook, _("Artwork by"), priv->artists);

  gtk_widget_show_all (dialog);
}

static void
set_policy (GtkWidget *sw)
{
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
}

static void
display_license_dialog (GtkWidget *button,
                        gpointer   data)
{
  GtkAboutDialog *about = (GtkAboutDialog *)data;
  GtkAboutDialogPrivate *priv = (GtkAboutDialogPrivate *)about->private_data;
  GtkWidget *dialog, *view, *sw;
  GtkDialog *licence_dialog;
  gchar *strings[2];

  if (priv->license_dialog != NULL)
    {
      gtk_window_present (GTK_WINDOW (priv->license_dialog));
      return;
    }

  dialog = gtk_dialog_new_with_buttons (_("License"),
                                        GTK_WINDOW (about),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_STOCK_CLOSE, GTK_RESPONSE_CANCEL,
                                        NULL);
  licence_dialog = GTK_DIALOG (dialog);
  gtk_dialog_set_has_separator (licence_dialog, FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (licence_dialog), 5);
  gtk_box_set_spacing (GTK_BOX (licence_dialog->vbox), 2); /* 2 * 5 + 2 = 12 */
  gtk_container_set_border_width (GTK_CONTAINER (licence_dialog->action_area), 5);

  priv->license_dialog = dialog;
  gtk_window_set_default_size (GTK_WINDOW (dialog), 420, 320);
  gtk_dialog_set_default_response (licence_dialog, GTK_RESPONSE_CANCEL);

  gtk_window_set_modal (GTK_WINDOW (dialog),
                        gtk_window_get_modal (GTK_WINDOW (about)));

  g_signal_connect (dialog, "response",
                    G_CALLBACK (gtk_widget_destroy), dialog);
  g_signal_connect (dialog, "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &(priv->license_dialog));

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_set_border_width (GTK_CONTAINER (sw), 5);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
                                       GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);
  g_signal_connect (sw, "map", G_CALLBACK (set_policy), NULL);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), sw, TRUE, TRUE, 0);

  strings[0] = priv->license;
  strings[1] = NULL;
  view = text_view_new (about, dialog, strings,
                        priv->wrap_license ? GTK_WRAP_WORD : GTK_WRAP_NONE);

  gtk_container_add (GTK_CONTAINER (sw), view);

  gtk_widget_show_all (dialog);
}

/**
 * gtk_about_dialog_new:
 *
 * Creates a new #GtkAboutDialog.
 *
 * Returns: a newly created #GtkAboutDialog
 *
 * Since: 2.6
 */
GtkWidget *
gtk_about_dialog_new (void)
{
  GtkAboutDialog *dialog = g_object_new (GTK_TYPE_ABOUT_DIALOG, NULL);

  return GTK_WIDGET (dialog);
}

/**
 * gtk_about_dialog_set_email_hook:
 * @func: a function to call when an email link is activated.
 * @data: data to pass to @func
 * @destroy: #GDestroyNotify for @data
 *
 * Installs a global function to be called whenever the user activates an
 * email link in an about dialog.
 *
 * Since 2.18 there exists a default function which uses gtk_show_uri(). To
 * deactivate it, you can pass %NULL for @func.
 *
 * Return value: the previous email hook.
 *
 * Since: 2.6
 *
 * Deprecated: 2.24: Use the #GtkAboutDialog::activate-link signal
 */
GtkAboutDialogActivateLinkFunc
gtk_about_dialog_set_email_hook (GtkAboutDialogActivateLinkFunc func,
                                 gpointer                       data,
                                 GDestroyNotify                 destroy)
{
  GtkAboutDialogActivateLinkFunc old;

  if (activate_email_hook_destroy != NULL)
    (* activate_email_hook_destroy) (activate_email_hook_data);

  old = activate_email_hook;

  activate_email_hook_set = TRUE;
  activate_email_hook = func;
  activate_email_hook_data = data;
  activate_email_hook_destroy = destroy;

  return old;
}

/**
 * gtk_about_dialog_set_url_hook:
 * @func: a function to call when a URL link is activated.
 * @data: data to pass to @func
 * @destroy: #GDestroyNotify for @data
 *
 * Installs a global function to be called whenever the user activates a
 * URL link in an about dialog.
 *
 * Since 2.18 there exists a default function which uses gtk_show_uri(). To
 * deactivate it, you can pass %NULL for @func.
 *
 * Return value: the previous URL hook.
 *
 * Since: 2.6
 *
 * Deprecated: 2.24: Use the #GtkAboutDialog::activate-link signal
 */
GtkAboutDialogActivateLinkFunc
gtk_about_dialog_set_url_hook (GtkAboutDialogActivateLinkFunc func,
                               gpointer                       data,
                               GDestroyNotify                 destroy)
{
  GtkAboutDialogActivateLinkFunc old;

  if (activate_url_hook_destroy != NULL)
    (* activate_url_hook_destroy) (activate_url_hook_data);

  old = activate_url_hook;

  activate_url_hook_set = TRUE;
  activate_url_hook = func;
  activate_url_hook_data = data;
  activate_url_hook_destroy = destroy;

  return old;
}

static void
close_cb (GtkAboutDialog *about)
{
  GtkAboutDialogPrivate *priv = (GtkAboutDialogPrivate *)about->private_data;

  if (priv->license_dialog != NULL)
    {
      gtk_widget_destroy (priv->license_dialog);
      priv->license_dialog = NULL;
    }

  if (priv->credits_dialog != NULL)
    {
      gtk_widget_destroy (priv->credits_dialog);
      priv->credits_dialog = NULL;
    }

  gtk_widget_hide (GTK_WIDGET (about));

}

/**
 * gtk_show_about_dialog:
 * @parent: (allow-none): transient parent, or %NULL for none
 * @first_property_name: the name of the first property
 * @Varargs: value of first property, followed by more properties, %NULL-terminated
 *
 * This is a convenience function for showing an application's about box.
 * The constructed dialog is associated with the parent window and
 * reused for future invocations of this function.
 *
 * Since: 2.6
 */
void
gtk_show_about_dialog (GtkWindow   *parent,
                       const gchar *first_property_name,
                       ...)
{
  static GtkWidget *global_about_dialog = NULL;
  GtkWidget *dialog = NULL;
  va_list var_args;

  if (parent)
    dialog = g_object_get_data (G_OBJECT (parent), "gtk-about-dialog");
  else
    dialog = global_about_dialog;

  if (!dialog)
    {
      dialog = gtk_about_dialog_new ();

      g_object_ref_sink (dialog);

      g_signal_connect (dialog, "delete-event",
                        G_CALLBACK (gtk_widget_hide_on_delete), NULL);

      /* Close dialog on user response */
      g_signal_connect (dialog, "response",
                        G_CALLBACK (close_cb), NULL);

      va_start (var_args, first_property_name);
      g_object_set_valist (G_OBJECT (dialog), first_property_name, var_args);
      va_end (var_args);

      if (parent)
        {
          gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);
          gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
          g_object_set_data_full (G_OBJECT (parent),
                                  I_("gtk-about-dialog"),
                                  dialog, g_object_unref);
        }
      else
        global_about_dialog = dialog;

    }

  gtk_window_present (GTK_WINDOW (dialog));
}

#define __GTK_ABOUT_DIALOG_C__
#include "gtkaliasdef.c"
