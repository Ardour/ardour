/* gtkrbtree.c
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

#include "config.h"
#include "gtkrbtree.h"
#include "gtkdebug.h"
#include "gtkalias.h"

static GtkRBNode * _gtk_rbnode_new                (GtkRBTree  *tree,
						   gint        height);
static void        _gtk_rbnode_free               (GtkRBNode  *node);
static void        _gtk_rbnode_rotate_left        (GtkRBTree  *tree,
						   GtkRBNode  *node);
static void        _gtk_rbnode_rotate_right       (GtkRBTree  *tree,
						   GtkRBNode  *node);
static void        _gtk_rbtree_insert_fixup       (GtkRBTree  *tree,
						   GtkRBNode  *node);
static void        _gtk_rbtree_remove_node_fixup  (GtkRBTree  *tree,
						   GtkRBNode  *node);
static inline void _fixup_validation              (GtkRBTree  *tree,
						   GtkRBNode  *node);
static inline void _fixup_parity                  (GtkRBTree  *tree,
						   GtkRBNode  *node);



static GtkRBNode *
_gtk_rbnode_new (GtkRBTree *tree,
		 gint       height)
{
  GtkRBNode *node = g_slice_new (GtkRBNode);

  node->left = tree->nil;
  node->right = tree->nil;
  node->parent = tree->nil;
  node->flags = GTK_RBNODE_RED;
  node->parity = 1;
  node->count = 1;
  node->children = NULL;
  node->offset = height;
  return node;
}

static void
_gtk_rbnode_free (GtkRBNode *node)
{
  if (gtk_debug_flags & GTK_DEBUG_TREE)
    {
      node->left = (gpointer) 0xdeadbeef;
      node->right = (gpointer) 0xdeadbeef;
      node->parent = (gpointer) 0xdeadbeef;
      node->offset = 56789;
      node->count = 56789;
      node->flags = 0;
    }
  g_slice_free (GtkRBNode, node);
}

static void
_gtk_rbnode_rotate_left (GtkRBTree *tree,
			 GtkRBNode *node)
{
  gint node_height, right_height;
  GtkRBNode *right = node->right;

  g_return_if_fail (node != tree->nil);

  node_height = node->offset -
    (node->left?node->left->offset:0) -
    (node->right?node->right->offset:0) -
    (node->children?node->children->root->offset:0);
  right_height = right->offset -
    (right->left?right->left->offset:0) -
    (right->right?right->right->offset:0) -
    (right->children?right->children->root->offset:0);
  node->right = right->left;
  if (right->left != tree->nil)
    right->left->parent = node;

  if (right != tree->nil)
    right->parent = node->parent;
  if (node->parent != tree->nil)
    {
      if (node == node->parent->left)
	node->parent->left = right;
      else
	node->parent->right = right;
    } else {
      tree->root = right;
    }

  right->left = node;
  if (node != tree->nil)
    node->parent = right;

  node->count = 1 + (node->left?node->left->count:0) +
    (node->right?node->right->count:0);
  right->count = 1 + (right->left?right->left->count:0) +
    (right->right?right->right->count:0);

  node->offset = node_height +
    (node->left?node->left->offset:0) +
    (node->right?node->right->offset:0) +
    (node->children?node->children->root->offset:0);
  right->offset = right_height +
    (right->left?right->left->offset:0) +
    (right->right?right->right->offset:0) +
    (right->children?right->children->root->offset:0);

  _fixup_validation (tree, node);
  _fixup_validation (tree, right);
  _fixup_parity (tree, node);
  _fixup_parity (tree, right);
}

static void
_gtk_rbnode_rotate_right (GtkRBTree *tree,
			  GtkRBNode *node)
{
  gint node_height, left_height;
  GtkRBNode *left = node->left;

  g_return_if_fail (node != tree->nil);

  node_height = node->offset -
    (node->left?node->left->offset:0) -
    (node->right?node->right->offset:0) -
    (node->children?node->children->root->offset:0);
  left_height = left->offset -
    (left->left?left->left->offset:0) -
    (left->right?left->right->offset:0) -
    (left->children?left->children->root->offset:0);
  
  node->left = left->right;
  if (left->right != tree->nil)
    left->right->parent = node;

  if (left != tree->nil)
    left->parent = node->parent;
  if (node->parent != tree->nil)
    {
      if (node == node->parent->right)
	node->parent->right = left;
      else
	node->parent->left = left;
    }
  else
    {
      tree->root = left;
    }

  /* link node and left */
  left->right = node;
  if (node != tree->nil)
    node->parent = left;

  node->count = 1 + (node->left?node->left->count:0) +
    (node->right?node->right->count:0);
  left->count = 1 + (left->left?left->left->count:0) +
    (left->right?left->right->count:0);

  node->offset = node_height +
    (node->left?node->left->offset:0) +
    (node->right?node->right->offset:0) +
    (node->children?node->children->root->offset:0);
  left->offset = left_height +
    (left->left?left->left->offset:0) +
    (left->right?left->right->offset:0) +
    (left->children?left->children->root->offset:0);

  _fixup_validation (tree, node);
  _fixup_validation (tree, left);
  _fixup_parity (tree, node);
  _fixup_parity (tree, left);
}

static void
_gtk_rbtree_insert_fixup (GtkRBTree *tree,
			  GtkRBNode *node)
{

  /* check Red-Black properties */
  while (node != tree->root && GTK_RBNODE_GET_COLOR (node->parent) == GTK_RBNODE_RED)
    {
      /* we have a violation */
      if (node->parent == node->parent->parent->left)
	{
	  GtkRBNode *y = node->parent->parent->right;
	  if (GTK_RBNODE_GET_COLOR (y) == GTK_RBNODE_RED)
	    {
				/* uncle is GTK_RBNODE_RED */
	      GTK_RBNODE_SET_COLOR (node->parent, GTK_RBNODE_BLACK);
	      GTK_RBNODE_SET_COLOR (y, GTK_RBNODE_BLACK);
	      GTK_RBNODE_SET_COLOR (node->parent->parent, GTK_RBNODE_RED);
	      node = node->parent->parent;
	    }
	  else
	    {
				/* uncle is GTK_RBNODE_BLACK */
	      if (node == node->parent->right)
		{
		  /* make node a left child */
		  node = node->parent;
		  _gtk_rbnode_rotate_left (tree, node);
		}

				/* recolor and rotate */
	      GTK_RBNODE_SET_COLOR (node->parent, GTK_RBNODE_BLACK);
	      GTK_RBNODE_SET_COLOR (node->parent->parent, GTK_RBNODE_RED);
	      _gtk_rbnode_rotate_right(tree, node->parent->parent);
	    }
	}
      else
	{
	  /* mirror image of above code */
	  GtkRBNode *y = node->parent->parent->left;
	  if (GTK_RBNODE_GET_COLOR (y) == GTK_RBNODE_RED)
	    {
				/* uncle is GTK_RBNODE_RED */
	      GTK_RBNODE_SET_COLOR (node->parent, GTK_RBNODE_BLACK);
	      GTK_RBNODE_SET_COLOR (y, GTK_RBNODE_BLACK);
	      GTK_RBNODE_SET_COLOR (node->parent->parent, GTK_RBNODE_RED);
	      node = node->parent->parent;
	    }
	  else
	    {
				/* uncle is GTK_RBNODE_BLACK */
	      if (node == node->parent->left)
		{
		  node = node->parent;
		  _gtk_rbnode_rotate_right (tree, node);
		}
	      GTK_RBNODE_SET_COLOR (node->parent, GTK_RBNODE_BLACK);
	      GTK_RBNODE_SET_COLOR (node->parent->parent, GTK_RBNODE_RED);
	      _gtk_rbnode_rotate_left (tree, node->parent->parent);
	    }
	}
    }
  GTK_RBNODE_SET_COLOR (tree->root, GTK_RBNODE_BLACK);
}

