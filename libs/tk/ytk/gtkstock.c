/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc. 
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"
#include <string.h>

#include "gtkprivate.h"
#include "gtkstock.h"
#include "gtkiconfactory.h"
#include "gtkintl.h"
#include <gdk/gdkkeysyms.h>
#include "gtkprivate.h"
#include "gtkalias.h"

/**
 * SECTION:gtkstock
 * @Short_description:
Prebuilt common menu/toolbar items and corresponding icons
 * @Title: Stock Items
 *
 * Stock items represent commonly-used menu or toolbar items such as
 * "Open" or "Exit". Each stock item is identified by a stock ID;
 * stock IDs are just strings, but macros such as #GTK_STOCK_OPEN are
 * provided to avoid typing mistakes in the strings.
 * Applications can register their own stock items in addition to those
 * built-in to GTK+.
 *
 * Each stock ID can be associated with a #GtkStockItem, which contains
 * the user-visible label, keyboard accelerator, and translation domain
 * of the menu or toolbar item; and/or with an icon stored in a
 * #GtkIconFactory. See <link
 * linkend="gtk-Themeable-Stock-Images">GtkIconFactory</link> for
 * more information on stock icons. The connection between a
 * #GtkStockItem and stock icons is purely conventional (by virtue of
 * using the same stock ID); it's possible to register a stock item but
 * no icon, and vice versa. Stock icons may have a RTL variant which gets
 * used for right-to-left locales.
 */

static GHashTable *translate_hash = NULL;
static GHashTable *stock_hash = NULL;
static void init_stock_hash (void);

/* We use an unused modifier bit to mark stock items which
 * must be freed when they are removed from the hash table.
 */
#define NON_STATIC_MASK (1 << 29)

typedef struct _GtkStockTranslateFunc GtkStockTranslateFunc;
struct _GtkStockTranslateFunc
{
  GtkTranslateFunc func;
  gpointer data;
  GDestroyNotify notify;
};

static void
real_add (const GtkStockItem *items,
          guint               n_items,
          gboolean            copy)
{
  int i;

  init_stock_hash ();

  if (n_items == 0)
    return;

  i = 0;
  while (i < n_items)
    {
      gpointer old_key, old_value;
      const GtkStockItem *item = &items[i];

      if (item->modifier & NON_STATIC_MASK)
	{
	  g_warning ("Bit 29 set in stock accelerator.\n");
	  copy = TRUE;
	}

      if (copy)
	{
	  item = gtk_stock_item_copy (item);
	  ((GtkStockItem *)item)->modifier |= NON_STATIC_MASK;
	}

      if (g_hash_table_lookup_extended (stock_hash, item->stock_id,
                                        &old_key, &old_value))
        {
          g_hash_table_remove (stock_hash, old_key);
	  if (((GtkStockItem *)old_value)->modifier & NON_STATIC_MASK)
	    gtk_stock_item_free (old_value);
        }
      
      g_hash_table_insert (stock_hash,
                           (gchar*)item->stock_id, (GtkStockItem*)item);

      ++i;
    }
}

/**
 * gtk_stock_add:
 * @items: (array length=n_items): a #GtkStockItem or array of items
 * @n_items: number of #GtkStockItem in @items
 *
 * Registers each of the stock items in @items. If an item already
 * exists with the same stock ID as one of the @items, the old item
 * gets replaced. The stock items are copied, so GTK+ does not hold
 * any pointer into @items and @items can be freed. Use
 * gtk_stock_add_static() if @items is persistent and GTK+ need not
 * copy the array.
 * 
 **/
void
gtk_stock_add (const GtkStockItem *items,
               guint               n_items)
{
  g_return_if_fail (items != NULL);

  real_add (items, n_items, TRUE);
}

/**
 * gtk_stock_add_static:
 * @items: (array length=n_items): a #GtkStockItem or array of #GtkStockItem
 * @n_items: number of items
 *
 * Same as gtk_stock_add(), but doesn't copy @items, so
 * @items must persist until application exit.
 * 
 **/
