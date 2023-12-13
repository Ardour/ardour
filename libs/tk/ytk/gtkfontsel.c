/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * Massively updated for Pango by Owen Taylor, May 2000
 * GtkFontSelection widget for Gtk+, by Damon Chaplin, May 1998.
 * Based on the GnomeFontSelector widget, by Elliot Lee, but major changes.
 * The GnomeFontSelector was derived from app/text_tool.c in the GIMP.
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
#include <stdlib.h>
#include <glib/gprintf.h>
#include <string.h>

#include <atk/atk.h>

#include "gdk/gdk.h"
#include "gdk/gdkkeysyms.h"

#include "gtkfontsel.h"

#include "gtkbutton.h"
#include "gtkcellrenderertext.h"
#include "gtkentry.h"
#include "gtkframe.h"
#include "gtkhbbox.h"
#include "gtkhbox.h"
#include "gtklabel.h"
#include "gtkliststore.h"
#include "gtkrc.h"
#include "gtkstock.h"
#include "gtktable.h"
#include "gtktreeselection.h"
#include "gtktreeview.h"
#include "gtkvbox.h"
#include "gtkscrolledwindow.h"
#include "gtkintl.h"
#include "gtkaccessible.h"
#include "gtkprivate.h"
#include "gtkbuildable.h"
#include "gtkalias.h"

/* We don't enable the font and style entries because they don't add
 * much in terms of visible effect and have a weird effect on keynav.
 * the Windows font selector has entries similarly positioned but they
 * act in conjunction with the associated lists to form a single focus
 * location.
 */
#undef INCLUDE_FONT_ENTRIES

/* This is the default text shown in the preview entry, though the user
   can set it. Remember that some fonts only have capital letters. */
#define PREVIEW_TEXT N_("abcdefghijk ABCDEFGHIJK")

#define DEFAULT_FONT_NAME "Sans 10"

/* This is the initial and maximum height of the preview entry (it expands
   when large font sizes are selected). Initial height is also the minimum. */
#define INITIAL_PREVIEW_HEIGHT 44
#define MAX_PREVIEW_HEIGHT 300

/* These are the sizes of the font, style & size lists. */
#define FONT_LIST_HEIGHT	136
#define FONT_LIST_WIDTH		190
#define FONT_STYLE_LIST_WIDTH	170
#define FONT_SIZE_LIST_WIDTH	60

/* These are what we use as the standard font sizes, for the size list.
 */
static const guint16 font_sizes[] = {
  6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 22, 24, 26, 28,
  32, 36, 40, 48, 56, 64, 72
};

enum {
   PROP_0,
   PROP_FONT_NAME,
   PROP_FONT,
   PROP_PREVIEW_TEXT
};


enum {
  FAMILY_COLUMN,
  FAMILY_NAME_COLUMN
};

enum {
  FACE_COLUMN,
  FACE_NAME_COLUMN
};

enum {
  SIZE_COLUMN
};

static void    gtk_font_selection_set_property       (GObject         *object,
						      guint            prop_id,
						      const GValue    *value,
						      GParamSpec      *pspec);
static void    gtk_font_selection_get_property       (GObject         *object,
						      guint            prop_id,
						      GValue          *value,
						      GParamSpec      *pspec);
static void    gtk_font_selection_finalize	     (GObject         *object);
static void    gtk_font_selection_screen_changed     (GtkWidget	      *widget,
						      GdkScreen       *previous_screen);
static void    gtk_font_selection_style_set          (GtkWidget      *widget,
						      GtkStyle       *prev_style);

/* These are the callbacks & related functions. */
static void     gtk_font_selection_select_font           (GtkTreeSelection *selection,
							  gpointer          data);
static void     gtk_font_selection_show_available_fonts  (GtkFontSelection *fs);

static void     gtk_font_selection_show_available_styles (GtkFontSelection *fs);
static void     gtk_font_selection_select_best_style     (GtkFontSelection *fs,
							  gboolean          use_first);
static void     gtk_font_selection_select_style          (GtkTreeSelection *selection,
							  gpointer          data);

static void     gtk_font_selection_select_best_size      (GtkFontSelection *fs);
static void     gtk_font_selection_show_available_sizes  (GtkFontSelection *fs,
							  gboolean          first_time);
static void     gtk_font_selection_size_activate         (GtkWidget        *w,
							  gpointer          data);
static gboolean gtk_font_selection_size_focus_out        (GtkWidget        *w,
							  GdkEventFocus    *event,
							  gpointer          data);
static void     gtk_font_selection_select_size           (GtkTreeSelection *selection,
							  gpointer          data);

static void     gtk_font_selection_scroll_on_map         (GtkWidget        *w,
							  gpointer          data);

static void     gtk_font_selection_preview_changed       (GtkWidget        *entry,
							  GtkFontSelection *fontsel);
static void     gtk_font_selection_scroll_to_selection   (GtkFontSelection *fontsel);


/* Misc. utility functions. */
static void    gtk_font_selection_load_font          (GtkFontSelection *fs);
static void    gtk_font_selection_update_preview     (GtkFontSelection *fs);

static GdkFont* gtk_font_selection_get_font_internal (GtkFontSelection *fontsel);
static PangoFontDescription *gtk_font_selection_get_font_description (GtkFontSelection *fontsel);
static gboolean gtk_font_selection_select_font_desc  (GtkFontSelection      *fontsel,
						      PangoFontDescription  *new_desc,
						      PangoFontFamily      **pfamily,
						      PangoFontFace        **pface);
static void     gtk_font_selection_reload_fonts          (GtkFontSelection *fontsel);
static void     gtk_font_selection_ref_family            (GtkFontSelection *fontsel,
							  PangoFontFamily  *family);
static void     gtk_font_selection_ref_face              (GtkFontSelection *fontsel,
							  PangoFontFace    *face);

G_DEFINE_TYPE (GtkFontSelection, gtk_font_selection, GTK_TYPE_VBOX)

