/*
 * gtktextsegment.c --
 *
 * Code for segments in general, and toggle/char segments in particular.
 *
 * Copyright (c) 1992-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 * Copyright (c) 2000      Red Hat, Inc.
 * Tk -> Gtk port by Havoc Pennington <hp@redhat.com>
 *
 * This software is copyrighted by the Regents of the University of
 * California, Sun Microsystems, Inc., and other parties.  The
 * following terms apply to all files associated with the software
 * unless explicitly disclaimed in individual files.
 *
 * The authors hereby grant permission to use, copy, modify,
 * distribute, and license this software and its documentation for any
 * purpose, provided that existing copyright notices are retained in
 * all copies and that this notice is included verbatim in any
 * distributions. No written agreement, license, or royalty fee is
 * required for any of the authorized uses.  Modifications to this
 * software may be copyrighted by their authors and need not follow
 * the licensing terms described here, provided that the new terms are
 * clearly indicated on the first page of each file where they apply.
 *
 * IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY
 * PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
 * DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION,
 * OR ANY DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND
 * NON-INFRINGEMENT.  THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS,
 * AND THE AUTHORS AND DISTRIBUTORS HAVE NO OBLIGATION TO PROVIDE
 * MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * GOVERNMENT USE: If you are acquiring this software on behalf of the
 * U.S. government, the Government shall have only "Restricted Rights"
 * in the software and related documentation as defined in the Federal
 * Acquisition Regulations (FARs) in Clause 52.227.19 (c) (2).  If you
 * are acquiring the software on behalf of the Department of Defense,
 * the software shall be classified as "Commercial Computer Software"
 * and the Government shall have only "Restricted Rights" as defined
 * in Clause 252.227-7013 (c) (1) of DFARs.  Notwithstanding the
 * foregoing, the authors grant the U.S. Government and others acting
 * in its behalf permission to use and distribute the software in
 * accordance with the terms specified in this license.
 *
 */

#define GTK_TEXT_USE_INTERNAL_UNSUPPORTED_API
#include "config.h"
#include "gtktextbtree.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "gtktexttag.h"
#include "gtktexttagtable.h"
#include "gtktextlayout.h"
#include "gtktextiterprivate.h"
#include "gtkdebug.h"
#include "gtkalias.h"

/*
 *--------------------------------------------------------------
 *
 * split_segment --
 *
 *      This procedure is called before adding or deleting
 *      segments.  It does three things: (a) it finds the segment
 *      containing iter;  (b) if there are several such
 *      segments (because some segments have zero length) then
 *      it picks the first segment that does not have left
 *      gravity;  (c) if the index refers to the middle of
 *      a segment then it splits the segment so that the
 *      index now refers to the beginning of a segment.
 *
 * Results:
 *      The return value is a pointer to the segment just
 *      before the segment corresponding to iter (as
 *      described above).  If the segment corresponding to
 *      iter is the first in its line then the return
 *      value is NULL.
 *
 * Side effects:
 *      The segment referred to by iter is split unless
 *      iter refers to its first character.
 *
 *--------------------------------------------------------------
 */

GtkTextLineSegment*
gtk_text_line_segment_split (const GtkTextIter *iter)
{
  GtkTextLineSegment *prev, *seg;
  GtkTextBTree *tree;
  GtkTextLine *line;
  int count;

  line = _gtk_text_iter_get_text_line (iter);
  tree = _gtk_text_iter_get_btree (iter);

  count = gtk_text_iter_get_line_index (iter);

  if (gtk_debug_flags & GTK_DEBUG_TEXT)
    _gtk_text_iter_check (iter);
  
  prev = NULL;
  seg = line->segments;

  while (seg != NULL)
    {
      if (seg->byte_count > count)
        {
          if (count == 0)
            {
              return prev;
            }
          else
            {
              g_assert (count != seg->byte_count);
              g_assert (seg->byte_count > 0);

              _gtk_text_btree_segments_changed (tree);

              seg = (*seg->type->splitFunc)(seg, count);

              if (prev == NULL)
                line->segments = seg;
              else
                prev->next = seg;

              return seg;
            }
        }
      else if ((seg->byte_count == 0) && (count == 0)
               && !seg->type->leftGravity)
        {
          return prev;
        }

      count -= seg->byte_count;
      prev = seg;
      seg = seg->next;
    }
  g_error ("split_segment reached end of line!");
  return NULL;
}


/*
 * Macros that determine how much space to allocate for new segments:
 */

#define CSEG_SIZE(chars) ((unsigned) (G_STRUCT_OFFSET (GtkTextLineSegment, body) \
        + 1 + (chars)))
