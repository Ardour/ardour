#ifndef GNOME_CANVAS_PATH_DEF_H
#define GNOME_CANVAS_PATH_DEF_H

/*
 * GnomeCanvasPathDef
 *
 * (C) 1999-2000 Lauris Kaplinski <lauris@ximian.com>
 * Released under LGPL
 *
 * This is mostly like GnomeCanvasBpathDef, but with added functionality:
 * - can be constructed from scratch, from existing bpath of from static bpath
 * - Path is always terminated with ART_END
 * - Has closed flag
 * - has concat, split and copy methods
 *
 */

#include <glib-object.h>
#include <libart_lgpl/art_bpath.h>

G_BEGIN_DECLS

typedef struct _GnomeCanvasPathDef GnomeCanvasPathDef;

#define GNOME_TYPE_CANVAS_PATH_DEF	(gnome_canvas_path_def_get_type ())
GType gnome_canvas_path_def_get_type (void) G_GNUC_CONST;

/* Constructors */

GnomeCanvasPathDef * gnome_canvas_path_def_new (void);
GnomeCanvasPathDef * gnome_canvas_path_def_new_sized (gint length);
GnomeCanvasPathDef * gnome_canvas_path_def_new_from_bpath (ArtBpath * bpath);
GnomeCanvasPathDef * gnome_canvas_path_def_new_from_static_bpath (ArtBpath * bpath);
GnomeCanvasPathDef * gnome_canvas_path_def_new_from_foreign_bpath (ArtBpath * bpath);

void gnome_canvas_path_def_ref (GnomeCanvasPathDef * path);
void gnome_canvas_path_def_finish (GnomeCanvasPathDef * path);
void gnome_canvas_path_def_ensure_space (GnomeCanvasPathDef * path, gint space);

/*
 * Misc constructors
 * All these return NEW path, not unrefing old
 * Also copy and duplicate force bpath to be private (otherwise you
 * would use ref :)
 */

void gnome_canvas_path_def_copy (GnomeCanvasPathDef * dst, const GnomeCanvasPathDef * src);
GnomeCanvasPathDef * gnome_canvas_path_def_duplicate (const GnomeCanvasPathDef * path);
GnomeCanvasPathDef * gnome_canvas_path_def_concat (const GSList * list);
GSList * gnome_canvas_path_def_split (const GnomeCanvasPathDef * path);
GnomeCanvasPathDef * gnome_canvas_path_def_open_parts (const GnomeCanvasPathDef * path);
GnomeCanvasPathDef * gnome_canvas_path_def_closed_parts (const GnomeCanvasPathDef * path);
GnomeCanvasPathDef * gnome_canvas_path_def_close_all (const GnomeCanvasPathDef * path);

/* Destructor */

void gnome_canvas_path_def_unref (GnomeCanvasPathDef * path);

/* Methods */

/* Sets GnomeCanvasPathDef to zero length */

void gnome_canvas_path_def_reset (GnomeCanvasPathDef * path);

/* Drawing methods */

void gnome_canvas_path_def_moveto (GnomeCanvasPathDef * path, gdouble x, gdouble y);
void gnome_canvas_path_def_lineto (GnomeCanvasPathDef * path, gdouble x, gdouble y);

/* Does not create new ArtBpath, but simply changes last lineto position */

void gnome_canvas_path_def_lineto_moving (GnomeCanvasPathDef * path, gdouble x, gdouble y);
void gnome_canvas_path_def_curveto (GnomeCanvasPathDef * path, gdouble x0, gdouble y0,gdouble x1, gdouble y1, gdouble x2, gdouble y2);
void gnome_canvas_path_def_closepath (GnomeCanvasPathDef * path);

/* Does not draw new line to startpoint, but moves last lineto */

void gnome_canvas_path_def_closepath_current (GnomeCanvasPathDef * path);

/* Various methods */

ArtBpath * gnome_canvas_path_def_bpath (const GnomeCanvasPathDef * path);
gint gnome_canvas_path_def_length (const GnomeCanvasPathDef * path);
gboolean gnome_canvas_path_def_is_empty (const GnomeCanvasPathDef * path);
gboolean gnome_canvas_path_def_has_currentpoint (const GnomeCanvasPathDef * path);
void gnome_canvas_path_def_currentpoint (const GnomeCanvasPathDef * path, ArtPoint * p);
ArtBpath * gnome_canvas_path_def_last_bpath (const GnomeCanvasPathDef * path);
ArtBpath * gnome_canvas_path_def_first_bpath (const GnomeCanvasPathDef * path);
gboolean gnome_canvas_path_def_any_open (const GnomeCanvasPathDef * path);
gboolean gnome_canvas_path_def_all_open (const GnomeCanvasPathDef * path);
gboolean gnome_canvas_path_def_any_closed (const GnomeCanvasPathDef * path);
gboolean gnome_canvas_path_def_all_closed (const GnomeCanvasPathDef * path);

G_END_DECLS

#endif
