/* Helpful functions when dealing with cairo in gtk engines */

#include <gtk/gtk.h>
#include <math.h>

typedef struct
{
	gdouble r;
	gdouble g;
	gdouble b;
	gdouble a;
} CairoColor;

typedef struct
{
	CairoColor bg[5];
	CairoColor fg[5];

	CairoColor dark[5];
	CairoColor light[5];
	CairoColor mid[5];

	CairoColor base[5];
	CairoColor text[5];
	CairoColor text_aa[5];

	CairoColor black;
	CairoColor white;
} CairoColorCube;

typedef enum
{
	CR_CORNER_NONE        = 0,
	CR_CORNER_TOPLEFT     = 1,
	CR_CORNER_TOPRIGHT    = 2,
	CR_CORNER_BOTTOMLEFT  = 4,
	CR_CORNER_BOTTOMRIGHT = 8,
	CR_CORNER_ALL         = 15
} CairoCorners;

typedef enum
{
	CR_MIRROR_NONE       = 0,
	CR_MIRROR_HORIZONTAL = 1 << 0,
	CR_MIRROR_VERTICAL   = 1 << 1
} CairoMirror;

/*****************************/
/* Pattern Fills             */
/*****************************/
typedef enum {
	GE_DIRECTION_VERTICAL,
	GE_DIRECTION_HORIZONTAL,
	GE_DIRECTION_BOTH,
	GE_DIRECTION_NONE
} GeDirection;

#if  ((CAIRO_VERSION_MAJOR < 1) || ((CAIRO_VERSION_MAJOR == 1) && (CAIRO_VERSION_MINOR < 2)))
typedef enum _cairo_pattern_type {
    CAIRO_PATTERN_TYPE_SOLID,
    CAIRO_PATTERN_TYPE_SURFACE,
    CAIRO_PATTERN_TYPE_LINEAR,
    CAIRO_PATTERN_TYPE_RADIAL
} cairo_pattern_type_t;

#	define CAIRO_PATTERN_TYPE(pattern) pattern->type;
#else
#	define CAIRO_PATTERN_TYPE(pattern) cairo_pattern_get_type (pattern->handle);
#endif

typedef struct
{
#if  ((CAIRO_VERSION_MAJOR < 1) || ((CAIRO_VERSION_MAJOR == 1) && (CAIRO_VERSION_MINOR < 2)))
	cairo_pattern_type_t type;
#endif
	GeDirection scale;
	GeDirection translate;

	cairo_pattern_t *handle;
	cairo_operator_t operator;
} CairoPattern;

GE_INTERNAL void ge_hsb_from_color (const CairoColor *color, gdouble *hue, gdouble *saturation, gdouble *brightness);
GE_INTERNAL void ge_color_from_hsb (gdouble hue, gdouble saturation, gdouble brightness, CairoColor *color);

GE_INTERNAL void ge_gdk_color_to_cairo (const GdkColor * gc, CairoColor * cc);
GE_INTERNAL void ge_cairo_color_to_gtk (const CairoColor *cc, GdkColor *c);
GE_INTERNAL void ge_gtk_style_to_cairo_color_cube (GtkStyle * style, CairoColorCube *cube);

GE_INTERNAL void ge_shade_color(const CairoColor *base, gdouble shade_ratio, CairoColor *composite);
GE_INTERNAL void ge_saturate_color (const CairoColor * base, gdouble saturate_level, CairoColor *composite);
GE_INTERNAL void ge_mix_color (const CairoColor *color1, const CairoColor *color2, gdouble mix_factor, CairoColor *composite);

GE_INTERNAL cairo_t * ge_gdk_drawable_to_cairo (GdkDrawable  *window, GdkRectangle *area);
GE_INTERNAL void ge_cairo_set_color (cairo_t *cr, const CairoColor *color);
GE_INTERNAL void ge_cairo_set_gdk_color_with_alpha (cairo_t *cr, const GdkColor *color, gdouble alpha);
GE_INTERNAL void ge_cairo_pattern_add_color_stop_color (cairo_pattern_t *pattern, gfloat offset, const CairoColor *color);
GE_INTERNAL void ge_cairo_pattern_add_color_stop_shade (cairo_pattern_t *pattern, gdouble offset, const CairoColor *color, gdouble shade);

GE_INTERNAL void ge_cairo_rounded_corner (cairo_t *cr, double x, double y, double radius, CairoCorners corner);
GE_INTERNAL void ge_cairo_rounded_rectangle (cairo_t *cr, double x, double y, double w, double h, double radius, CairoCorners corners);

GE_INTERNAL void ge_cairo_stroke_rectangle (cairo_t *cr, double x, double y, double w, double h);
GE_INTERNAL void ge_cairo_simple_border (cairo_t *cr, const CairoColor * tl, const CairoColor * br, gint x, gint y, gint width, gint height, gboolean topleft_overlap);

GE_INTERNAL void ge_cairo_line (cairo_t *cr, const CairoColor *color, gint x1, gint y1, gint x2, gint y2);
GE_INTERNAL void ge_cairo_polygon (cairo_t *cr, const CairoColor *color, GdkPoint *points, gint npoints);

GE_INTERNAL void ge_cairo_mirror (cairo_t *cr, CairoMirror mirror, gint *x, gint *y, gint *width, gint *height);
GE_INTERNAL void ge_cairo_exchange_axis (cairo_t *cr, gint *x, gint *y, gint *width, gint *height);

GE_INTERNAL void ge_cairo_pattern_fill(cairo_t *canvas, CairoPattern *pattern, gint x, gint y, gint width, gint height);

GE_INTERNAL CairoPattern *ge_cairo_color_pattern(CairoColor *base);
GE_INTERNAL CairoPattern *ge_cairo_pixbuf_pattern(GdkPixbuf *pixbuf);
GE_INTERNAL CairoPattern *ge_cairo_pixmap_pattern(GdkPixmap *pixmap);
GE_INTERNAL CairoPattern *ge_cairo_linear_shade_gradient_pattern(CairoColor *base, gdouble shade1, gdouble shade2, gboolean vertical);
GE_INTERNAL void ge_cairo_pattern_destroy(CairoPattern *pattern);
