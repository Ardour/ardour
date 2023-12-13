/* GTK - The GIMP Toolkit
 * gtktextbtree.h Copyright (C) 2000 Red Hat, Inc.
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

#ifndef __GTK_TEXT_BTREE_H__
#define __GTK_TEXT_BTREE_H__

#if 0
#define DEBUG_VALIDATION_AND_SCROLLING
#endif

#ifdef DEBUG_VALIDATION_AND_SCROLLING
#define DV(x) (x)
#else
#define DV(x)
#endif

#include <gtk/gtktextbuffer.h>
#include <gtk/gtktexttag.h>
#include <gtk/gtktextmark.h>
#include <gtk/gtktextchild.h>
#include <gtk/gtktextsegment.h>
#include <gtk/gtktextiter.h>

G_BEGIN_DECLS

GtkTextBTree  *_gtk_text_btree_new        (GtkTextTagTable *table,
                                           GtkTextBuffer   *buffer);
void           _gtk_text_btree_ref        (GtkTextBTree    *tree);
void           _gtk_text_btree_unref      (GtkTextBTree    *tree);
GtkTextBuffer *_gtk_text_btree_get_buffer (GtkTextBTree    *tree);


guint _gtk_text_btree_get_chars_changed_stamp    (GtkTextBTree *tree);
guint _gtk_text_btree_get_segments_changed_stamp (GtkTextBTree *tree);
void  _gtk_text_btree_segments_changed           (GtkTextBTree *tree);

gboolean _gtk_text_btree_is_end (GtkTextBTree       *tree,
                                 GtkTextLine        *line,
                                 GtkTextLineSegment *seg,
                                 int                 byte_index,
                                 int                 char_offset);

/* Indexable segment mutation */

void _gtk_text_btree_delete        (GtkTextIter *start,
                                    GtkTextIter *end);
void _gtk_text_btree_insert        (GtkTextIter *iter,
                                    const gchar *text,
                                    gint         len);
void _gtk_text_btree_insert_pixbuf (GtkTextIter *iter,
                                    GdkPixbuf   *pixbuf);

void _gtk_text_btree_insert_child_anchor (GtkTextIter        *iter,
                                          GtkTextChildAnchor *anchor);

void _gtk_text_btree_unregister_child_anchor (GtkTextChildAnchor *anchor);

/* View stuff */
GtkTextLine *_gtk_text_btree_find_line_by_y    (GtkTextBTree      *tree,
                                                gpointer           view_id,
                                                gint               ypixel,
                                                gint              *line_top_y);
gint         _gtk_text_btree_find_line_top     (GtkTextBTree      *tree,
                                                GtkTextLine       *line,
                                                gpointer           view_id);
void         _gtk_text_btree_add_view          (GtkTextBTree      *tree,
                                                GtkTextLayout     *layout);
void         _gtk_text_btree_remove_view       (GtkTextBTree      *tree,
                                                gpointer           view_id);
void         _gtk_text_btree_invalidate_region (GtkTextBTree      *tree,
                                                const GtkTextIter *start,
                                                const GtkTextIter *end,
                                                gboolean           cursors_only);
void         _gtk_text_btree_get_view_size     (GtkTextBTree      *tree,
                                                gpointer           view_id,
                                                gint              *width,
                                                gint              *height);
gboolean     _gtk_text_btree_is_valid          (GtkTextBTree      *tree,
                                                gpointer           view_id);
gboolean     _gtk_text_btree_validate          (GtkTextBTree      *tree,
                                                gpointer           view_id,
                                                gint               max_pixels,
                                                gint              *y,
                                                gint              *old_height,
                                                gint              *new_height);
void         _gtk_text_btree_validate_line     (GtkTextBTree      *tree,
                                                GtkTextLine       *line,
                                                gpointer           view_id);

/* Tag */

