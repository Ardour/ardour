#define GNOME_CANVAS_PATH_DEF_C

/*
 * GnomeCanvasPathDef
 *
 * (C) 1999-2000 Lauris Kaplinski <lauris@ximian.com>
 * Released under LGPL
 *
 * Authors: Lauris Kaplinski <lauris@ximian.com>
 *          Rusty Conover <rconover@bangtail.net>
 *
 * Copyright 1999-2001 Ximian Inc. and authors.
 */

#include <string.h>
#include <libart_lgpl/art_misc.h>
#include "gnome-canvas-path-def.h"

/* The number of points to allocate at once */
#define GNOME_CANVAS_PATH_DEF_LENSTEP 32

struct _GnomeCanvasPathDef {
	gint refcount;
	ArtBpath * bpath;
	gint end;		/* ART_END position */
	gint length;		/* Num allocated Bpaths */
	gint substart;		/* subpath start */
	gdouble x, y;		/* previous moveto position */
	guint sbpath : 1;	/* Bpath is static */
	guint hascpt : 1;	/* Currentpoint is defined */
	guint posset : 1;	/* Previous was moveto */
	guint moving : 1;	/* Bpath end is moving */
	guint allclosed : 1;	/* All subpaths are closed */
	guint allopen : 1;	/* All subpaths are open */
};

static gboolean sp_bpath_good (ArtBpath * bpath);
static ArtBpath * sp_bpath_check_subpath (ArtBpath * bpath);
static gint sp_bpath_length (const ArtBpath * bpath);
static gboolean sp_bpath_all_closed (const ArtBpath * bpath);
static gboolean sp_bpath_all_open (const ArtBpath * bpath);

/* Boxed Type Support */

static GnomeCanvasPathDef *
path_def_boxed_copy (GnomeCanvasPathDef * path_def)
{
	if (path_def)
		gnome_canvas_path_def_ref (path_def);
	return path_def;
}

GType
gnome_canvas_path_def_get_type (void)
{
	static GType t = 0;
	if (t == 0)
		t = g_boxed_type_register_static
				("GnomeCanvasPathDef",
				 (GBoxedCopyFunc)path_def_boxed_copy,
				 (GBoxedFreeFunc)gnome_canvas_path_def_unref);
	return t;
}

/* Constructors */

/**
 * gnome_canvas_path_def_new:
 * 
 * This function creates a new empty #gnome_canvas_path_def.
 *
 * Returns: the new canvas path definition. 
 */
GnomeCanvasPathDef *
gnome_canvas_path_def_new (void)
{
	GnomeCanvasPathDef * path;

	path = gnome_canvas_path_def_new_sized (GNOME_CANVAS_PATH_DEF_LENSTEP);

	return path;
}

/**
 * gnome_canvas_path_def_new_sized:
 * @length: number of points to allocate for the path
 *
 * This function creates a new #gnome_canvas_path_def with @length
 * number of points allocated. It is useful, if you know the exact
 * number of points in path, so you can avoid automatic point
 * array reallocation.
 *
 * Returns: the new canvas path definition
 */
GnomeCanvasPathDef *
gnome_canvas_path_def_new_sized (gint length)
{
	GnomeCanvasPathDef * path;

	g_return_val_if_fail (length > 0, NULL);

	path = g_new (GnomeCanvasPathDef, 1);

	path->refcount = 1;
	path->bpath = art_new (ArtBpath, length);
	path->end = 0;
	path->bpath[path->end].code = ART_END;
	path->length = length;
	path->sbpath = FALSE;
	path->hascpt = FALSE;
	path->posset = FALSE;
	path->moving = FALSE;
	path->allclosed = TRUE;
	path->allopen = TRUE;

	return path;
}

/**
 * gnome_canvas_path_def_new_from_bpath:
 * @bpath: libart bezier path
 *
 * This function constructs a new #gnome_canvas_path_def and uses the
 * passed @bpath as the contents.  The passed bpath should not be
 * static as the path definition is editable when constructed with
 * this function. Also, passed bpath will be freed with art_free, if
 * path is destroyed, so use it with caution.
 * For constructing a #gnome_canvas_path_def
 * from (non-modifiable) bpath use
 * #gnome_canvas_path_def_new_from_static_bpath.
 *
 * Returns: the new canvas path definition that is populated with the
 * passed bezier path, if the @bpath is bad NULL is returned.
 */