static void
_gtk_rbtree_remove_node_fixup (GtkRBTree *tree,
			       GtkRBNode *node)
{
  while (node != tree->root && GTK_RBNODE_GET_COLOR (node) == GTK_RBNODE_BLACK)
    {
      if (node == node->parent->left)
	{
	  GtkRBNode *w = node->parent->right;
	  if (GTK_RBNODE_GET_COLOR (w) == GTK_RBNODE_RED)
	    {
	      GTK_RBNODE_SET_COLOR (w, GTK_RBNODE_BLACK);
	      GTK_RBNODE_SET_COLOR (node->parent, GTK_RBNODE_RED);
	      _gtk_rbnode_rotate_left (tree, node->parent);
	      w = node->parent->right;
	    }
	  if (GTK_RBNODE_GET_COLOR (w->left) == GTK_RBNODE_BLACK && GTK_RBNODE_GET_COLOR (w->right) == GTK_RBNODE_BLACK)
	    {
	      GTK_RBNODE_SET_COLOR (w, GTK_RBNODE_RED);
	      node = node->parent;
	    }
	  else
	    {
	      if (GTK_RBNODE_GET_COLOR (w->right) == GTK_RBNODE_BLACK)
		{
		  GTK_RBNODE_SET_COLOR (w->left, GTK_RBNODE_BLACK);
		  GTK_RBNODE_SET_COLOR (w, GTK_RBNODE_RED);
		  _gtk_rbnode_rotate_right (tree, w);
		  w = node->parent->right;
		}
	      GTK_RBNODE_SET_COLOR (w, GTK_RBNODE_GET_COLOR (node->parent));
	      GTK_RBNODE_SET_COLOR (node->parent, GTK_RBNODE_BLACK);
	      GTK_RBNODE_SET_COLOR (w->right, GTK_RBNODE_BLACK);
	      _gtk_rbnode_rotate_left (tree, node->parent);
	      node = tree->root;
	    }
	}
      else
	{
	  GtkRBNode *w = node->parent->left;
	  if (GTK_RBNODE_GET_COLOR (w) == GTK_RBNODE_RED)
	    {
	      GTK_RBNODE_SET_COLOR (w, GTK_RBNODE_BLACK);
	      GTK_RBNODE_SET_COLOR (node->parent, GTK_RBNODE_RED);
	      _gtk_rbnode_rotate_right (tree, node->parent);
	      w = node->parent->left;
	    }
	  if (GTK_RBNODE_GET_COLOR (w->right) == GTK_RBNODE_BLACK && GTK_RBNODE_GET_COLOR (w->left) == GTK_RBNODE_BLACK)
	    {
	      GTK_RBNODE_SET_COLOR (w, GTK_RBNODE_RED);
	      node = node->parent;
	    }
	  else
	    {
	      if (GTK_RBNODE_GET_COLOR (w->left) == GTK_RBNODE_BLACK)
		{
		  GTK_RBNODE_SET_COLOR (w->right, GTK_RBNODE_BLACK);
		  GTK_RBNODE_SET_COLOR (w, GTK_RBNODE_RED);
		  _gtk_rbnode_rotate_left (tree, w);
		  w = node->parent->left;
		}
	      GTK_RBNODE_SET_COLOR (w, GTK_RBNODE_GET_COLOR (node->parent));
	      GTK_RBNODE_SET_COLOR (node->parent, GTK_RBNODE_BLACK);
	      GTK_RBNODE_SET_COLOR (w->left, GTK_RBNODE_BLACK);
	      _gtk_rbnode_rotate_right (tree, node->parent);
	      node = tree->root;
	    }
	}
    }
  GTK_RBNODE_SET_COLOR (node, GTK_RBNODE_BLACK);
}

GtkRBTree *
_gtk_rbtree_new (void)
{
  GtkRBTree *retval;

  retval = g_new (GtkRBTree, 1);
  retval->parent_tree = NULL;
  retval->parent_node = NULL;

  retval->nil = g_slice_new (GtkRBNode);
  retval->nil->left = NULL;
  retval->nil->right = NULL;
  retval->nil->parent = NULL;
  retval->nil->flags = GTK_RBNODE_BLACK;
  retval->nil->count = 0;
  retval->nil->offset = 0;
  retval->nil->parity = 0;

  retval->root = retval->nil;
  return retval;
}

static void
_gtk_rbtree_free_helper (GtkRBTree  *tree,
			 GtkRBNode  *node,
			 gpointer    data)
{
  if (node->children)
    _gtk_rbtree_free (node->children);

  _gtk_rbnode_free (node);
}

void
_gtk_rbtree_free (GtkRBTree *tree)
{
  _gtk_rbtree_traverse (tree,
			tree->root,
			G_POST_ORDER,
			_gtk_rbtree_free_helper,
			NULL);

  if (tree->parent_node &&
      tree->parent_node->children == tree)
    tree->parent_node->children = NULL;
  _gtk_rbnode_free (tree->nil);
  g_free (tree);
}

