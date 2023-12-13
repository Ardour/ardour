/* 
 * GTK - The GIMP Toolkit
 * Copyright (C) 1998 David Abilleira Freijeiro <odaf@nexo.es>
 * All rights reserved.
 *
 * Based on gnome-color-picker by Federico Mena <federico@nuclecu.unam.mx>
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Cambridge, MA 02139, USA.
 */
/*
 * Modified by the GTK+ Team and others 2003.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include "gtkfontbutton.h"

#include "gtkmain.h"
#include "gtkalignment.h"
#include "gtkhbox.h"
#include "gtklabel.h"
#include "gtkvseparator.h"
#include "gtkfontsel.h"
#include "gtkimage.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkalias.h"

#include <string.h>
#include <stdio.h>

#define GTK_FONT_BUTTON_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_FONT_BUTTON, GtkFontButtonPrivate))

struct _GtkFontButtonPrivate 
{
  gchar         *title;
  
  gchar         *fontname;
  
  guint         use_font : 1;
  guint         use_size : 1;
  guint         show_style : 1;
  guint         show_size : 1;
   
  GtkWidget     *font_dialog;
  GtkWidget     *inside;
  GtkWidget     *font_label;
  GtkWidget     *size_label;
};

/* Signals */
enum 
{
  FONT_SET,
  LAST_SIGNAL
};

enum 
{
  PROP_0,
  PROP_TITLE,
  PROP_FONT_NAME,
  PROP_USE_FONT,
  PROP_USE_SIZE,
  PROP_SHOW_STYLE,
  PROP_SHOW_SIZE
};

/* Prototypes */
static void gtk_font_button_finalize               (GObject            *object);
static void gtk_font_button_get_property           (GObject            *object,
                                                    guint               param_id,
                                                    GValue             *value,
                                                    GParamSpec         *pspec);
static void gtk_font_button_set_property           (GObject            *object,
                                                    guint               param_id,
                                                    const GValue       *value,
                                                    GParamSpec         *pspec);

static void gtk_font_button_clicked                 (GtkButton         *button);

/* Dialog response functions */
static void dialog_ok_clicked                       (GtkWidget         *widget,
                                                     gpointer           data);
static void dialog_cancel_clicked                   (GtkWidget         *widget,
                                                     gpointer           data);
static void dialog_destroy                          (GtkWidget         *widget,
                                                     gpointer           data);

/* Auxiliary functions */
static GtkWidget *gtk_font_button_create_inside     (GtkFontButton     *gfs);
static void gtk_font_button_label_use_font          (GtkFontButton     *gfs);
static void gtk_font_button_update_font_info        (GtkFontButton     *gfs);

static guint font_button_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GtkFontButton, gtk_font_button, GTK_TYPE_BUTTON)