GnomeCanvasPathDef *
gnome_canvas_path_def_new_from_bpath (ArtBpath * bpath)
{
	GnomeCanvasPathDef * path;

	g_return_val_if_fail (sp_bpath_good (bpath), NULL);

	path = g_new (GnomeCanvasPathDef, 1);

	path->refcount = 1;
	path->bpath = bpath;
	path->length = sp_bpath_length (bpath);
	path->end = path->length - 1;
	path->sbpath = FALSE;
	path->hascpt = FALSE;
	path->posset = FALSE;
	path->moving = FALSE;
	path->allclosed = sp_bpath_all_closed (bpath);
	path->allopen = sp_bpath_all_open (bpath);

	return path;
}

/**
 * gnome_canvas_path_def_new_from_static_bpath:
 * @bpath: libart bezier path
 *
 * This function constructs a new #gnome_canvas_path_def and
 * references the passed @bpath as its contents.  The
 * #gnome_canvas_path_def returned from this function is to be
 * considered static and non-editable (meaning you cannot change the
 * path from what you passed in @bpath). The bpath will not be freed,
 * if path will be destroyed, so use it with caution.
 *
 * Returns: the new canvas path definition that references the passed
 * @bpath, or if the path is bad NULL is returned.
 */
GnomeCanvasPathDef *
gnome_canvas_path_def_new_from_static_bpath (ArtBpath * bpath)
{
	GnomeCanvasPathDef * path;

	g_return_val_if_fail (sp_bpath_good (bpath), NULL);

	path = g_new (GnomeCanvasPathDef, 1);

	path->refcount = 1;
	path->bpath = bpath;
	path->length = sp_bpath_length (bpath);
	path->end = path->length - 1;
	path->sbpath = TRUE;
	path->hascpt = FALSE;
	path->posset = FALSE;
	path->moving = FALSE;
	path->allclosed = sp_bpath_all_closed (bpath);
	path->allopen = sp_bpath_all_open (bpath);

	return path;
}

/**
 * gnome_canvas_path_def_new_from_foreign_bpath:
 * @bpath: libart bezier path
 *
 * This function constructs a new #gnome_canvas_path_def and
 * duplicates the contents of the passed @bpath in the definition.
 *
 * Returns: the newly created #gnome_canvas_path_def or NULL if the
 * path is invalid.
 */
GnomeCanvasPathDef *
gnome_canvas_path_def_new_from_foreign_bpath (ArtBpath * bpath)
{
	GnomeCanvasPathDef * path;
	gint length;

	g_return_val_if_fail (sp_bpath_good (bpath), NULL);

	length = sp_bpath_length (bpath);

	path = gnome_canvas_path_def_new_sized (length);
	memcpy (path->bpath, bpath, sizeof (ArtBpath) * length);
	path->end = length - 1;
	path->allclosed = sp_bpath_all_closed (bpath);
	path->allopen = sp_bpath_all_open (bpath);

	return path;
}

/**
 * gnome_canvas_path_def_ref:
 * @path: a GnomeCanvasPathDef
 *
 * Increment the reference count of the GnomeCanvasPathDef.
 */
void
gnome_canvas_path_def_ref (GnomeCanvasPathDef * path)
{
	g_return_if_fail (path != NULL);

	path->refcount++;
}

/**
 * gnome_canvas_path_def_finish:
 * @path: a GnomeCanvasPathDef
 *
 * Trims dynamic point array to exact length of path.
 */
void
gnome_canvas_path_def_finish (GnomeCanvasPathDef * path)
{
	g_return_if_fail (path != NULL);
	g_return_if_fail (path->sbpath);

	if ((path->end + 1) < path->length) {
		path->bpath = art_renew (path->bpath, ArtBpath, path->end + 1);
		path->length = path->end + 1;
	}

	path->hascpt = FALSE;
	path->posset = FALSE;
	path->moving = FALSE;
}
/**
 * gnome_canvas_path_def_ensure_space:
 * @path: a GnomeCanvasPathDef
 * @space: number of points to guarantee are allocated at the end of
 * the path.
 *
 * This function ensures that enough space for @space points is
 * allocated at the end of the path.
 */
void
gnome_canvas_path_def_ensure_space (GnomeCanvasPathDef * path, gint space)
{
	g_return_if_fail (path != NULL);
	g_return_if_fail (space > 0);

	if (path->end + space < path->length) return;

	if (space < GNOME_CANVAS_PATH_DEF_LENSTEP) space = GNOME_CANVAS_PATH_DEF_LENSTEP;

	path->bpath = art_renew (path->bpath, ArtBpath, path->length + space);

	path->length += space;
}

/**
 * gnome_canvas_path_def_copy:
 * @dst: a GnomeCanvasPathDef where the contents of @src will be stored.
 * @src: a GnomeCanvasPathDef whose contents will be copied it @src.
 *
 * This function copies the contents @src to @dest. The old @dest path
 * array is freed and @dest is marked as non-static (editable),
 * regardless of the status of @src.
 */
