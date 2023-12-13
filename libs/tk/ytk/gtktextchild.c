/* gtktextchild.c - child pixmaps and widgets
 *
 * Copyright (c) 1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
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
#include "gtktextchild.h"
#include "gtktextbtree.h"
#include "gtktextlayout.h"
#include "gtkintl.h"
#include "gtkalias.h"

#define CHECK_IN_BUFFER(anchor)                                         \
  G_STMT_START {                                                        \
    if ((anchor)->segment == NULL)                                      \
      {                                                                 \
        g_warning ("%s: GtkTextChildAnchor hasn't been in a buffer yet",\
                   G_STRFUNC);                                          \
      }                                                                 \
  } G_STMT_END

#define CHECK_IN_BUFFER_RETURN(anchor, val)                             \
  G_STMT_START {                                                        \
    if ((anchor)->segment == NULL)                                      \
      {                                                                 \
        g_warning ("%s: GtkTextChildAnchor hasn't been in a buffer yet",\
                   G_STRFUNC);                                          \
        return (val);                                                   \
      }                                                                 \
  } G_STMT_END

static GtkTextLineSegment *
pixbuf_segment_cleanup_func (GtkTextLineSegment *seg,
                             GtkTextLine        *line)
{
  /* nothing */
  return seg;
}

static int
pixbuf_segment_delete_func (GtkTextLineSegment *seg,
                            GtkTextLine        *line,
                            gboolean            tree_gone)
{
  if (seg->body.pixbuf.pixbuf)
    g_object_unref (seg->body.pixbuf.pixbuf);

  g_free (seg);

  return 0;
}

static void
pixbuf_segment_check_func (GtkTextLineSegment *seg,
                           GtkTextLine        *line)
{
  if (seg->next == NULL)
    g_error ("pixbuf segment is the last segment in a line");

  if (seg->byte_count != 3)
    g_error ("pixbuf segment has byte count of %d", seg->byte_count);

  if (seg->char_count != 1)
    g_error ("pixbuf segment has char count of %d", seg->char_count);
}


const GtkTextLineSegmentClass gtk_text_pixbuf_type = {
  "pixbuf",                          /* name */
  FALSE,                                            /* leftGravity */
  NULL,                                          /* splitFunc */
  pixbuf_segment_delete_func,                             /* deleteFunc */
  pixbuf_segment_cleanup_func,                            /* cleanupFunc */
  NULL,                                                    /* lineChangeFunc */
  pixbuf_segment_check_func                               /* checkFunc */

};

#define PIXBUF_SEG_SIZE ((unsigned) (G_STRUCT_OFFSET (GtkTextLineSegment, body) \
        + sizeof (GtkTextPixbuf)))

GtkTextLineSegment *
_gtk_pixbuf_segment_new (GdkPixbuf *pixbuf)
{
  GtkTextLineSegment *seg;

  seg = g_malloc (PIXBUF_SEG_SIZE);

  seg->type = &gtk_text_pixbuf_type;

  seg->next = NULL;

  seg->byte_count = 3; /* We convert to the 0xFFFC "unknown character",
                        * a 3-byte sequence in UTF-8
                        */
  seg->char_count = 1;

  seg->body.pixbuf.pixbuf = pixbuf;

  g_object_ref (pixbuf);

  return seg;
}


static GtkTextLineSegment *
child_segment_cleanup_func (GtkTextLineSegment *seg,
                            GtkTextLine        *line)
{
  seg->body.child.line = line;

  return seg;
}

static int
child_segment_delete_func (GtkTextLineSegment *seg,
                           GtkTextLine       *line,
                           gboolean           tree_gone)
{
  GSList *tmp_list;
  GSList *copy;

  _gtk_text_btree_unregister_child_anchor (seg->body.child.obj);
  
  seg->body.child.tree = NULL;
  seg->body.child.line = NULL;

  /* avoid removing widgets while walking the list */
  copy = g_slist_copy (seg->body.child.widgets);
  tmp_list = copy;
  while (tmp_list != NULL)
    {
      GtkWidget *child = tmp_list->data;

      gtk_widget_destroy (child);
      
      tmp_list = g_slist_next (tmp_list);
    }

  /* On removal from the widget's parents (GtkTextView),
   * the widget should have been removed from the anchor.
   */
  g_assert (seg->body.child.widgets == NULL);

  g_slist_free (copy);
  
  _gtk_widget_segment_unref (seg);  
  
  return 0;
}

static void
child_segment_check_func (GtkTextLineSegment *seg,
                          GtkTextLine        *line)
{
  if (seg->next == NULL)
    g_error ("child segment is the last segment in a line");

  if (seg->byte_count != 3)
    g_error ("child segment has byte count of %d", seg->byte_count);

  if (seg->char_count != 1)
    g_error ("child segment has char count of %d", seg->char_count);
}