void
_gtk_rbtree_remove (GtkRBTree *tree)
{
  GtkRBTree *tmp_tree;
  GtkRBNode *tmp_node;

  gint height = tree->root->offset;

#ifdef G_ENABLE_DEBUG  
  if (gtk_debug_flags & GTK_DEBUG_TREE)
    _gtk_rbtree_test (G_STRLOC, tree);
#endif
  
  tmp_tree = tree->parent_tree;
  tmp_node = tree->parent_node;

  /* ugly hack to make _fixup_validation work in the first iteration of the
   * loop below */
  GTK_RBNODE_UNSET_FLAG (tree->root, GTK_RBNODE_DESCENDANTS_INVALID);
  
  while (tmp_tree && tmp_node && tmp_node != tmp_tree->nil)
    {
      _fixup_validation (tmp_tree, tmp_node);
      tmp_node->offset -= height;

      /* If the removed tree was odd, flip all parents */
      if (tree->root->parity)
        tmp_node->parity = !tmp_node->parity;
      
      tmp_node = tmp_node->parent;
      if (tmp_node == tmp_tree->nil)
	{
	  tmp_node = tmp_tree->parent_node;
	  tmp_tree = tmp_tree->parent_tree;
	}
    }

  tmp_tree = tree->parent_tree;
  tmp_node = tree->parent_node;
  _gtk_rbtree_free (tree);

#ifdef G_ENABLE_DEBUG  
  if (gtk_debug_flags & GTK_DEBUG_TREE)
    _gtk_rbtree_test (G_STRLOC, tmp_tree);
#endif
}


GtkRBNode *
_gtk_rbtree_insert_after (GtkRBTree *tree,
			  GtkRBNode *current,
			  gint       height,
			  gboolean   valid)
{
  GtkRBNode *node;
  gboolean right = TRUE;
  GtkRBNode *tmp_node;
  GtkRBTree *tmp_tree;  

#ifdef G_ENABLE_DEBUG  
  if (gtk_debug_flags & GTK_DEBUG_TREE)
    {
      g_print ("\n\n_gtk_rbtree_insert_after: %p\n", current);
      _gtk_rbtree_debug_spew (tree);
      _gtk_rbtree_test (G_STRLOC, tree);
    }
#endif /* G_ENABLE_DEBUG */  

  if (current != NULL && current->right != tree->nil)
    {
      current = current->right;
      while (current->left != tree->nil)
	current = current->left;
      right = FALSE;
    }
  /* setup new node */
  node = _gtk_rbnode_new (tree, height);
  node->parent = (current?current:tree->nil);

  /* insert node in tree */
  if (current)
    {
      if (right)
	current->right = node;
      else
	current->left = node;
      tmp_node = node->parent;
      tmp_tree = tree;
    }
  else
    {
      tree->root = node;
      tmp_node = tree->parent_node;
      tmp_tree = tree->parent_tree;
    }

  while (tmp_tree && tmp_node && tmp_node != tmp_tree->nil)
    {
      /* We only want to propagate the count if we are in the tree we
       * started in. */
      if (tmp_tree == tree)
	tmp_node->count++;

      tmp_node->parity += 1;
      tmp_node->offset += height;
      tmp_node = tmp_node->parent;
      if (tmp_node == tmp_tree->nil)
	{
	  tmp_node = tmp_tree->parent_node;
	  tmp_tree = tmp_tree->parent_tree;
	}
    }

  if (valid)
    _gtk_rbtree_node_mark_valid (tree, node);
  else
    _gtk_rbtree_node_mark_invalid (tree, node);

  _gtk_rbtree_insert_fixup (tree, node);

#ifdef G_ENABLE_DEBUG  
  if (gtk_debug_flags & GTK_DEBUG_TREE)
    {
      g_print ("_gtk_rbtree_insert_after finished...\n");
      _gtk_rbtree_debug_spew (tree);
      g_print ("\n\n");
      _gtk_rbtree_test (G_STRLOC, tree);
    }
#endif /* G_ENABLE_DEBUG */  

  return node;
}

GtkRBNode *
_gtk_rbtree_insert_before (GtkRBTree *tree,
			   GtkRBNode *current,
			   gint       height,
			   gboolean   valid)
{
  GtkRBNode *node;
  gboolean left = TRUE;
  GtkRBNode *tmp_node;
  GtkRBTree *tmp_tree;

#ifdef G_ENABLE_DEBUG  
  if (gtk_debug_flags & GTK_DEBUG_TREE)
    {
      g_print ("\n\n_gtk_rbtree_insert_before: %p\n", current);
      _gtk_rbtree_debug_spew (tree);
      _gtk_rbtree_test (G_STRLOC, tree);
    }
#endif /* G_ENABLE_DEBUG */
  
  if (current != NULL && current->left != tree->nil)
    {
      current = current->left;
      while (current->right != tree->nil)
	current = current->right;
      left = FALSE;
    }

  /* setup new node */
  node = _gtk_rbnode_new (tree, height);
  node->parent = (current?current:tree->nil);

  /* insert node in tree */
  if (current)
    {
      if (left)
	current->left = node;
      else
	current->right = node;
      tmp_node = node->parent;
      tmp_tree = tree;
    }
  else
    {
      tree->root = node;
      tmp_node = tree->parent_node;
      tmp_tree = tree->parent_tree;
    }

  while (tmp_tree && tmp_node && tmp_node != tmp_tree->nil)
    {
      /* We only want to propagate the count if we are in the tree we
       * started in. */
      if (tmp_tree == tree)
	tmp_node->count++;

      tmp_node->parity += 1;
      tmp_node->offset += height;
      tmp_node = tmp_node->parent;
      if (tmp_node == tmp_tree->nil)
	{
	  tmp_node = tmp_tree->parent_node;
	  tmp_tree = tmp_tree->parent_tree;
	}
    }

  if (valid)
    _gtk_rbtree_node_mark_valid (tree, node);
  else
    _gtk_rbtree_node_mark_invalid (tree, node);

  _gtk_rbtree_insert_fixup (tree, node);

#ifdef G_ENABLE_DEBUG  
  if (gtk_debug_flags & GTK_DEBUG_TREE)
    {
      g_print ("_gtk_rbtree_insert_before finished...\n");
      _gtk_rbtree_debug_spew (tree);
      g_print ("\n\n");
      _gtk_rbtree_test (G_STRLOC, tree);
    }
#endif /* G_ENABLE_DEBUG */
  
  return node;
}

GtkRBNode *
_gtk_rbtree_find_count (GtkRBTree *tree,
			gint       count)
{
  GtkRBNode *node;

  node = tree->root;
  while (node != tree->nil && (node->left->count + 1 != count))
    {
      if (node->left->count >= count)
	node = node->left;
      else
	{
	  count -= (node->left->count + 1);
	  node = node->right;
	}
    }
  if (node == tree->nil)
    return NULL;
  return node;
}