void 
gnome_canvas_path_def_copy (GnomeCanvasPathDef * dst, const GnomeCanvasPathDef * src)
{
	g_return_if_fail (dst != NULL);
	g_return_if_fail (src != NULL);

	if (!dst->sbpath) g_free (dst->bpath);

	memcpy (dst, src, sizeof (GnomeCanvasPathDef));

	dst->bpath = g_new (ArtBpath, src->end + 1);
	memcpy (dst->bpath, src->bpath, (src->end + 1) * sizeof (ArtBpath));

	dst->sbpath = FALSE;
}


/**
 * gnome_canvas_path_def_duplicate:
 * @path: a GnomeCanvasPathDef to duplicate
 *
 * This function duplicates the passed @path. The new path is
 * marked as non-static regardless of the state of original.
 *
 * Returns: a GnomeCanvasPathDef which is a duplicate of @path.
 */
GnomeCanvasPathDef *
gnome_canvas_path_def_duplicate (const GnomeCanvasPathDef * path)
{
	GnomeCanvasPathDef * new;

	g_return_val_if_fail (path != NULL, NULL);

	new = gnome_canvas_path_def_new_from_foreign_bpath (path->bpath);
	new->x = path->x;
	new->y = path->y;
	new->hascpt = path->hascpt;
	new->posset = path->posset;
	new->moving = path->moving;
	new->allclosed = path->allclosed;
	new->allopen = path->allopen;

	return new;
}

/**
 * gnome_canvas_path_def_concat:
 * @list: a GSList of GnomeCanvasPathDefs to concatenate into one new
 * path.
 *
 * This function concatenates a list of GnomeCanvasPathDefs into one
 * newly created GnomeCanvasPathDef.
 *
 * Returns: a new GnomeCanvasPathDef
 */
GnomeCanvasPathDef *
gnome_canvas_path_def_concat (const GSList * list)
{
	GnomeCanvasPathDef * c, * new;
	ArtBpath * bp;
	const GSList * l;
	gint length;

	g_return_val_if_fail (list != NULL, NULL);

	length = 1;

	for (l = list; l != NULL; l = l->next) {
		c = (GnomeCanvasPathDef *) l->data;
		length += c->end;
	}

	new = gnome_canvas_path_def_new_sized (length);

	bp = new->bpath;

	for (l = list; l != NULL; l = l->next) {
		c = (GnomeCanvasPathDef *) l->data;
		memcpy (bp, c->bpath, c->end * sizeof (ArtBpath));
		bp += c->end;
	}

	bp->code = ART_END;

	new->end = length - 1;

	new->allclosed = sp_bpath_all_closed (new->bpath);
	new->allopen = sp_bpath_all_open (new->bpath);

	return new;
}

/**
 * gnome_canvas_path_def_split:
 * @path: a GnomeCanvasPathDef
 *
 * This function splits the passed @path into a list of
 * GnomeCanvasPathDefs which represent each segment of the origional
 * path.  The path is split when ever an ART_MOVETO or ART_MOVETO_OPEN
 * is encountered. The closedness of resulting paths is set accordingly
 * to closedness of corresponding segment.
 *
 * Returns: a list of GnomeCanvasPathDef(s).
 */
GSList *
gnome_canvas_path_def_split (const GnomeCanvasPathDef * path)
{
	GnomeCanvasPathDef * new;
	GSList * l;
	gint p, i;

	g_return_val_if_fail (path != NULL, NULL);

	p = 0;
	l = NULL;

	while (p < path->end) {
		i = 1;
		while ((path->bpath[p + i].code == ART_LINETO) || (path->bpath[p + i].code == ART_CURVETO)) i++;
		new = gnome_canvas_path_def_new_sized (i + 1);
		memcpy (new->bpath, path->bpath + p, i * sizeof (ArtBpath));
		new->end = i;
		new->bpath[i].code = ART_END;
		new->allclosed = (new->bpath->code == ART_MOVETO);
		new->allopen = (new->bpath->code == ART_MOVETO_OPEN);
		l = g_slist_append (l, new);
		p += i;
	}

	return l;
}

/**
 * gnome_canvas_path_def_open_parts:
 * @path: a GnomeCanvasPathDef
 * 
 * This function creates a new GnomeCanvasPathDef that contains all of
 * the open segments on the passed @path.
 *
 * Returns: a new GnomeCanvasPathDef that contains all of the open segemtns in @path.
 */