static void
gtk_font_selection_class_init (GtkFontSelectionClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  
  gobject_class->set_property = gtk_font_selection_set_property;
  gobject_class->get_property = gtk_font_selection_get_property;

  widget_class->screen_changed = gtk_font_selection_screen_changed;
  widget_class->style_set = gtk_font_selection_style_set;
   
  g_object_class_install_property (gobject_class,
                                   PROP_FONT_NAME,
                                   g_param_spec_string ("font-name",
                                                        P_("Font name"),
                                                        P_("The string that represents this font"),
                                                        DEFAULT_FONT_NAME,
                                                        GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_FONT,
				   g_param_spec_boxed ("font",
						       P_("Font"),
						       P_("The GdkFont that is currently selected"),
						       GDK_TYPE_FONT,
						       GTK_PARAM_READABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_PREVIEW_TEXT,
                                   g_param_spec_string ("preview-text",
                                                        P_("Preview text"),
                                                        P_("The text to display in order to demonstrate the selected font"),
                                                        _(PREVIEW_TEXT),
                                                        GTK_PARAM_READWRITE));
  gobject_class->finalize = gtk_font_selection_finalize;
}

static void 
gtk_font_selection_set_property (GObject         *object,
				 guint            prop_id,
				 const GValue    *value,
				 GParamSpec      *pspec)
{
  GtkFontSelection *fontsel;

  fontsel = GTK_FONT_SELECTION (object);

  switch (prop_id)
    {
    case PROP_FONT_NAME:
      gtk_font_selection_set_font_name (fontsel, g_value_get_string (value));
      break;
    case PROP_PREVIEW_TEXT:
      gtk_font_selection_set_preview_text (fontsel, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void gtk_font_selection_get_property (GObject         *object,
					     guint            prop_id,
					     GValue          *value,
					     GParamSpec      *pspec)
{
  GtkFontSelection *fontsel;

  fontsel = GTK_FONT_SELECTION (object);

  switch (prop_id)
    {
    case PROP_FONT_NAME:
      g_value_take_string (value, gtk_font_selection_get_font_name (fontsel));
      break;
    case PROP_FONT:
      g_value_set_boxed (value, gtk_font_selection_get_font_internal (fontsel));
      break;
    case PROP_PREVIEW_TEXT:
      g_value_set_string (value, gtk_font_selection_get_preview_text (fontsel));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/* Handles key press events on the lists, so that we can trap Enter to
 * activate the default button on our own.
 */
static gboolean
list_row_activated (GtkWidget *widget)
{
  GtkWindow *window;
  
  window = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (widget)));
  if (!gtk_widget_is_toplevel (GTK_WIDGET (window)))
    window = NULL;
  
  if (window
      && widget != window->default_widget
      && !(widget == window->focus_widget &&
	   (!window->default_widget || !gtk_widget_get_sensitive (window->default_widget))))
    {
      gtk_window_activate_default (window);
    }
  
  return TRUE;
}

static void
gtk_font_selection_init (GtkFontSelection *fontsel)
{
  GtkWidget *scrolled_win;
  GtkWidget *text_box;
  GtkWidget *table, *label;
  GtkWidget *font_label, *style_label;
  GtkWidget *vbox;
  GtkListStore *model;
  GtkTreeViewColumn *column;
  GList *focus_chain = NULL;
  AtkObject *atk_obj;

  gtk_widget_push_composite_child ();

  gtk_box_set_spacing (GTK_BOX (fontsel), 12);
  fontsel->size = 12 * PANGO_SCALE;
  
  /* Create the table of font, style & size. */
  table = gtk_table_new (3, 3, FALSE);
  gtk_widget_show (table);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table), 12);
  gtk_box_pack_start (GTK_BOX (fontsel), table, TRUE, TRUE, 0);

#ifdef INCLUDE_FONT_ENTRIES
  fontsel->font_entry = gtk_entry_new ();
  gtk_editable_set_editable (GTK_EDITABLE (fontsel->font_entry), FALSE);
  gtk_widget_set_size_request (fontsel->font_entry, 20, -1);
  gtk_widget_show (fontsel->font_entry);
  gtk_table_attach (GTK_TABLE (table), fontsel->font_entry, 0, 1, 1, 2,
		    GTK_FILL, 0, 0, 0);
  
  fontsel->font_style_entry = gtk_entry_new ();
  gtk_editable_set_editable (GTK_EDITABLE (fontsel->font_style_entry), FALSE);
  gtk_widget_set_size_request (fontsel->font_style_entry, 20, -1);
  gtk_widget_show (fontsel->font_style_entry);
  gtk_table_attach (GTK_TABLE (table), fontsel->font_style_entry, 1, 2, 1, 2,
		    GTK_FILL, 0, 0, 0);
#endif /* INCLUDE_FONT_ENTRIES */
  
  fontsel->size_entry = gtk_entry_new ();
  gtk_widget_set_size_request (fontsel->size_entry, 20, -1);
  gtk_widget_show (fontsel->size_entry);
  gtk_table_attach (GTK_TABLE (table), fontsel->size_entry, 2, 3, 1, 2,
		    GTK_FILL, 0, 0, 0);
  g_signal_connect (fontsel->size_entry, "activate",
		    G_CALLBACK (gtk_font_selection_size_activate),
		    fontsel);
  g_signal_connect_after (fontsel->size_entry, "focus-out-event",
			  G_CALLBACK (gtk_font_selection_size_focus_out),
			  fontsel);
  
  font_label = gtk_label_new_with_mnemonic (_("_Family:"));
  gtk_misc_set_alignment (GTK_MISC (font_label), 0.0, 0.5);
  gtk_widget_show (font_label);
  gtk_table_attach (GTK_TABLE (table), font_label, 0, 1, 0, 1,
		    GTK_FILL, 0, 0, 0);  

  style_label = gtk_label_new_with_mnemonic (_("_Style:"));
  gtk_misc_set_alignment (GTK_MISC (style_label), 0.0, 0.5);
  gtk_widget_show (style_label);
  gtk_table_attach (GTK_TABLE (table), style_label, 1, 2, 0, 1,
		    GTK_FILL, 0, 0, 0);
  
  label = gtk_label_new_with_mnemonic (_("Si_ze:"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),
                                 fontsel->size_entry);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label, 2, 3, 0, 1,
		    GTK_FILL, 0, 0, 0);
  
  
  /* Create the lists  */

  model = gtk_list_store_new (2,
			      G_TYPE_OBJECT,  /* FAMILY_COLUMN */
			      G_TYPE_STRING); /* FAMILY_NAME_COLUMN */
  fontsel->family_list = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
  g_object_unref (model);

  g_signal_connect (fontsel->family_list, "row-activated",
		    G_CALLBACK (list_row_activated), fontsel);

  column = gtk_tree_view_column_new_with_attributes ("Family",
						     gtk_cell_renderer_text_new (),
						     "text", FAMILY_NAME_COLUMN,
						     NULL);
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (fontsel->family_list), column);

  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (fontsel->family_list), FALSE);
  gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (fontsel->family_list)),
			       GTK_SELECTION_BROWSE);
  
  gtk_label_set_mnemonic_widget (GTK_LABEL (font_label), fontsel->family_list);

  scrolled_win = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_win), GTK_SHADOW_IN);
  gtk_widget_set_size_request (scrolled_win,
			       FONT_LIST_WIDTH, FONT_LIST_HEIGHT);
  gtk_container_add (GTK_CONTAINER (scrolled_win), fontsel->family_list);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
  gtk_widget_show (fontsel->family_list);
  gtk_widget_show (scrolled_win);

  gtk_table_attach (GTK_TABLE (table), scrolled_win, 0, 1, 1, 3,
		    GTK_EXPAND | GTK_FILL,
		    GTK_EXPAND | GTK_FILL, 0, 0);
  focus_chain = g_list_append (focus_chain, scrolled_win);
  
  model = gtk_list_store_new (2,
			      G_TYPE_OBJECT,  /* FACE_COLUMN */
			      G_TYPE_STRING); /* FACE_NAME_COLUMN */
  fontsel->face_list = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
  g_object_unref (model);
  g_signal_connect (fontsel->face_list, "row-activated",
		    G_CALLBACK (list_row_activated), fontsel);

  gtk_label_set_mnemonic_widget (GTK_LABEL (style_label), fontsel->face_list);

  column = gtk_tree_view_column_new_with_attributes ("Face",
						     gtk_cell_renderer_text_new (),
						     "text", FACE_NAME_COLUMN,
						     NULL);
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (fontsel->face_list), column);

  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (fontsel->face_list), FALSE);
  gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (fontsel->face_list)),
			       GTK_SELECTION_BROWSE);
  
  scrolled_win = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_win), GTK_SHADOW_IN);
  gtk_widget_set_size_request (scrolled_win,
			       FONT_STYLE_LIST_WIDTH, FONT_LIST_HEIGHT);
  gtk_container_add (GTK_CONTAINER (scrolled_win), fontsel->face_list);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
  gtk_widget_show (fontsel->face_list);
  gtk_widget_show (scrolled_win);
  gtk_table_attach (GTK_TABLE (table), scrolled_win, 1, 2, 1, 3,
		    GTK_EXPAND | GTK_FILL,
		    GTK_EXPAND | GTK_FILL, 0, 0);
  focus_chain = g_list_append (focus_chain, scrolled_win);
  
  focus_chain = g_list_append (focus_chain, fontsel->size_entry);

  model = gtk_list_store_new (1, G_TYPE_INT);
  fontsel->size_list = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
  g_object_unref (model);
  g_signal_connect (fontsel->size_list, "row-activated",
		    G_CALLBACK (list_row_activated), fontsel);

  column = gtk_tree_view_column_new_with_attributes ("Size",
						     gtk_cell_renderer_text_new (),
						     "text", SIZE_COLUMN,
						     NULL);
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (fontsel->size_list), column);

  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (fontsel->size_list), FALSE);
  gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (fontsel->size_list)),
			       GTK_SELECTION_BROWSE);
  
  scrolled_win = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_win), GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (scrolled_win), fontsel->size_list);
  gtk_widget_set_size_request (scrolled_win, -1, FONT_LIST_HEIGHT);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				  GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
  gtk_widget_show (fontsel->size_list);
  gtk_widget_show (scrolled_win);
  gtk_table_attach (GTK_TABLE (table), scrolled_win, 2, 3, 2, 3,
		    GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  focus_chain = g_list_append (focus_chain, scrolled_win);

  gtk_container_set_focus_chain (GTK_CONTAINER (table), focus_chain);
  g_list_free (focus_chain);
  
  /* Insert the fonts. */
  g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (fontsel->family_list)), "changed",
		    G_CALLBACK (gtk_font_selection_select_font), fontsel);

  g_signal_connect_after (fontsel->family_list, "map",
			  G_CALLBACK (gtk_font_selection_scroll_on_map),
			  fontsel);
  
  g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (fontsel->face_list)), "changed",
		    G_CALLBACK (gtk_font_selection_select_style), fontsel);

  g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (fontsel->size_list)), "changed",
		    G_CALLBACK (gtk_font_selection_select_size), fontsel);
  atk_obj = gtk_widget_get_accessible (fontsel->size_list);
  if (GTK_IS_ACCESSIBLE (atk_obj))
    {
      /* Accessibility support is enabled.
       * Make the label ATK_RELATON_LABEL_FOR for the size list as well.
       */
      AtkObject *atk_label;
      AtkRelationSet *relation_set;
      AtkRelation *relation;
      AtkObject *obj_array[1];

      atk_label = gtk_widget_get_accessible (label);
      relation_set = atk_object_ref_relation_set (atk_obj);
      relation = atk_relation_set_get_relation_by_type (relation_set, ATK_RELATION_LABELLED_BY);
      if (relation)
        {
          atk_relation_add_target (relation, atk_label);
        }
      else 
        {
          obj_array[0] = atk_label;
          relation = atk_relation_new (obj_array, 1, ATK_RELATION_LABELLED_BY);
          atk_relation_set_add (relation_set, relation);
        }
      g_object_unref (relation_set);

      relation_set = atk_object_ref_relation_set (atk_label);
      relation = atk_relation_set_get_relation_by_type (relation_set, ATK_RELATION_LABEL_FOR);
      if (relation)
        {
          atk_relation_add_target (relation, atk_obj);
        }
      else 
        {
          obj_array[0] = atk_obj;
          relation = atk_relation_new (obj_array, 1, ATK_RELATION_LABEL_FOR);
          atk_relation_set_add (relation_set, relation);
        }
      g_object_unref (relation_set);
    }    
      

  vbox = gtk_vbox_new (FALSE, 6);
  gtk_widget_show (vbox);
  gtk_box_pack_start (GTK_BOX (fontsel), vbox, FALSE, TRUE, 0);
  
  /* create the text entry widget */
  label = gtk_label_new_with_mnemonic (_("_Preview:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 0);
  
  text_box = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (text_box);
  gtk_box_pack_start (GTK_BOX (vbox), text_box, FALSE, TRUE, 0);
  
  fontsel->preview_entry = gtk_entry_new ();
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), fontsel->preview_entry);
  gtk_entry_set_text (GTK_ENTRY (fontsel->preview_entry), _(PREVIEW_TEXT));
  
  gtk_widget_show (fontsel->preview_entry);
  g_signal_connect (fontsel->preview_entry, "changed",
		    G_CALLBACK (gtk_font_selection_preview_changed), fontsel);
  gtk_widget_set_size_request (fontsel->preview_entry,
			       -1, INITIAL_PREVIEW_HEIGHT);
  gtk_box_pack_start (GTK_BOX (text_box), fontsel->preview_entry,
		      TRUE, TRUE, 0);
  gtk_widget_pop_composite_child();
}