void
_gtk_rbtree_node_set_height (GtkRBTree *tree,
			     GtkRBNode *node,
			     gint       height)
{
  gint diff = height - GTK_RBNODE_GET_HEIGHT (node);
  GtkRBNode *tmp_node = node;
  GtkRBTree *tmp_tree = tree;

  if (diff == 0)
    return;

  while (tmp_tree && tmp_node && tmp_node != tmp_tree->nil)
    {
      tmp_node->offset += diff;
      tmp_node = tmp_node->parent;
      if (tmp_node == tmp_tree->nil)
	{
	  tmp_node = tmp_tree->parent_node;
	  tmp_tree = tmp_tree->parent_tree;
	}
    }
#ifdef G_ENABLE_DEBUG  
  if (gtk_debug_flags & GTK_DEBUG_TREE)
    _gtk_rbtree_test (G_STRLOC, tree);
#endif
}

void
_gtk_rbtree_node_mark_invalid (GtkRBTree *tree,
			       GtkRBNode *node)
{
  if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_INVALID))
    return;

  GTK_RBNODE_SET_FLAG (node, GTK_RBNODE_INVALID);
  do
    {
      if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_DESCENDANTS_INVALID))
	return;
      GTK_RBNODE_SET_FLAG (node, GTK_RBNODE_DESCENDANTS_INVALID);
      node = node->parent;
      if (node == tree->nil)
	{
	  node = tree->parent_node;
	  tree = tree->parent_tree;
	}
    }
  while (node);
}

#if 0
/* Draconian version. */
void
_gtk_rbtree_node_mark_invalid (GtkRBTree *tree,
			       GtkRBNode *node)
{
  GTK_RBNODE_SET_FLAG (node, GTK_RBNODE_INVALID);
  do
    {
      _fixup_validation (tree, node);
      node = node->parent;
      if (node == tree->nil)
	{
	  node = tree->parent_node;
	  tree = tree->parent_tree;
	}
    }
  while (node);
}
#endif

void
_gtk_rbtree_node_mark_valid (GtkRBTree *tree,
			     GtkRBNode *node)
{
  if ((!GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_INVALID)) &&
      (!GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_COLUMN_INVALID)))
    return;

  GTK_RBNODE_UNSET_FLAG (node, GTK_RBNODE_INVALID);
  GTK_RBNODE_UNSET_FLAG (node, GTK_RBNODE_COLUMN_INVALID);

  do
    {
      if ((GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_INVALID)) ||
	  (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_COLUMN_INVALID)) ||
	  (node->children && GTK_RBNODE_FLAG_SET (node->children->root, GTK_RBNODE_DESCENDANTS_INVALID)) ||
	  (node->left != tree->nil && GTK_RBNODE_FLAG_SET (node->left, GTK_RBNODE_DESCENDANTS_INVALID)) ||
	  (node->right != tree->nil && GTK_RBNODE_FLAG_SET (node->right, GTK_RBNODE_DESCENDANTS_INVALID)))
	return;

      GTK_RBNODE_UNSET_FLAG (node, GTK_RBNODE_DESCENDANTS_INVALID);
      node = node->parent;
      if (node == tree->nil)
	{
	  node = tree->parent_node;
	  tree = tree->parent_tree;
	}
    }
  while (node);
}

#if 0
/* Draconian version */
void
_gtk_rbtree_node_mark_valid (GtkRBTree *tree,
			     GtkRBNode *node)
{
  GTK_RBNODE_UNSET_FLAG (node, GTK_RBNODE_INVALID);
  GTK_RBNODE_UNSET_FLAG (node, GTK_RBNODE_COLUMN_INVALID);

  do
    {
      _fixup_validation (tree, node);
      node = node->parent;
      if (node == tree->nil)
	{
	  node = tree->parent_node;
	  tree = tree->parent_tree;
	}
    }
  while (node);
}
#endif
/* Assume tree is the root node as it doesn't set DESCENDANTS_INVALID above.
 */
void
_gtk_rbtree_column_invalid (GtkRBTree *tree)
{
  GtkRBNode *node;

  if (tree == NULL)
    return;
  node = tree->root;
  g_assert (node);

  while (node->left != tree->nil)
    node = node->left;

  do
    {
      if (! (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_INVALID)))
	GTK_RBNODE_SET_FLAG (node, GTK_RBNODE_COLUMN_INVALID);
      GTK_RBNODE_SET_FLAG (node, GTK_RBNODE_DESCENDANTS_INVALID);

      if (node->children)
	_gtk_rbtree_column_invalid (node->children);
    }
  while ((node = _gtk_rbtree_next (tree, node)) != NULL);
}

void
_gtk_rbtree_mark_invalid (GtkRBTree *tree)
{
  GtkRBNode *node;

  if (tree == NULL)
    return;
  node = tree->root;
  g_assert (node);

  while (node->left != tree->nil)
    node = node->left;

  do
    {
      GTK_RBNODE_SET_FLAG (node, GTK_RBNODE_INVALID);
      GTK_RBNODE_SET_FLAG (node, GTK_RBNODE_DESCENDANTS_INVALID);

      if (node->children)
	_gtk_rbtree_mark_invalid (node->children);
    }
  while ((node = _gtk_rbtree_next (tree, node)) != NULL);
}

void
_gtk_rbtree_set_fixed_height (GtkRBTree *tree,
			      gint       height,
			      gboolean   mark_valid)
{
  GtkRBNode *node;

  if (tree == NULL)
    return;

  node = tree->root;
  g_assert (node);

  while (node->left != tree->nil)
    node = node->left;

  do
    {
      if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_INVALID))
        {
	  _gtk_rbtree_node_set_height (tree, node, height);
	  if (mark_valid)
	    _gtk_rbtree_node_mark_valid (tree, node);
	}

      if (node->children)
	_gtk_rbtree_set_fixed_height (node->children, height, mark_valid);
    }
  while ((node = _gtk_rbtree_next (tree, node)) != NULL);
}

typedef struct _GtkRBReorder
{
  GtkRBTree *children;
  gint height;
  gint flags;
  gint order;
  gint invert_order;
  gint parity;
} GtkRBReorder;

static int
gtk_rbtree_reorder_sort_func (gconstpointer a,
			      gconstpointer b)
{
  return ((GtkRBReorder *) a)->order - ((GtkRBReorder *) b)->order;
}

static int
gtk_rbtree_reorder_invert_func (gconstpointer a,
				gconstpointer b)
{
  return ((GtkRBReorder *) a)->invert_order - ((GtkRBReorder *) b)->invert_order;
}