GnomeCanvasPathDef *
gnome_canvas_path_def_open_parts (const GnomeCanvasPathDef * path)
{
	GnomeCanvasPathDef * new;
	ArtBpath * p, * d;
	gint len;
	gboolean closed;

	g_return_val_if_fail (path != NULL, NULL);

	closed = TRUE;
	len = 0;

	for (p = path->bpath; p->code != ART_END; p++) {
		switch (p->code) {
		case ART_MOVETO_OPEN:
			closed = FALSE;
			len++;
			break;
		case ART_MOVETO:
			closed = TRUE;
			break;
		case ART_LINETO:
		case ART_CURVETO:
			if (!closed) len++;
			break;
		default:
			g_assert_not_reached ();
		}
	}

	new = gnome_canvas_path_def_new_sized (len + 1);

	closed = TRUE;
	d = new->bpath;

	for (p = path->bpath; p->code != ART_END; p++) {
		switch (p->code) {
		case ART_MOVETO_OPEN:
			closed = FALSE;
			*d++ = *p;
			break;
		case ART_MOVETO:
			closed = TRUE;
			break;
		case ART_LINETO:
		case ART_CURVETO:
			if (!closed) *d++ = *p;
			break;
		default:
			g_assert_not_reached ();
		}
	}

	d->code = ART_END;

	new->end = len;
	new->allclosed = FALSE;
	new->allopen = TRUE;

	return new;
}

/**
 * gnome_canvas_path_def_closed_parts:
 * @path: a GnomeCanvasPathDef
 * 
 * This function returns a new GnomeCanvasPathDef that contains the
 * all of close parts of passed @path.
 *
 * Returns: a new GnomeCanvasPathDef that contains all of the closed
 * parts of passed @path.
 */
GnomeCanvasPathDef *
gnome_canvas_path_def_closed_parts (const GnomeCanvasPathDef * path)
{
	GnomeCanvasPathDef * new;
	ArtBpath * p, * d;
	gint len;
	gboolean closed;

	g_return_val_if_fail (path != NULL, NULL);

	closed = FALSE;
	len = 0;

	for (p = path->bpath; p->code != ART_END; p++) {
		switch (p->code) {
		case ART_MOVETO_OPEN:
			closed = FALSE;
			break;
		case ART_MOVETO:
			closed = TRUE;
			len++;
			break;
		case ART_LINETO:
		case ART_CURVETO:
			if (closed) len++;
			break;
		default:
			g_assert_not_reached ();
		}
	}

	new = gnome_canvas_path_def_new_sized (len + 1);

	closed = FALSE;
	d = new->bpath;

	for (p = path->bpath; p->code != ART_END; p++) {
		switch (p->code) {
		case ART_MOVETO_OPEN:
			closed = FALSE;
			break;
		case ART_MOVETO:
			closed = TRUE;
			*d++ = *p;
			break;
		case ART_LINETO:
		case ART_CURVETO:
			if (closed) *d++ = *p;
			break;
		default:
			g_assert_not_reached ();
		}
	}

	d->code = ART_END;

	new->end = len;
	new->allclosed = TRUE;
	new->allopen = FALSE;

	return new;
}

/**
 * gnome_canvas_path_def_close_all:
 * @path: a GnomeCanvasPathDef
 *
 * This function closes all of the open segments in the passed path
 * and returns a new GnomeCanvasPathDef.
 *
 * Returns: a GnomeCanvasPathDef that contains the contents of @path
 * but has modified the path is fully closed
 */
GnomeCanvasPathDef *
gnome_canvas_path_def_close_all (const GnomeCanvasPathDef * path)
{
	GnomeCanvasPathDef * new;
	ArtBpath * p, * d, * start;
	gint len;
	gboolean closed;

	g_return_val_if_fail (path != NULL, NULL);

	if (path->allclosed) {
		new = gnome_canvas_path_def_duplicate (path);
		return new;
	}

	len = 1;

	/* Count MOVETO_OPEN */

	for (p = path->bpath; p->code != ART_END; p++) {
		len += 1;
		if (p->code == ART_MOVETO_OPEN) len += 2;
	}

	new = gnome_canvas_path_def_new_sized (len);

	d = start = new->bpath;
	closed = TRUE;

	for (p = path->bpath; p->code != ART_END; p++) {
		switch (p->code) {
		case ART_MOVETO_OPEN:
			start = p;
			closed = FALSE;
		case ART_MOVETO:
			if ((!closed) && ((start->x3 != p->x3) || (start->y3 != p->y3))) {
				d->code = ART_LINETO;
				d->x3 = start->x3;
				d->y3 = start->y3;
				d++;
			}
			if (p->code == ART_MOVETO) closed = TRUE;
			d->code = ART_MOVETO;
			d->x3 = p->x3;
			d->y3 = p->y3;
			d++;
			break;
		case ART_LINETO:
		case ART_CURVETO:
			*d++ = *p;
			break;
		default:
			g_assert_not_reached ();
		}
	}

	if ((!closed) && ((start->x3 != p->x3) || (start->y3 != p->y3))) {
		d->code = ART_LINETO;
		d->x3 = start->x3;
		d->y3 = start->y3;
		d++;
	}

	d->code = ART_END;

	new->end = d - new->bpath;
	new->allclosed = TRUE;
	new->allopen = FALSE;

	return new;
}