/**
 * gtk_font_selection_new:
 *
 * Creates a new #GtkFontSelection.
 *
 * Return value: a n ew #GtkFontSelection
 */
GtkWidget *
gtk_font_selection_new (void)
{
  GtkFontSelection *fontsel;
  
  fontsel = g_object_new (GTK_TYPE_FONT_SELECTION, NULL);
  
  return GTK_WIDGET (fontsel);
}

static void
gtk_font_selection_finalize (GObject *object)
{
  GtkFontSelection *fontsel;
  
  g_return_if_fail (GTK_IS_FONT_SELECTION (object));
  
  fontsel = GTK_FONT_SELECTION (object);

  if (fontsel->font)
    gdk_font_unref (fontsel->font);

  gtk_font_selection_ref_family (fontsel, NULL);
  gtk_font_selection_ref_face (fontsel, NULL);

  G_OBJECT_CLASS (gtk_font_selection_parent_class)->finalize (object);
}

static void
gtk_font_selection_ref_family (GtkFontSelection *fontsel,
			       PangoFontFamily  *family)
{
  if (family)
    family = g_object_ref (family);
  if (fontsel->family)
    g_object_unref (fontsel->family);
  fontsel->family = family;
}

static void gtk_font_selection_ref_face (GtkFontSelection *fontsel,
					 PangoFontFace    *face)
{
  if (face)
    face = g_object_ref (face);
  if (fontsel->face)
    g_object_unref (fontsel->face);
  fontsel->face = face;
}

static void
gtk_font_selection_reload_fonts (GtkFontSelection *fontsel)
{
  if (gtk_widget_has_screen (GTK_WIDGET (fontsel)))
    {
      PangoFontDescription *desc;
      desc = gtk_font_selection_get_font_description (fontsel);

      gtk_font_selection_show_available_fonts (fontsel);
      gtk_font_selection_show_available_sizes (fontsel, TRUE);
      gtk_font_selection_show_available_styles (fontsel);

      gtk_font_selection_select_font_desc (fontsel, desc, NULL, NULL);
      gtk_font_selection_scroll_to_selection (fontsel);

      pango_font_description_free (desc);
    }
}

static void
gtk_font_selection_screen_changed (GtkWidget *widget,
				   GdkScreen *previous_screen)
{
  gtk_font_selection_reload_fonts (GTK_FONT_SELECTION (widget));
}

static void
gtk_font_selection_style_set (GtkWidget *widget,
			      GtkStyle  *prev_style)
{
  /* Maybe fonts where installed or removed... */
  gtk_font_selection_reload_fonts (GTK_FONT_SELECTION (widget));
}

static void
gtk_font_selection_preview_changed (GtkWidget        *entry,
				    GtkFontSelection *fontsel)
{
  g_object_notify (G_OBJECT (fontsel), "preview-text");
}

static void
scroll_to_selection (GtkTreeView *tree_view)
{
  GtkTreeSelection *selection = gtk_tree_view_get_selection (tree_view);
  GtkTreeModel *model;
  GtkTreeIter iter;

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      GtkTreePath *path = gtk_tree_model_get_path (model, &iter);
      gtk_tree_view_scroll_to_cell (tree_view, path, NULL, TRUE, 0.5, 0.5);
      gtk_tree_path_free (path);
    }
}

static void
set_cursor_to_iter (GtkTreeView *view,
		    GtkTreeIter *iter)
{
  GtkTreeModel *model = gtk_tree_view_get_model (view);
  GtkTreePath *path = gtk_tree_model_get_path (model, iter);
  
  gtk_tree_view_set_cursor (view, path, NULL, FALSE);

  gtk_tree_path_free (path);
}