static void
gtk_rbtree_reorder_fixup (GtkRBTree *tree,
			  GtkRBNode *node)
{
  if (node == tree->nil)
    return;

  node->parity = 1;

  if (node->left != tree->nil)
    {
      gtk_rbtree_reorder_fixup (tree, node->left);
      node->offset += node->left->offset;
      node->parity += node->left->parity;
    }
  if (node->right != tree->nil)
    {
      gtk_rbtree_reorder_fixup (tree, node->right);
      node->offset += node->right->offset;
      node->parity += node->right->parity;
    }
      
  if (node->children)
    {
      node->offset += node->children->root->offset;
      node->parity += node->children->root->parity;
    }
  
  if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_INVALID) ||
      (node->right != tree->nil && GTK_RBNODE_FLAG_SET (node->right, GTK_RBNODE_DESCENDANTS_INVALID)) ||
      (node->left != tree->nil && GTK_RBNODE_FLAG_SET (node->left, GTK_RBNODE_DESCENDANTS_INVALID)) ||
      (node->children && GTK_RBNODE_FLAG_SET (node->children->root, GTK_RBNODE_DESCENDANTS_INVALID)))
    GTK_RBNODE_SET_FLAG (node, GTK_RBNODE_DESCENDANTS_INVALID);
  else
    GTK_RBNODE_UNSET_FLAG (node, GTK_RBNODE_DESCENDANTS_INVALID);
}

/* It basically pulls everything out of the tree, rearranges it, and puts it
 * back together.  Our strategy is to keep the old RBTree intact, and just
 * rearrange the contents.  When that is done, we go through and update the
 * heights.  There is probably a more elegant way to write this function.  If
 * anyone wants to spend the time writing it, patches will be accepted.
 */
void
_gtk_rbtree_reorder (GtkRBTree *tree,
		     gint      *new_order,
		     gint       length)
{
  GtkRBReorder reorder = { NULL };
  GArray *array;
  GtkRBNode *node;
  gint i;
  
  g_return_if_fail (tree != NULL);
  g_return_if_fail (length > 0);
  g_return_if_fail (tree->root->count == length);
  
  /* Sort the trees values in the new tree. */
  array = g_array_sized_new (FALSE, FALSE, sizeof (GtkRBReorder), length);
  for (i = 0; i < length; i++)
    {
      reorder.order = new_order[i];
      reorder.invert_order = i;
      g_array_append_val (array, reorder);
    }

  g_array_sort(array, gtk_rbtree_reorder_sort_func);

  /* rewind node*/
  node = tree->root;
  while (node && node->left != tree->nil)
    node = node->left;

  for (i = 0; i < length; i++)
    {
      g_assert (node != tree->nil);
      g_array_index (array, GtkRBReorder, i).children = node->children;
      g_array_index (array, GtkRBReorder, i).flags = GTK_RBNODE_NON_COLORS & node->flags;
      g_array_index (array, GtkRBReorder, i).height = GTK_RBNODE_GET_HEIGHT (node);

      node = _gtk_rbtree_next (tree, node);
    }

  g_array_sort (array, gtk_rbtree_reorder_invert_func);
 
  /* rewind node*/
  node = tree->root;
  while (node && node->left != tree->nil)
    node = node->left;

  /* Go through the tree and change the values to the new ones. */
  for (i = 0; i < length; i++)
    {
      reorder = g_array_index (array, GtkRBReorder, i);
      node->children = reorder.children;
      if (node->children)
	node->children->parent_node = node;
      node->flags = GTK_RBNODE_GET_COLOR (node) | reorder.flags;
      /* We temporarily set the height to this. */
      node->offset = reorder.height;
      node = _gtk_rbtree_next (tree, node);
    }
  gtk_rbtree_reorder_fixup (tree, tree->root);

  g_array_free (array, TRUE);
}


gint
_gtk_rbtree_node_find_offset (GtkRBTree *tree,
			      GtkRBNode *node)
{
  GtkRBNode *last;
  gint retval;

  g_assert (node);
  g_assert (node->left);
  
  retval = node->left->offset;

  while (tree && node && node != tree->nil)
    {
      last = node;
      node = node->parent;

      /* Add left branch, plus children, iff we came from the right */
      if (node->right == last)
	retval += node->offset - node->right->offset;
      
      if (node == tree->nil)
	{
	  node = tree->parent_node;
	  tree = tree->parent_tree;

          /* Add the parent node, plus the left branch. */
	  if (node)
	    retval += node->left->offset + GTK_RBNODE_GET_HEIGHT (node);
	}
    }
  return retval;
}

gint
_gtk_rbtree_node_find_parity (GtkRBTree *tree,
                              GtkRBNode *node)
{
  GtkRBNode *last;
  gint retval;  
  
  g_assert (node);
  g_assert (node->left);
  
  retval = node->left->parity;

  while (tree && node && node != tree->nil)
    {
      last = node;
      node = node->parent;

      /* Add left branch, plus children, iff we came from the right */
      if (node->right == last)
	retval += node->parity - node->right->parity;
      
      if (node == tree->nil)
	{
	  node = tree->parent_node;
	  tree = tree->parent_tree;

          /* Add the parent node, plus the left branch. */
	  if (node)
	    retval += node->left->parity + 1; /* 1 == GTK_RBNODE_GET_PARITY() */
	}
    }
  
  return retval % 2;
}

gint
_gtk_rbtree_real_find_offset (GtkRBTree  *tree,
			      gint        height,
			      GtkRBTree **new_tree,
			      GtkRBNode **new_node)
{
  GtkRBNode *tmp_node;

  g_assert (tree);

  if (height < 0)
    {
      *new_tree = NULL;
      *new_node = NULL;

      return 0;
    }
  
    
  tmp_node = tree->root;
  while (tmp_node != tree->nil &&
	 (tmp_node->left->offset > height ||
	  (tmp_node->offset - tmp_node->right->offset) < height))
    {
      if (tmp_node->left->offset > height)
	tmp_node = tmp_node->left;
      else
	{
	  height -= (tmp_node->offset - tmp_node->right->offset);
	  tmp_node = tmp_node->right;
	}
    }
  if (tmp_node == tree->nil)
    {
      *new_tree = NULL;
      *new_node = NULL;
      return 0;
    }
  if (tmp_node->children)
    {
      if ((tmp_node->offset -
	   tmp_node->right->offset -
	   tmp_node->children->root->offset) > height)
	{
	  *new_tree = tree;
	  *new_node = tmp_node;
	  return (height - tmp_node->left->offset);
	}
      return _gtk_rbtree_real_find_offset (tmp_node->children,
					   height - tmp_node->left->offset -
					   (tmp_node->offset -
					    tmp_node->left->offset -
					    tmp_node->right->offset -
					    tmp_node->children->root->offset),
					   new_tree,
					   new_node);
    }
  *new_tree = tree;
  *new_node = tmp_node;
  return (height - tmp_node->left->offset);
}

gint
_gtk_rbtree_find_offset (GtkRBTree  *tree,
			      gint        height,
			      GtkRBTree **new_tree,
			      GtkRBNode **new_node)
{
  g_assert (tree);

  if ((height < 0) ||
      (height >= tree->root->offset))
    {
      *new_tree = NULL;
      *new_node = NULL;

      return 0;
    }
  return _gtk_rbtree_real_find_offset (tree, height, new_tree, new_node);
}