/* Destructor */

/**
 * gnome_canvas_path_def_unref:
 * @path: a GnomeCanvasPathDef
 *
 * Decrease the reference count of the passed @path.  If the reference
 * count is < 1 the path is deallocated.
 */
void
gnome_canvas_path_def_unref (GnomeCanvasPathDef * path)
{
	g_return_if_fail (path != NULL);

	if (--path->refcount < 1) {
		if ((!path->sbpath) && (path->bpath)) art_free (path->bpath);
		g_free (path);
	}
}


/* Methods */
/**
 * gnome_canvas_path_def_reset:
 * @path: a GnomeCanvasPathDef
 *
 * This function clears the contents of the passed @path.
 */
void
gnome_canvas_path_def_reset (GnomeCanvasPathDef * path)
{
	g_return_if_fail (path != NULL);
	g_return_if_fail (!path->sbpath);

	path->bpath->code = ART_END;
	path->end = 0;
	path->hascpt = FALSE;
	path->posset = FALSE;
	path->moving = FALSE;
	path->allclosed = TRUE;
	path->allopen = TRUE;
}

/* Several consequtive movetos are ALLOWED */

/**
 * gnome_canvas_path_def_moveto:
 * @path: a GnomeCanvasPathDef
 * @x: x coordinate
 * @y: y coordinate
 *
 * This function adds starts new subpath on @path, and sets its
 * starting point to @x and @y. If current subpath is empty, it
 * simply changes its starting coordinates to new values.
 */
void
gnome_canvas_path_def_moveto (GnomeCanvasPathDef * path, gdouble x, gdouble y)
{
	g_return_if_fail (path != NULL);
	g_return_if_fail (!path->sbpath);
	g_return_if_fail (!path->moving);

	path->substart = path->end;
	path->hascpt = TRUE;
	path->posset = TRUE;
	path->x = x;
	path->y = y;

	path->allclosed = FALSE;
}

/**
 * gnome_canvas_path_def_lineto:
 * @path: a GnomeCanvasPathDef
 * @x: x coordinate
 * @y: y coordinate
 *
 * This function add a line segment to the passed @path with the
 * specified @x and @y coordinates.
 */
void
gnome_canvas_path_def_lineto (GnomeCanvasPathDef * path, gdouble x, gdouble y)
{
	ArtBpath * bp;

	g_return_if_fail (path != NULL);
	g_return_if_fail (!path->sbpath);
	g_return_if_fail (path->hascpt);

	if (path->moving) {
		/* simply fix endpoint */
		g_return_if_fail (!path->posset);
		g_return_if_fail (path->end > 1);
		bp = path->bpath + path->end - 1;
		g_return_if_fail (bp->code == ART_LINETO);
		bp->x3 = x;
		bp->y3 = y;
		path->moving = FALSE;
		return;
	}

	if (path->posset) {
		/* start a new segment */
		gnome_canvas_path_def_ensure_space (path, 2);
		bp = path->bpath + path->end;
		bp->code = ART_MOVETO_OPEN;
		bp->x3 = path->x;
		bp->y3 = path->y;
		bp++;
		bp->code = ART_LINETO;
		bp->x3 = x;
		bp->y3 = y;
		bp++;
		bp->code = ART_END;
		path->end += 2;
		path->posset = FALSE;
		path->allclosed = FALSE;
		return;
	}

	/* Simply add line */

	g_return_if_fail (path->end > 1);
	gnome_canvas_path_def_ensure_space (path, 1);
	bp = path->bpath + path->end;
	bp->code = ART_LINETO;
	bp->x3 = x;
	bp->y3 = y;
	bp++;
	bp->code = ART_END;
	path->end++;
}


/**
 * gnome_canvas_path_def_lineto_moving:
 * @path: a GnomeCanvasPathDef
 * @x: x coordinate
 * @y: y coordinate
 *
 * This functions adds a new line segment with loose endpoint to the path, or
 * if endpoint is already loose, changes its coordinates to @x, @y. You
 * can change the coordinates of loose endpoint as many times as you want,
 * the last ones set will be fixed, if you continue line. This is useful
 * for handling drawing with mouse.
 */ 