static void
gtk_font_selection_scroll_to_selection (GtkFontSelection *fontsel)
{
  /* Try to scroll the font family list to the selected item */
  scroll_to_selection (GTK_TREE_VIEW (fontsel->family_list));
      
  /* Try to scroll the font family list to the selected item */
  scroll_to_selection (GTK_TREE_VIEW (fontsel->face_list));
      
  /* Try to scroll the font family list to the selected item */
  scroll_to_selection (GTK_TREE_VIEW (fontsel->size_list));
/* This is called when the list is mapped. Here we scroll to the current
   font if necessary. */
}

static void
gtk_font_selection_scroll_on_map (GtkWidget		*widget,
                                  gpointer		 data)
{
  gtk_font_selection_scroll_to_selection (GTK_FONT_SELECTION (data));
}

/* This is called when a family is selected in the list. */
static void
gtk_font_selection_select_font (GtkTreeSelection *selection,
				gpointer          data)
{
  GtkFontSelection *fontsel;
  GtkTreeModel *model;
  GtkTreeIter iter;
#ifdef INCLUDE_FONT_ENTRIES
  const gchar *family_name;
#endif

  fontsel = GTK_FONT_SELECTION (data);

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      PangoFontFamily *family;
      
      gtk_tree_model_get (model, &iter, FAMILY_COLUMN, &family, -1);
      if (fontsel->family != family)
	{
	  gtk_font_selection_ref_family (fontsel, family);
	  
#ifdef INCLUDE_FONT_ENTRIES
	  family_name = pango_font_family_get_name (fontsel->family);
	  gtk_entry_set_text (GTK_ENTRY (fontsel->font_entry), family_name);
#endif
	  
	  gtk_font_selection_show_available_styles (fontsel);
	  gtk_font_selection_select_best_style (fontsel, TRUE);
	}

      g_object_unref (family);
    }
}

static int
cmp_families (const void *a, const void *b)
{
  const char *a_name = pango_font_family_get_name (*(PangoFontFamily **)a);
  const char *b_name = pango_font_family_get_name (*(PangoFontFamily **)b);
  
  return g_utf8_collate (a_name, b_name);
}

static void
gtk_font_selection_show_available_fonts (GtkFontSelection *fontsel)
{
  GtkListStore *model;
  PangoFontFamily **families;
  PangoFontFamily *match_family = NULL;
  gint n_families, i;
  GtkTreeIter match_row;
  
  model = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (fontsel->family_list)));
  
  pango_context_list_families (gtk_widget_get_pango_context (GTK_WIDGET (fontsel)),
			       &families, &n_families);
  qsort (families, n_families, sizeof (PangoFontFamily *), cmp_families);

  gtk_list_store_clear (model);

  for (i=0; i<n_families; i++)
    {
      const gchar *name = pango_font_family_get_name (families[i]);
      GtkTreeIter iter;

      gtk_list_store_append (model, &iter);
      gtk_list_store_set (model, &iter,
			  FAMILY_COLUMN, families[i],
			  FAMILY_NAME_COLUMN, name,
			  -1);
      
      if (i == 0 || !g_ascii_strcasecmp (name, "sans"))
	{
	  match_family = families[i];
	  match_row = iter;
	}
    }

  gtk_font_selection_ref_family (fontsel, match_family);
  if (match_family)
    {
      set_cursor_to_iter (GTK_TREE_VIEW (fontsel->family_list), &match_row);
#ifdef INCLUDE_FONT_ENTRIES
      gtk_entry_set_text (GTK_ENTRY (fontsel->font_entry), 
			  pango_font_family_get_name (match_family));
#endif /* INCLUDE_FONT_ENTRIES */
    }

  g_free (families);
}

static int
compare_font_descriptions (const PangoFontDescription *a, const PangoFontDescription *b)
{
  int val = strcmp (pango_font_description_get_family (a), pango_font_description_get_family (b));
  if (val != 0)
    return val;

  if (pango_font_description_get_weight (a) != pango_font_description_get_weight (b))
    return pango_font_description_get_weight (a) - pango_font_description_get_weight (b);

  if (pango_font_description_get_style (a) != pango_font_description_get_style (b))
    return pango_font_description_get_style (a) - pango_font_description_get_style (b);
  
  if (pango_font_description_get_stretch (a) != pango_font_description_get_stretch (b))
    return pango_font_description_get_stretch (a) - pango_font_description_get_stretch (b);

  if (pango_font_description_get_variant (a) != pango_font_description_get_variant (b))
    return pango_font_description_get_variant (a) - pango_font_description_get_variant (b);

  return 0;
}

static int
faces_sort_func (const void *a, const void *b)
{
  PangoFontDescription *desc_a = pango_font_face_describe (*(PangoFontFace **)a);
  PangoFontDescription *desc_b = pango_font_face_describe (*(PangoFontFace **)b);
  
  int ord = compare_font_descriptions (desc_a, desc_b);

  pango_font_description_free (desc_a);
  pango_font_description_free (desc_b);

  return ord;
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

/* This fills the font style list with all the possible style combinations
   for the current font family. */
static void
gtk_font_selection_show_available_styles (GtkFontSelection *fontsel)
{
  gint n_faces, i;
  PangoFontFace **faces;
  PangoFontDescription *old_desc;
  GtkListStore *model;
  GtkTreeIter match_row;
  PangoFontFace *match_face = NULL;
  
  model = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (fontsel->face_list)));
  
  if (fontsel->face)
    old_desc = pango_font_face_describe (fontsel->face);
  else
    old_desc= NULL;

  pango_font_family_list_faces (fontsel->family, &faces, &n_faces);
  qsort (faces, n_faces, sizeof (PangoFontFace *), faces_sort_func);

  gtk_list_store_clear (model);

  for (i=0; i < n_faces; i++)
    {
      GtkTreeIter iter;
      const gchar *str = pango_font_face_get_face_name (faces[i]);

      gtk_list_store_append (model, &iter);
      gtk_list_store_set (model, &iter,
			  FACE_COLUMN, faces[i],
			  FACE_NAME_COLUMN, str,
			  -1);

      if (i == 0)
	{
	  match_row = iter;
	  match_face = faces[i];
	}
      else if (old_desc)
	{
	  PangoFontDescription *tmp_desc = pango_font_face_describe (faces[i]);
	  
	  if (font_description_style_equal (tmp_desc, old_desc))
	    {
	      match_row = iter;
	      match_face = faces[i];
	    }
      
	  pango_font_description_free (tmp_desc);
	}
    }

  if (old_desc)
    pango_font_description_free (old_desc);

  gtk_font_selection_ref_face (fontsel, match_face);
  if (match_face)
    {
#ifdef INCLUDE_FONT_ENTRIES
      const gchar *str = pango_font_face_get_face_name (fontsel->face);

      gtk_entry_set_text (GTK_ENTRY (fontsel->font_style_entry), str);
#endif      
      set_cursor_to_iter (GTK_TREE_VIEW (fontsel->face_list), &match_row);
    }

  g_free (faces);
}

/* This selects a style when the user selects a font. It just uses the first
   available style at present. I was thinking of trying to maintain the
   selected style, e.g. bold italic, when the user selects different fonts.
   However, the interface is so easy to use now I'm not sure it's worth it.
   Note: This will load a font. */
static void
gtk_font_selection_select_best_style (GtkFontSelection *fontsel,
				      gboolean	        use_first)
{
  GtkTreeIter iter;
  GtkTreeModel *model;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (fontsel->face_list));

  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      set_cursor_to_iter (GTK_TREE_VIEW (fontsel->face_list), &iter);
      scroll_to_selection (GTK_TREE_VIEW (fontsel->face_list));
    }

  gtk_font_selection_show_available_sizes (fontsel, FALSE);
  gtk_font_selection_select_best_size (fontsel);
}


