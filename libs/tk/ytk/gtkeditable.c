/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

#include "gtkeditable.h"
#include "gtkmarshalers.h"
#include "gtkintl.h"
#include "gtkalias.h"


static void gtk_editable_base_init (gpointer g_class);


GType
gtk_editable_get_type (void)
{
  static GType editable_type = 0;

  if (!editable_type)
    {
      const GTypeInfo editable_info =
      {
	sizeof (GtkEditableClass),  /* class_size */
	gtk_editable_base_init,	    /* base_init */
	NULL,			    /* base_finalize */
      };

      editable_type = g_type_register_static (G_TYPE_INTERFACE, I_("GtkEditable"),
					      &editable_info, 0);
    }

  return editable_type;
}

static void
gtk_editable_base_init (gpointer g_class)
{
  static gboolean initialized = FALSE;

  if (! initialized)
    {
      /**
       * GtkEditable::insert-text:
       * @editable: the object which received the signal
       * @new_text: the new text to insert
       * @new_text_length: the length of the new text, in bytes,
       *     or -1 if new_text is nul-terminated
       * @position: (inout) (type int): the position, in characters,
       *     at which to insert the new text. this is an in-out
       *     parameter.  After the signal emission is finished, it
       *     should point after the newly inserted text.
       *
       * This signal is emitted when text is inserted into
       * the widget by the user. The default handler for
       * this signal will normally be responsible for inserting
       * the text, so by connecting to this signal and then
       * stopping the signal with g_signal_stop_emission(), it
       * is possible to modify the inserted text, or prevent
       * it from being inserted entirely.
       */
      g_signal_new (I_("insert-text"),
		    GTK_TYPE_EDITABLE,
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkEditableClass, insert_text),
		    NULL, NULL,
		    _gtk_marshal_VOID__STRING_INT_POINTER,
		    G_TYPE_NONE, 3,
		    G_TYPE_STRING,
		    G_TYPE_INT,
		    G_TYPE_POINTER);

      /**
       * GtkEditable::delete-text:
       * @editable: the object which received the signal
       * @start_pos: the starting position
       * @end_pos: the end position
       * 
       * This signal is emitted when text is deleted from
       * the widget by the user. The default handler for
       * this signal will normally be responsible for deleting
       * the text, so by connecting to this signal and then
       * stopping the signal with g_signal_stop_emission(), it
       * is possible to modify the range of deleted text, or
       * prevent it from being deleted entirely. The @start_pos
       * and @end_pos parameters are interpreted as for
       * gtk_editable_delete_text().
       */
      g_signal_new (I_("delete-text"),
		    GTK_TYPE_EDITABLE,
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkEditableClass, delete_text),
		    NULL, NULL,
		    _gtk_marshal_VOID__INT_INT,
		    G_TYPE_NONE, 2,
		    G_TYPE_INT,
		    G_TYPE_INT);
      /**
       * GtkEditable::changed:
       * @editable: the object which received the signal
       *
       * The ::changed signal is emitted at the end of a single
       * user-visible operation on the contents of the #GtkEditable.
       *
       * E.g., a paste operation that replaces the contents of the
       * selection will cause only one signal emission (even though it
       * is implemented by first deleting the selection, then inserting
       * the new content, and may cause multiple ::notify::text signals
       * to be emitted).
       */ 
      g_signal_new (I_("changed"),
		    GTK_TYPE_EDITABLE,
		    G_SIGNAL_RUN_LAST,
		    G_STRUCT_OFFSET (GtkEditableClass, changed),
		    NULL, NULL,
		    _gtk_marshal_VOID__VOID,
		    G_TYPE_NONE, 0);

      initialized = TRUE;
    }
}

/**
 * gtk_editable_insert_text:
 * @editable: a #GtkEditable
 * @new_text: the text to append
 * @new_text_length: the length of the text in bytes, or -1
 * @position: (inout): location of the position text will be inserted at
 *
 * Inserts @new_text_length bytes of @new_text into the contents of the
 * widget, at position @position.
 *
 * Note that the position is in characters, not in bytes. 
 * The function updates @position to point after the newly inserted text.
 */