const GtkTextLineSegmentClass gtk_text_child_type = {
  "child-widget",                                        /* name */
  FALSE,                                                 /* leftGravity */
  NULL,                                                  /* splitFunc */
  child_segment_delete_func,                             /* deleteFunc */
  child_segment_cleanup_func,                            /* cleanupFunc */
  NULL,                                                  /* lineChangeFunc */
  child_segment_check_func                               /* checkFunc */
};

#define WIDGET_SEG_SIZE ((unsigned) (G_STRUCT_OFFSET (GtkTextLineSegment, body) \
        + sizeof (GtkTextChildBody)))

GtkTextLineSegment *
_gtk_widget_segment_new (GtkTextChildAnchor *anchor)
{
  GtkTextLineSegment *seg;

  seg = g_malloc (WIDGET_SEG_SIZE);

  seg->type = &gtk_text_child_type;

  seg->next = NULL;

  seg->byte_count = 3; /* We convert to the 0xFFFC "unknown character",
                        * a 3-byte sequence in UTF-8
                        */
  seg->char_count = 1;

  seg->body.child.obj = anchor;
  seg->body.child.obj->segment = seg;
  seg->body.child.widgets = NULL;
  seg->body.child.tree = NULL;
  seg->body.child.line = NULL;

  g_object_ref (anchor);
  
  return seg;
}

void
_gtk_widget_segment_add    (GtkTextLineSegment *widget_segment,
                            GtkWidget          *child)
{
  g_return_if_fail (widget_segment->type == &gtk_text_child_type);
  g_return_if_fail (widget_segment->body.child.tree != NULL);

  g_object_ref (child);
  
  widget_segment->body.child.widgets =
    g_slist_prepend (widget_segment->body.child.widgets,
                     child);
}

void
_gtk_widget_segment_remove (GtkTextLineSegment *widget_segment,
                            GtkWidget          *child)
{
  g_return_if_fail (widget_segment->type == &gtk_text_child_type);
  
  widget_segment->body.child.widgets =
    g_slist_remove (widget_segment->body.child.widgets,
                    child);

  g_object_unref (child);
}

void
_gtk_widget_segment_ref (GtkTextLineSegment *widget_segment)
{
  g_assert (widget_segment->type == &gtk_text_child_type);

  g_object_ref (widget_segment->body.child.obj);
}

void
_gtk_widget_segment_unref (GtkTextLineSegment *widget_segment)
{
  g_assert (widget_segment->type == &gtk_text_child_type);

  g_object_unref (widget_segment->body.child.obj);
}

GtkTextLayout*
_gtk_anchored_child_get_layout (GtkWidget *child)
{
  return g_object_get_data (G_OBJECT (child), "gtk-text-child-anchor-layout");  
}

static void
_gtk_anchored_child_set_layout (GtkWidget     *child,
                                GtkTextLayout *layout)
{
  g_object_set_data (G_OBJECT (child),
                     I_("gtk-text-child-anchor-layout"),
                     layout);  
}
     
static void gtk_text_child_anchor_finalize (GObject *obj);

G_DEFINE_TYPE (GtkTextChildAnchor, gtk_text_child_anchor, G_TYPE_OBJECT)

static void
gtk_text_child_anchor_init (GtkTextChildAnchor *child_anchor)
{
  child_anchor->segment = NULL;
}

static void
gtk_text_child_anchor_class_init (GtkTextChildAnchorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_text_child_anchor_finalize;
}

/**
 * gtk_text_child_anchor_new:
 * 
 * Creates a new #GtkTextChildAnchor. Usually you would then insert
 * it into a #GtkTextBuffer with gtk_text_buffer_insert_child_anchor().
 * To perform the creation and insertion in one step, use the
 * convenience function gtk_text_buffer_create_child_anchor().
 * 
 * Return value: a new #GtkTextChildAnchor
 **/
GtkTextChildAnchor*
gtk_text_child_anchor_new (void)
{
  return g_object_new (GTK_TYPE_TEXT_CHILD_ANCHOR, NULL);
}

static void
gtk_text_child_anchor_finalize (GObject *obj)
{
  GtkTextChildAnchor *anchor;
  GSList *tmp_list;
  GtkTextLineSegment *seg;
  
  anchor = GTK_TEXT_CHILD_ANCHOR (obj);

  seg = anchor->segment;
  
  if (seg)
    {
      if (seg->body.child.tree != NULL)
        {
          g_warning ("Someone removed a reference to a GtkTextChildAnchor "
                     "they didn't own; the anchor is still in the text buffer "
                     "and the refcount is 0.");
          return;
        }
      
      tmp_list = seg->body.child.widgets;
      while (tmp_list)
        {
          g_object_unref (tmp_list->data);
          tmp_list = g_slist_next (tmp_list);
        }
  
      g_slist_free (seg->body.child.widgets);
  
      g_free (seg);
    }

  anchor->segment = NULL;

  G_OBJECT_CLASS (gtk_text_child_anchor_parent_class)->finalize (obj);
}