void
_gtk_rbtree_remove_node (GtkRBTree *tree,
			 GtkRBNode *node)
{
  GtkRBNode *x, *y;
  GtkRBTree *tmp_tree;
  GtkRBNode *tmp_node;
  gint y_height;
  
  g_return_if_fail (tree != NULL);
  g_return_if_fail (node != NULL);

  
#ifdef G_ENABLE_DEBUG
  if (gtk_debug_flags & GTK_DEBUG_TREE)
    {
      g_print ("\n\n_gtk_rbtree_remove_node: %p\n", node);
      _gtk_rbtree_debug_spew (tree);
      _gtk_rbtree_test (G_STRLOC, tree);
    }
#endif /* G_ENABLE_DEBUG */
  
  /* make sure we're deleting a node that's actually in the tree */
  for (x = node; x->parent != tree->nil; x = x->parent)
    ;
  g_return_if_fail (x == tree->root);

#ifdef G_ENABLE_DEBUG  
  if (gtk_debug_flags & GTK_DEBUG_TREE)
    _gtk_rbtree_test (G_STRLOC, tree);
#endif
  
  if (node->left == tree->nil || node->right == tree->nil)
    {
      y = node;
    }
  else
    {
      y = node->right;

      while (y->left != tree->nil)
	y = y->left;
    }

  /* adjust count only beneath tree */
  for (x = y; x != tree->nil; x = x->parent)
    {
      x->count--;
    }

  /* offsets and parity adjust all the way up through parent trees */
  y_height = GTK_RBNODE_GET_HEIGHT (y);

  tmp_tree = tree;
  tmp_node = y;
  while (tmp_tree && tmp_node && tmp_node != tmp_tree->nil)
    {
      tmp_node->offset -= (y_height + (y->children?y->children->root->offset:0));
      _fixup_validation (tmp_tree, tmp_node);
      _fixup_parity (tmp_tree, tmp_node);
      tmp_node = tmp_node->parent;
      if (tmp_node == tmp_tree->nil)
	{
	  tmp_node = tmp_tree->parent_node;
	  tmp_tree = tmp_tree->parent_tree;
	}
    }

  /* x is y's only child, or nil */
  if (y->left != tree->nil)
    x = y->left;
  else
    x = y->right;

  /* remove y from the parent chain */
  x->parent = y->parent;
  if (y->parent != tree->nil)
    {
      if (y == y->parent->left)
	y->parent->left = x;
      else
	y->parent->right = x;
    }
  else
    {
      tree->root = x;
    }

  /* We need to clean up the validity of the tree.
   */

  tmp_tree = tree;
  tmp_node = x;
  do
    {
      /* We skip the first time, iff x is nil */
      if (tmp_node != tmp_tree->nil)
	{
	  _fixup_validation (tmp_tree, tmp_node);
	  _fixup_parity (tmp_tree, tmp_node);
	}
      tmp_node = tmp_node->parent;
      if (tmp_node == tmp_tree->nil)
	{
	  tmp_node = tmp_tree->parent_node;
	  tmp_tree = tmp_tree->parent_tree;
	}
    }
  while (tmp_tree != NULL);

  if (y != node)
    {
      gint diff;

      /* Copy the node over */
      if (GTK_RBNODE_GET_COLOR (node) == GTK_RBNODE_BLACK)
	node->flags = ((y->flags & (GTK_RBNODE_NON_COLORS)) | GTK_RBNODE_BLACK);
      else
	node->flags = ((y->flags & (GTK_RBNODE_NON_COLORS)) | GTK_RBNODE_RED);
      node->children = y->children;
      if (y->children)
	{
	  node->children = y->children;
	  node->children->parent_node = node;
	}
      else
	{
	  node->children = NULL;
	}
      _fixup_validation (tree, node);
      _fixup_parity (tree, node);
      /* We want to see how different our height is from the previous node.
       * To do this, we compare our current height with our supposed height.
       */
      diff = y_height - GTK_RBNODE_GET_HEIGHT (node);
      tmp_tree = tree;
      tmp_node = node;

      while (tmp_tree && tmp_node && tmp_node != tmp_tree->nil)
	{
	  tmp_node->offset += diff;
	  _fixup_validation (tmp_tree, tmp_node);
	  _fixup_parity (tmp_tree, tmp_node);
	  tmp_node = tmp_node->parent;
	  if (tmp_node == tmp_tree->nil)
	    {
	      tmp_node = tmp_tree->parent_node;
	      tmp_tree = tmp_tree->parent_tree;
	    }
	}
    }

  if (GTK_RBNODE_GET_COLOR (y) == GTK_RBNODE_BLACK)
    _gtk_rbtree_remove_node_fixup (tree, x);
  _gtk_rbnode_free (y);

#ifdef G_ENABLE_DEBUG  
  if (gtk_debug_flags & GTK_DEBUG_TREE)
    {
      g_print ("_gtk_rbtree_remove_node finished...\n");
      _gtk_rbtree_debug_spew (tree);
      g_print ("\n\n");
      _gtk_rbtree_test (G_STRLOC, tree);
    }
#endif /* G_ENABLE_DEBUG */  
}

GtkRBNode *
_gtk_rbtree_next (GtkRBTree *tree,
		  GtkRBNode *node)
{
  g_return_val_if_fail (tree != NULL, NULL);
  g_return_val_if_fail (node != NULL, NULL);

  /* Case 1: the node's below us. */
  if (node->right != tree->nil)
    {
      node = node->right;
      while (node->left != tree->nil)
	node = node->left;
      return node;
    }

  /* Case 2: it's an ancestor */
  while (node->parent != tree->nil)
    {
      if (node->parent->right == node)
	node = node->parent;
      else
	return (node->parent);
    }

  /* Case 3: There is no next node */
  return NULL;
}

GtkRBNode *
_gtk_rbtree_prev (GtkRBTree *tree,
		  GtkRBNode *node)
{
  g_return_val_if_fail (tree != NULL, NULL);
  g_return_val_if_fail (node != NULL, NULL);

  /* Case 1: the node's below us. */
  if (node->left != tree->nil)
    {
      node = node->left;
      while (node->right != tree->nil)
	node = node->right;
      return node;
    }

  /* Case 2: it's an ancestor */
  while (node->parent != tree->nil)
    {
      if (node->parent->left == node)
	node = node->parent;
      else
	return (node->parent);
    }

  /* Case 3: There is no next node */
  return NULL;
}