void _gtk_text_btree_tag (const GtkTextIter *start,
                          const GtkTextIter *end,
                          GtkTextTag        *tag,
                          gboolean           apply);

/* "Getters" */

GtkTextLine * _gtk_text_btree_get_line          (GtkTextBTree      *tree,
                                                 gint               line_number,
                                                 gint              *real_line_number);
GtkTextLine * _gtk_text_btree_get_line_no_last  (GtkTextBTree      *tree,
                                                 gint               line_number,
                                                 gint              *real_line_number);
GtkTextLine * _gtk_text_btree_get_end_iter_line (GtkTextBTree      *tree);
GtkTextLine * _gtk_text_btree_get_line_at_char  (GtkTextBTree      *tree,
                                                 gint               char_index,
                                                 gint              *line_start_index,
                                                 gint              *real_char_index);
GtkTextTag**  _gtk_text_btree_get_tags          (const GtkTextIter *iter,
                                                 gint              *num_tags);
gchar        *_gtk_text_btree_get_text          (const GtkTextIter *start,
                                                 const GtkTextIter *end,
                                                 gboolean           include_hidden,
                                                 gboolean           include_nonchars);
gint          _gtk_text_btree_line_count        (GtkTextBTree      *tree);
gint          _gtk_text_btree_char_count        (GtkTextBTree      *tree);
gboolean      _gtk_text_btree_char_is_invisible (const GtkTextIter *iter);



/* Get iterators (these are implemented in gtktextiter.c) */
void     _gtk_text_btree_get_iter_at_char         (GtkTextBTree       *tree,
                                                   GtkTextIter        *iter,
                                                   gint                char_index);
void     _gtk_text_btree_get_iter_at_line_char    (GtkTextBTree       *tree,
                                                   GtkTextIter        *iter,
                                                   gint                line_number,
                                                   gint                char_index);
void     _gtk_text_btree_get_iter_at_line_byte    (GtkTextBTree       *tree,
                                                   GtkTextIter        *iter,
                                                   gint                line_number,
                                                   gint                byte_index);
gboolean _gtk_text_btree_get_iter_from_string     (GtkTextBTree       *tree,
                                                   GtkTextIter        *iter,
                                                   const gchar        *string);
gboolean _gtk_text_btree_get_iter_at_mark_name    (GtkTextBTree       *tree,
                                                   GtkTextIter        *iter,
                                                   const gchar        *mark_name);
void     _gtk_text_btree_get_iter_at_mark         (GtkTextBTree       *tree,
                                                   GtkTextIter        *iter,
                                                   GtkTextMark        *mark);
void     _gtk_text_btree_get_end_iter             (GtkTextBTree       *tree,
                                                   GtkTextIter        *iter);
void     _gtk_text_btree_get_iter_at_line         (GtkTextBTree       *tree,
                                                   GtkTextIter        *iter,
                                                   GtkTextLine        *line,
                                                   gint                byte_offset);
gboolean _gtk_text_btree_get_iter_at_first_toggle (GtkTextBTree       *tree,
                                                   GtkTextIter        *iter,
                                                   GtkTextTag         *tag);
gboolean _gtk_text_btree_get_iter_at_last_toggle  (GtkTextBTree       *tree,
                                                   GtkTextIter        *iter,
                                                   GtkTextTag         *tag);

void     _gtk_text_btree_get_iter_at_child_anchor  (GtkTextBTree       *tree,
                                                    GtkTextIter        *iter,
                                                    GtkTextChildAnchor *anchor);



/* Manipulate marks */
GtkTextMark        *_gtk_text_btree_set_mark                (GtkTextBTree       *tree,
                                                             GtkTextMark         *existing_mark,
                                                             const gchar        *name,
                                                             gboolean            left_gravity,
                                                             const GtkTextIter  *index,
                                                             gboolean           should_exist);
void                _gtk_text_btree_remove_mark_by_name     (GtkTextBTree       *tree,
                                                             const gchar        *name);