void
gtk_editable_insert_text (GtkEditable *editable,
			  const gchar *new_text,
			  gint         new_text_length,
			  gint        *position)
{
  g_return_if_fail (GTK_IS_EDITABLE (editable));
  g_return_if_fail (position != NULL);

  if (new_text_length < 0)
    new_text_length = strlen (new_text);
  
  GTK_EDITABLE_GET_CLASS (editable)->do_insert_text (editable, new_text, new_text_length, position);
}

/**
 * gtk_editable_delete_text:
 * @editable: a #GtkEditable
 * @start_pos: start position
 * @end_pos: end position
 *
 * Deletes a sequence of characters. The characters that are deleted are 
 * those characters at positions from @start_pos up to, but not including 
 * @end_pos. If @end_pos is negative, then the the characters deleted
 * are those from @start_pos to the end of the text.
 *
 * Note that the positions are specified in characters, not bytes.
 */
void
gtk_editable_delete_text (GtkEditable *editable,
			  gint         start_pos,
			  gint         end_pos)
{
  g_return_if_fail (GTK_IS_EDITABLE (editable));

  GTK_EDITABLE_GET_CLASS (editable)->do_delete_text (editable, start_pos, end_pos);
}

/**
 * gtk_editable_get_chars:
 * @editable: a #GtkEditable
 * @start_pos: start of text
 * @end_pos: end of text
 *
 * Retrieves a sequence of characters. The characters that are retrieved 
 * are those characters at positions from @start_pos up to, but not 
 * including @end_pos. If @end_pos is negative, then the the characters 
 * retrieved are those characters from @start_pos to the end of the text.
 * 
 * Note that positions are specified in characters, not bytes.
 *
 * Return value: a pointer to the contents of the widget as a
 *      string. This string is allocated by the #GtkEditable
 *      implementation and should be freed by the caller.
 */
gchar *    
gtk_editable_get_chars (GtkEditable *editable,
			gint         start_pos,
			gint         end_pos)
{
  g_return_val_if_fail (GTK_IS_EDITABLE (editable), NULL);

  return GTK_EDITABLE_GET_CLASS (editable)->get_chars (editable, start_pos, end_pos);
}

/**
 * gtk_editable_set_position:
 * @editable: a #GtkEditable
 * @position: the position of the cursor 
 *
 * Sets the cursor position in the editable to the given value.
 *
 * The cursor is displayed before the character with the given (base 0) 
 * index in the contents of the editable. The value must be less than or 
 * equal to the number of characters in the editable. A value of -1 
 * indicates that the position should be set after the last character 
 * of the editable. Note that @position is in characters, not in bytes.
 */
void
gtk_editable_set_position (GtkEditable      *editable,
			   gint              position)
{
  g_return_if_fail (GTK_IS_EDITABLE (editable));

  GTK_EDITABLE_GET_CLASS (editable)->set_position (editable, position);
}

/**
 * gtk_editable_get_position:
 * @editable: a #GtkEditable
 *
 * Retrieves the current position of the cursor relative to the start
 * of the content of the editable. 
 * 
 * Note that this position is in characters, not in bytes.
 *
 * Return value: the cursor position
 */
gint
gtk_editable_get_position (GtkEditable *editable)
{
  g_return_val_if_fail (GTK_IS_EDITABLE (editable), 0);

  return GTK_EDITABLE_GET_CLASS (editable)->get_position (editable);
}

/**
 * gtk_editable_get_selection_bounds:
 * @editable: a #GtkEditable
 * @start_pos: (out) (allow-none): location to store the starting position, or %NULL
 * @end_pos: (out) (allow-none): location to store the end position, or %NULL
 *
 * Retrieves the selection bound of the editable. start_pos will be filled
 * with the start of the selection and @end_pos with end. If no text was
 * selected both will be identical and %FALSE will be returned.
 *
 * Note that positions are specified in characters, not bytes.
 *
 * Return value: %TRUE if an area is selected, %FALSE otherwise
 */
