/* GTK - The GIMP Toolkit
 * gtktextsegment.h Copyright (C) 2000 Red Hat, Inc.
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

#ifndef __GTK_TEXT_SEGMENT_H__
#define __GTK_TEXT_SEGMENT_H__

#include <gtk/gtktexttag.h>
#include <gtk/gtktextiter.h>
#include <gtk/gtktextmarkprivate.h>
#include <gtk/gtktextchild.h>
#include <gtk/gtktextchildprivate.h>

G_BEGIN_DECLS

/*
 * Segments: each line is divided into one or more segments, where each
 * segment is one of several things, such as a group of characters, a
 * tag toggle, a mark, or an embedded widget.  Each segment starts with
 * a standard header followed by a body that varies from type to type.
 */

/* This header has the segment type, and two specific segments
   (character and toggle segments) */

/* Information a BTree stores about a tag. */
typedef struct _GtkTextTagInfo GtkTextTagInfo;
struct _GtkTextTagInfo {
  GtkTextTag *tag;
  GtkTextBTreeNode *tag_root; /* highest-level node containing the tag */
  gint toggle_count;      /* total toggles of this tag below tag_root */
};

/* Body of a segment that toggles a tag on or off */
struct _GtkTextToggleBody {
  GtkTextTagInfo *info;             /* Tag that starts or ends here. */
  gboolean inNodeCounts;             /* TRUE means this toggle has been
                                      * accounted for in node toggle
                                      * counts; FALSE means it hasn't, yet. */
};


/* Class struct for segments */

/* Split seg at index, returning list of two new segments, and freeing seg */
typedef GtkTextLineSegment* (*GtkTextSegSplitFunc)      (GtkTextLineSegment *seg,
                                                         gint                index);

/* Delete seg which is contained in line; if tree_gone, the tree is being
 * freed in its entirety, which may matter for some reason (?)
 * Return TRUE if the segment is not deleteable, e.g. a mark.
 */
typedef gboolean            (*GtkTextSegDeleteFunc)     (GtkTextLineSegment *seg,
                                                         GtkTextLine        *line,
                                                         gboolean            tree_gone);

/* Called after segment structure of line changes, so segments can
 * cleanup (e.g. merge with adjacent segments). Returns a segment list
 * to replace the original segment list with. The line argument is
 * the current line.
 */
typedef GtkTextLineSegment* (*GtkTextSegCleanupFunc)    (GtkTextLineSegment *seg,
                                                         GtkTextLine        *line);

/* Called when a segment moves from one line to another. CleanupFunc is also
 * called in that case, so many segments just use CleanupFunc, I'm not sure
 * what's up with that (this function may not be needed...)
 */
typedef void                (*GtkTextSegLineChangeFunc) (GtkTextLineSegment *seg,
                                                         GtkTextLine        *line);

/* Called to do debug checks on the segment. */
typedef void                (*GtkTextSegCheckFunc)      (GtkTextLineSegment *seg,
                                                         GtkTextLine        *line);

struct _GtkTextLineSegmentClass {
  char *name;                           /* Name of this kind of segment. */
  gboolean leftGravity;                 /* If a segment has zero size (e.g. a
                                         * mark or tag toggle), does it
                                         * attach to character to its left
                                         * or right?  1 means left, 0 means
                                         * right. */
  GtkTextSegSplitFunc splitFunc;        /* Procedure to split large segment
                                         * into two smaller ones. */
  GtkTextSegDeleteFunc deleteFunc;      /* Procedure to call to delete
                                         * segment. */
  GtkTextSegCleanupFunc cleanupFunc;   /* After any change to a line, this
                                        * procedure is invoked for all
                                        * segments left in the line to
                                        * perform any cleanup they wish
                                        * (e.g. joining neighboring
                                        * segments). */
  GtkTextSegLineChangeFunc lineChangeFunc;
  /* Invoked when a segment is about
   * to be moved from its current line
   * to an earlier line because of
   * a deletion.  The line is that
   * for the segment's old line.
   * CleanupFunc will be invoked after
   * the deletion is finished. */

  GtkTextSegCheckFunc checkFunc;       /* Called during consistency checks
                                        * to check internal consistency of
                                        * segment. */
};

/*
 * The data structure below defines line segments.
 */

struct _GtkTextLineSegment {
  const GtkTextLineSegmentClass *type;  /* Pointer to record describing
                                         * segment's type. */
  GtkTextLineSegment *next;             /* Next in list of segments for this
                                         * line, or NULL for end of list. */

  int char_count;                       /* # of chars of index space occupied */

  int byte_count;                       /* Size of this segment (# of bytes
                                         * of index space it occupies). */
  union {
    char chars[4];                      /* Characters that make up character
                                         * info.  Actual length varies to
                                         * hold as many characters as needed.*/
    GtkTextToggleBody toggle;              /* Information about tag toggle. */
    GtkTextMarkBody mark;              /* Information about mark. */
    GtkTextPixbuf pixbuf;              /* Child pixbuf */
    GtkTextChildBody child;            /* Child widget */
  } body;
};


GtkTextLineSegment  *gtk_text_line_segment_split (const GtkTextIter *iter);

GtkTextLineSegment *_gtk_char_segment_new                  (const gchar    *text,
                                                            guint           len);
GtkTextLineSegment *_gtk_char_segment_new_from_two_strings (const gchar    *text1,
                                                            guint           len1,
							    guint           chars1,
                                                            const gchar    *text2,
                                                            guint           len2,
							    guint           chars2);
GtkTextLineSegment *_gtk_toggle_segment_new                (GtkTextTagInfo *info,
                                                            gboolean        on);


G_END_DECLS

#endif