void                _gtk_text_btree_remove_mark             (GtkTextBTree       *tree,
                                                             GtkTextMark        *segment);
gboolean            _gtk_text_btree_get_selection_bounds    (GtkTextBTree       *tree,
                                                             GtkTextIter        *start,
                                                             GtkTextIter        *end);
void                _gtk_text_btree_place_cursor            (GtkTextBTree       *tree,
                                                             const GtkTextIter  *where);
void                _gtk_text_btree_select_range            (GtkTextBTree       *tree,
                                                             const GtkTextIter  *ins,
							     const GtkTextIter  *bound);
gboolean            _gtk_text_btree_mark_is_insert          (GtkTextBTree       *tree,
                                                             GtkTextMark        *segment);
gboolean            _gtk_text_btree_mark_is_selection_bound (GtkTextBTree       *tree,
                                                             GtkTextMark        *segment);
GtkTextMark        *_gtk_text_btree_get_insert		    (GtkTextBTree       *tree);
GtkTextMark        *_gtk_text_btree_get_selection_bound	    (GtkTextBTree       *tree);
GtkTextMark        *_gtk_text_btree_get_mark_by_name        (GtkTextBTree       *tree,
                                                             const gchar        *name);
GtkTextLine *       _gtk_text_btree_first_could_contain_tag (GtkTextBTree       *tree,
                                                             GtkTextTag         *tag);
GtkTextLine *       _gtk_text_btree_last_could_contain_tag  (GtkTextBTree       *tree,
                                                             GtkTextTag         *tag);

/* Lines */

/* Chunk of data associated with a line; views can use this to store
   info at the line. They should "subclass" the header struct here. */
struct _GtkTextLineData {
  gpointer view_id;
  GtkTextLineData *next;
  gint height;
  signed int width : 24;
  guint valid : 8;		/* Actually a boolean */
};

/*
 * The data structure below defines a single line of text (from newline
 * to newline, not necessarily what appears on one line of the screen).
 *
 * You can consider this line a "paragraph" also
 */

struct _GtkTextLine {
  GtkTextBTreeNode *parent;             /* Pointer to parent node containing
                                         * line. */
  GtkTextLine *next;            /* Next in linked list of lines with
                                 * same parent node in B-tree.  NULL
                                 * means end of list. */
  GtkTextLineSegment *segments; /* First in ordered list of segments
                                 * that make up the line. */
  GtkTextLineData *views;      /* data stored here by views */
  guchar dir_strong;                /* BiDi algo dir of line */
  guchar dir_propagated_back;       /* BiDi algo dir of next line */
  guchar dir_propagated_forward;    /* BiDi algo dir of prev line */
};


gint                _gtk_text_line_get_number                 (GtkTextLine         *line);
gboolean            _gtk_text_line_char_has_tag               (GtkTextLine         *line,
                                                               GtkTextBTree        *tree,
                                                               gint                 char_in_line,
                                                               GtkTextTag          *tag);
gboolean            _gtk_text_line_byte_has_tag               (GtkTextLine         *line,
                                                               GtkTextBTree        *tree,
                                                               gint                 byte_in_line,
                                                               GtkTextTag          *tag);
gboolean            _gtk_text_line_is_last                    (GtkTextLine         *line,
                                                               GtkTextBTree        *tree);
gboolean            _gtk_text_line_contains_end_iter          (GtkTextLine         *line,
                                                               GtkTextBTree        *tree);
GtkTextLine *       _gtk_text_line_next                       (GtkTextLine         *line);
GtkTextLine *       _gtk_text_line_next_excluding_last        (GtkTextLine         *line);
GtkTextLine *       _gtk_text_line_previous                   (GtkTextLine         *line);
void                _gtk_text_line_add_data                   (GtkTextLine         *line,
                                                               GtkTextLineData     *data);
gpointer            _gtk_text_line_remove_data                (GtkTextLine         *line,
                                                               gpointer             view_id);