void
gnome_canvas_path_def_lineto_moving (GnomeCanvasPathDef * path, gdouble x, gdouble y)
{
	ArtBpath * bp;

	g_return_if_fail (path != NULL);
	g_return_if_fail (!path->sbpath);
	g_return_if_fail (path->hascpt);

	if (path->moving) {
		/* simply change endpoint */
		g_return_if_fail (!path->posset);
		g_return_if_fail (path->end > 1);
		bp = path->bpath + path->end - 1;
		g_return_if_fail (bp->code == ART_LINETO);
		bp->x3 = x;
		bp->y3 = y;
		return;
	}

	if (path->posset) {
		/* start a new segment */
		gnome_canvas_path_def_ensure_space (path, 2);
		bp = path->bpath + path->end;
		bp->code = ART_MOVETO_OPEN;
		bp->x3 = path->x;
		bp->y3 = path->y;
		bp++;
		bp->code = ART_LINETO;
		bp->x3 = x;
		bp->y3 = y;
		bp++;
		bp->code = ART_END;
		path->end += 2;
		path->posset = FALSE;
		path->moving = TRUE;
		path->allclosed = FALSE;
		return;
	}

	/* Simply add line */

	g_return_if_fail (path->end > 1);
	gnome_canvas_path_def_ensure_space (path, 1);
	bp = path->bpath + path->end;
	bp->code = ART_LINETO;
	bp->x3 = x;
	bp->y3 = y;
	bp++;
	bp->code = ART_END;
	path->end++;
	path->moving = TRUE;
}

/**
 * gnome_canvas_path_def_curveto:
 * @path: a GnomeCanvasPathDef
 * @x0: first control point x coordinate
 * @y0: first control point y coordinate
 * @x1: second control point x coordinate
 * @y1: second control point y coordinate
 * @x2: end of curve x coordinate
 * @y2: end of curve y coordinate
 *
 * This function adds a bezier curve segment to the path definition.
 */
 
void
gnome_canvas_path_def_curveto (GnomeCanvasPathDef * path, gdouble x0, gdouble y0, gdouble x1, gdouble y1, gdouble x2, gdouble y2)
{
	ArtBpath * bp;

	g_return_if_fail (path != NULL);
	g_return_if_fail (!path->sbpath);
	g_return_if_fail (path->hascpt);
	g_return_if_fail (!path->moving);

	if (path->posset) {
		/* start a new segment */
		gnome_canvas_path_def_ensure_space (path, 2);
		bp = path->bpath + path->end;
		bp->code = ART_MOVETO_OPEN;
		bp->x3 = path->x;
		bp->y3 = path->y;
		bp++;
		bp->code = ART_CURVETO;
		bp->x1 = x0;
		bp->y1 = y0;
		bp->x2 = x1;
		bp->y2 = y1;
		bp->x3 = x2;
		bp->y3 = y2;
		bp++;
		bp->code = ART_END;
		path->end += 2;
		path->posset = FALSE;
		path->allclosed = FALSE;
		return;
	}

	/* Simply add path */

	g_return_if_fail (path->end > 1);
	gnome_canvas_path_def_ensure_space (path, 1);
	bp = path->bpath + path->end;
	bp->code = ART_CURVETO;
	bp->x1 = x0;
	bp->y1 = y0;
	bp->x2 = x1;
	bp->y2 = y1;
	bp->x3 = x2;
	bp->y3 = y2;
	bp++;
	bp->code = ART_END;
	path->end++;
}

/**
 * gnome_canvas_path_def_closepath:
 * @path: a GnomeCanvasPathDef
 *
 * This function closes the last subpath of @path, adding a ART_LINETO to
 * subpath starting point, if needed and changing starting pathcode to
 * ART_MOVETO
 */
void
gnome_canvas_path_def_closepath (GnomeCanvasPathDef * path)
{
	ArtBpath * bs, * be;

	g_return_if_fail (path != NULL);
	g_return_if_fail (!path->sbpath);
	g_return_if_fail (path->hascpt);
	g_return_if_fail (!path->posset);
	g_return_if_fail (!path->moving);
	g_return_if_fail (!path->allclosed);
	/* We need at last M + L + L + E */
	g_return_if_fail (path->end - path->substart > 2);

	bs = path->bpath + path->substart;
	be = path->bpath + path->end - 1;

	if ((bs->x3 != be->x3) || (bs->y3 != be->y3)) {
		gnome_canvas_path_def_lineto (path, bs->x3, bs->y3);
	}

	bs = path->bpath + path->substart; /* NB. def_lineto can
					      realloc bpath */
	bs->code = ART_MOVETO;

	path->allclosed = sp_bpath_all_closed (path->bpath);
	path->allopen = sp_bpath_all_open (path->bpath);

	path->hascpt = FALSE;
}