static void
gtk_font_button_class_init (GtkFontButtonClass *klass)
{
  GObjectClass *gobject_class;
  GtkButtonClass *button_class;
  
  gobject_class = (GObjectClass *) klass;
  button_class = (GtkButtonClass *) klass;

  gobject_class->finalize = gtk_font_button_finalize;
  gobject_class->set_property = gtk_font_button_set_property;
  gobject_class->get_property = gtk_font_button_get_property;
  
  button_class->clicked = gtk_font_button_clicked;
  
  klass->font_set = NULL;

  /**
   * GtkFontButton:title:
   * 
   * The title of the font selection dialog.
   *
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_TITLE,
                                   g_param_spec_string ("title",
                                                        P_("Title"),
                                                        P_("The title of the font selection dialog"),
                                                        _("Pick a Font"),
                                                        (GTK_PARAM_READABLE |
                                                         GTK_PARAM_WRITABLE)));

  /**
   * GtkFontButton:font-name:
   * 
   * The name of the currently selected font.
   *
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_FONT_NAME,
                                   g_param_spec_string ("font-name",
                                                        P_("Font name"),
                                                        P_("The name of the selected font"),
                                                        P_("Sans 12"),
                                                        (GTK_PARAM_READABLE |
                                                         GTK_PARAM_WRITABLE)));

  /**
   * GtkFontButton:use-font:
   * 
   * If this property is set to %TRUE, the label will be drawn 
   * in the selected font.
   *
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_USE_FONT,
                                   g_param_spec_boolean ("use-font",
                                                         P_("Use font in label"),
                                                         P_("Whether the label is drawn in the selected font"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkFontButton:use-size:
   * 
   * If this property is set to %TRUE, the label will be drawn 
   * with the selected font size.
   *
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_USE_SIZE,
                                   g_param_spec_boolean ("use-size",
                                                         P_("Use size in label"),
                                                         P_("Whether the label is drawn with the selected font size"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkFontButton:show-style:
   * 
   * If this property is set to %TRUE, the name of the selected font style 
   * will be shown in the label. For a more WYSIWYG way to show the selected 
   * style, see the ::use-font property. 
   *
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_STYLE,
                                   g_param_spec_boolean ("show-style",
                                                         P_("Show style"),
                                                         P_("Whether the selected font style is shown in the label"),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE));
  /**
   * GtkFontButton:show-size:
   * 
   * If this property is set to %TRUE, the selected font size will be shown 
   * in the label. For a more WYSIWYG way to show the selected size, see the 
   * ::use-size property. 
   *
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_SIZE,
                                   g_param_spec_boolean ("show-size",
                                                         P_("Show size"),
                                                         P_("Whether selected font size is shown in the label"),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkFontButton::font-set:
   * @widget: the object which received the signal.
   * 
   * The ::font-set signal is emitted when the user selects a font. 
   * When handling this signal, use gtk_font_button_get_font_name() 
   * to find out which font was just selected.
   *
   * Note that this signal is only emitted when the <emphasis>user</emphasis>
   * changes the font. If you need to react to programmatic font changes
   * as well, use the notify::font-name signal.
   *
   * Since: 2.4
   */
  font_button_signals[FONT_SET] = g_signal_new (I_("font-set"),
                                                G_TYPE_FROM_CLASS (gobject_class),
                                                G_SIGNAL_RUN_FIRST,
                                                G_STRUCT_OFFSET (GtkFontButtonClass, font_set),
                                                NULL, NULL,
                                                g_cclosure_marshal_VOID__VOID,
                                                G_TYPE_NONE, 0);
  
  g_type_class_add_private (gobject_class, sizeof (GtkFontButtonPrivate));
}

static void
gtk_font_button_init (GtkFontButton *font_button)
{
  font_button->priv = GTK_FONT_BUTTON_GET_PRIVATE (font_button);

  /* Initialize fields */
  font_button->priv->fontname = g_strdup (_("Sans 12"));
  font_button->priv->use_font = FALSE;
  font_button->priv->use_size = FALSE;
  font_button->priv->show_style = TRUE;
  font_button->priv->show_size = TRUE;
  font_button->priv->font_dialog = NULL;
  font_button->priv->title = g_strdup (_("Pick a Font"));

  font_button->priv->inside = gtk_font_button_create_inside (font_button);
  gtk_container_add (GTK_CONTAINER (font_button), font_button->priv->inside);

  gtk_font_button_update_font_info (font_button);  
}


static void
gtk_font_button_finalize (GObject *object)
{
  GtkFontButton *font_button = GTK_FONT_BUTTON (object);

  if (font_button->priv->font_dialog != NULL) 
    gtk_widget_destroy (font_button->priv->font_dialog);
  font_button->priv->font_dialog = NULL;

  g_free (font_button->priv->fontname);
  font_button->priv->fontname = NULL;
  
  g_free (font_button->priv->title);
  font_button->priv->title = NULL;
  
  G_OBJECT_CLASS (gtk_font_button_parent_class)->finalize (object);
}