gboolean
gtk_editable_get_selection_bounds (GtkEditable *editable,
				   gint        *start_pos,
				   gint        *end_pos)
{
  gint tmp_start, tmp_end;
  gboolean result;
  
  g_return_val_if_fail (GTK_IS_EDITABLE (editable), FALSE);

  result = GTK_EDITABLE_GET_CLASS (editable)->get_selection_bounds (editable, &tmp_start, &tmp_end);

  if (start_pos)
    *start_pos = MIN (tmp_start, tmp_end);
  if (end_pos)
    *end_pos = MAX (tmp_start, tmp_end);

  return result;
}

/**
 * gtk_editable_delete_selection:
 * @editable: a #GtkEditable
 *
 * Deletes the currently selected text of the editable.
 * This call doesn't do anything if there is no selected text.
 */
void
gtk_editable_delete_selection (GtkEditable *editable)
{
  gint start, end;

  g_return_if_fail (GTK_IS_EDITABLE (editable));

  if (gtk_editable_get_selection_bounds (editable, &start, &end))
    gtk_editable_delete_text (editable, start, end);
}

/**
 * gtk_editable_select_region:
 * @editable: a #GtkEditable
 * @start_pos: start of region
 * @end_pos: end of region
 *
 * Selects a region of text. The characters that are selected are 
 * those characters at positions from @start_pos up to, but not 
 * including @end_pos. If @end_pos is negative, then the the 
 * characters selected are those characters from @start_pos to 
 * the end of the text.
 * 
 * Note that positions are specified in characters, not bytes.
 */
void
gtk_editable_select_region (GtkEditable *editable,
			    gint         start_pos,
			    gint         end_pos)
{
  g_return_if_fail (GTK_IS_EDITABLE (editable));
  
  GTK_EDITABLE_GET_CLASS (editable)->set_selection_bounds (editable, start_pos, end_pos);
}

/**
 * gtk_editable_cut_clipboard:
 * @editable: a #GtkEditable
 *
 * Removes the contents of the currently selected content in the editable and
 * puts it on the clipboard.
 */
void
gtk_editable_cut_clipboard (GtkEditable *editable)
{
  g_return_if_fail (GTK_IS_EDITABLE (editable));
  
  g_signal_emit_by_name (editable, "cut-clipboard");
}

/**
 * gtk_editable_copy_clipboard:
 * @editable: a #GtkEditable
 *
 * Copies the contents of the currently selected content in the editable and
 * puts it on the clipboard.
 */
void
gtk_editable_copy_clipboard (GtkEditable *editable)
{
  g_return_if_fail (GTK_IS_EDITABLE (editable));
  
  g_signal_emit_by_name (editable, "copy-clipboard");
}

/**
 * gtk_editable_paste_clipboard:
 * @editable: a #GtkEditable
 *
 * Pastes the content of the clipboard to the current position of the
 * cursor in the editable.
 */
void
gtk_editable_paste_clipboard (GtkEditable *editable)
{
  g_return_if_fail (GTK_IS_EDITABLE (editable));
  
  g_signal_emit_by_name (editable, "paste-clipboard");
}

/**
 * gtk_editable_set_editable:
 * @editable: a #GtkEditable
 * @is_editable: %TRUE if the user is allowed to edit the text
 *   in the widget
 *
 * Determines if the user can edit the text in the editable
 * widget or not. 
 */
void
gtk_editable_set_editable (GtkEditable    *editable,
			   gboolean        is_editable)
{
  g_return_if_fail (GTK_IS_EDITABLE (editable));

  g_object_set (editable,
		"editable", is_editable != FALSE,
		NULL);
}

/**
 * gtk_editable_get_editable:
 * @editable: a #GtkEditable
 *
 * Retrieves whether @editable is editable. See
 * gtk_editable_set_editable().
 *
 * Return value: %TRUE if @editable is editable.
 */
gboolean
gtk_editable_get_editable (GtkEditable *editable)
{
  gboolean value;

  g_return_val_if_fail (GTK_IS_EDITABLE (editable), FALSE);

  g_object_get (editable, "editable", &value, NULL);

  return value;
}

#define __GTK_EDITABLE_C__
#include "gtkaliasdef.c"