#define TSEG_SIZE ((unsigned) (G_STRUCT_OFFSET (GtkTextLineSegment, body) \
        + sizeof (GtkTextToggleBody)))

/*
 * Type functions
 */

static void
char_segment_self_check (GtkTextLineSegment *seg)
{
  /* This function checks the segment itself, but doesn't
     assume the segment has been validly inserted into
     the btree. */

  g_assert (seg != NULL);

  if (seg->byte_count <= 0)
    {
      g_error ("segment has size <= 0");
    }

  if (strlen (seg->body.chars) != seg->byte_count)
    {
      g_error ("segment has wrong size");
    }

  if (g_utf8_strlen (seg->body.chars, seg->byte_count) != seg->char_count)
    {
      g_error ("char segment has wrong character count");
    }
}

GtkTextLineSegment*
_gtk_char_segment_new (const gchar *text, guint len)
{
  GtkTextLineSegment *seg;

  g_assert (gtk_text_byte_begins_utf8_char (text));

  seg = g_malloc (CSEG_SIZE (len));
  seg->type = (GtkTextLineSegmentClass *)&gtk_text_char_type;
  seg->next = NULL;
  seg->byte_count = len;
  memcpy (seg->body.chars, text, len);
  seg->body.chars[len] = '\0';

  seg->char_count = g_utf8_strlen (seg->body.chars, seg->byte_count);

  if (gtk_debug_flags & GTK_DEBUG_TEXT)
    char_segment_self_check (seg);

  return seg;
}

GtkTextLineSegment*
_gtk_char_segment_new_from_two_strings (const gchar *text1, 
					guint        len1, 
					guint        chars1,
                                        const gchar *text2, 
					guint        len2, 
					guint        chars2)
{
  GtkTextLineSegment *seg;

  g_assert (gtk_text_byte_begins_utf8_char (text1));
  g_assert (gtk_text_byte_begins_utf8_char (text2));

  seg = g_malloc (CSEG_SIZE (len1+len2));
  seg->type = &gtk_text_char_type;
  seg->next = NULL;
  seg->byte_count = len1 + len2;
  memcpy (seg->body.chars, text1, len1);
  memcpy (seg->body.chars + len1, text2, len2);
  seg->body.chars[len1+len2] = '\0';

  seg->char_count = chars1 + chars2;

  if (gtk_debug_flags & GTK_DEBUG_TEXT)
    char_segment_self_check (seg);

  return seg;
}

/*
 *--------------------------------------------------------------
 *
 * char_segment_split_func --
 *
 *      This procedure implements splitting for character segments.
 *
 * Results:
 *      The return value is a pointer to a chain of two segments
 *      that have the same characters as segPtr except split
 *      among the two segments.
 *
 * Side effects:
 *      Storage for segPtr is freed.
 *
 *--------------------------------------------------------------
 */

static GtkTextLineSegment *
char_segment_split_func (GtkTextLineSegment *seg, int index)
{
  GtkTextLineSegment *new1, *new2;

  g_assert (index < seg->byte_count);

  if (gtk_debug_flags & GTK_DEBUG_TEXT)
    {
      char_segment_self_check (seg);
    }

  new1 = _gtk_char_segment_new (seg->body.chars, index);
  new2 = _gtk_char_segment_new (seg->body.chars + index, seg->byte_count - index);

  g_assert (gtk_text_byte_begins_utf8_char (new1->body.chars));
  g_assert (gtk_text_byte_begins_utf8_char (new2->body.chars));
  g_assert (new1->byte_count + new2->byte_count == seg->byte_count);
  g_assert (new1->char_count + new2->char_count == seg->char_count);

  new1->next = new2;
  new2->next = seg->next;

  if (gtk_debug_flags & GTK_DEBUG_TEXT)
    {
      char_segment_self_check (new1);
      char_segment_self_check (new2);
    }

  g_free (seg);
  return new1;
}

/*
 *--------------------------------------------------------------
 *
 * char_segment_cleanup_func --
 *
 *      This procedure merges adjacent character segments into
 *      a single character segment, if possible.
 *
 * Arguments:
 *      segPtr: Pointer to the first of two adjacent segments to
 *              join.
 *      line:   Line containing segments (not used).
 *
 * Results:
 *      The return value is a pointer to the first segment in
 *      the (new) list of segments that used to start with segPtr.
 *
 * Side effects:
 *      Storage for the segments may be allocated and freed.
 *
 *--------------------------------------------------------------
 */

        /* ARGSUSED */
