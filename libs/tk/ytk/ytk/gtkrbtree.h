/* gtkrbtree.h
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
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

/* A Red-Black Tree implementation used specifically by GtkTreeView.
 */
#ifndef __GTK_RBTREE_H__
#define __GTK_RBTREE_H__

#include <glib.h>


G_BEGIN_DECLS


typedef enum
{
  GTK_RBNODE_BLACK = 1 << 0,
  GTK_RBNODE_RED = 1 << 1,
  GTK_RBNODE_IS_PARENT = 1 << 2,
  GTK_RBNODE_IS_SELECTED = 1 << 3,
  GTK_RBNODE_IS_PRELIT = 1 << 4,
  GTK_RBNODE_IS_SEMI_COLLAPSED = 1 << 5,
  GTK_RBNODE_IS_SEMI_EXPANDED = 1 << 6,
  GTK_RBNODE_INVALID = 1 << 7,
  GTK_RBNODE_COLUMN_INVALID = 1 << 8,
  GTK_RBNODE_DESCENDANTS_INVALID = 1 << 9,
  GTK_RBNODE_NON_COLORS = GTK_RBNODE_IS_PARENT |
  			  GTK_RBNODE_IS_SELECTED |
  			  GTK_RBNODE_IS_PRELIT |
                          GTK_RBNODE_IS_SEMI_COLLAPSED |
                          GTK_RBNODE_IS_SEMI_EXPANDED |
                          GTK_RBNODE_INVALID |
                          GTK_RBNODE_COLUMN_INVALID |
                          GTK_RBNODE_DESCENDANTS_INVALID
} GtkRBNodeColor;

typedef struct _GtkRBTree GtkRBTree;
typedef struct _GtkRBNode GtkRBNode;
typedef struct _GtkRBTreeView GtkRBTreeView;

typedef void (*GtkRBTreeTraverseFunc) (GtkRBTree  *tree,
                                       GtkRBNode  *node,
                                       gpointer  data);

struct _GtkRBTree
{
  GtkRBNode *root;
  GtkRBNode *nil;
  GtkRBTree *parent_tree;
  GtkRBNode *parent_node;
};

struct _GtkRBNode
{
  guint flags : 14;

  /* We keep track of whether the aggregate count of children plus 1
   * for the node itself comes to an even number.  The parity flag is
   * the total count of children mod 2, where the total count of
   * children gets computed in the same way that the total offset gets
   * computed. i.e. not the same as the "count" field below which
   * doesn't include children. We could replace parity with a
   * full-size int field here, and then take % 2 to get the parity flag,
   * but that would use extra memory.
   */

  guint parity : 1;
  
  GtkRBNode *left;
  GtkRBNode *right;
  GtkRBNode *parent;

  /* count is the number of nodes beneath us, plus 1 for ourselves.
   * i.e. node->left->count + node->right->count + 1
   */
  gint count;
  
  /* this is the total of sizes of
   * node->left, node->right, our own height, and the height
   * of all trees in ->children, iff children exists because
   * the thing is expanded.
   */
  gint offset;

  /* Child trees */
  GtkRBTree *children;
};


#define GTK_RBNODE_GET_COLOR(node)		(node?(((node->flags&GTK_RBNODE_RED)==GTK_RBNODE_RED)?GTK_RBNODE_RED:GTK_RBNODE_BLACK):GTK_RBNODE_BLACK)
#define GTK_RBNODE_SET_COLOR(node,color) 	if((node->flags&color)!=color)node->flags=node->flags^(GTK_RBNODE_RED|GTK_RBNODE_BLACK)
#define GTK_RBNODE_GET_HEIGHT(node) 		(node->offset-(node->left->offset+node->right->offset+(node->children?node->children->root->offset:0)))
#define GTK_RBNODE_SET_FLAG(node, flag)   	G_STMT_START{ (node->flags|=flag); }G_STMT_END
#define GTK_RBNODE_UNSET_FLAG(node, flag) 	G_STMT_START{ (node->flags&=~(flag)); }G_STMT_END
#define GTK_RBNODE_FLAG_SET(node, flag) 	(node?(((node->flags&flag)==flag)?TRUE:FALSE):FALSE)


GtkRBTree *_gtk_rbtree_new              (void);
void       _gtk_rbtree_free             (GtkRBTree              *tree);
void       _gtk_rbtree_remove           (GtkRBTree              *tree);
void       _gtk_rbtree_destroy          (GtkRBTree              *tree);
GtkRBNode *_gtk_rbtree_insert_before    (GtkRBTree              *tree,
					 GtkRBNode              *node,
					 gint                    height,
					 gboolean                valid);
GtkRBNode *_gtk_rbtree_insert_after     (GtkRBTree              *tree,
					 GtkRBNode              *node,
					 gint                    height,
					 gboolean                valid);
void       _gtk_rbtree_remove_node      (GtkRBTree              *tree,
					 GtkRBNode              *node);
void       _gtk_rbtree_reorder          (GtkRBTree              *tree,
					 gint                   *new_order,
					 gint                    length);
GtkRBNode *_gtk_rbtree_find_count       (GtkRBTree              *tree,
					 gint                    count);
void       _gtk_rbtree_node_set_height  (GtkRBTree              *tree,
					 GtkRBNode              *node,
					 gint                    height);
void       _gtk_rbtree_node_mark_invalid(GtkRBTree              *tree,
					 GtkRBNode              *node);
void       _gtk_rbtree_node_mark_valid  (GtkRBTree              *tree,
					 GtkRBNode              *node);
void       _gtk_rbtree_column_invalid   (GtkRBTree              *tree);
void       _gtk_rbtree_mark_invalid     (GtkRBTree              *tree);
void       _gtk_rbtree_set_fixed_height (GtkRBTree              *tree,
					 gint                    height,
					 gboolean                mark_valid);
gint       _gtk_rbtree_node_find_offset (GtkRBTree              *tree,
					 GtkRBNode              *node);
gint       _gtk_rbtree_node_find_parity (GtkRBTree              *tree,
					 GtkRBNode              *node);
gint       _gtk_rbtree_find_offset      (GtkRBTree              *tree,
					 gint                    offset,
					 GtkRBTree             **new_tree,
					 GtkRBNode             **new_node);
void       _gtk_rbtree_traverse         (GtkRBTree              *tree,
					 GtkRBNode              *node,
					 GTraverseType           order,
					 GtkRBTreeTraverseFunc   func,
					 gpointer                data);
GtkRBNode *_gtk_rbtree_next             (GtkRBTree              *tree,
					 GtkRBNode              *node);
GtkRBNode *_gtk_rbtree_prev             (GtkRBTree              *tree,
					 GtkRBNode              *node);
void       _gtk_rbtree_next_full        (GtkRBTree              *tree,
					 GtkRBNode              *node,
					 GtkRBTree             **new_tree,
					 GtkRBNode             **new_node);
void       _gtk_rbtree_prev_full        (GtkRBTree              *tree,
					 GtkRBNode              *node,
					 GtkRBTree             **new_tree,
					 GtkRBNode             **new_node);

gint       _gtk_rbtree_get_depth        (GtkRBTree              *tree);

/* This func checks the integrity of the tree */
#ifdef G_ENABLE_DEBUG  
void       _gtk_rbtree_test             (const gchar            *where,
                                         GtkRBTree              *tree);
void       _gtk_rbtree_debug_spew       (GtkRBTree              *tree);
#endif


G_END_DECLS


#endif /* __GTK_RBTREE_H__ */