/**
 * gtk_text_child_anchor_get_widgets:
 * @anchor: a #GtkTextChildAnchor
 * 
 * Gets a list of all widgets anchored at this child anchor.
 * The returned list should be freed with g_list_free().
 *
 *
 * Return value: (element-type GtkWidget) (transfer container): list of widgets anchored at @anchor
 **/
GList*
gtk_text_child_anchor_get_widgets (GtkTextChildAnchor *anchor)
{
  GtkTextLineSegment *seg = anchor->segment;
  GList *list = NULL;
  GSList *iter;

  CHECK_IN_BUFFER_RETURN (anchor, NULL);
  
  g_return_val_if_fail (seg->type == &gtk_text_child_type, NULL);

  iter = seg->body.child.widgets;
  while (iter != NULL)
    {
      list = g_list_prepend (list, iter->data);

      iter = g_slist_next (iter);
    }

  /* Order is not relevant, so we don't need to reverse the list
   * again.
   */
  return list;
}

/**
 * gtk_text_child_anchor_get_deleted:
 * @anchor: a #GtkTextChildAnchor
 * 
 * Determines whether a child anchor has been deleted from
 * the buffer. Keep in mind that the child anchor will be
 * unreferenced when removed from the buffer, so you need to
 * hold your own reference (with g_object_ref()) if you plan
 * to use this function &mdash; otherwise all deleted child anchors
 * will also be finalized.
 * 
 * Return value: %TRUE if the child anchor has been deleted from its buffer
 **/
gboolean
gtk_text_child_anchor_get_deleted (GtkTextChildAnchor *anchor)
{
  GtkTextLineSegment *seg = anchor->segment;

  CHECK_IN_BUFFER_RETURN (anchor, TRUE);
  
  g_return_val_if_fail (seg->type == &gtk_text_child_type, TRUE);

  return seg->body.child.tree == NULL;
}

void
gtk_text_child_anchor_register_child (GtkTextChildAnchor *anchor,
                                      GtkWidget          *child,
                                      GtkTextLayout      *layout)
{
  g_return_if_fail (GTK_IS_TEXT_CHILD_ANCHOR (anchor));
  g_return_if_fail (GTK_IS_WIDGET (child));

  CHECK_IN_BUFFER (anchor);
  
  _gtk_anchored_child_set_layout (child, layout);
  
  _gtk_widget_segment_add (anchor->segment, child);

  gtk_text_child_anchor_queue_resize (anchor, layout);
}

void
gtk_text_child_anchor_unregister_child (GtkTextChildAnchor *anchor,
                                        GtkWidget          *child)
{
  g_return_if_fail (GTK_IS_TEXT_CHILD_ANCHOR (anchor));
  g_return_if_fail (GTK_IS_WIDGET (child));

  CHECK_IN_BUFFER (anchor);
  
  if (_gtk_anchored_child_get_layout (child))
    {
      gtk_text_child_anchor_queue_resize (anchor,
                                          _gtk_anchored_child_get_layout (child));
    }
  
  _gtk_anchored_child_set_layout (child, NULL);
  
  _gtk_widget_segment_remove (anchor->segment, child);
}

void
gtk_text_child_anchor_queue_resize (GtkTextChildAnchor *anchor,
                                    GtkTextLayout      *layout)
{
  GtkTextIter start;
  GtkTextIter end;
  GtkTextLineSegment *seg;
  
  g_return_if_fail (GTK_IS_TEXT_CHILD_ANCHOR (anchor));
  g_return_if_fail (GTK_IS_TEXT_LAYOUT (layout));

  CHECK_IN_BUFFER (anchor);
  
  seg = anchor->segment;

  if (seg->body.child.tree == NULL)
    return;
  
  gtk_text_buffer_get_iter_at_child_anchor (layout->buffer,
                                            &start, anchor);
  end = start;
  gtk_text_iter_forward_char (&end);
  
  gtk_text_layout_invalidate (layout, &start, &end);
}

void
gtk_text_anchored_child_set_layout (GtkWidget     *child,
                                    GtkTextLayout *layout)
{
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (layout == NULL || GTK_IS_TEXT_LAYOUT (layout));
  
  _gtk_anchored_child_set_layout (child, layout);
}

#define __GTK_TEXT_CHILD_C__
#include "gtkaliasdef.c"
