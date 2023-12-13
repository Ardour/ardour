/* ATK - The Accessibility Toolkit for GTK+
 * Copyright 2001 Sun Microsystems Inc.
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

#include "atk.h"
#include "atkmarshal.h"

#include <string.h>

/**
 * SECTION:atktext
 * @Short_description: The ATK interface implemented by components
 *  with text content.
 * @Title:AtkText
 *
 * #AtkText should be implemented by #AtkObjects on behalf of widgets
 * that have text content which is either attributed or otherwise
 * non-trivial.  #AtkObjects whose text content is simple,
 * unattributed, and very brief may expose that content via
 * #atk_object_get_name instead; however if the text is editable,
 * multi-line, typically longer than three or four words, attributed,
 * selectable, or if the object already uses the 'name' ATK property
 * for other information, the #AtkText interface should be used to
 * expose the text content.  In the case of editable text content,
 * #AtkEditableText (a subtype of the #AtkText interface) should be
 * implemented instead.
 *
 *  #AtkText provides not only traversal facilities and change
 * notification for text content, but also caret tracking and glyph
 * bounding box calculations.  Note that the text strings are exposed
 * as UTF-8, and are therefore potentially multi-byte, and
 * caret-to-byte offset mapping makes no assumptions about the
 * character length; also bounding box glyph-to-offset mapping may be
 * complex for languages which use ligatures.
 */

static GPtrArray *extra_attributes = NULL;

enum {
  TEXT_CHANGED,
  TEXT_CARET_MOVED,
  TEXT_SELECTION_CHANGED,
  TEXT_ATTRIBUTES_CHANGED,
  TEXT_INSERT,
  TEXT_REMOVE,
  LAST_SIGNAL
};

static const char boolean[] =
  "false\0"
  "true";
static const guint8 boolean_offsets[] = {
  0, 6
};

static const char style[] =
  "normal\0"
  "oblique\0"
  "italic";
static const guint8 style_offsets[] = {
  0, 7, 15
};

static const char variant[] =
  "normal\0"
  "small_caps";
static const guint8 variant_offsets[] = {
  0, 7
};

static const char stretch[] =
  "ultra_condensed\0"
  "extra_condensed\0"
  "condensed\0"
  "semi_condensed\0"
  "normal\0"
  "semi_expanded\0"
  "expanded\0"
  "extra_expanded\0"
  "ultra_expanded";
static const guint8 stretch_offsets[] = {
  0, 16, 32, 42, 57, 64, 78, 87, 102
};

static const char justification[] =
  "left\0"
  "right\0"
  "center\0"
  "fill";
static const guint8 justification_offsets[] = {
  0, 5, 11, 18
};

static const char direction[] =
  "none\0"
  "ltr\0"
  "rtl";
static const guint8 direction_offsets[] = {
  0, 5, 9
};

static const char wrap_mode[] =
  "none\0"
  "char\0"
  "word\0"
  "word_char";
static const guint8 wrap_mode_offsets[] = {
  0, 5, 10, 15
};

static const char underline[] =
  "none\0"
  "single\0"
  "double\0"
  "low\0"
  "error";
static const guint8 underline_offsets[] = {
  0, 5, 12, 19, 23
};

static void atk_text_base_init (AtkTextIface *class);

static void atk_text_real_get_range_extents  (AtkText          *text,
                                              gint             start_offset,
                                              gint             end_offset,
                                              AtkCoordType     coord_type,
                                              AtkTextRectangle *rect);

static AtkTextRange** atk_text_real_get_bounded_ranges (AtkText          *text,
                                                        AtkTextRectangle *rect,
                                                        AtkCoordType     coord_type,
                                                        AtkTextClipType  x_clip_type,
                                                        AtkTextClipType  y_clip_type);

static guint atk_text_signals[LAST_SIGNAL] = { 0 };

GType
atk_text_get_type (void)
{
  static GType type = 0;

  if (!type) 
    {
      static const GTypeInfo tinfo =
      {
        sizeof (AtkTextIface),
        (GBaseInitFunc) atk_text_base_init,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) NULL /* atk_text_interface_init */ ,
        (GClassFinalizeFunc) NULL,

      };

      type = g_type_register_static (G_TYPE_INTERFACE, "AtkText", &tinfo, 0);
    }

  return type;
}