gpointer            _gtk_text_line_get_data                   (GtkTextLine         *line,
                                                               gpointer             view_id);
void                _gtk_text_line_invalidate_wrap            (GtkTextLine         *line,
                                                               GtkTextLineData     *ld);
gint                _gtk_text_line_char_count                 (GtkTextLine         *line);
gint                _gtk_text_line_byte_count                 (GtkTextLine         *line);
gint                _gtk_text_line_char_index                 (GtkTextLine         *line);
GtkTextLineSegment *_gtk_text_line_byte_to_segment            (GtkTextLine         *line,
                                                               gint                 byte_offset,
                                                               gint                *seg_offset);
GtkTextLineSegment *_gtk_text_line_char_to_segment            (GtkTextLine         *line,
                                                               gint                 char_offset,
                                                               gint                *seg_offset);
gboolean            _gtk_text_line_byte_locate                (GtkTextLine         *line,
                                                               gint                 byte_offset,
                                                               GtkTextLineSegment **segment,
                                                               GtkTextLineSegment **any_segment,
                                                               gint                *seg_byte_offset,
                                                               gint                *line_byte_offset);
gboolean            _gtk_text_line_char_locate                (GtkTextLine         *line,
                                                               gint                 char_offset,
                                                               GtkTextLineSegment **segment,
                                                               GtkTextLineSegment **any_segment,
                                                               gint                *seg_char_offset,
                                                               gint                *line_char_offset);
void                _gtk_text_line_byte_to_char_offsets       (GtkTextLine         *line,
                                                               gint                 byte_offset,
                                                               gint                *line_char_offset,
                                                               gint                *seg_char_offset);
void                _gtk_text_line_char_to_byte_offsets       (GtkTextLine         *line,
                                                               gint                 char_offset,
                                                               gint                *line_byte_offset,
                                                               gint                *seg_byte_offset);
GtkTextLineSegment *_gtk_text_line_byte_to_any_segment        (GtkTextLine         *line,
                                                               gint                 byte_offset,
                                                               gint                *seg_offset);
GtkTextLineSegment *_gtk_text_line_char_to_any_segment        (GtkTextLine         *line,
                                                               gint                 char_offset,
                                                               gint                *seg_offset);
gint                _gtk_text_line_byte_to_char               (GtkTextLine         *line,
                                                               gint                 byte_offset);
gint                _gtk_text_line_char_to_byte               (GtkTextLine         *line,
                                                               gint                 char_offset);
GtkTextLine    *    _gtk_text_line_next_could_contain_tag     (GtkTextLine         *line,
                                                               GtkTextBTree        *tree,
                                                               GtkTextTag          *tag);
GtkTextLine    *    _gtk_text_line_previous_could_contain_tag (GtkTextLine         *line,
                                                               GtkTextBTree        *tree,
                                                               GtkTextTag          *tag);

GtkTextLineData    *_gtk_text_line_data_new                   (GtkTextLayout     *layout,
                                                               GtkTextLine       *line);

/* Debug */
void _gtk_text_btree_check (GtkTextBTree *tree);
void _gtk_text_btree_spew (GtkTextBTree *tree);
extern gboolean _gtk_text_view_debug_btree;

/* ignore, exported only for gtktextsegment.c */
void _gtk_toggle_segment_check_func (GtkTextLineSegment *segPtr,
                                     GtkTextLine        *line);
void _gtk_change_node_toggle_count  (GtkTextBTreeNode   *node,
                                     GtkTextTagInfo     *info,
                                     gint                delta);

/* for gtktextmark.c */
void _gtk_text_btree_release_mark_segment (GtkTextBTree       *tree,
                                           GtkTextLineSegment *segment);

/* for coordination with the tag table */
void _gtk_text_btree_notify_will_remove_tag (GtkTextBTree *tree,
                                             GtkTextTag   *tag);

G_END_DECLS

#endif