/**
 * gnome_canvas_path_def_closepath_current:
 * @path: a GnomeCanvasPathDef
 *
 * This function closes the last subpath by setting the coordinates of
 * the endpoint of the last segment (line or curve) to starting point.
 */
void
gnome_canvas_path_def_closepath_current (GnomeCanvasPathDef * path)
{
	ArtBpath * bs, * be;

	g_return_if_fail (path != NULL);
	g_return_if_fail (!path->sbpath);
	g_return_if_fail (path->hascpt);
	g_return_if_fail (!path->posset);
	g_return_if_fail (!path->allclosed);
	/* We need at last M + L + L + E */
	g_return_if_fail (path->end - path->substart > 2);

	bs = path->bpath + path->substart;
	be = path->bpath + path->end - 1;

	be->x3 = bs->x3;
	be->y3 = bs->y3;

	bs->code = ART_MOVETO;

	path->allclosed = sp_bpath_all_closed (path->bpath);
	path->allopen = sp_bpath_all_open (path->bpath);

	path->hascpt = FALSE;
	path->moving = FALSE;
}

/**
 * gnome_canvas_path_def_bpath:
 * @path: a GnomeCanvasPathDef
 *
 * This function returns a ArtBpath that consists of the path
 * definition.
 *
 * Returns: ArtBpath
 */
ArtBpath * gnome_canvas_path_def_bpath (const GnomeCanvasPathDef * path)
{
	g_return_val_if_fail (path != NULL, NULL);

	return path->bpath;
}

/**
 * gnome_canvas_path_def_length:
 * @path: a GnomeCanvasPathDef
 *
 * This function returns the length of the path definition.  Not
 * Euclidian length of the path but rather the number of points on the
 * path.
 *
 * Returns: integer, number of points on the path.
 */
gint gnome_canvas_path_def_length (const GnomeCanvasPathDef * path)
{
	g_return_val_if_fail (path != NULL, -1);

	return path->end + 1;
}

/**
 * gnome_canvas_path_def_is_empty:
 * @path: a GnomeCanvasPathDef
 *
 * This function is a boolean test to see if the path is empty,
 * meaning containing no line segments.
 *
 * Returns: boolean, indicating if the path is empty.
 */
gboolean
gnome_canvas_path_def_is_empty (const GnomeCanvasPathDef * path)
{
	g_return_val_if_fail (path != NULL, TRUE);

	return (path->bpath->code == ART_END);
}

/**
 * gnome_canvas_path_def_has_currentpoint:
 * @path: a GnomeCanvasPathdef
 *
 * This function is a boolean test checking to see if the path has a
 * current point defined. Current point will be set by line operators,
 * and cleared by closing subpath.
 *
 * Returns: boolean, indicating if the path has a current point defined.
 */
gboolean
gnome_canvas_path_def_has_currentpoint (const GnomeCanvasPathDef * path)
{
	g_return_val_if_fail (path != NULL, FALSE);

	return (path->hascpt);
}

/**
 * gnome_canvas_path_def_currentpoint:
 * @path: a GnomeCanvasPathDef
 * @p: a ArtPoint where to store the current point
 *
 * Stores the current point of the path definition in the passed ArtPoint @p.
 */
void
gnome_canvas_path_def_currentpoint (const GnomeCanvasPathDef * path, ArtPoint * p)
{
	g_return_if_fail (path != NULL);
	g_return_if_fail (p != NULL);
	g_return_if_fail (path->hascpt);

	if (path->posset) {
		p->x = path->x;
		p->y = path->y;
	} else {
		p->x = (path->bpath + path->end - 1)->x3;
		p->y = (path->bpath + path->end - 1)->y3;
	}	
}

/**
 * gnome_canvas_path_def_last_bpath:
 * @path: a GnomeCanvasPathDef
 *
 * This function returns pointer to the last ArtBpath segment in the path
 * definition.
 *
 * Returns: ArtBpath, being the last segment in the path definition or
 * null if no line segments have been defined.
 */
ArtBpath *
gnome_canvas_path_def_last_bpath (const GnomeCanvasPathDef * path)
{
	g_return_val_if_fail (path != NULL, NULL);

	if (path->end == 0) return NULL;

	return path->bpath + path->end - 1;
}