static GtkTextLineSegment *
char_segment_cleanup_func (GtkTextLineSegment *segPtr, GtkTextLine *line)
{
  GtkTextLineSegment *segPtr2, *newPtr;

  if (gtk_debug_flags & GTK_DEBUG_TEXT)
    char_segment_self_check (segPtr);

  segPtr2 = segPtr->next;
  if ((segPtr2 == NULL) || (segPtr2->type != &gtk_text_char_type))
    {
      return segPtr;
    }

  newPtr =
    _gtk_char_segment_new_from_two_strings (segPtr->body.chars, 
					    segPtr->byte_count,
					    segPtr->char_count,
                                            segPtr2->body.chars, 
					    segPtr2->byte_count,
					    segPtr2->char_count);

  newPtr->next = segPtr2->next;

  if (gtk_debug_flags & GTK_DEBUG_TEXT)
    char_segment_self_check (newPtr);

  g_free (segPtr);
  g_free (segPtr2);
  return newPtr;
}

/*
 *--------------------------------------------------------------
 *
 * char_segment_delete_func --
 *
 *      This procedure is invoked to delete a character segment.
 *
 * Arguments:
 *      segPtr   : Segment to delete
 *      line     : Line containing segment
 *      treeGone : Non-zero means the entire tree is being
 *                 deleted, so everything must get cleaned up.
 *
 * Results:
 *      Always returns 0 to indicate that the segment was deleted.
 *
 * Side effects:
 *      Storage for the segment is freed.
 *
 *--------------------------------------------------------------
 */

        /* ARGSUSED */
static int
char_segment_delete_func (GtkTextLineSegment *segPtr, GtkTextLine *line, int treeGone)
{
  g_free ((char*) segPtr);
  return 0;
}

/*
 *--------------------------------------------------------------
 *
 * char_segment_check_func --
 *
 *      This procedure is invoked to perform consistency checks
 *      on character segments.
 *
 * Arguments:
 *      segPtr : Segment to check
 *      line   : Line containing segment
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      If the segment isn't inconsistent then the procedure
 *      g_errors.
 *
 *--------------------------------------------------------------
 */

        /* ARGSUSED */
static void
char_segment_check_func (GtkTextLineSegment *segPtr, GtkTextLine *line)
{
  char_segment_self_check (segPtr);

  if (segPtr->next != NULL)
    {
      if (segPtr->next->type == &gtk_text_char_type)
        {
          g_error ("adjacent character segments weren't merged");
        }
    }
}

GtkTextLineSegment*
_gtk_toggle_segment_new (GtkTextTagInfo *info, gboolean on)
{
  GtkTextLineSegment *seg;

  seg = g_malloc (TSEG_SIZE);

  seg->type = on ? &gtk_text_toggle_on_type : &gtk_text_toggle_off_type;

  seg->next = NULL;

  seg->byte_count = 0;
  seg->char_count = 0;

  seg->body.toggle.info = info;
  seg->body.toggle.inNodeCounts = 0;

  return seg;
}

/*
 *--------------------------------------------------------------
 *
 * toggle_segment_delete_func --
 *
 *      This procedure is invoked to delete toggle segments.
 *
 * Arguments:
 *      segPtr   : Segment to check
 *      line     : Line containing segment
 *      treeGone : Non-zero means the entire tree is being
 *                 deleted so everything must get cleaned up
 *
 * Results:
 *      Returns 1 to indicate that the segment may not be deleted,
 *      unless the entire B-tree is going away.
 *
 * Side effects:
 *      If the tree is going away then the toggle's memory is
 *      freed;  otherwise the toggle counts in GtkTextBTreeNodes above the
 *      segment get updated.
 *
 *--------------------------------------------------------------
 */

static int
toggle_segment_delete_func (GtkTextLineSegment *segPtr, GtkTextLine *line, int treeGone)
{
  if (treeGone)
    {
      g_free ((char *) segPtr);
      return 0;
    }

  /*
   * This toggle is in the middle of a range of characters that's
   * being deleted.  Refuse to die.  We'll be moved to the end of
   * the deleted range and our cleanup procedure will be called
   * later.  Decrement GtkTextBTreeNode toggle counts here, and set a flag
   * so we'll re-increment them in the cleanup procedure.
   */

  if (segPtr->body.toggle.inNodeCounts)
    {
      _gtk_change_node_toggle_count (line->parent,
                                     segPtr->body.toggle.info, -1);
      segPtr->body.toggle.inNodeCounts = 0;
    }
  return 1;
}