static void
atk_text_base_init (AtkTextIface *class)
{
  static gboolean initialized = FALSE;
  
  if (! initialized)
    {
      /* 
       * Note that text_changed signal supports details "insert", "delete", 
       * possibly "replace". 
       */
     
      class->get_range_extents = atk_text_real_get_range_extents; 
      class->get_bounded_ranges = atk_text_real_get_bounded_ranges; 

      /**
       * AtkText::text-changed:
       * @atktext: the object which received the signal.
       * @arg1: The position (character offset) of the insertion or deletion.
       * @arg2: The length (in characters) of text inserted or deleted.
       *
       * The "text-changed" signal is emitted when the text of the
       * object which implements the AtkText interface changes, This
       * signal will have a detail which is either "insert" or
       * "delete" which identifies whether the text change was an
       * insertion or a deletion.
       *
       * Deprecated: Since 2.9.4. Use #AtkObject::text-insert or
       * #AtkObject::text-remove instead.
       */
      atk_text_signals[TEXT_CHANGED] =
	g_signal_new ("text_changed",
		      ATK_TYPE_TEXT,
		      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
		      G_STRUCT_OFFSET (AtkTextIface, text_changed), 
		      (GSignalAccumulator) NULL, NULL,
		      atk_marshal_VOID__INT_INT,
		      G_TYPE_NONE,
		      2, G_TYPE_INT, G_TYPE_INT);

      /**
       * AtkText::text-insert:
       * @atktext: the object which received the signal.
       * @arg1: The position (character offset) of the insertion.
       * @arg2: The length (in characters) of text inserted.
       * @arg3: The new text inserted
       *
       * The "text-insert" signal is emitted when a new text is
       * inserted. If the signal was not triggered by the user
       * (e.g. typing or pasting text), the "system" detail should be
       * included.
       */
      atk_text_signals[TEXT_INSERT] =
	g_signal_new ("text_insert",
		      ATK_TYPE_TEXT,
		      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
		      0,
		      (GSignalAccumulator) NULL, NULL,
		      atk_marshal_VOID__INT_INT_STRING,
		      G_TYPE_NONE,
		      3, G_TYPE_INT, G_TYPE_INT, G_TYPE_STRING);

      /**
       * AtkText::text-remove:
       * @atktext: the object which received the signal.
       * @arg1: The position (character offset) of the removal.
       * @arg2: The length (in characters) of text removed.
       * @arg3: The old text removed
       *
       * The "text-remove" signal is emitted when a new text is
       * removed. If the signal was not triggered by the user
       * (e.g. typing or pasting text), the "system" detail should be
       * included.
       */
      atk_text_signals[TEXT_REMOVE] =
	g_signal_new ("text_remove",
		      ATK_TYPE_TEXT,
		      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
		      0,
		      (GSignalAccumulator) NULL, NULL,
		      atk_marshal_VOID__INT_INT_STRING,
		      G_TYPE_NONE,
		      3, G_TYPE_INT, G_TYPE_INT, G_TYPE_STRING);

      /**
       * AtkText::text-caret-moved:
       * @atktext: the object which received the signal.
       * @arg1: The new position of the text caret.
       *
       * The "text-caret-moved" signal is emitted when the caret
       * position of the text of an object which implements AtkText
       * changes.
       */
      atk_text_signals[TEXT_CARET_MOVED] =
	g_signal_new ("text_caret_moved",
		      ATK_TYPE_TEXT,
		      G_SIGNAL_RUN_LAST,
		      G_STRUCT_OFFSET (AtkTextIface, text_caret_moved),
		      (GSignalAccumulator) NULL, NULL,
		      g_cclosure_marshal_VOID__INT,
		      G_TYPE_NONE,
		      1, G_TYPE_INT);

      /**
       * AtkText::text-selection-changed:
       * @atktext: the object which received the signal.
       *
       * The "text-selection-changed" signal is emitted when the
       * selected text of an object which implements AtkText changes.
       */
      atk_text_signals[TEXT_SELECTION_CHANGED] =
        g_signal_new ("text_selection_changed",
                      ATK_TYPE_TEXT,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (AtkTextIface, text_selection_changed),
                      (GSignalAccumulator) NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
      /**
       * AtkText::text-attributes-changed:
       * @atktext: the object which received the signal.
       *
       * The "text-attributes-changed" signal is emitted when the text
       * attributes of the text of an object which implements AtkText
       * changes.
       */
      atk_text_signals[TEXT_ATTRIBUTES_CHANGED] =
        g_signal_new ("text_attributes_changed",
                      ATK_TYPE_TEXT,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (AtkTextIface, text_attributes_changed),
                      (GSignalAccumulator) NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

      
      initialized = TRUE;
    }
}

/**
 * atk_text_get_text:
 * @text: an #AtkText
 * @start_offset: start position
 * @end_offset: end position, or -1 for the end of the string.
 *
 * Gets the specified text.
 *
 * Returns: a newly allocated string containing the text from @start_offset up
 *   to, but not including @end_offset. Use g_free() to free the returned string.
 **/
gchar*
atk_text_get_text (AtkText      *text,
                   gint         start_offset,
                   gint         end_offset)
{
  AtkTextIface *iface;
  
  g_return_val_if_fail (ATK_IS_TEXT (text), NULL);

  iface = ATK_TEXT_GET_IFACE (text);

  if (start_offset < 0 || end_offset < -1 ||
      (end_offset != -1 && end_offset < start_offset))
    return NULL;

  if (iface->get_text)
    return (*(iface->get_text)) (text, start_offset, end_offset);
  else
    return NULL;
}

/**
 * atk_text_get_character_at_offset:
 * @text: an #AtkText
 * @offset: position
 *
 * Gets the specified text.
 *
 * Returns: the character at @offset.
 **/
gunichar
atk_text_get_character_at_offset (AtkText      *text,
                                  gint         offset)
{
  AtkTextIface *iface;

  g_return_val_if_fail (ATK_IS_TEXT (text), (gunichar) 0);

  iface = ATK_TEXT_GET_IFACE (text);

  if (iface->get_character_at_offset)
    return (*(iface->get_character_at_offset)) (text, offset);
  else
    return (gunichar) 0;
}

/**
 * atk_text_get_text_after_offset:
 * @text: an #AtkText
 * @offset: position
 * @boundary_type: An #AtkTextBoundary
 * @start_offset: (out): the start offset of the returned string
 * @end_offset: (out): the offset of the first character after the
 *              returned substring
 *
 * Gets the specified text.
 *
 * Deprecated: This method is deprecated since ATK version
 * 2.9.3. Please use atk_text_get_string_at_offset() instead.
 *
 * Returns: a newly allocated string containing the text after @offset bounded
 *   by the specified @boundary_type. Use g_free() to free the returned string.
 **/
gchar*
atk_text_get_text_after_offset (AtkText          *text,
                                gint             offset,
                                AtkTextBoundary  boundary_type,
 				gint             *start_offset,
				gint		 *end_offset)
{
  AtkTextIface *iface;
  gint local_start_offset, local_end_offset;
  gint *real_start_offset, *real_end_offset;

  g_return_val_if_fail (ATK_IS_TEXT (text), NULL);

  if (start_offset)
    real_start_offset = start_offset;
  else
    real_start_offset = &local_start_offset;
  if (end_offset)
    real_end_offset = end_offset;
  else
    real_end_offset = &local_end_offset;

  if (offset < 0)
    return NULL;

  iface = ATK_TEXT_GET_IFACE (text);

  if (iface->get_text_after_offset)
    return (*(iface->get_text_after_offset)) (text, offset, boundary_type, real_start_offset, real_end_offset);
  else
    return NULL;
}

/**
 * atk_text_get_text_at_offset:
 * @text: an #AtkText
 * @offset: position
 * @boundary_type: An #AtkTextBoundary
 * @start_offset: (out): the start offset of the returned string
 * @end_offset: (out): the offset of the first character after the
 *              returned substring
 *
 * Gets the specified text.
 *
 * If the boundary_type if ATK_TEXT_BOUNDARY_CHAR the character at the
 * offset is returned.
 *
 * If the boundary_type is ATK_TEXT_BOUNDARY_WORD_START the returned string
 * is from the word start at or before the offset to the word start after
 * the offset.
 *
 * The returned string will contain the word at the offset if the offset
 * is inside a word and will contain the word before the offset if the
 * offset is not inside a word.
 *
 * If the boundary type is ATK_TEXT_BOUNDARY_SENTENCE_START the returned
 * string is from the sentence start at or before the offset to the sentence
 * start after the offset.
 *
 * The returned string will contain the sentence at the offset if the offset
 * is inside a sentence and will contain the sentence before the offset
 * if the offset is not inside a sentence.
 *
 * If the boundary type is ATK_TEXT_BOUNDARY_LINE_START the returned
 * string is from the line start at or before the offset to the line
 * start after the offset.
 *
 * Deprecated: This method is deprecated since ATK version
 * 2.9.4. Please use atk_text_get_string_at_offset() instead.
 *
 * Returns: a newly allocated string containing the text at @offset bounded by
 *   the specified @boundary_type. Use g_free() to free the returned string.
 **/
gchar*
atk_text_get_text_at_offset (AtkText          *text,
                             gint             offset,
                             AtkTextBoundary  boundary_type,
			     gint             *start_offset,
			     gint             *end_offset)
{
  AtkTextIface *iface;
  gint local_start_offset, local_end_offset;
  gint *real_start_offset, *real_end_offset;

  g_return_val_if_fail (ATK_IS_TEXT (text), NULL);

  if (start_offset)
    real_start_offset = start_offset;
  else
    real_start_offset = &local_start_offset;
  if (end_offset)
    real_end_offset = end_offset;
  else
    real_end_offset = &local_end_offset;

  iface = ATK_TEXT_GET_IFACE (text);

  if (iface->get_text_at_offset)
    return (*(iface->get_text_at_offset)) (text, offset, boundary_type, real_start_offset, real_end_offset);
  else
    return NULL;
}

/**
 * atk_text_get_text_before_offset:
 * @text: an #AtkText
 * @offset: position
 * @boundary_type: An #AtkTextBoundary
 * @start_offset: (out): the start offset of the returned string
 * @end_offset: (out): the offset of the first character after the
 *              returned substring
 *
 * Gets the specified text.
 *
 * Deprecated: This method is deprecated since ATK version
 * 2.9.3. Please use atk_text_get_string_at_offset() instead.
 *
 * Returns: a newly allocated string containing the text before @offset bounded
 *   by the specified @boundary_type. Use g_free() to free the returned string.
 **/
gchar*
atk_text_get_text_before_offset (AtkText          *text,
                                 gint             offset,
                                 AtkTextBoundary  boundary_type,
				 gint             *start_offset,
				 gint		  *end_offset)
{
  AtkTextIface *iface;
  gint local_start_offset, local_end_offset;
  gint *real_start_offset, *real_end_offset;

  g_return_val_if_fail (ATK_IS_TEXT (text), NULL);

  if (start_offset)
    real_start_offset = start_offset;
  else
    real_start_offset = &local_start_offset;
  if (end_offset)
    real_end_offset = end_offset;
  else
    real_end_offset = &local_end_offset;

  if (offset < 0)
    return NULL;

  iface = ATK_TEXT_GET_IFACE (text);

  if (iface->get_text_before_offset)
    return (*(iface->get_text_before_offset)) (text, offset, boundary_type, real_start_offset, real_end_offset);
  else
    return NULL;
}

/**
 * atk_text_get_string_at_offset:
 * @text: an #AtkText
 * @offset: position
 * @granularity: An #AtkTextGranularity
 * @start_offset: (out): the start offset of the returned string, or -1
 *                if an error has occurred (e.g. invalid offset, not implemented)
 * @end_offset: (out): the offset of the first character after the returned string,
 *              or -1 if an error has occurred (e.g. invalid offset, not implemented)
 *
 * Gets a portion of the text exposed through an #AtkText according to a given @offset
 * and a specific @granularity, along with the start and end offsets defining the
 * boundaries of such a portion of text.
 *
 * If @granularity is ATK_TEXT_GRANULARITY_CHAR the character at the
 * offset is returned.
 *
 * If @granularity is ATK_TEXT_GRANULARITY_WORD the returned string
 * is from the word start at or before the offset to the word start after
 * the offset.
 *
 * The returned string will contain the word at the offset if the offset
 * is inside a word and will contain the word before the offset if the
 * offset is not inside a word.
 *
 * If @granularity is ATK_TEXT_GRANULARITY_SENTENCE the returned string
 * is from the sentence start at or before the offset to the sentence
 * start after the offset.
 *
 * The returned string will contain the sentence at the offset if the offset
 * is inside a sentence and will contain the sentence before the offset
 * if the offset is not inside a sentence.
 *
 * If @granularity is ATK_TEXT_GRANULARITY_LINE the returned string
 * is from the line start at or before the offset to the line
 * start after the offset.
 *
 * If @granularity is ATK_TEXT_GRANULARITY_PARAGRAPH the returned string
 * is from the start of the paragraph at or before the offset to the start
 * of the following paragraph after the offset.
 *
 * Since: 2.10
 *
 * Returns: (nullable): a newly allocated string containing the text
 *   at the @offset bounded by the specified @granularity. Use
 *   g_free() to free the returned string.  Returns %NULL if the
 *   offset is invalid or no implementation is available.
 **/
gchar* atk_text_get_string_at_offset (AtkText *text,
                                      gint offset,
                                      AtkTextGranularity granularity,
                                      gint *start_offset,
                                      gint *end_offset)
{
  AtkTextIface *iface;
  gint local_start_offset, local_end_offset;
  gint *real_start_offset, *real_end_offset;

  g_return_val_if_fail (ATK_IS_TEXT (text), NULL);

  if (start_offset)
    {
      *start_offset = -1;
      real_start_offset = start_offset;
    }
  else
    real_start_offset = &local_start_offset;

  if (end_offset)
    {
      *end_offset = -1;
      real_end_offset = end_offset;
    }
  else
    real_end_offset = &local_end_offset;

  if (offset < 0)
    return NULL;

  iface = ATK_TEXT_GET_IFACE (text);

  if (iface->get_string_at_offset)
    return (*(iface->get_string_at_offset)) (text, offset, granularity, real_start_offset, real_end_offset);
  else
    return NULL;
}

/**
 * atk_text_get_caret_offset:
 * @text: an #AtkText
 *
 * Gets the offset position of the caret (cursor).
 *
 * Returns: the offset position of the caret (cursor).
 **/
gint
atk_text_get_caret_offset (AtkText *text)
{
  AtkTextIface *iface;

  g_return_val_if_fail (ATK_IS_TEXT (text), 0);

  iface = ATK_TEXT_GET_IFACE (text);

  if (iface->get_caret_offset)
    return (*(iface->get_caret_offset)) (text);
  else
    return 0;
}

/**
 * atk_text_get_character_extents:
 * @text: an #AtkText
 * @offset: The offset of the text character for which bounding information is required.
 * @x: Pointer for the x cordinate of the bounding box
 * @y: Pointer for the y cordinate of the bounding box
 * @width: Pointer for the width of the bounding box
 * @height: Pointer for the height of the bounding box
 * @coords: specify whether coordinates are relative to the screen or widget window 
 *
 * Get the bounding box containing the glyph representing the character at 
 *     a particular text offset. 
 **/
void
atk_text_get_character_extents (AtkText *text,
                                gint offset,
                                gint *x,
                                gint *y,
                                gint *width,
                                gint *height,
			        AtkCoordType coords)
{
  AtkTextIface *iface;
  gint local_x, local_y, local_width, local_height;
  gint *real_x, *real_y, *real_width, *real_height;

  g_return_if_fail (ATK_IS_TEXT (text));

  if (x)
    real_x = x;
  else
    real_x = &local_x;
  if (y)
    real_y = y;
  else
    real_y = &local_y;
  if (width)
    real_width = width;
  else
    real_width = &local_width;
  if (height)
    real_height = height;
  else
    real_height = &local_height;

  *real_x = 0;
  *real_y = 0;
  *real_width = 0;
  *real_height = 0;

  if (offset < 0)
    return;
 
  iface = ATK_TEXT_GET_IFACE (text);

  if (iface->get_character_extents)
    (*(iface->get_character_extents)) (text, offset, real_x, real_y, real_width, real_height, coords);

  if (*real_width <0)
    {
      *real_x = *real_x + *real_width;
      *real_width *= -1;
    }
}

/**
 * atk_text_get_run_attributes:
 *@text: an #AtkText
 *@offset: the offset at which to get the attributes, -1 means the offset of
 *the character to be inserted at the caret location.
 *@start_offset: (out): the address to put the start offset of the range
 *@end_offset: (out): the address to put the end offset of the range
 *
 *Creates an #AtkAttributeSet which consists of the attributes explicitly
 *set at the position @offset in the text. @start_offset and @end_offset are
 *set to the start and end of the range around @offset where the attributes are
 *invariant. Note that @end_offset is the offset of the first character
 *after the range.  See the enum AtkTextAttribute for types of text 
 *attributes that can be returned. Note that other attributes may also be 
 *returned.
 *
 *Returns: (transfer full): an #AtkAttributeSet which contains the attributes
 * explicitly set at @offset. This #AtkAttributeSet should be freed by a call
 * to atk_attribute_set_free().
 **/
AtkAttributeSet* 
atk_text_get_run_attributes (AtkText          *text,
                             gint             offset,
                             gint             *start_offset,
                             gint             *end_offset)
{
  AtkTextIface *iface;
  gint local_start_offset, local_end_offset;
  gint *real_start_offset, *real_end_offset;

  g_return_val_if_fail (ATK_IS_TEXT (text), NULL);

  if (start_offset)
    real_start_offset = start_offset;
  else
    real_start_offset = &local_start_offset;
  if (end_offset)
    real_end_offset = end_offset;
  else
    real_end_offset = &local_end_offset;

  if (offset < -1)
    return NULL;

  iface = ATK_TEXT_GET_IFACE (text);

  if (iface->get_run_attributes)
    return (*(iface->get_run_attributes)) (text, offset, real_start_offset, real_end_offset);
  else
    return NULL;
}

/**
 * atk_text_get_default_attributes:
 *@text: an #AtkText
 *
 *Creates an #AtkAttributeSet which consists of the default values of
 *attributes for the text. See the enum AtkTextAttribute for types of text 
 *attributes that can be returned. Note that other attributes may also be 
 *returned.
 *
 *Returns: (transfer full): an #AtkAttributeSet which contains the default
 * values of attributes.  at @offset. this #atkattributeset should be freed by
 * a call to atk_attribute_set_free().
 */
AtkAttributeSet* 
atk_text_get_default_attributes (AtkText          *text)
{
  AtkTextIface *iface;

  g_return_val_if_fail (ATK_IS_TEXT (text), NULL);

  iface = ATK_TEXT_GET_IFACE (text);

  if (iface->get_default_attributes)
    return (*(iface->get_default_attributes)) (text);
  else
    return NULL;
}

/**
 * atk_text_get_character_count:
 * @text: an #AtkText
 *
 * Gets the character count.
 *
 * Returns: the number of characters.
 **/
gint
atk_text_get_character_count (AtkText *text)
{
  AtkTextIface *iface;

  g_return_val_if_fail (ATK_IS_TEXT (text), -1);

  iface = ATK_TEXT_GET_IFACE (text);

  if (iface->get_character_count)
    return (*(iface->get_character_count)) (text);
  else
    return -1;
}

/**
 * atk_text_get_offset_at_point:
 * @text: an #AtkText
 * @x: screen x-position of character
 * @y: screen y-position of character
 * @coords: specify whether coordinates are relative to the screen or
 * widget window 
 *
 * Gets the offset of the character located at coordinates @x and @y. @x and @y
 * are interpreted as being relative to the screen or this widget's window
 * depending on @coords.
 *
 * Returns: the offset to the character which is located at
 * the specified @x and @y coordinates.
 **/
gint
atk_text_get_offset_at_point (AtkText *text,
                              gint x,
                              gint y,
			      AtkCoordType coords)
{
  AtkTextIface *iface;

  g_return_val_if_fail (ATK_IS_TEXT (text), -1);

  iface = ATK_TEXT_GET_IFACE (text);

  if (iface->get_offset_at_point)
    return (*(iface->get_offset_at_point)) (text, x, y, coords);
  else
    return -1;
}

/**
 * atk_text_get_n_selections:
 * @text: an #AtkText
 *
 * Gets the number of selected regions.
 *
 * Returns: The number of selected regions, or -1 if a failure
 *   occurred.
 **/
gint
atk_text_get_n_selections (AtkText *text)
{
  AtkTextIface *iface;

  g_return_val_if_fail (ATK_IS_TEXT (text), -1);

  iface = ATK_TEXT_GET_IFACE (text);

  if (iface->get_n_selections)
    return (*(iface->get_n_selections)) (text);
  else
    return -1;
}

/**
 * atk_text_get_selection:
 * @text: an #AtkText
 * @selection_num: The selection number.  The selected regions are
 * assigned numbers that correspond to how far the region is from the
 * start of the text.  The selected region closest to the beginning
 * of the text region is assigned the number 0, etc.  Note that adding,
 * moving or deleting a selected region can change the numbering.
 * @start_offset: (out): passes back the start position of the selected region
 * @end_offset: (out): passes back the end position of (e.g. offset immediately past)
 * the selected region
 *
 * Gets the text from the specified selection.
 *
 * Returns: a newly allocated string containing the selected text. Use g_free()
 *   to free the returned string.
 **/
gchar*
atk_text_get_selection (AtkText *text, 
                        gint    selection_num,
                        gint    *start_offset,
                        gint    *end_offset)
{
  AtkTextIface *iface;
  gint local_start_offset, local_end_offset;
  gint *real_start_offset, *real_end_offset;

  g_return_val_if_fail (ATK_IS_TEXT (text), NULL);

  if (start_offset)
    real_start_offset = start_offset;
  else
    real_start_offset = &local_start_offset;
  if (end_offset)
    real_end_offset = end_offset;
  else
    real_end_offset = &local_end_offset;

  iface = ATK_TEXT_GET_IFACE (text);

  if (iface->get_selection)
  {
    return (*(iface->get_selection)) (text, selection_num,
       real_start_offset, real_end_offset);
  }
  else
    return NULL;
}

/**
 * atk_text_add_selection:
 * @text: an #AtkText
 * @start_offset: the start position of the selected region
 * @end_offset: the offset of the first character after the selected region.
 *
 * Adds a selection bounded by the specified offsets.
 *
 * Returns: %TRUE if success, %FALSE otherwise
 **/
gboolean
atk_text_add_selection (AtkText *text, 
                        gint    start_offset,
                        gint    end_offset)
{
  AtkTextIface *iface;

  g_return_val_if_fail (ATK_IS_TEXT (text), FALSE);

  iface = ATK_TEXT_GET_IFACE (text);

  if (iface->add_selection)
    return (*(iface->add_selection)) (text, start_offset, end_offset);
  else
    return FALSE;
}

/**
 * atk_text_remove_selection:
 * @text: an #AtkText
 * @selection_num: The selection number.  The selected regions are
 * assigned numbers that correspond to how far the region is from the
 * start of the text.  The selected region closest to the beginning
 * of the text region is assigned the number 0, etc.  Note that adding,
 * moving or deleting a selected region can change the numbering.
 *
 * Removes the specified selection.
 *
 * Returns: %TRUE if success, %FALSE otherwise
 **/
gboolean
atk_text_remove_selection (AtkText *text, 
                           gint    selection_num)
{
  AtkTextIface *iface;

  g_return_val_if_fail (ATK_IS_TEXT (text), FALSE);

  iface = ATK_TEXT_GET_IFACE (text);

  if (iface->remove_selection)
    return (*(iface->remove_selection)) (text, selection_num);
  else
    return FALSE;
}

/**
 * atk_text_set_selection:
 * @text: an #AtkText
 * @selection_num: The selection number.  The selected regions are
 * assigned numbers that correspond to how far the region is from the
 * start of the text.  The selected region closest to the beginning
 * of the text region is assigned the number 0, etc.  Note that adding,
 * moving or deleting a selected region can change the numbering.
 * @start_offset: the new start position of the selection
 * @end_offset: the new end position of (e.g. offset immediately past) 
 * the selection
 *
 * Changes the start and end offset of the specified selection.
 *
 * Returns: %TRUE if success, %FALSE otherwise
 **/
gboolean
atk_text_set_selection (AtkText *text, 
                        gint    selection_num,
                        gint    start_offset, 
                        gint    end_offset)
{
  AtkTextIface *iface;

  g_return_val_if_fail (ATK_IS_TEXT (text), FALSE);

  iface = ATK_TEXT_GET_IFACE (text);

  if (iface->set_selection)
  {
    return (*(iface->set_selection)) (text, selection_num,
       start_offset, end_offset);
  }
  else
    return FALSE;
}

/**
 * atk_text_set_caret_offset:
 * @text: an #AtkText
 * @offset: position
 *
 * Sets the caret (cursor) position to the specified @offset.
 *
 * Returns: %TRUE if success, %FALSE otherwise.
 **/
gboolean
atk_text_set_caret_offset (AtkText *text,
                           gint    offset)
{
  AtkTextIface *iface;

  g_return_val_if_fail (ATK_IS_TEXT (text), FALSE);

  iface = ATK_TEXT_GET_IFACE (text);

  if (iface->set_caret_offset)
    {
      return (*(iface->set_caret_offset)) (text, offset);
    }
  else
    {
      return FALSE;
    }
}

/**
 * atk_text_get_range_extents:
 * @text: an #AtkText
 * @start_offset: The offset of the first text character for which boundary 
 *        information is required.
 * @end_offset: The offset of the text character after the last character 
 *        for which boundary information is required.
 * @coord_type: Specify whether coordinates are relative to the screen or widget window.
 * @rect: A pointer to a AtkTextRectangle which is filled in by this function.
 *
 * Get the bounding box for text within the specified range.
 *
 * Since: 1.3
 **/
void
atk_text_get_range_extents (AtkText          *text,
                            gint             start_offset,
                            gint             end_offset,
                            AtkCoordType     coord_type,
                            AtkTextRectangle *rect)
{
  AtkTextIface *iface;

  g_return_if_fail (ATK_IS_TEXT (text));
  g_return_if_fail (rect);
  g_return_if_fail (start_offset >= 0 && start_offset < end_offset);

  iface = ATK_TEXT_GET_IFACE (text);

  if (iface->get_range_extents)
    (*(iface->get_range_extents)) (text, start_offset, end_offset, coord_type, rect);
}

/**
 * atk_text_get_bounded_ranges:
 * @text: an #AtkText
 * @rect: An AtkTextRectangle giving the dimensions of the bounding box.
 * @coord_type: Specify whether coordinates are relative to the screen or widget window.
 * @x_clip_type: Specify the horizontal clip type.
 * @y_clip_type: Specify the vertical clip type.
 *
 * Get the ranges of text in the specified bounding box.
 *
 * Since: 1.3
 *
 * Returns: (array zero-terminated=1): Array of AtkTextRange. The last
 *          element of the array returned by this function will be NULL.
 **/
AtkTextRange**
atk_text_get_bounded_ranges (AtkText          *text,
                             AtkTextRectangle *rect,
                             AtkCoordType      coord_type,
                             AtkTextClipType   x_clip_type,
                             AtkTextClipType   y_clip_type)
{
  AtkTextIface *iface;

  g_return_val_if_fail (ATK_IS_TEXT (text), NULL);
  g_return_val_if_fail (rect, NULL);

  iface = ATK_TEXT_GET_IFACE (text);

  if (iface->get_bounded_ranges)
    return (*(iface->get_bounded_ranges)) (text, rect, coord_type, x_clip_type, y_clip_type);
  else
    return NULL;
}

/**
 * atk_attribute_set_free:
 * @attrib_set: The #AtkAttributeSet to free
 *
 * Frees the memory used by an #AtkAttributeSet, including all its
 * #AtkAttributes.
 **/
void
atk_attribute_set_free (AtkAttributeSet *attrib_set)
{
  GSList *temp;

  temp = attrib_set;

  while (temp != NULL)
    {
      AtkAttribute *att;

      att = temp->data;

      g_free (att->name);
      g_free (att->value);
      g_free (att);
      temp = temp->next;
    }
  g_slist_free (attrib_set);
}

/**
 * atk_text_attribute_register:
 * @name: a name string
 *
 * Associate @name with a new #AtkTextAttribute
 *
 * Returns: an #AtkTextAttribute associated with @name
 **/
AtkTextAttribute
atk_text_attribute_register (const gchar *name)
{
  g_return_val_if_fail (name, ATK_TEXT_ATTR_INVALID);

  if (!extra_attributes)
    extra_attributes = g_ptr_array_new ();

  g_ptr_array_add (extra_attributes, g_strdup (name));
  return extra_attributes->len + ATK_TEXT_ATTR_LAST_DEFINED;
}

/**
 * atk_text_attribute_get_name:
 * @attr: The #AtkTextAttribute whose name is required
 *
 * Gets the name corresponding to the #AtkTextAttribute
 *
 * Returns: a string containing the name; this string should not be freed
 **/
const gchar*
atk_text_attribute_get_name (AtkTextAttribute attr)
{
  GTypeClass *type_class;
  GEnumValue *value;
  const gchar *name = NULL;

  type_class = g_type_class_ref (ATK_TYPE_TEXT_ATTRIBUTE);
  g_return_val_if_fail (G_IS_ENUM_CLASS (type_class), NULL);

  value = g_enum_get_value (G_ENUM_CLASS (type_class), attr);

  if (value)
    {
      name = value->value_nick;
    }
  else
    {
      if (extra_attributes)
        {
          gint n = attr;

          n -= ATK_TEXT_ATTR_LAST_DEFINED + 1;

          if (n < extra_attributes->len)

            name = g_ptr_array_index (extra_attributes, n);
        }
    }
  g_type_class_unref (type_class);
  return name;
}

/**
 * atk_text_attribute_for_name:
 * @name: a string which is the (non-localized) name of an ATK text attribute.
 *
 * Get the #AtkTextAttribute type corresponding to a text attribute name.
 *
 * Returns: the #AtkTextAttribute enumerated type corresponding to the specified
name,
 *          or #ATK_TEXT_ATTRIBUTE_INVALID if no matching text attribute is found.
 **/
AtkTextAttribute
atk_text_attribute_for_name (const gchar *name)
{
  GTypeClass *type_class;
  GEnumValue *value;
  AtkTextAttribute type = ATK_TEXT_ATTR_INVALID;

  g_return_val_if_fail (name, ATK_TEXT_ATTR_INVALID);

  type_class = g_type_class_ref (ATK_TYPE_TEXT_ATTRIBUTE);
  g_return_val_if_fail (G_IS_ENUM_CLASS (type_class), ATK_TEXT_ATTR_INVALID);

  value = g_enum_get_value_by_nick (G_ENUM_CLASS (type_class), name);

  if (value)
    {
      type = value->value;
    }
  else
    {
      gint i;

      if (extra_attributes)
        {
          for (i = 0; i < extra_attributes->len; i++)
            {
              gchar *extra_attribute = (gchar *)g_ptr_array_index (extra_attributes, i);

              g_return_val_if_fail (extra_attribute, ATK_TEXT_ATTR_INVALID);

              if (strcmp (name, extra_attribute) == 0)
                {
                  type = i + 1 + ATK_TEXT_ATTR_LAST_DEFINED;
                  break;
                }
            }
        }
    }
  g_type_class_unref (type_class);

  return type;
}

/**
 * atk_text_attribute_get_value:
 * @attr: The #AtkTextAttribute for which a value is required
 * @index_: The index of the required value
 *
 * Gets the value for the index of the #AtkTextAttribute
 *
 * Returns: (nullable): a string containing the value; this string
 * should not be freed; %NULL is returned if there are no values
 * maintained for the attr value.
 **/
const gchar*
atk_text_attribute_get_value (AtkTextAttribute attr,
                              gint             index)
{
  switch (attr)
    {
    case ATK_TEXT_ATTR_INVISIBLE:
    case ATK_TEXT_ATTR_EDITABLE:
    case ATK_TEXT_ATTR_BG_FULL_HEIGHT:
    case ATK_TEXT_ATTR_STRIKETHROUGH:
    case ATK_TEXT_ATTR_BG_STIPPLE:
    case ATK_TEXT_ATTR_FG_STIPPLE:
      g_assert (index >= 0 && index < G_N_ELEMENTS (boolean_offsets));
      return boolean + boolean_offsets[index];
    case ATK_TEXT_ATTR_UNDERLINE:
      g_assert (index >= 0 && index < G_N_ELEMENTS (underline_offsets));
      return underline + underline_offsets[index];
    case ATK_TEXT_ATTR_WRAP_MODE:
      g_assert (index >= 0 && index < G_N_ELEMENTS (wrap_mode_offsets));
      return wrap_mode + wrap_mode_offsets[index];
    case ATK_TEXT_ATTR_DIRECTION:
      g_assert (index >= 0 && index < G_N_ELEMENTS (direction_offsets));
      return direction + direction_offsets[index];
    case ATK_TEXT_ATTR_JUSTIFICATION:
      g_assert (index >= 0 && index < G_N_ELEMENTS (justification_offsets));
      return justification + justification_offsets[index];
    case ATK_TEXT_ATTR_STRETCH:
      g_assert (index >= 0 && index < G_N_ELEMENTS (stretch_offsets));
      return stretch + stretch_offsets[index];
    case ATK_TEXT_ATTR_VARIANT:
      g_assert (index >= 0 && index < G_N_ELEMENTS (variant_offsets));
      return variant + variant_offsets[index];
    case ATK_TEXT_ATTR_STYLE:
      g_assert (index >= 0 && index < G_N_ELEMENTS (style_offsets));
      return style + style_offsets[index];
    default:
      return NULL;
   }
}

static void
atk_text_rectangle_union (AtkTextRectangle *src1,
                          AtkTextRectangle *src2,
                          AtkTextRectangle *dest)
{
  gint dest_x, dest_y;

  dest_x = MIN (src1->x, src2->x);
  dest_y = MIN (src1->y, src2->y);
  dest->width = MAX (src1->x + src1->width, src2->x + src2->width) - dest_x;
  dest->height = MAX (src1->y + src1->height, src2->y + src2->height) - dest_y;
  dest->x = dest_x;
  dest->y = dest_y;
}

static gboolean
atk_text_rectangle_contain (AtkTextRectangle *clip,
                            AtkTextRectangle *bounds,
                            AtkTextClipType  x_clip_type,
                            AtkTextClipType  y_clip_type)
{
  gboolean x_min_ok, x_max_ok, y_min_ok, y_max_ok;

  x_min_ok = (bounds->x >= clip->x) ||
             ((bounds->x + bounds->width >= clip->x) &&
              ((x_clip_type == ATK_TEXT_CLIP_NONE) ||
               (x_clip_type == ATK_TEXT_CLIP_MAX)));

  x_max_ok = (bounds->x + bounds->width <= clip->x + clip->width) ||
             ((bounds->x <= clip->x + clip->width) &&
              ((x_clip_type == ATK_TEXT_CLIP_NONE) ||
               (x_clip_type == ATK_TEXT_CLIP_MIN)));

  y_min_ok = (bounds->y >= clip->y) ||
             ((bounds->y + bounds->height >= clip->y) &&
              ((y_clip_type == ATK_TEXT_CLIP_NONE) ||
               (y_clip_type == ATK_TEXT_CLIP_MAX)));

  y_max_ok = (bounds->y + bounds->height <= clip->y + clip->height) ||
             ((bounds->y <= clip->y + clip->height) &&
              ((y_clip_type == ATK_TEXT_CLIP_NONE) ||
               (y_clip_type == ATK_TEXT_CLIP_MIN)));

  return (x_min_ok && x_max_ok && y_min_ok && y_max_ok);
  
}

static void 
atk_text_real_get_range_extents (AtkText           *text,
                                 gint              start_offset,
                                 gint              end_offset,
                                 AtkCoordType      coord_type,
                                 AtkTextRectangle  *rect)
{
  gint i;
  AtkTextRectangle cbounds, bounds;

  atk_text_get_character_extents (text, start_offset,
                                  &bounds.x, &bounds.y,
                                  &bounds.width, &bounds.height,
                                  coord_type);

  for (i = start_offset + 1; i < end_offset; i++)
    {
      atk_text_get_character_extents (text, i,
                                      &cbounds.x, &cbounds.y, 
                                      &cbounds.width, &cbounds.height, 
                                      coord_type);
      atk_text_rectangle_union (&bounds, &cbounds, &bounds);
    }

  rect->x = bounds.x;
  rect->y = bounds.y;
  rect->width = bounds.width;
  rect->height = bounds.height;
}

static AtkTextRange**
atk_text_real_get_bounded_ranges (AtkText          *text,
                                  AtkTextRectangle *rect,
                                  AtkCoordType     coord_type,
                                  AtkTextClipType  x_clip_type,
                                  AtkTextClipType  y_clip_type)
{
  gint bounds_min_offset, bounds_max_offset;
  gint min_line_start, min_line_end;
  gint max_line_start, max_line_end;
  gchar *line;
  gint curr_offset;
  gint offset;
  gint num_ranges = 0;
  gint range_size = 1;
  AtkTextRectangle cbounds;
  AtkTextRange **range;

  range = NULL;
  bounds_min_offset = atk_text_get_offset_at_point (text, rect->x, rect->y, coord_type);
  bounds_max_offset = atk_text_get_offset_at_point (text, rect->x + rect->width, rect->y + rect->height, coord_type);

  if (bounds_min_offset == 0 &&
      bounds_min_offset == bounds_max_offset)
    return NULL;

  line = atk_text_get_text_at_offset (text, bounds_min_offset, 
                                      ATK_TEXT_BOUNDARY_LINE_START,
                                      &min_line_start, &min_line_end);
  g_free (line);
  line = atk_text_get_text_at_offset (text, bounds_max_offset, 
                                      ATK_TEXT_BOUNDARY_LINE_START,
                                      &max_line_start, &max_line_end);
  g_free (line);
  bounds_min_offset = MIN (min_line_start, max_line_start);
  bounds_max_offset = MAX (min_line_end, max_line_end);

  curr_offset = bounds_min_offset;
  while (curr_offset < bounds_max_offset)
    {
      offset = curr_offset;

      while (curr_offset < bounds_max_offset)
        {
          atk_text_get_character_extents (text, curr_offset,
                                          &cbounds.x, &cbounds.y,
                                          &cbounds.width, &cbounds.height,
                                          coord_type);
          if (!atk_text_rectangle_contain (rect, &cbounds, x_clip_type, y_clip_type))
	    break;
          curr_offset++;
        }
      if (curr_offset > offset)
        {
          AtkTextRange *one_range = g_new (AtkTextRange, 1);

          one_range->start_offset = offset;
          one_range->end_offset = curr_offset;
          one_range->content = atk_text_get_text (text, offset, curr_offset);
          atk_text_get_range_extents (text, offset, curr_offset, coord_type, &one_range->bounds);

          if (num_ranges >= range_size - 1)
            {
              range_size *= 2;
              range = g_realloc (range, range_size * sizeof (gpointer));
            }
          range[num_ranges] = one_range;
          num_ranges++; 
        }   
      curr_offset++;
      if (range)
        range[num_ranges] = NULL;
    }
  return range;
}

/**
 * atk_text_free_ranges:
 * @ranges: (array): A pointer to an array of #AtkTextRange which is
 *   to be freed.
 *
 * Frees the memory associated with an array of AtkTextRange. It is assumed
 * that the array was returned by the function atk_text_get_bounded_ranges
 * and is NULL terminated.
 *
 * Since: 1.3
 **/
void
atk_text_free_ranges (AtkTextRange **ranges)
{
  AtkTextRange **first = ranges;

  if (ranges)
    {
      while (*ranges)
        {
          AtkTextRange *range;

          range = *ranges;
          ranges++;
          g_free (range->content);
          g_free (range);
        }
      g_free (first);
    }
}

static AtkTextRange *
atk_text_range_copy (AtkTextRange *src)
{
  AtkTextRange *dst = g_new0 (AtkTextRange, 1);
  dst->bounds = src->bounds;
  dst->start_offset = src->start_offset;
  dst->end_offset = src->end_offset;
  if (src->content)
    dst->content = g_strdup (src->content);
  return dst;
}

static void
atk_text_range_free (AtkTextRange *range)
{
  g_free (range->content);
  g_free (range);
}

G_DEFINE_BOXED_TYPE (AtkTextRange, atk_text_range, atk_text_range_copy,
                     atk_text_range_free)