void
_gtk_rbtree_next_full (GtkRBTree  *tree,
		       GtkRBNode  *node,
		       GtkRBTree **new_tree,
		       GtkRBNode **new_node)
{
  g_return_if_fail (tree != NULL);
  g_return_if_fail (node != NULL);
  g_return_if_fail (new_tree != NULL);
  g_return_if_fail (new_node != NULL);

  if (node->children)
    {
      *new_tree = node->children;
      *new_node = (*new_tree)->root;
      while ((*new_node)->left != (*new_tree)->nil)
	*new_node = (*new_node)->left;
      return;
    }

  *new_tree = tree;
  *new_node = _gtk_rbtree_next (tree, node);

  while ((*new_node == NULL) &&
	 (*new_tree != NULL))
    {
      *new_node = (*new_tree)->parent_node;
      *new_tree = (*new_tree)->parent_tree;
      if (*new_tree)
	*new_node = _gtk_rbtree_next (*new_tree, *new_node);
    }
}

void
_gtk_rbtree_prev_full (GtkRBTree  *tree,
		       GtkRBNode  *node,
		       GtkRBTree **new_tree,
		       GtkRBNode **new_node)
{
  g_return_if_fail (tree != NULL);
  g_return_if_fail (node != NULL);
  g_return_if_fail (new_tree != NULL);
  g_return_if_fail (new_node != NULL);

  *new_tree = tree;
  *new_node = _gtk_rbtree_prev (tree, node);

  if (*new_node == NULL)
    {
      *new_node = (*new_tree)->parent_node;
      *new_tree = (*new_tree)->parent_tree;
    }
  else
    {
      while ((*new_node)->children)
	{
	  *new_tree = (*new_node)->children;
	  *new_node = (*new_tree)->root;
	  while ((*new_node)->right != (*new_tree)->nil)
	    *new_node = (*new_node)->right;
	}
    }
}

gint
_gtk_rbtree_get_depth (GtkRBTree *tree)
{
  GtkRBTree *tmp_tree;
  gint depth = 0;

  tmp_tree = tree->parent_tree;
  while (tmp_tree)
    {
      ++depth;
      tmp_tree = tmp_tree->parent_tree;
    }

  return depth;
}

static void
_gtk_rbtree_traverse_pre_order (GtkRBTree             *tree,
				GtkRBNode             *node,
				GtkRBTreeTraverseFunc  func,
				gpointer               data)
{
  if (node == tree->nil)
    return;

  (* func) (tree, node, data);
  _gtk_rbtree_traverse_pre_order (tree, node->left, func, data);
  _gtk_rbtree_traverse_pre_order (tree, node->right, func, data);
}

static void
_gtk_rbtree_traverse_post_order (GtkRBTree             *tree,
				 GtkRBNode             *node,
				 GtkRBTreeTraverseFunc  func,
				 gpointer               data)
{
  if (node == tree->nil)
    return;

  _gtk_rbtree_traverse_post_order (tree, node->left, func, data);
  _gtk_rbtree_traverse_post_order (tree, node->right, func, data);
  (* func) (tree, node, data);
}

void
_gtk_rbtree_traverse (GtkRBTree             *tree,
		      GtkRBNode             *node,
		      GTraverseType          order,
		      GtkRBTreeTraverseFunc  func,
		      gpointer               data)
{
  g_return_if_fail (tree != NULL);
  g_return_if_fail (node != NULL);
  g_return_if_fail (func != NULL);
  g_return_if_fail (order <= G_LEVEL_ORDER);

  switch (order)
    {
    case G_PRE_ORDER:
      _gtk_rbtree_traverse_pre_order (tree, node, func, data);
      break;
    case G_POST_ORDER:
      _gtk_rbtree_traverse_post_order (tree, node, func, data);
      break;
    case G_IN_ORDER:
    case G_LEVEL_ORDER:
    default:
      g_warning ("unsupported traversal order.");
      break;
    }
}

static inline
void _fixup_validation (GtkRBTree *tree,
			GtkRBNode *node)
{
  if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_INVALID) ||
      GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_COLUMN_INVALID) ||
      (node->left != tree->nil && GTK_RBNODE_FLAG_SET (node->left, GTK_RBNODE_DESCENDANTS_INVALID)) ||
      (node->right != tree->nil && GTK_RBNODE_FLAG_SET (node->right, GTK_RBNODE_DESCENDANTS_INVALID)) ||
      (node->children != NULL && GTK_RBNODE_FLAG_SET (node->children->root, GTK_RBNODE_DESCENDANTS_INVALID)))
    {
      GTK_RBNODE_SET_FLAG (node, GTK_RBNODE_DESCENDANTS_INVALID);
    }
  else
    {
      GTK_RBNODE_UNSET_FLAG (node, GTK_RBNODE_DESCENDANTS_INVALID);
    }
}

static inline
void _fixup_parity (GtkRBTree *tree,
		    GtkRBNode *node)
{
  node->parity = 1 +
    ((node->children != NULL && node->children->root != node->children->nil) ? node->children->root->parity : 0) + 
    ((node->left != tree->nil) ? node->left->parity : 0) + 
    ((node->right != tree->nil) ? node->right->parity : 0);
}

#ifdef G_ENABLE_DEBUG
static guint
get_parity (GtkRBNode *node)
{
  guint child_total = 0;
  guint rem;

  /* The parity of a node is node->parity minus
   * the parity of left, right, and children.
   *
   * This is equivalent to saying that if left, right, children
   * sum to 0 parity, then node->parity is the parity of node,
   * and if left, right, children are odd parity, then
   * node->parity is the reverse of the node's parity.
   */
  
  child_total += (guint) node->left->parity;
  child_total += (guint) node->right->parity;

  if (node->children)
    child_total += (guint) node->children->root->parity;

  rem = child_total % 2;
  
  if (rem == 0)
    return node->parity;
  else
    return !node->parity;
}

static guint
count_parity (GtkRBTree *tree,
              GtkRBNode *node)
{
  guint res;
  
  if (node == tree->nil)
    return 0;
  
  res =
    count_parity (tree, node->left) +
    count_parity (tree, node->right) +
    (guint)1 +
    (node->children ? count_parity (node->children, node->children->root) : 0);

  res = res % (guint)2;
  
  if (res != node->parity)
    g_print ("parity incorrect for node\n");

  if (get_parity (node) != 1)
    g_error ("Node has incorrect parity %u", get_parity (node));
  
  return res;
}

static gint
_count_nodes (GtkRBTree *tree,
              GtkRBNode *node)
{
  gint res;
  if (node == tree->nil)
    return 0;

  g_assert (node->left);
  g_assert (node->right);

  res = (_count_nodes (tree, node->left) +
         _count_nodes (tree, node->right) + 1);

  if (res != node->count)
    g_print ("Tree failed\n");
  return res;
}