/*
 *--------------------------------------------------------------
 *
 * toggle_segment_cleanup_func --
 *
 *      This procedure is called when a toggle is part of a line that's
 *      been modified in some way.  It's invoked after the
 *      modifications are complete.
 *
 * Arguments:
 *      segPtr : Segment to check
 *      line   : Line that now contains segment
 *
 * Results:
 *      The return value is the head segment in a new list
 *      that is to replace the tail of the line that used to
 *      start at segPtr.  This allows the procedure to delete
 *      or modify segPtr.
 *
 * Side effects:
 *      Toggle counts in the GtkTextBTreeNodes above the new line will be
 *      updated if they're not already.  Toggles may be collapsed
 *      if there are duplicate toggles at the same position.
 *
 *--------------------------------------------------------------
 */

static GtkTextLineSegment *
toggle_segment_cleanup_func (GtkTextLineSegment *segPtr, GtkTextLine *line)
{
  GtkTextLineSegment *segPtr2, *prevPtr;
  int counts;

  /*
   * If this is a toggle-off segment, look ahead through the next
   * segments to see if there's a toggle-on segment for the same tag
   * before any segments with non-zero size.  If so then the two
   * toggles cancel each other;  remove them both.
   */

  if (segPtr->type == &gtk_text_toggle_off_type)
    {
      for (prevPtr = segPtr, segPtr2 = prevPtr->next;
           (segPtr2 != NULL) && (segPtr2->byte_count == 0);
           prevPtr = segPtr2, segPtr2 = prevPtr->next)
        {
          if (segPtr2->type != &gtk_text_toggle_on_type)
            {
              continue;
            }
          if (segPtr2->body.toggle.info != segPtr->body.toggle.info)
            {
              continue;
            }
          counts = segPtr->body.toggle.inNodeCounts
            + segPtr2->body.toggle.inNodeCounts;
          if (counts != 0)
            {
              _gtk_change_node_toggle_count (line->parent,
                                             segPtr->body.toggle.info, -counts);
            }
          prevPtr->next = segPtr2->next;
          g_free ((char *) segPtr2);
          segPtr2 = segPtr->next;
          g_free ((char *) segPtr);
          return segPtr2;
        }
    }

  if (!segPtr->body.toggle.inNodeCounts)
    {
      _gtk_change_node_toggle_count (line->parent,
                                     segPtr->body.toggle.info, 1);
      segPtr->body.toggle.inNodeCounts = 1;
    }
  return segPtr;
}

/*
 *--------------------------------------------------------------
 *
 * toggle_segment_line_change_func --
 *
 *      This procedure is invoked when a toggle segment is about
 *      to move from one line to another.
 *
 * Arguments:
 *      segPtr : Segment to check
 *      line   : Line that used to contain segment
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Toggle counts are decremented in the GtkTextBTreeNodes above the line.
 *
 *--------------------------------------------------------------
 */

static void
toggle_segment_line_change_func (GtkTextLineSegment *segPtr, GtkTextLine *line)
{
  if (segPtr->body.toggle.inNodeCounts)
    {
      _gtk_change_node_toggle_count (line->parent,
                                     segPtr->body.toggle.info, -1);
      segPtr->body.toggle.inNodeCounts = 0;
    }
}

/*
 * Virtual tables
 */


const GtkTextLineSegmentClass gtk_text_char_type = {
  "character",                          /* name */
  0,                                            /* leftGravity */
  char_segment_split_func,                              /* splitFunc */
  char_segment_delete_func,                             /* deleteFunc */
  char_segment_cleanup_func,                            /* cleanupFunc */
  NULL,         /* lineChangeFunc */
  char_segment_check_func                               /* checkFunc */
};

/*
 * Type record for segments marking the beginning of a tagged
 * range:
 */

const GtkTextLineSegmentClass gtk_text_toggle_on_type = {
  "toggleOn",                                   /* name */
  0,                                            /* leftGravity */
  NULL,                 /* splitFunc */
  toggle_segment_delete_func,                           /* deleteFunc */
  toggle_segment_cleanup_func,                          /* cleanupFunc */
  toggle_segment_line_change_func,                      /* lineChangeFunc */
  _gtk_toggle_segment_check_func                        /* checkFunc */
};

/*
 * Type record for segments marking the end of a tagged
 * range:
 */

const GtkTextLineSegmentClass gtk_text_toggle_off_type = {
  "toggleOff",                          /* name */
  1,                                            /* leftGravity */
  NULL,                 /* splitFunc */
  toggle_segment_delete_func,                           /* deleteFunc */
  toggle_segment_cleanup_func,                          /* cleanupFunc */
  toggle_segment_line_change_func,                      /* lineChangeFunc */
  _gtk_toggle_segment_check_func                        /* checkFunc */
};

#define __GTK_TEXT_SEGMENT_C__
#include "gtkaliasdef.c"