/* This is called when a style is selected in the list. */
static void
gtk_font_selection_select_style (GtkTreeSelection *selection,
				 gpointer          data)
{
  GtkFontSelection *fontsel = GTK_FONT_SELECTION (data);
  GtkTreeModel *model;
  GtkTreeIter iter;
  
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      PangoFontFace *face;
      
      gtk_tree_model_get (model, &iter, FACE_COLUMN, &face, -1);
      gtk_font_selection_ref_face (fontsel, face);
      g_object_unref (face);
    }

  gtk_font_selection_show_available_sizes (fontsel, FALSE);
  gtk_font_selection_select_best_size (fontsel);
}

static void
gtk_font_selection_show_available_sizes (GtkFontSelection *fontsel,
					 gboolean          first_time)
{
  gint i;
  GtkListStore *model;
  gchar buffer[128];
  gchar *p;
      
  model = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (fontsel->size_list)));

  /* Insert the standard font sizes */
  if (first_time)
    {
      gtk_list_store_clear (model);

      for (i = 0; i < G_N_ELEMENTS (font_sizes); i++)
	{
	  GtkTreeIter iter;
	  
	  gtk_list_store_append (model, &iter);
	  gtk_list_store_set (model, &iter, SIZE_COLUMN, font_sizes[i], -1);
	  
	  if (font_sizes[i] * PANGO_SCALE == fontsel->size)
	    set_cursor_to_iter (GTK_TREE_VIEW (fontsel->size_list), &iter);
	}
    }
  else
    {
      GtkTreeIter iter;
      gboolean found = FALSE;
      
      gtk_tree_model_get_iter_first (GTK_TREE_MODEL (model), &iter);
      for (i = 0; i < G_N_ELEMENTS (font_sizes) && !found; i++)
	{
	  if (font_sizes[i] * PANGO_SCALE == fontsel->size)
	    {
	      set_cursor_to_iter (GTK_TREE_VIEW (fontsel->size_list), &iter);
	      found = TRUE;
	    }

	  gtk_tree_model_iter_next (GTK_TREE_MODEL (model), &iter);
	}

      if (!found)
	{
	  GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (fontsel->size_list));
	  gtk_tree_selection_unselect_all (selection);
	}
    }

  /* Set the entry to the new size, rounding to 1 digit,
   * trimming of trailing 0's and a trailing period
   */
  g_snprintf (buffer, sizeof (buffer), "%.1f", fontsel->size / (1.0 * PANGO_SCALE));
  if (strchr (buffer, '.'))
    {
      p = buffer + strlen (buffer) - 1;
      while (*p == '0')
	p--;
      if (*p == '.')
	p--;
      p[1] = '\0';
    }

  /* Compare, to avoid moving the cursor unecessarily */
  if (strcmp (gtk_entry_get_text (GTK_ENTRY (fontsel->size_entry)), buffer) != 0)
    gtk_entry_set_text (GTK_ENTRY (fontsel->size_entry), buffer);
}

static void
gtk_font_selection_select_best_size (GtkFontSelection *fontsel)
{
  gtk_font_selection_load_font (fontsel);  
}

static void
gtk_font_selection_set_size (GtkFontSelection *fontsel,
			     gint              new_size)
{
  if (fontsel->size != new_size)
    {
      fontsel->size = new_size;

      gtk_font_selection_show_available_sizes (fontsel, FALSE);      
      gtk_font_selection_load_font (fontsel);
    }
}

/* If the user hits return in the font size entry, we change to the new font
   size. */
static void
gtk_font_selection_size_activate (GtkWidget   *w,
                                  gpointer     data)
{
  GtkFontSelection *fontsel;
  gint new_size;
  const gchar *text;
  
  fontsel = GTK_FONT_SELECTION (data);

  text = gtk_entry_get_text (GTK_ENTRY (fontsel->size_entry));
  new_size = MAX (0.1, atof (text) * PANGO_SCALE + 0.5);

  if (fontsel->size != new_size)
    gtk_font_selection_set_size (fontsel, new_size);
  else 
    list_row_activated (w);
}

static gboolean
gtk_font_selection_size_focus_out (GtkWidget     *w,
				   GdkEventFocus *event,
				   gpointer       data)
{
  GtkFontSelection *fontsel;
  gint new_size;
  const gchar *text;
  
  fontsel = GTK_FONT_SELECTION (data);

  text = gtk_entry_get_text (GTK_ENTRY (fontsel->size_entry));
  new_size = MAX (0.1, atof (text) * PANGO_SCALE + 0.5);

  gtk_font_selection_set_size (fontsel, new_size);
  
  return TRUE;
}

/* This is called when a size is selected in the list. */
static void
gtk_font_selection_select_size (GtkTreeSelection *selection,
				gpointer          data)
{
  GtkFontSelection *fontsel;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gint new_size;
  
  fontsel = GTK_FONT_SELECTION (data);
  
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      gtk_tree_model_get (model, &iter, SIZE_COLUMN, &new_size, -1);
      gtk_font_selection_set_size (fontsel, new_size * PANGO_SCALE);
    }
}

static void
gtk_font_selection_load_font (GtkFontSelection *fontsel)
{
  if (fontsel->font)
    gdk_font_unref (fontsel->font);
  fontsel->font = NULL;

  gtk_font_selection_update_preview (fontsel);
}

static PangoFontDescription *
gtk_font_selection_get_font_description (GtkFontSelection *fontsel)
{
  PangoFontDescription *font_desc;

  if (fontsel->face)
    {
      font_desc = pango_font_face_describe (fontsel->face);
      pango_font_description_set_size (font_desc, fontsel->size);
    }
  else
    font_desc = pango_font_description_from_string (DEFAULT_FONT_NAME);

  return font_desc;
}

/* This sets the font in the preview entry to the selected font, and tries to
   make sure that the preview entry is a reasonable size, i.e. so that the
   text can be seen with a bit of space to spare. But it tries to avoid
   resizing the entry every time the font changes.
   This also used to shrink the preview if the font size was decreased, but
   that made it awkward if the user wanted to resize the window themself. */
static void
gtk_font_selection_update_preview (GtkFontSelection *fontsel)
{
  GtkRcStyle *rc_style;
  gint new_height;
  GtkRequisition old_requisition;
  GtkWidget *preview_entry = fontsel->preview_entry;
  const gchar *text;

  gtk_widget_get_child_requisition (preview_entry, &old_requisition);
  
  rc_style = gtk_rc_style_new ();
  rc_style->font_desc = gtk_font_selection_get_font_description (fontsel);
  
  gtk_widget_modify_style (preview_entry, rc_style);
  g_object_unref (rc_style);

  gtk_widget_size_request (preview_entry, NULL);
  
  /* We don't ever want to be over MAX_PREVIEW_HEIGHT pixels high. */
  new_height = CLAMP (preview_entry->requisition.height, INITIAL_PREVIEW_HEIGHT, MAX_PREVIEW_HEIGHT);

  if (new_height > old_requisition.height || new_height < old_requisition.height - 30)
    gtk_widget_set_size_request (preview_entry, -1, new_height);
  
  /* This sets the preview text, if it hasn't been set already. */
  text = gtk_entry_get_text (GTK_ENTRY (preview_entry));
  if (strlen (text) == 0)
    gtk_entry_set_text (GTK_ENTRY (preview_entry), _(PREVIEW_TEXT));
  gtk_editable_set_position (GTK_EDITABLE (preview_entry), 0);
}