/**
 * gnome_canvas_path_def_first_bpath:
 * @path: a GnomeCanvasPathDef
 *
 * This function returns the first ArtBpath point in the definition.
 *
 * Returns: ArtBpath being the first point in the path definition or
 * null if no points are defined
*/
ArtBpath *
gnome_canvas_path_def_first_bpath (const GnomeCanvasPathDef * path)
{
	g_return_val_if_fail (path != NULL, NULL);

	if (path->end == 0) return NULL;

	return path->bpath;
}

/**
 * gnome_canvas_path_def_any_open:
 * @path: a GnomeCanvasPathDef
 *
 * This function returns a boolean value indicating if the path has
 * any open segments.
 *
 * Returns: boolean, indicating if the path has any open segments.
 */
gboolean
gnome_canvas_path_def_any_open (const GnomeCanvasPathDef * path)
{
	g_return_val_if_fail (path != NULL, FALSE);

	return (!path->allclosed);
}

/**
 * gnome_canvas_path_def_all_open:
 * @path: a GnomeCanvasPathDef
 *
 * This function returns a boolean value indicating if the path only
 * contains open segments.
 *
 * Returns: boolean, indicating if the path has all open segments.
 */
gboolean
gnome_canvas_path_def_all_open (const GnomeCanvasPathDef * path)
{
	g_return_val_if_fail (path != NULL, FALSE);

	return (path->allopen);
}

/**
 * gnome_canvas_path_def_any_closed:
 * @path: a GnomeCanvasPathDef
 *
 * This function returns a boolean valid indicating if the path has
 * any closed segements.
 *
 * Returns: boolean, indicating if the path has any closed segments.
 */
gboolean
gnome_canvas_path_def_any_closed (const GnomeCanvasPathDef * path)
{
	g_return_val_if_fail (path != NULL, FALSE);

	return (!path->allopen);
}

/**
 * gnome_canvas_path_def_all_closed:
 * @path: a GnomeCanvasPathDef
 *
 * This function returns a boolean value indicating if the path only
 * contains closed segments.
 *
 * Returns: boolean, indicating if the path has all closed segments.
 */
gboolean
gnome_canvas_path_def_all_closed (const GnomeCanvasPathDef * path)
{
	g_return_val_if_fail (path != NULL, FALSE);

	return (path->allclosed);
}

/* Private methods */

static
gboolean sp_bpath_good (ArtBpath * bpath)
{
	ArtBpath * bp;

	g_return_val_if_fail (bpath != NULL, FALSE);

	if (bpath->code == ART_END)
                return TRUE;

	bp = bpath;

	while (bp->code != ART_END) {
		bp = sp_bpath_check_subpath (bp);
		if (bp == NULL) return FALSE;
	}

	return TRUE;
}

static ArtBpath *
sp_bpath_check_subpath (ArtBpath * bpath)
{
	gint i, len;
	gboolean closed;

	g_return_val_if_fail (bpath != NULL, NULL);

	if (bpath->code == ART_MOVETO) {
		closed = TRUE;
	} else if (bpath->code == ART_MOVETO_OPEN) {
		closed = FALSE;
	} else {
		return NULL;
	}

	len = 0;

	for (i = 1; (bpath[i].code != ART_END) && (bpath[i].code != ART_MOVETO) && (bpath[i].code != ART_MOVETO_OPEN); i++) {
		switch (bpath[i].code) {
			case ART_LINETO:
			case ART_CURVETO:
				len++;
				break;
			default:
				return NULL;
		}
	}

	if (closed) {
		if (len < 2) return NULL;
		if ((bpath->x3 != bpath[i-1].x3) || (bpath->y3 != bpath[i-1].y3)) return NULL;
	} else {
		if (len < 1) return NULL;
	}

	return bpath + i;
}

static gint
sp_bpath_length (const ArtBpath * bpath)
{
	gint l;

	g_return_val_if_fail (bpath != NULL, FALSE);

	for (l = 0; bpath[l].code != ART_END; l++) ;

	l++;

	return l;
}

static gboolean
sp_bpath_all_closed (const ArtBpath * bpath)
{
	const ArtBpath * bp;

	g_return_val_if_fail (bpath != NULL, FALSE);

	for (bp = bpath; bp->code != ART_END; bp++)
		if (bp->code == ART_MOVETO_OPEN) return FALSE;

	return TRUE;
}

static gboolean
sp_bpath_all_open (const ArtBpath * bpath)
{
	const ArtBpath * bp;

	g_return_val_if_fail (bpath != NULL, FALSE);

	for (bp = bpath; bp->code != ART_END; bp++)
		if (bp->code == ART_MOVETO) return FALSE;

	return TRUE;
}