void
gtk_stock_add_static (const GtkStockItem *items,
                      guint               n_items)
{
  g_return_if_fail (items != NULL);

  real_add (items, n_items, FALSE);
}

/**
 * gtk_stock_lookup:
 * @stock_id: a stock item name
 * @item: (out): stock item to initialize with values
 * 
 * Fills @item with the registered values for @stock_id, returning %TRUE
 * if @stock_id was known.
 * 
 * 
 * Return value: %TRUE if @item was initialized
 **/
gboolean
gtk_stock_lookup (const gchar  *stock_id,
                  GtkStockItem *item)
{
  const GtkStockItem *found;

  g_return_val_if_fail (stock_id != NULL, FALSE);
  g_return_val_if_fail (item != NULL, FALSE);

  init_stock_hash ();

  found = g_hash_table_lookup (stock_hash, stock_id);

  if (found)
    {
      *item = *found;
      item->modifier &= ~NON_STATIC_MASK;
      if (item->label)
	{
	  GtkStockTranslateFunc *translate;
	  
	  if (item->translation_domain)
	    translate = (GtkStockTranslateFunc *) 
	      g_hash_table_lookup (translate_hash, item->translation_domain);
	  else
	    translate = NULL;
	  
	  if (translate != NULL && translate->func != NULL)
	    item->label = (* translate->func) (item->label, translate->data);
	  else
	    item->label = (gchar *) g_dgettext (item->translation_domain, item->label);
	}
    }

  return found != NULL;
}

/**
 * gtk_stock_list_ids:
 * 
 * Retrieves a list of all known stock IDs added to a #GtkIconFactory
 * or registered with gtk_stock_add(). The list must be freed with g_slist_free(),
 * and each string in the list must be freed with g_free().
 *
 * Return value: (element-type utf8) (transfer full): a list of known stock IDs
 **/
GSList*
gtk_stock_list_ids (void)
{
  GList *ids;
  GList *icon_ids;
  GSList *retval;
  const gchar *last_id;
  
  init_stock_hash ();

  ids = g_hash_table_get_keys (stock_hash);
  icon_ids = _gtk_icon_factory_list_ids ();
  ids = g_list_concat (ids, icon_ids);

  ids = g_list_sort (ids, (GCompareFunc)strcmp);

  last_id = NULL;
  retval = NULL;
  while (ids != NULL)
    {
      GList *next;

      next = g_list_next (ids);

      if (last_id && strcmp (ids->data, last_id) == 0)
        {
          /* duplicate, ignore */
        }
      else
        {
          retval = g_slist_prepend (retval, g_strdup (ids->data));
          last_id = ids->data;
        }

      g_list_free_1 (ids);
      
      ids = next;
    }

  return retval;
}

/**
 * gtk_stock_item_copy:
 * @item: a #GtkStockItem
 * 
 * Copies a stock item, mostly useful for language bindings and not in applications.
 * 
 * Return value: a new #GtkStockItem
 **/
GtkStockItem *
gtk_stock_item_copy (const GtkStockItem *item)
{
  GtkStockItem *copy;

  g_return_val_if_fail (item != NULL, NULL);

  copy = g_new (GtkStockItem, 1);

  *copy = *item;

  copy->stock_id = g_strdup (item->stock_id);
  copy->label = g_strdup (item->label);
  copy->translation_domain = g_strdup (item->translation_domain);

  return copy;
}

/**
 * gtk_stock_item_free:
 * @item: a #GtkStockItem
 *
 * Frees a stock item allocated on the heap, such as one returned by
 * gtk_stock_item_copy(). Also frees the fields inside the stock item,
 * if they are not %NULL.
 * 
 **/
void
gtk_stock_item_free (GtkStockItem *item)
{
  g_return_if_fail (item != NULL);

  g_free ((gchar*)item->stock_id);
  g_free ((gchar*)item->label);
  g_free ((gchar*)item->translation_domain);

  g_free (item);
}