static GdkFont*
gtk_font_selection_get_font_internal (GtkFontSelection *fontsel)
{
  if (!fontsel->font)
    {
      PangoFontDescription *font_desc = gtk_font_selection_get_font_description (fontsel);
      fontsel->font = gdk_font_from_description_for_display (gtk_widget_get_display (GTK_WIDGET (fontsel)), font_desc);
      pango_font_description_free (font_desc);
    }
  
  return fontsel->font;
}


/*****************************************************************************
 * These functions are the main public interface for getting/setting the font.
 *****************************************************************************/

/**
 * gtk_font_selection_get_family_list:
 * @fontsel: a #GtkFontSelection
 *
 * This returns the #GtkTreeView that lists font families, for
 * example, 'Sans', 'Serif', etc.
 *
 * Return value: (transfer none): A #GtkWidget that is part of @fontsel
 *
 * Since: 2.14
 */
GtkWidget *
gtk_font_selection_get_family_list (GtkFontSelection *fontsel)
{
  g_return_val_if_fail (GTK_IS_FONT_SELECTION (fontsel), NULL);
  
  return fontsel->family_list;
}

/**
 * gtk_font_selection_get_face_list:
 * @fontsel: a #GtkFontSelection
 *
 * This returns the #GtkTreeView which lists all styles available for
 * the selected font. For example, 'Regular', 'Bold', etc.
 * 
 * Return value: (transfer none): A #GtkWidget that is part of @fontsel
 *
 * Since: 2.14
 */
GtkWidget *
gtk_font_selection_get_face_list (GtkFontSelection *fontsel)
{
  g_return_val_if_fail (GTK_IS_FONT_SELECTION (fontsel), NULL);
  
  return fontsel->face_list;
}

/**
 * gtk_font_selection_get_size_entry:
 * @fontsel: a #GtkFontSelection
 *
 * This returns the #GtkEntry used to allow the user to edit the font
 * number manually instead of selecting it from the list of font sizes.
 *
 * Return value: (transfer none): A #GtkWidget that is part of @fontsel
 *
 * Since: 2.14
 */
GtkWidget *
gtk_font_selection_get_size_entry (GtkFontSelection *fontsel)
{
  g_return_val_if_fail (GTK_IS_FONT_SELECTION (fontsel), NULL);
  
  return fontsel->size_entry;
}

/**
 * gtk_font_selection_get_size_list:
 * @fontsel: a #GtkFontSelection
 *
 * This returns the #GtkTreeeView used to list font sizes.
 *
 * Return value: (transfer none): A #GtkWidget that is part of @fontsel
 *
 * Since: 2.14
 */
GtkWidget *
gtk_font_selection_get_size_list (GtkFontSelection *fontsel)
{
  g_return_val_if_fail (GTK_IS_FONT_SELECTION (fontsel), NULL);
  
  return fontsel->size_list;
}

/**
 * gtk_font_selection_get_preview_entry:
 * @fontsel: a #GtkFontSelection
 *
 * This returns the #GtkEntry used to display the font as a preview.
 *
 * Return value: (transfer none): A #GtkWidget that is part of @fontsel
 *
 * Since: 2.14
 */
GtkWidget *
gtk_font_selection_get_preview_entry (GtkFontSelection *fontsel)
{
  g_return_val_if_fail (GTK_IS_FONT_SELECTION (fontsel), NULL);
  
  return fontsel->preview_entry;
}

/**
 * gtk_font_selection_get_family:
 * @fontsel: a #GtkFontSelection
 *
 * Gets the #PangoFontFamily representing the selected font family.
 *
 * Return value: (transfer none): A #PangoFontFamily representing the
 *     selected font family. Font families are a collection of font
 *     faces. The returned object is owned by @fontsel and must not
 *     be modified or freed.
 *
 * Since: 2.14
 */
PangoFontFamily *
gtk_font_selection_get_family (GtkFontSelection *fontsel)
{
  g_return_val_if_fail (GTK_IS_FONT_SELECTION (fontsel), NULL);
  
  return fontsel->family;
}

/**
 * gtk_font_selection_get_face:
 * @fontsel: a #GtkFontSelection
 *
 * Gets the #PangoFontFace representing the selected font group
 * details (i.e. family, slant, weight, width, etc).
 *
 * Return value: (transfer none): A #PangoFontFace representing the
 *     selected font group details. The returned object is owned by
 *     @fontsel and must not be modified or freed.
 *
 * Since: 2.14
 */
PangoFontFace *
gtk_font_selection_get_face (GtkFontSelection *fontsel)
{
  g_return_val_if_fail (GTK_IS_FONT_SELECTION (fontsel), NULL);
  
  return fontsel->face;
}

/**
 * gtk_font_selection_get_size:
 * @fontsel: a #GtkFontSelection
 *
 * The selected font size.
 *
 * Return value: A n integer representing the selected font size,
 *     or -1 if no font size is selected.
 *
 * Since: 2.14
 **/
gint
gtk_font_selection_get_size (GtkFontSelection *fontsel)
{
  g_return_val_if_fail (GTK_IS_FONT_SELECTION (fontsel), -1);
  
  return fontsel->size;
}

/**
 * gtk_font_selection_get_font:
 * @fontsel: a #GtkFontSelection
 *
 * Gets the currently-selected font.
 * 
 * Return value: A #GdkFont.
 *
 * Deprecated: 2.0: Use gtk_font_selection_get_font_name() instead.
 */
GdkFont *
gtk_font_selection_get_font (GtkFontSelection *fontsel)
{
  g_return_val_if_fail (GTK_IS_FONT_SELECTION (fontsel), NULL);

  return gtk_font_selection_get_font_internal (fontsel);
}

/**
 * gtk_font_selection_get_font_name:
 * @fontsel: a #GtkFontSelection
 * 
 * Gets the currently-selected font name. 
 *
 * Note that this can be a different string than what you set with 
 * gtk_font_selection_set_font_name(), as the font selection widget may 
 * normalize font names and thus return a string with a different structure. 
 * For example, "Helvetica Italic Bold 12" could be normalized to 
 * "Helvetica Bold Italic 12". Use pango_font_description_equal()
 * if you want to compare two font descriptions.
 * 
 * Return value: A string with the name of the current font, or %NULL if 
 *     no font is selected. You must free this string with g_free().
 */
gchar *
gtk_font_selection_get_font_name (GtkFontSelection *fontsel)
{
  gchar *result;
  
  PangoFontDescription *font_desc = gtk_font_selection_get_font_description (fontsel);
  result = pango_font_description_to_string (font_desc);
  pango_font_description_free (font_desc);

  return result;
}

/* This selects the appropriate list rows.
   First we check the fontname is valid and try to find the font family
   - i.e. the name in the main list. If we can't find that, then just return.
   Next we try to set each of the properties according to the fontname.
   Finally we select the font family & style in the lists. */