static void
gtk_font_button_set_property (GObject      *object,
                              guint         param_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkFontButton *font_button = GTK_FONT_BUTTON (object);
  
  switch (param_id) 
    {
    case PROP_TITLE:
      gtk_font_button_set_title (font_button, g_value_get_string (value));
      break;
    case PROP_FONT_NAME:
      gtk_font_button_set_font_name (font_button, g_value_get_string (value));
      break;
    case PROP_USE_FONT:
      gtk_font_button_set_use_font (font_button, g_value_get_boolean (value));
      break;
    case PROP_USE_SIZE:
      gtk_font_button_set_use_size (font_button, g_value_get_boolean (value));
      break;
    case PROP_SHOW_STYLE:
      gtk_font_button_set_show_style (font_button, g_value_get_boolean (value));
      break;
    case PROP_SHOW_SIZE:
      gtk_font_button_set_show_size (font_button, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
  }
}

static void
gtk_font_button_get_property (GObject    *object,
                              guint       param_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkFontButton *font_button = GTK_FONT_BUTTON (object);
  
  switch (param_id) 
    {
    case PROP_TITLE:
      g_value_set_string (value, gtk_font_button_get_title (font_button));
      break;
    case PROP_FONT_NAME:
      g_value_set_string (value, gtk_font_button_get_font_name (font_button));
      break;
    case PROP_USE_FONT:
      g_value_set_boolean (value, gtk_font_button_get_use_font (font_button));
      break;
    case PROP_USE_SIZE:
      g_value_set_boolean (value, gtk_font_button_get_use_size (font_button));
      break;
    case PROP_SHOW_STYLE:
      g_value_set_boolean (value, gtk_font_button_get_show_style (font_button));
      break;
    case PROP_SHOW_SIZE:
      g_value_set_boolean (value, gtk_font_button_get_show_size (font_button));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
} 


/**
 * gtk_font_button_new:
 *
 * Creates a new font picker widget.
 *
 * Returns: a new font picker widget.
 *
 * Since: 2.4
 */
GtkWidget *
gtk_font_button_new (void)
{
  return g_object_new (GTK_TYPE_FONT_BUTTON, NULL);
} 

/**
 * gtk_font_button_new_with_font:
 * @fontname: Name of font to display in font selection dialog
 *
 * Creates a new font picker widget.
 *
 * Returns: a new font picker widget.
 *
 * Since: 2.4
 */
GtkWidget *
gtk_font_button_new_with_font (const gchar *fontname)
{
  return g_object_new (GTK_TYPE_FONT_BUTTON, "font-name", fontname, NULL);
} 

/**
 * gtk_font_button_set_title:
 * @font_button: a #GtkFontButton
 * @title: a string containing the font selection dialog title
 *
 * Sets the title for the font selection dialog.  
 *
 * Since: 2.4
 */
void
gtk_font_button_set_title (GtkFontButton *font_button, 
                           const gchar   *title)
{
  gchar *old_title;
  g_return_if_fail (GTK_IS_FONT_BUTTON (font_button));
  
  old_title = font_button->priv->title;
  font_button->priv->title = g_strdup (title);
  g_free (old_title);
  
  if (font_button->priv->font_dialog)
    gtk_window_set_title (GTK_WINDOW (font_button->priv->font_dialog),
                          font_button->priv->title);

  g_object_notify (G_OBJECT (font_button), "title");
} 

/**
 * gtk_font_button_get_title:
 * @font_button: a #GtkFontButton
 *
 * Retrieves the title of the font selection dialog.
 *
 * Returns: an internal copy of the title string which must not be freed.
 *
 * Since: 2.4
 */
const gchar*
gtk_font_button_get_title (GtkFontButton *font_button)
{
  g_return_val_if_fail (GTK_IS_FONT_BUTTON (font_button), NULL);

  return font_button->priv->title;
} 

/**
 * gtk_font_button_get_use_font:
 * @font_button: a #GtkFontButton
 *
 * Returns whether the selected font is used in the label.
 *
 * Returns: whether the selected font is used in the label.
 *
 * Since: 2.4
 */
gboolean
gtk_font_button_get_use_font (GtkFontButton *font_button)
{
  g_return_val_if_fail (GTK_IS_FONT_BUTTON (font_button), FALSE);

  return font_button->priv->use_font;
} 

/**
 * gtk_font_button_set_use_font:
 * @font_button: a #GtkFontButton
 * @use_font: If %TRUE, font name will be written using font chosen.
 *
 * If @use_font is %TRUE, the font name will be written using the selected font.  
 *
 * Since: 2.4
 */
void  
gtk_font_button_set_use_font (GtkFontButton *font_button,
			      gboolean       use_font)
{
  g_return_if_fail (GTK_IS_FONT_BUTTON (font_button));
  
  use_font = (use_font != FALSE);
  
  if (font_button->priv->use_font != use_font) 
    {
      font_button->priv->use_font = use_font;

      if (use_font)
        gtk_font_button_label_use_font (font_button);
      else
	gtk_widget_set_style (font_button->priv->font_label, NULL);
 
     g_object_notify (G_OBJECT (font_button), "use-font");
    }
} 


/**
 * gtk_font_button_get_use_size:
 * @font_button: a #GtkFontButton
 *
 * Returns whether the selected size is used in the label.
 *
 * Returns: whether the selected size is used in the label.
 *
 * Since: 2.4
 */
gboolean
gtk_font_button_get_use_size (GtkFontButton *font_button)
{
  g_return_val_if_fail (GTK_IS_FONT_BUTTON (font_button), FALSE);

  return font_button->priv->use_size;
} 

/**
 * gtk_font_button_set_use_size:
 * @font_button: a #GtkFontButton
 * @use_size: If %TRUE, font name will be written using the selected size.
 *
 * If @use_size is %TRUE, the font name will be written using the selected size.
 *
 * Since: 2.4
 */
void  
gtk_font_button_set_use_size (GtkFontButton *font_button,
			      gboolean       use_size)
{
  g_return_if_fail (GTK_IS_FONT_BUTTON (font_button));
  
  use_size = (use_size != FALSE);
  if (font_button->priv->use_size != use_size) 
    {
      font_button->priv->use_size = use_size;

      if (font_button->priv->use_font)
        gtk_font_button_label_use_font (font_button);

      g_object_notify (G_OBJECT (font_button), "use-size");
    }
} 

/**
 * gtk_font_button_get_show_style:
 * @font_button: a #GtkFontButton
 * 
 * Returns whether the name of the font style will be shown in the label.
 * 
 * Return value: whether the font style will be shown in the label.
 *
 * Since: 2.4
 **/
gboolean 
gtk_font_button_get_show_style (GtkFontButton *font_button)
{
  g_return_val_if_fail (GTK_IS_FONT_BUTTON (font_button), FALSE);

  return font_button->priv->show_style;
}

/**
 * gtk_font_button_set_show_style:
 * @font_button: a #GtkFontButton
 * @show_style: %TRUE if font style should be displayed in label.
 *
 * If @show_style is %TRUE, the font style will be displayed along with name of the selected font.
 *
 * Since: 2.4
 */
void
gtk_font_button_set_show_style (GtkFontButton *font_button,
                                gboolean       show_style)
{
  g_return_if_fail (GTK_IS_FONT_BUTTON (font_button));
  
  show_style = (show_style != FALSE);
  if (font_button->priv->show_style != show_style) 
    {
      font_button->priv->show_style = show_style;
      
      gtk_font_button_update_font_info (font_button);
  
      g_object_notify (G_OBJECT (font_button), "show-style");
    }
} 


/**
 * gtk_font_button_get_show_size:
 * @font_button: a #GtkFontButton
 * 
 * Returns whether the font size will be shown in the label.
 * 
 * Return value: whether the font size will be shown in the label.
 *
 * Since: 2.4
 **/
gboolean 
gtk_font_button_get_show_size (GtkFontButton *font_button)
{
  g_return_val_if_fail (GTK_IS_FONT_BUTTON (font_button), FALSE);

  return font_button->priv->show_size;
}

/**
 * gtk_font_button_set_show_size:
 * @font_button: a #GtkFontButton
 * @show_size: %TRUE if font size should be displayed in dialog.
 *
 * If @show_size is %TRUE, the font size will be displayed along with the name of the selected font.
 *
 * Since: 2.4
 */
void
gtk_font_button_set_show_size (GtkFontButton *font_button,
                               gboolean       show_size)
{
  g_return_if_fail (GTK_IS_FONT_BUTTON (font_button));
  
  show_size = (show_size != FALSE);

  if (font_button->priv->show_size != show_size) 
    {
      font_button->priv->show_size = show_size;

      gtk_container_remove (GTK_CONTAINER (font_button), font_button->priv->inside);
      font_button->priv->inside = gtk_font_button_create_inside (font_button);
      gtk_container_add (GTK_CONTAINER (font_button), font_button->priv->inside);
      
      gtk_font_button_update_font_info (font_button);

      g_object_notify (G_OBJECT (font_button), "show-size");
    }
} 


/**
 * gtk_font_button_get_font_name:
 * @font_button: a #GtkFontButton
 *
 * Retrieves the name of the currently selected font. This name includes
 * style and size information as well. If you want to render something
 * with the font, use this string with pango_font_description_from_string() .
 * If you're interested in peeking certain values (family name,
 * style, size, weight) just query these properties from the
 * #PangoFontDescription object.
 *
 * Returns: an internal copy of the font name which must not be freed.
 *
 * Since: 2.4
 */
const gchar *
gtk_font_button_get_font_name (GtkFontButton *font_button)
{
  g_return_val_if_fail (GTK_IS_FONT_BUTTON (font_button), NULL);
  
  return font_button->priv->fontname;
}

/**
 * gtk_font_button_set_font_name:
 * @font_button: a #GtkFontButton
 * @fontname: Name of font to display in font selection dialog
 *
 * Sets or updates the currently-displayed font in font picker dialog.
 *
 * Returns: Return value of gtk_font_selection_dialog_set_font_name() if the
 * font selection dialog exists, otherwise %FALSE.
 *
 * Since: 2.4
 */
gboolean 
gtk_font_button_set_font_name (GtkFontButton *font_button,
                               const gchar    *fontname)
{
  gboolean result;
  gchar *old_fontname;

  g_return_val_if_fail (GTK_IS_FONT_BUTTON (font_button), FALSE);
  g_return_val_if_fail (fontname != NULL, FALSE);
  
  if (g_ascii_strcasecmp (font_button->priv->fontname, fontname)) 
    {
      old_fontname = font_button->priv->fontname;
      font_button->priv->fontname = g_strdup (fontname);
      g_free (old_fontname);
    }
  
  gtk_font_button_update_font_info (font_button);
  
  if (font_button->priv->font_dialog)
    result = gtk_font_selection_dialog_set_font_name (GTK_FONT_SELECTION_DIALOG (font_button->priv->font_dialog), 
                                                      font_button->priv->fontname);
  else
    result = FALSE;

  g_object_notify (G_OBJECT (font_button), "font-name");

  return result;
}

static void
gtk_font_button_clicked (GtkButton *button)
{
  GtkFontSelectionDialog *font_dialog;
  GtkFontButton    *font_button = GTK_FONT_BUTTON (button);
  
  if (!font_button->priv->font_dialog) 
    {
      GtkWidget *parent;
      
      parent = gtk_widget_get_toplevel (GTK_WIDGET (font_button));
      
      font_button->priv->font_dialog = gtk_font_selection_dialog_new (font_button->priv->title);
      
      font_dialog = GTK_FONT_SELECTION_DIALOG (font_button->priv->font_dialog);
      
      if (gtk_widget_is_toplevel (parent) && GTK_IS_WINDOW (parent))
        {
          if (GTK_WINDOW (parent) != gtk_window_get_transient_for (GTK_WINDOW (font_dialog)))
 	    gtk_window_set_transient_for (GTK_WINDOW (font_dialog), GTK_WINDOW (parent));
	       
	  gtk_window_set_modal (GTK_WINDOW (font_dialog),
				gtk_window_get_modal (GTK_WINDOW (parent)));
	}

      g_signal_connect (font_dialog->ok_button, "clicked",
                        G_CALLBACK (dialog_ok_clicked), font_button);
      g_signal_connect (font_dialog->cancel_button, "clicked",
			G_CALLBACK (dialog_cancel_clicked), font_button);
      g_signal_connect (font_dialog, "destroy",
                        G_CALLBACK (dialog_destroy), font_button);
    }
  
  if (!gtk_widget_get_visible (font_button->priv->font_dialog))
    {
      font_dialog = GTK_FONT_SELECTION_DIALOG (font_button->priv->font_dialog);
      
      gtk_font_selection_dialog_set_font_name (font_dialog, font_button->priv->fontname);
      
    } 

  gtk_window_present (GTK_WINDOW (font_button->priv->font_dialog));
}

static void
dialog_ok_clicked (GtkWidget *widget,
		   gpointer   data)
{
  GtkFontButton *font_button = GTK_FONT_BUTTON (data);
  
  gtk_widget_hide (font_button->priv->font_dialog);
  
  g_free (font_button->priv->fontname);
  font_button->priv->fontname = gtk_font_selection_dialog_get_font_name (GTK_FONT_SELECTION_DIALOG (font_button->priv->font_dialog));
  
  /* Set label font */
  gtk_font_button_update_font_info (font_button);

  g_object_notify (G_OBJECT (font_button), "font-name");
  
  /* Emit font_set signal */
  g_signal_emit (font_button, font_button_signals[FONT_SET], 0);
}


static void
dialog_cancel_clicked (GtkWidget *widget,
		       gpointer   data)
{
  GtkFontButton *font_button = GTK_FONT_BUTTON (data);
  
  gtk_widget_hide (font_button->priv->font_dialog);  
}

static void
dialog_destroy (GtkWidget *widget,
		gpointer   data)
{
  GtkFontButton *font_button = GTK_FONT_BUTTON (data);
    
  /* Dialog will get destroyed so reference is not valid now */
  font_button->priv->font_dialog = NULL;
} 

static GtkWidget *
gtk_font_button_create_inside (GtkFontButton *font_button)
{
  GtkWidget *widget;
  
  gtk_widget_push_composite_child ();

  widget = gtk_hbox_new (FALSE, 0);
  
  font_button->priv->font_label = gtk_label_new (_("Font"));
  
  gtk_label_set_justify (GTK_LABEL (font_button->priv->font_label), GTK_JUSTIFY_LEFT);
  gtk_box_pack_start (GTK_BOX (widget), font_button->priv->font_label, TRUE, TRUE, 5);

  if (font_button->priv->show_size) 
    {
      gtk_box_pack_start (GTK_BOX (widget), gtk_vseparator_new (), FALSE, FALSE, 0);
      font_button->priv->size_label = gtk_label_new ("14");
      gtk_box_pack_start (GTK_BOX (widget), font_button->priv->size_label, FALSE, FALSE, 5);
    }

  gtk_widget_show_all (widget);

  gtk_widget_pop_composite_child ();

  return widget;
} 

static void
gtk_font_button_label_use_font (GtkFontButton *font_button)
{
  PangoFontDescription *desc;

  if (!font_button->priv->use_font)
    return;

  desc = pango_font_description_from_string (font_button->priv->fontname);
  
  if (!font_button->priv->use_size)
    pango_font_description_unset_fields (desc, PANGO_FONT_MASK_SIZE);

  gtk_widget_modify_font (font_button->priv->font_label, desc);

  pango_font_description_free (desc);
}

static gboolean
font_description_style_equal (const PangoFontDescription *a,
                              const PangoFontDescription *b)
{
  return (pango_font_description_get_weight (a) == pango_font_description_get_weight (b) &&
          pango_font_description_get_style (a) == pango_font_description_get_style (b) &&
          pango_font_description_get_stretch (a) == pango_font_description_get_stretch (b) &&
          pango_font_description_get_variant (a) == pango_font_description_get_variant (b));
}

static void
gtk_font_button_update_font_info (GtkFontButton *font_button)
{
  PangoFontDescription *desc;
  const gchar *family;
  gchar *style;
  gchar *family_style;
  
  desc = pango_font_description_from_string (font_button->priv->fontname);
  family = pango_font_description_get_family (desc);
  
#if 0
  /* This gives the wrong names, e.g. Italic when the font selection
   * dialog displayed Oblique.
   */
  pango_font_description_unset_fields (desc, PANGO_FONT_MASK_FAMILY | PANGO_FONT_MASK_SIZE);
  style = pango_font_description_to_string (desc);
  gtk_label_set_text (GTK_LABEL (font_button->priv->style_label), style);      
#endif

  style = NULL;
  if (font_button->priv->show_style && family) 
    {
      PangoFontFamily **families;
      PangoFontFace **faces;
      gint n_families, n_faces, i;

      n_families = 0;
      families = NULL;
      pango_context_list_families (gtk_widget_get_pango_context (GTK_WIDGET (font_button)),
                                   &families, &n_families);
      n_faces = 0;
      faces = NULL;
      for (i = 0; i < n_families; i++) 
        {
          const gchar *name = pango_font_family_get_name (families[i]);
          
          if (!g_ascii_strcasecmp (name, family)) 
            {
              pango_font_family_list_faces (families[i], &faces, &n_faces);
              break;
            }
        }
      g_free (families);
      
      for (i = 0; i < n_faces; i++) 
        {
          PangoFontDescription *tmp_desc = pango_font_face_describe (faces[i]);
          
          if (font_description_style_equal (tmp_desc, desc)) 
            {
              style = g_strdup (pango_font_face_get_face_name (faces[i]));
              pango_font_description_free (tmp_desc);
              break;
            }
          else
            pango_font_description_free (tmp_desc);
        }
      g_free (faces);
    }

  if (style == NULL || !g_ascii_strcasecmp (style, "Regular"))
    family_style = g_strdup (family);
  else
    family_style = g_strdup_printf ("%s %s", family, style);
  
  gtk_label_set_text (GTK_LABEL (font_button->priv->font_label), family_style);
  
  g_free (style);
  g_free (family_style);

  if (font_button->priv->show_size) 
    {
      gchar *size = g_strdup_printf ("%g",
                                     pango_font_description_get_size (desc) / (double)PANGO_SCALE);
      
      gtk_label_set_text (GTK_LABEL (font_button->priv->size_label), size);
      
      g_free (size);
    }

  gtk_font_button_label_use_font (font_button);
  
  pango_font_description_free (desc);
} 

#define __GTK_FONT_BUTTON_C__
#include "gtkaliasdef.c"