static void
_gtk_rbtree_test_height (GtkRBTree *tree,
                         GtkRBNode *node)
{
  gint computed_offset = 0;

  /* This whole test is sort of a useless truism. */
  
  if (node->left != tree->nil)
    computed_offset += node->left->offset;

  if (node->right != tree->nil)
    computed_offset += node->right->offset;

  if (node->children && node->children->root != node->children->nil)
    computed_offset += node->children->root->offset;

  if (GTK_RBNODE_GET_HEIGHT (node) + computed_offset != node->offset)
    g_error ("node has broken offset\n");

  if (node->left != tree->nil)
    _gtk_rbtree_test_height (tree, node->left);

  if (node->right != tree->nil)
    _gtk_rbtree_test_height (tree, node->right);

  if (node->children && node->children->root != node->children->nil)
    _gtk_rbtree_test_height (node->children, node->children->root);
}

static void
_gtk_rbtree_test_dirty (GtkRBTree *tree,
			GtkRBNode *node,
			 gint      expected_dirtyness)
{

  if (expected_dirtyness)
    {
      g_assert (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_COLUMN_INVALID) ||
		GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_INVALID) ||
		(node->left != tree->nil && GTK_RBNODE_FLAG_SET (node->left, GTK_RBNODE_DESCENDANTS_INVALID)) ||
		(node->right != tree->nil && GTK_RBNODE_FLAG_SET (node->right, GTK_RBNODE_DESCENDANTS_INVALID)) ||
		(node->children && GTK_RBNODE_FLAG_SET (node->children->root, GTK_RBNODE_DESCENDANTS_INVALID)));
    }
  else
    {
      g_assert (! GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_COLUMN_INVALID) &&
		! GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_INVALID));
      if (node->left != tree->nil)
	g_assert (! GTK_RBNODE_FLAG_SET (node->left, GTK_RBNODE_DESCENDANTS_INVALID));
      if (node->right != tree->nil)
	g_assert (! GTK_RBNODE_FLAG_SET (node->right, GTK_RBNODE_DESCENDANTS_INVALID));
      if (node->children != NULL)
	g_assert (! GTK_RBNODE_FLAG_SET (node->children->root, GTK_RBNODE_DESCENDANTS_INVALID));
    }

  if (node->left != tree->nil)
    _gtk_rbtree_test_dirty (tree, node->left, GTK_RBNODE_FLAG_SET (node->left, GTK_RBNODE_DESCENDANTS_INVALID));
  if (node->right != tree->nil)
    _gtk_rbtree_test_dirty (tree, node->right, GTK_RBNODE_FLAG_SET (node->right, GTK_RBNODE_DESCENDANTS_INVALID));
  if (node->children != NULL && node->children->root != node->children->nil)
    _gtk_rbtree_test_dirty (node->children, node->children->root, GTK_RBNODE_FLAG_SET (node->children->root, GTK_RBNODE_DESCENDANTS_INVALID));
}

static void _gtk_rbtree_test_structure (GtkRBTree *tree);

static void
_gtk_rbtree_test_structure_helper (GtkRBTree *tree,
				   GtkRBNode *node)
{
  g_assert (node != tree->nil);

  g_assert (node->left != NULL);
  g_assert (node->right != NULL);
  g_assert (node->parent != NULL);

  if (node->left != tree->nil)
    {
      g_assert (node->left->parent == node);
      _gtk_rbtree_test_structure_helper (tree, node->left);
    }
  if (node->right != tree->nil)
    {
      g_assert (node->right->parent == node);
      _gtk_rbtree_test_structure_helper (tree, node->right);
    }

  if (node->children != NULL)
    {
      g_assert (node->children->parent_tree == tree);
      g_assert (node->children->parent_node == node);

      _gtk_rbtree_test_structure (node->children);
    }
}
static void
_gtk_rbtree_test_structure (GtkRBTree *tree)
{
  g_assert (tree->root);
  if (tree->root == tree->nil)
    return;

  g_assert (tree->root->parent == tree->nil);
  _gtk_rbtree_test_structure_helper (tree, tree->root);
}

void
_gtk_rbtree_test (const gchar *where,
                  GtkRBTree   *tree)
{
  GtkRBTree *tmp_tree;

  if (tree == NULL)
    return;

  /* Test the entire tree */
  tmp_tree = tree;
  while (tmp_tree->parent_tree)
    tmp_tree = tmp_tree->parent_tree;
  
  g_assert (tmp_tree->nil != NULL);

  if (tmp_tree->root == tmp_tree->nil)
    return;

  _gtk_rbtree_test_structure (tmp_tree);

  g_assert ((_count_nodes (tmp_tree, tmp_tree->root->left) +
	     _count_nodes (tmp_tree, tmp_tree->root->right) + 1) == tmp_tree->root->count);
      
      
  _gtk_rbtree_test_height (tmp_tree, tmp_tree->root);
  _gtk_rbtree_test_dirty (tmp_tree, tmp_tree->root, GTK_RBNODE_FLAG_SET (tmp_tree->root, GTK_RBNODE_DESCENDANTS_INVALID));
  g_assert (count_parity (tmp_tree, tmp_tree->root) == tmp_tree->root->parity);
}

static void
_gtk_rbtree_debug_spew_helper (GtkRBTree *tree,
			       GtkRBNode *node,
			       gint       depth)
{
  gint i;
  for (i = 0; i < depth; i++)
    g_print ("\t");

  g_print ("(%p - %s) (Offset %d) (Parity %d) (Validity %d%d%d)\n",
	   node,
	   (GTK_RBNODE_GET_COLOR (node) == GTK_RBNODE_BLACK)?"BLACK":" RED ",
	   node->offset,
	   node->parity?1:0,
	   (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_DESCENDANTS_INVALID))?1:0,
	   (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_INVALID))?1:0,
	   (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_COLUMN_INVALID))?1:0);
  if (node->children != NULL)
    {
      g_print ("Looking at child.\n");
      _gtk_rbtree_debug_spew (node->children);
      g_print ("Done looking at child.\n");
    }
  if (node->left != tree->nil)
    {
      _gtk_rbtree_debug_spew_helper (tree, node->left, depth+1);
    }
  if (node->right != tree->nil)
    {
      _gtk_rbtree_debug_spew_helper (tree, node->right, depth+1);
    }
}

void
_gtk_rbtree_debug_spew (GtkRBTree *tree)
{
  g_return_if_fail (tree != NULL);

  if (tree->root == tree->nil)
    g_print ("Empty tree...\n");
  else
    _gtk_rbtree_debug_spew_helper (tree, tree->root, 0);
}
#endif /* G_ENABLE_DEBUG */