static gboolean
gtk_font_selection_select_font_desc (GtkFontSelection      *fontsel,
				     PangoFontDescription  *new_desc,
				     PangoFontFamily      **pfamily,
				     PangoFontFace        **pface)
{
  PangoFontFamily *new_family = NULL;
  PangoFontFace *new_face = NULL;
  PangoFontFace *fallback_face = NULL;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreeIter match_iter;
  gboolean valid;
  const gchar *new_family_name;

  new_family_name = pango_font_description_get_family (new_desc);

  if (!new_family_name)
    return FALSE;

  /* Check to make sure that this is in the list of allowed fonts 
   */
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (fontsel->family_list));
  for (valid = gtk_tree_model_get_iter_first (model, &iter);
       valid;
       valid = gtk_tree_model_iter_next (model, &iter))
    {
      PangoFontFamily *family;
      
      gtk_tree_model_get (model, &iter, FAMILY_COLUMN, &family, -1);
      
      if (g_ascii_strcasecmp (pango_font_family_get_name (family),
			      new_family_name) == 0)
	new_family = g_object_ref (family);

      g_object_unref (family);
      
      if (new_family)
	break;
    }

  if (!new_family)
    return FALSE;

  if (pfamily)
    *pfamily = new_family;
  else
    g_object_unref (new_family);
  set_cursor_to_iter (GTK_TREE_VIEW (fontsel->family_list), &iter);
  gtk_font_selection_show_available_styles (fontsel);

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (fontsel->face_list));
  for (valid = gtk_tree_model_get_iter_first (model, &iter);
       valid;
       valid = gtk_tree_model_iter_next (model, &iter))
    {
      PangoFontFace *face;
      PangoFontDescription *tmp_desc;
      
      gtk_tree_model_get (model, &iter, FACE_COLUMN, &face, -1);
      tmp_desc = pango_font_face_describe (face);
      
      if (font_description_style_equal (tmp_desc, new_desc))
	new_face = g_object_ref (face);
      
      if (!fallback_face)
	{
	  fallback_face = g_object_ref (face);
	  match_iter = iter;
	}
      
      pango_font_description_free (tmp_desc);
      g_object_unref (face);
      
      if (new_face)
	{
	  match_iter = iter;
	  break;
	}
    }

  if (!new_face)
    new_face = fallback_face;
  else if (fallback_face)
    g_object_unref (fallback_face);

  if (pface)
    *pface = new_face;
  else if (new_face)
    g_object_unref (new_face);
  set_cursor_to_iter (GTK_TREE_VIEW (fontsel->face_list), &match_iter);  

  gtk_font_selection_set_size (fontsel, pango_font_description_get_size (new_desc));

  return TRUE;
}


/* This sets the current font, then selecting the appropriate list rows. */

/**
 * gtk_font_selection_set_font_name:
 * @fontsel: a #GtkFontSelection
 * @fontname: a font name like "Helvetica 12" or "Times Bold 18"
 * 
 * Sets the currently-selected font. 
 *
 * Note that the @fontsel needs to know the screen in which it will appear 
 * for this to work; this can be guaranteed by simply making sure that the 
 * @fontsel is inserted in a toplevel window before you call this function.
 * 
 * Return value: %TRUE if the font could be set successfully; %FALSE if no 
 *     such font exists or if the @fontsel doesn't belong to a particular 
 *     screen yet.
 */
gboolean
gtk_font_selection_set_font_name (GtkFontSelection *fontsel,
				  const gchar      *fontname)
{
  PangoFontFamily *family = NULL;
  PangoFontFace *face = NULL;
  PangoFontDescription *new_desc;
  
  g_return_val_if_fail (GTK_IS_FONT_SELECTION (fontsel), FALSE);

  if (!gtk_widget_has_screen (GTK_WIDGET (fontsel)))
    return FALSE;

  new_desc = pango_font_description_from_string (fontname);

  if (gtk_font_selection_select_font_desc (fontsel, new_desc, &family, &face))
    {
      gtk_font_selection_ref_family (fontsel, family);
      if (family)
        g_object_unref (family);

      gtk_font_selection_ref_face (fontsel, face);
      if (face)
        g_object_unref (face);
    }

  pango_font_description_free (new_desc);
  
  g_object_freeze_notify (G_OBJECT (fontsel));
  g_object_notify (G_OBJECT (fontsel), "font-name");
  g_object_notify (G_OBJECT (fontsel), "font");
  g_object_thaw_notify (G_OBJECT (fontsel));

  return TRUE;
}

/**
 * gtk_font_selection_get_preview_text:
 * @fontsel: a #GtkFontSelection
 *
 * Gets the text displayed in the preview area.
 * 
 * Return value: the text displayed in the preview area. 
 *     This string is owned by the widget and should not be 
 *     modified or freed 
 */
const gchar*
gtk_font_selection_get_preview_text (GtkFontSelection *fontsel)
{
  g_return_val_if_fail (GTK_IS_FONT_SELECTION (fontsel), NULL);

  return gtk_entry_get_text (GTK_ENTRY (fontsel->preview_entry));
}


/**
 * gtk_font_selection_set_preview_text:
 * @fontsel: a #GtkFontSelection
 * @text: the text to display in the preview area 
 *
 * Sets the text displayed in the preview area.
 * The @text is used to show how the selected font looks.
 */
void
gtk_font_selection_set_preview_text  (GtkFontSelection *fontsel,
				      const gchar      *text)
{
  g_return_if_fail (GTK_IS_FONT_SELECTION (fontsel));
  g_return_if_fail (text != NULL);

  gtk_entry_set_text (GTK_ENTRY (fontsel->preview_entry), text);
}

/*****************************************************************************
 * GtkFontSelectionDialog
 *****************************************************************************/

static void gtk_font_selection_dialog_buildable_interface_init     (GtkBuildableIface *iface);
static GObject * gtk_font_selection_dialog_buildable_get_internal_child (GtkBuildable *buildable,
									  GtkBuilder   *builder,
									  const gchar  *childname);

G_DEFINE_TYPE_WITH_CODE (GtkFontSelectionDialog, gtk_font_selection_dialog,
			 GTK_TYPE_DIALOG,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
						gtk_font_selection_dialog_buildable_interface_init))

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_font_selection_dialog_class_init (GtkFontSelectionDialogClass *klass)
{
}