static const GtkStockItem builtin_items [] =
{
  /* KEEP IN SYNC with gtkiconfactory.c stock icons, when appropriate */ 
 
  { GTK_STOCK_DIALOG_INFO, NC_("Stock label", "Information"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_DIALOG_WARNING, NC_("Stock label", "Warning"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_DIALOG_ERROR, NC_("Stock label", "Error"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_DIALOG_QUESTION, NC_("Stock label", "Question"), 0, 0, GETTEXT_PACKAGE },

  /*  FIXME these need accelerators when appropriate, and
   * need the mnemonics to be rationalized
   */
  { GTK_STOCK_ABOUT, NC_("Stock label", "_About"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_ADD, NC_("Stock label", "_Add"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_APPLY, NC_("Stock label", "_Apply"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_BOLD, NC_("Stock label", "_Bold"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_CANCEL, NC_("Stock label", "_Cancel"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_CDROM, NC_("Stock label", "_CD-Rom"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_CLEAR, NC_("Stock label", "_Clear"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_CLOSE, NC_("Stock label", "_Close"), GTK_DEFAULT_ACCEL_MOD_MASK_VIRTUAL, 'w', GETTEXT_PACKAGE },
  { GTK_STOCK_CONNECT, NC_("Stock label", "C_onnect"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_CONVERT, NC_("Stock label", "_Convert"), 0, 0, GETTEXT_PACKAGE },
   { GTK_STOCK_COPY, NC_("Stock label", "_Copy"), GTK_DEFAULT_ACCEL_MOD_MASK_VIRTUAL, 'c', GETTEXT_PACKAGE },
  { GTK_STOCK_CUT, NC_("Stock label", "Cu_t"), GTK_DEFAULT_ACCEL_MOD_MASK_VIRTUAL, 'x', GETTEXT_PACKAGE },
  { GTK_STOCK_DELETE, NC_("Stock label", "_Delete"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_DISCARD, NC_("Stock label", "_Discard"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_DISCONNECT, NC_("Stock label", "_Disconnect"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_EXECUTE, NC_("Stock label", "_Execute"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_EDIT, NC_("Stock label", "_Edit"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_FIND, NC_("Stock label", "_Find"), GTK_DEFAULT_ACCEL_MOD_MASK_VIRTUAL, 'f', GETTEXT_PACKAGE },
  { GTK_STOCK_FIND_AND_REPLACE, NC_("Stock label", "Find and _Replace"), GTK_DEFAULT_ACCEL_MOD_MASK_VIRTUAL, 'r', GETTEXT_PACKAGE },
  { GTK_STOCK_FLOPPY, NC_("Stock label", "_Floppy"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_FULLSCREEN, NC_("Stock label", "_Fullscreen"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_LEAVE_FULLSCREEN, NC_("Stock label", "_Leave Fullscreen"), 0, 0, GETTEXT_PACKAGE },
  /* This is a navigation label as in "go to the bottom of the page" */
  { GTK_STOCK_GOTO_BOTTOM, NC_("Stock label, navigation", "_Bottom"), 0, 0, GETTEXT_PACKAGE "-navigation" },
  /* This is a navigation label as in "go to the first page" */
  { GTK_STOCK_GOTO_FIRST, NC_("Stock label, navigation", "_First"), 0, 0, GETTEXT_PACKAGE "-navigation" },
  /* This is a navigation label as in "go to the last page" */
  { GTK_STOCK_GOTO_LAST, NC_("Stock label, navigation", "_Last"), 0, 0, GETTEXT_PACKAGE "-navigation" },
  /* This is a navigation label as in "go to the top of the page" */
  { GTK_STOCK_GOTO_TOP, NC_("Stock label, navigation", "_Top"), 0, 0, GETTEXT_PACKAGE "-navigation" },
  /* This is a navigation label as in "go back" */
  { GTK_STOCK_GO_BACK, NC_("Stock label, navigation", "_Back"), 0, 0, GETTEXT_PACKAGE "-navigation" },
  /* This is a navigation label as in "go down" */
  { GTK_STOCK_GO_DOWN, NC_("Stock label, navigation", "_Down"), 0, 0, GETTEXT_PACKAGE "-navigation" },
  /* This is a navigation label as in "go forward" */
  { GTK_STOCK_GO_FORWARD, NC_("Stock label, navigation", "_Forward"), 0, 0, GETTEXT_PACKAGE "-navigation" },
  /* This is a navigation label as in "go up" */
  { GTK_STOCK_GO_UP, NC_("Stock label, navigation", "_Up"), 0, 0, GETTEXT_PACKAGE "-navigation" },
  { GTK_STOCK_HARDDISK, NC_("Stock label", "_Harddisk"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_HELP, NC_("Stock label", "_Help"), GTK_DEFAULT_ACCEL_MOD_MASK_VIRTUAL, 'h', GETTEXT_PACKAGE },
  { GTK_STOCK_HOME, NC_("Stock label", "_Home"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_INDENT, NC_("Stock label", "Increase Indent"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_UNINDENT, NC_("Stock label", "Decrease Indent"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_INDEX, NC_("Stock label", "_Index"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_INFO, NC_("Stock label", "_Information"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_ITALIC, NC_("Stock label", "_Italic"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_JUMP_TO, NC_("Stock label", "_Jump to"), 0, 0, GETTEXT_PACKAGE },
  /* This is about text justification, "centered text" */
  { GTK_STOCK_JUSTIFY_CENTER, NC_("Stock label", "_Center"), 0, 0, GETTEXT_PACKAGE },
  /* This is about text justification */
  { GTK_STOCK_JUSTIFY_FILL, NC_("Stock label", "_Fill"), 0, 0, GETTEXT_PACKAGE },
  /* This is about text justification, "left-justified text" */
  { GTK_STOCK_JUSTIFY_LEFT, NC_("Stock label", "_Left"), 0, 0, GETTEXT_PACKAGE },
  /* This is about text justification, "right-justified text" */
  { GTK_STOCK_JUSTIFY_RIGHT, NC_("Stock label", "_Right"), 0, 0, GETTEXT_PACKAGE },

  /* Media label, as in "fast forward" */
  { GTK_STOCK_MEDIA_FORWARD, NC_("Stock label, media", "_Forward"), 0, 0, GETTEXT_PACKAGE "-media" },
  /* Media label, as in "next song" */
  { GTK_STOCK_MEDIA_NEXT, NC_("Stock label, media", "_Next"), 0, 0, GETTEXT_PACKAGE "-media" },
  /* Media label, as in "pause music" */
  { GTK_STOCK_MEDIA_PAUSE, NC_("Stock label, media", "P_ause"), 0, 0, GETTEXT_PACKAGE "-media" },
  /* Media label, as in "play music" */
  { GTK_STOCK_MEDIA_PLAY, NC_("Stock label, media", "_Play"), 0, 0, GETTEXT_PACKAGE "-media" },
  /* Media label, as in  "previous song" */
  { GTK_STOCK_MEDIA_PREVIOUS, NC_("Stock label, media", "Pre_vious"), 0, 0, GETTEXT_PACKAGE "-media" },
  /* Media label */
  { GTK_STOCK_MEDIA_RECORD, NC_("Stock label, media", "_Record"), 0, 0, GETTEXT_PACKAGE "-media" },
  /* Media label */
  { GTK_STOCK_MEDIA_REWIND, NC_("Stock label, media", "R_ewind"), 0, 0, GETTEXT_PACKAGE "-media" },
  /* Media label */
  { GTK_STOCK_MEDIA_STOP, NC_("Stock label, media", "_Stop"), 0, 0, GETTEXT_PACKAGE "-media" },
  { GTK_STOCK_NETWORK, NC_("Stock label", "_Network"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_NEW, NC_("Stock label", "_New"), GTK_DEFAULT_ACCEL_MOD_MASK_VIRTUAL, 'n', GETTEXT_PACKAGE },
  { GTK_STOCK_NO, NC_("Stock label", "_No"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_OK, NC_("Stock label", "_OK"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_OPEN, NC_("Stock label", "_Open"), GTK_DEFAULT_ACCEL_MOD_MASK_VIRTUAL, 'o', GETTEXT_PACKAGE },
  /* Page orientation */
  { GTK_STOCK_ORIENTATION_LANDSCAPE, NC_("Stock label", "Landscape"), 0, 0, GETTEXT_PACKAGE },
  /* Page orientation */
  { GTK_STOCK_ORIENTATION_PORTRAIT, NC_("Stock label", "Portrait"), 0, 0, GETTEXT_PACKAGE },
  /* Page orientation */
  { GTK_STOCK_ORIENTATION_REVERSE_LANDSCAPE, NC_("Stock label", "Reverse landscape"), 0, 0, GETTEXT_PACKAGE },
  /* Page orientation */
  { GTK_STOCK_ORIENTATION_REVERSE_PORTRAIT, NC_("Stock label", "Reverse portrait"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_PAGE_SETUP, NC_("Stock label", "Page Set_up"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_PASTE, NC_("Stock label", "_Paste"), GTK_DEFAULT_ACCEL_MOD_MASK_VIRTUAL, 'v', GETTEXT_PACKAGE },
  { GTK_STOCK_PREFERENCES, NC_("Stock label", "_Preferences"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_PRINT, NC_("Stock label", "_Print"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_PRINT_PREVIEW, NC_("Stock label", "Print Pre_view"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_PROPERTIES, NC_("Stock label", "_Properties"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_QUIT, NC_("Stock label", "_Quit"), GTK_DEFAULT_ACCEL_MOD_MASK_VIRTUAL, 'q', GETTEXT_PACKAGE },
  { GTK_STOCK_REDO, NC_("Stock label", "_Redo"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_REFRESH, NC_("Stock label", "_Refresh"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_REMOVE, NC_("Stock label", "_Remove"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_REVERT_TO_SAVED, NC_("Stock label", "_Revert"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_SAVE, NC_("Stock label", "_Save"), GTK_DEFAULT_ACCEL_MOD_MASK_VIRTUAL, 's', GETTEXT_PACKAGE },
  { GTK_STOCK_SAVE_AS, NC_("Stock label", "Save _As"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_SELECT_ALL, NC_("Stock label", "Select _All"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_SELECT_COLOR, NC_("Stock label", "_Color"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_SELECT_FONT, NC_("Stock label", "_Font"), 0, 0, GETTEXT_PACKAGE },
  /* Sorting direction */
  { GTK_STOCK_SORT_ASCENDING, NC_("Stock label", "_Ascending"), 0, 0, GETTEXT_PACKAGE },
  /* Sorting direction */
  { GTK_STOCK_SORT_DESCENDING, NC_("Stock label", "_Descending"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_SPELL_CHECK, NC_("Stock label", "_Spell Check"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_STOP, NC_("Stock label", "_Stop"), 0, 0, GETTEXT_PACKAGE },
  /* Font variant */
  { GTK_STOCK_STRIKETHROUGH, NC_("Stock label", "_Strikethrough"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_UNDELETE, NC_("Stock label", "_Undelete"), 0, 0, GETTEXT_PACKAGE },
  /* Font variant */
  { GTK_STOCK_UNDERLINE, NC_("Stock label", "_Underline"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_UNDO, NC_("Stock label", "_Undo"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_YES, NC_("Stock label", "_Yes"), 0, 0, GETTEXT_PACKAGE },
  /* Zoom */
  { GTK_STOCK_ZOOM_100, NC_("Stock label", "_Normal Size"), 0, 0, GETTEXT_PACKAGE },
  /* Zoom */
  { GTK_STOCK_ZOOM_FIT, NC_("Stock label", "Best _Fit"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_ZOOM_IN, NC_("Stock label", "Zoom _In"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_ZOOM_OUT, NC_("Stock label", "Zoom _Out"), 0, 0, GETTEXT_PACKAGE }
};

/**
 * gtk_stock_set_translate_func: 
 * @domain: the translation domain for which @func shall be used
 * @func: a #GtkTranslateFunc 
 * @data: data to pass to @func
 * @notify: a #GDestroyNotify that is called when @data is
 *   no longer needed
 *
 * Sets a function to be used for translating the @label of 
 * a stock item.
 * 
 * If no function is registered for a translation domain,
 * g_dgettext() is used.
 * 
 * The function is used for all stock items whose
 * @translation_domain matches @domain. Note that it is possible
 * to use strings different from the actual gettext translation domain
 * of your application for this, as long as your #GtkTranslateFunc uses
 * the correct domain when calling dgettext(). This can be useful, e.g.
 * when dealing with message contexts:
 *
 * |[
 * GtkStockItem items[] = { 
 *  { MY_ITEM1, NC_("odd items", "Item 1"), 0, 0, "odd-item-domain" },
 *  { MY_ITEM2, NC_("even items", "Item 2"), 0, 0, "even-item-domain" },
 * };
 *
 * gchar *
 * my_translate_func (const gchar *msgid,
 *                    gpointer     data)
 * {
 *   gchar *msgctxt = data;
 * 
 *   return (gchar*)g_dpgettext2 (GETTEXT_PACKAGE, msgctxt, msgid);
 * }
 *
 * /&ast; ... &ast;/
 *
 * gtk_stock_add (items, G_N_ELEMENTS (items));
 * gtk_stock_set_translate_func ("odd-item-domain", my_translate_func, "odd items"); 
 * gtk_stock_set_translate_func ("even-item-domain", my_translate_func, "even items"); 
 * ]|
 * 
 * Since: 2.8
 */
void
gtk_stock_set_translate_func (const gchar      *domain,
			      GtkTranslateFunc  func,
			      gpointer          data,
			      GDestroyNotify    notify)
{
  GtkStockTranslateFunc *translate;
  gchar *domainname;
 
  domainname = g_strdup (domain);

  translate = (GtkStockTranslateFunc *) 
    g_hash_table_lookup (translate_hash, domainname);

  if (translate)
    {
      if (translate->notify)
	(* translate->notify) (translate->data);
    }
  else
    translate = g_new0 (GtkStockTranslateFunc, 1);
    
  translate->func = func;
  translate->data = data;
  translate->notify = notify;
      
  g_hash_table_insert (translate_hash, domainname, translate);
}

static gchar *
sgettext_swapped (const gchar *msgid, 
		  gpointer     data)
{
  gchar *msgctxt = data;

  return (gchar *)g_dpgettext2 (GETTEXT_PACKAGE, msgctxt, msgid);
}

static void
init_stock_hash (void)
{
  if (stock_hash == NULL)
    {
      stock_hash = g_hash_table_new (g_str_hash, g_str_equal);

      gtk_stock_add_static (builtin_items, G_N_ELEMENTS (builtin_items));
    }

  if (translate_hash == NULL)
    {
      translate_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                      g_free, NULL);

      gtk_stock_set_translate_func (GETTEXT_PACKAGE, 
				    sgettext_swapped,
				    "Stock label",
				    NULL);
      gtk_stock_set_translate_func (GETTEXT_PACKAGE "-navigation", 
				    sgettext_swapped,
				    "Stock label, navigation",
				    NULL);
      gtk_stock_set_translate_func (GETTEXT_PACKAGE "-media", 
				    sgettext_swapped,
				    "Stock label, media",
				    NULL);
    }
}

#define __GTK_STOCK_C__
#include "gtkaliasdef.c"