static void
gtk_font_selection_dialog_init (GtkFontSelectionDialog *fontseldiag)
{
  GtkDialog *dialog = GTK_DIALOG (fontseldiag);
  
  gtk_dialog_set_has_separator (dialog, FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
  gtk_box_set_spacing (GTK_BOX (dialog->vbox), 2); /* 2 * 5 + 2 = 12 */
  gtk_container_set_border_width (GTK_CONTAINER (dialog->action_area), 5);
  gtk_box_set_spacing (GTK_BOX (dialog->action_area), 6);

  gtk_widget_push_composite_child ();

  gtk_window_set_resizable (GTK_WINDOW (fontseldiag), TRUE);
  
  fontseldiag->main_vbox = dialog->vbox;
  
  fontseldiag->fontsel = gtk_font_selection_new ();
  gtk_container_set_border_width (GTK_CONTAINER (fontseldiag->fontsel), 5);
  gtk_widget_show (fontseldiag->fontsel);
  gtk_box_pack_start (GTK_BOX (fontseldiag->main_vbox),
		      fontseldiag->fontsel, TRUE, TRUE, 0);
  
  /* Create the action area */
  fontseldiag->action_area = dialog->action_area;

  fontseldiag->cancel_button = gtk_dialog_add_button (dialog,
                                                      GTK_STOCK_CANCEL,
                                                      GTK_RESPONSE_CANCEL);

  fontseldiag->apply_button = gtk_dialog_add_button (dialog,
                                                     GTK_STOCK_APPLY,
                                                     GTK_RESPONSE_APPLY);
  gtk_widget_hide (fontseldiag->apply_button);

  fontseldiag->ok_button = gtk_dialog_add_button (dialog,
                                                  GTK_STOCK_OK,
                                                  GTK_RESPONSE_OK);
  gtk_widget_grab_default (fontseldiag->ok_button);
  
  gtk_dialog_set_alternative_button_order (GTK_DIALOG (fontseldiag),
					   GTK_RESPONSE_OK,
					   GTK_RESPONSE_APPLY,
					   GTK_RESPONSE_CANCEL,
					   -1);

  gtk_window_set_title (GTK_WINDOW (fontseldiag),
                        _("Font Selection"));

  gtk_widget_pop_composite_child ();

  _gtk_dialog_set_ignore_separator (dialog, TRUE);
}

/**
 * gtk_font_selection_dialog_new:
 * @title: the title of the dialog window 
 *
 * Creates a new #GtkFontSelectionDialog.
 *
 * Return value: a new #GtkFontSelectionDialog
 */
GtkWidget*
gtk_font_selection_dialog_new (const gchar *title)
{
  GtkFontSelectionDialog *fontseldiag;
  
  fontseldiag = g_object_new (GTK_TYPE_FONT_SELECTION_DIALOG, NULL);

  if (title)
    gtk_window_set_title (GTK_WINDOW (fontseldiag), title);
  
  return GTK_WIDGET (fontseldiag);
}

/**
 * gtk_font_selection_dialog_get_font_selection:
 * @fsd: a #GtkFontSelectionDialog
 *
 * Retrieves the #GtkFontSelection widget embedded in the dialog.
 *
 * Returns: (transfer none): the embedded #GtkFontSelection
 *
 * Since: 2.22
 **/
GtkWidget*
gtk_font_selection_dialog_get_font_selection (GtkFontSelectionDialog *fsd)
{
  g_return_val_if_fail (GTK_IS_FONT_SELECTION_DIALOG (fsd), NULL);

  return fsd->fontsel;
}

/**
 * gtk_font_selection_dialog_get_ok_button:
 * @fsd: a #GtkFontSelectionDialog
 *
 * Gets the 'OK' button.
 *
 * Return value: (transfer none): the #GtkWidget used in the dialog
 *     for the 'OK' button.
 *
 * Since: 2.14
 */
GtkWidget *
gtk_font_selection_dialog_get_ok_button (GtkFontSelectionDialog *fsd)
{
  g_return_val_if_fail (GTK_IS_FONT_SELECTION_DIALOG (fsd), NULL);

  return fsd->ok_button;
}

/**
 * gtk_font_selection_dialog_get_apply_button:
 * @fsd: a #GtkFontSelectionDialog
 *
 * Obtains a button. The button doesn't have any function.
 *
 * Return value: a #GtkWidget
 *
 * Since: 2.14
 *
 * Deprecated: 2.16: Don't use this function.
 */
GtkWidget *
gtk_font_selection_dialog_get_apply_button (GtkFontSelectionDialog *fsd)
{
  g_return_val_if_fail (GTK_IS_FONT_SELECTION_DIALOG (fsd), NULL);

  return fsd->apply_button;
}

/**
 * gtk_font_selection_dialog_get_cancel_button:
 * @fsd: a #GtkFontSelectionDialog
 *
 * Gets the 'Cancel' button.
 *
 * Return value: (transfer none): the #GtkWidget used in the dialog
 *     for the 'Cancel' button.
 *
 * Since: 2.14
 */
GtkWidget *
gtk_font_selection_dialog_get_cancel_button (GtkFontSelectionDialog *fsd)
{
  g_return_val_if_fail (GTK_IS_FONT_SELECTION_DIALOG (fsd), NULL);

  return fsd->cancel_button;
}

static void
gtk_font_selection_dialog_buildable_interface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->get_internal_child = gtk_font_selection_dialog_buildable_get_internal_child;
}

static GObject *
gtk_font_selection_dialog_buildable_get_internal_child (GtkBuildable *buildable,
							GtkBuilder   *builder,
							const gchar  *childname)
{
    if (strcmp(childname, "ok_button") == 0)
	return G_OBJECT (GTK_FONT_SELECTION_DIALOG(buildable)->ok_button);
    else if (strcmp(childname, "cancel_button") == 0)
	return G_OBJECT (GTK_FONT_SELECTION_DIALOG (buildable)->cancel_button);
    else if (strcmp(childname, "apply_button") == 0)
	return G_OBJECT (GTK_FONT_SELECTION_DIALOG(buildable)->apply_button);
    else if (strcmp(childname, "font_selection") == 0)
	return G_OBJECT (GTK_FONT_SELECTION_DIALOG(buildable)->fontsel);

    return parent_buildable_iface->get_internal_child (buildable, builder, childname);
}

/**
 * gtk_font_selection_dialog_get_font_name:
 * @fsd: a #GtkFontSelectionDialog
 * 
 * Gets the currently-selected font name.
 *
 * Note that this can be a different string than what you set with 
 * gtk_font_selection_dialog_set_font_name(), as the font selection widget
 * may normalize font names and thus return a string with a different 
 * structure. For example, "Helvetica Italic Bold 12" could be normalized 
 * to "Helvetica Bold Italic 12".  Use pango_font_description_equal()
 * if you want to compare two font descriptions.
 * 
 * Return value: A string with the name of the current font, or %NULL if no 
 *     font is selected. You must free this string with g_free().
 */
gchar*
gtk_font_selection_dialog_get_font_name (GtkFontSelectionDialog *fsd)
{
  g_return_val_if_fail (GTK_IS_FONT_SELECTION_DIALOG (fsd), NULL);

  return gtk_font_selection_get_font_name (GTK_FONT_SELECTION (fsd->fontsel));
}

/**
 * gtk_font_selection_dialog_get_font:
 * @fsd: a #GtkFontSelectionDialog
 *
 * Gets the currently-selected font.
 *
 * Return value: the #GdkFont from the #GtkFontSelection for the
 *     currently selected font in the dialog, or %NULL if no font is selected
 *
 * Deprecated: 2.0: Use gtk_font_selection_dialog_get_font_name() instead.
 */
GdkFont*
gtk_font_selection_dialog_get_font (GtkFontSelectionDialog *fsd)
{
  g_return_val_if_fail (GTK_IS_FONT_SELECTION_DIALOG (fsd), NULL);

  return gtk_font_selection_get_font_internal (GTK_FONT_SELECTION (fsd->fontsel));
}

/**
 * gtk_font_selection_dialog_set_font_name:
 * @fsd: a #GtkFontSelectionDialog
 * @fontname: a font name like "Helvetica 12" or "Times Bold 18"
 *
 * Sets the currently selected font. 
 * 
 * Return value: %TRUE if the font selected in @fsd is now the
 *     @fontname specified, %FALSE otherwise. 
 */
gboolean
gtk_font_selection_dialog_set_font_name (GtkFontSelectionDialog *fsd,
					 const gchar	        *fontname)
{
  g_return_val_if_fail (GTK_IS_FONT_SELECTION_DIALOG (fsd), FALSE);
  g_return_val_if_fail (fontname, FALSE);

  return gtk_font_selection_set_font_name (GTK_FONT_SELECTION (fsd->fontsel), fontname);
}

/**
 * gtk_font_selection_dialog_get_preview_text:
 * @fsd: a #GtkFontSelectionDialog
 *
 * Gets the text displayed in the preview area.
 * 
 * Return value: the text displayed in the preview area. 
 *     This string is owned by the widget and should not be 
 *     modified or freed 
 */
const gchar*
gtk_font_selection_dialog_get_preview_text (GtkFontSelectionDialog *fsd)
{
  g_return_val_if_fail (GTK_IS_FONT_SELECTION_DIALOG (fsd), NULL);

  return gtk_font_selection_get_preview_text (GTK_FONT_SELECTION (fsd->fontsel));
}

/**
 * gtk_font_selection_dialog_set_preview_text:
 * @fsd: a #GtkFontSelectionDialog
 * @text: the text to display in the preview area
 *
 * Sets the text displayed in the preview area. 
 */
void
gtk_font_selection_dialog_set_preview_text (GtkFontSelectionDialog *fsd,
					    const gchar	           *text)
{
  g_return_if_fail (GTK_IS_FONT_SELECTION_DIALOG (fsd));
  g_return_if_fail (text != NULL);

  gtk_font_selection_set_preview_text (GTK_FONT_SELECTION (fsd->fontsel), text);
}

#define __GTK_FONTSEL_C__
#include "gtkaliasdef.c"
