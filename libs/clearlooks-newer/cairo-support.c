#include <math.h>
#include "general-support.h"
#include "cairo-support.h"

/***********************************************
 * ge_hsb_from_color -
 *  
 *   Get HSB values from RGB values.
 *
 *   Modified from Smooth but originated in GTK+
 ***********************************************/
void
ge_hsb_from_color (const CairoColor *color, 
                        gdouble *hue, 
                        gdouble *saturation,
                        gdouble *brightness) 
{
	gdouble min, max, delta;
	gdouble red, green, blue;

	red = color->r;
	green = color->g;
	blue = color->b;
  
	if (red > green)
	{
		max = MAX(red, blue);
		min = MIN(green, blue);
	}
	else
	{
		max = MAX(green, blue);
		min = MIN(red, blue);
	}
  
	*brightness = (max + min) / 2;
 	
	if (fabs(max - min) < 0.0001)
	{
		*hue = 0;
		*saturation = 0;
	}	
	else
	{
		if (*brightness <= 0.5)
			*saturation = (max - min) / (max + min);
		else
			*saturation = (max - min) / (2 - max - min);
       
		delta = max -min;
 
		if (red == max)
			*hue = (green - blue) / delta;
		else if (green == max)
			*hue = 2 + (blue - red) / delta;
		else if (blue == max)
			*hue = 4 + (red - green) / delta;
 
		*hue *= 60;
		if (*hue < 0.0)
			*hue += 360;
	}
}
 
/***********************************************
 * ge_color_from_hsb -
 *  
 *   Get RGB values from HSB values.
 *
 *   Modified from Smooth but originated in GTK+
 ***********************************************/
#define MODULA(number, divisor) (((gint)number % divisor) + (number - (gint)number))
void
ge_color_from_hsb (gdouble hue, 
                        gdouble saturation,
                        gdouble brightness, 
                        CairoColor *color)
{
	gint i;
	gdouble hue_shift[3], color_shift[3];
	gdouble m1, m2, m3;

	if (!color) return;
  	  
	if (brightness <= 0.5)
		m2 = brightness * (1 + saturation);
	else
		m2 = brightness + saturation - brightness * saturation;
 
	m1 = 2 * brightness - m2;
 
	hue_shift[0] = hue + 120;
	hue_shift[1] = hue;
	hue_shift[2] = hue - 120;
 
	color_shift[0] = color_shift[1] = color_shift[2] = brightness;	
 
	i = (saturation == 0)?3:0;
 
	for (; i < 3; i++)
	{
		m3 = hue_shift[i];
 
		if (m3 > 360)
			m3 = MODULA(m3, 360);
		else if (m3 < 0)
			m3 = 360 - MODULA(ABS(m3), 360);
 
		if (m3 < 60)
			color_shift[i] = m1 + (m2 - m1) * m3 / 60;
		else if (m3 < 180)
			color_shift[i] = m2;
		else if (m3 < 240)
			color_shift[i] = m1 + (m2 - m1) * (240 - m3) / 60;
		else
			color_shift[i] = m1;
	}	
 
	color->r = color_shift[0];
	color->g = color_shift[1];
	color->b = color_shift[2];	
	color->a = 1.0;	
}

void
ge_gdk_color_to_cairo (const GdkColor *c, CairoColor *cc)
{
	gdouble r, g, b;

	g_return_if_fail (c && cc);

	r = c->red / 65535.0;
	g = c->green / 65535.0;
	b = c->blue / 65535.0;

	cc->r = r;
	cc->g = g;
	cc->b = b;
	cc->a = 1.0;
}

void
ge_cairo_color_to_gtk (const CairoColor *cc, GdkColor *c)
{
	gdouble r, g, b;

	g_return_if_fail (c && cc);

	r = cc->r * 65535.0;
	g = cc->g * 65535.0;
	b = cc->b * 65535.0;

	c->red = r;
	c->green = g;
	c->blue = b;
}

void 
ge_gtk_style_to_cairo_color_cube (GtkStyle * style, CairoColorCube *cube)
{
	int i;

	g_return_if_fail (style && cube);

	for (i = 0; i < 5; i++)
	{ 
		ge_gdk_color_to_cairo (&style->bg[i], &cube->bg[i]);
		ge_gdk_color_to_cairo (&style->fg[i], &cube->fg[i]);

		ge_gdk_color_to_cairo (&style->dark[i], &cube->dark[i]);
		ge_gdk_color_to_cairo (&style->light[i], &cube->light[i]);
		ge_gdk_color_to_cairo (&style->mid[i], &cube->mid[i]);

		ge_gdk_color_to_cairo (&style->base[i], &cube->base[i]);
		ge_gdk_color_to_cairo (&style->text[i], &cube->text[i]);
		ge_gdk_color_to_cairo (&style->text_aa[i], &cube->text_aa[i]);
    	}

	cube->black.r = cube->black.g = cube->black.b = 0;
	cube->black.a = 1;

	cube->white.r = cube->white.g = cube->white.b = 1;
	cube->white.a = 1;
}

void
ge_shade_color(const CairoColor *base, gdouble shade_ratio, CairoColor *composite)
{
	gdouble hue = 0;
	gdouble saturation = 0;
	gdouble brightness = 0;
 
	g_return_if_fail (base && composite);

	ge_hsb_from_color (base, &hue, &saturation, &brightness);
 
	brightness = MIN(brightness*shade_ratio, 1.0);
	brightness = MAX(brightness, 0.0);
  
	saturation = MIN(saturation*shade_ratio, 1.0);
	saturation = MAX(saturation, 0.0);
  
	ge_color_from_hsb (hue, saturation, brightness, composite);
	composite->a = base->a;	
}

void
ge_saturate_color (const CairoColor *base, gdouble saturate_level, CairoColor *composite)
{
	gdouble hue = 0;
	gdouble saturation = 0;
	gdouble brightness = 0;
 
	g_return_if_fail (base && composite);

	ge_hsb_from_color (base, &hue, &saturation, &brightness);

	saturation = MIN(saturation*saturate_level, 1.0);
	saturation = MAX(saturation, 0.0);

	ge_color_from_hsb (hue, saturation, brightness, composite);
	composite->a = base->a;	
}

void
ge_mix_color (const CairoColor *color1, const CairoColor *color2, 
              gdouble mix_factor, CairoColor *composite)
{
	g_return_if_fail (color1 && color2 && composite);

	composite->r = color1->r * (1-mix_factor) + color2->r * mix_factor;
	composite->g = color1->g * (1-mix_factor) + color2->g * mix_factor;
	composite->b = color1->b * (1-mix_factor) + color2->b * mix_factor;
	composite->a = 1.0;
}

cairo_t * 
ge_gdk_drawable_to_cairo (GdkDrawable  *window, GdkRectangle *area)
{
	cairo_t *cr;

	g_return_val_if_fail (window != NULL, NULL);

	cr = (cairo_t*) gdk_cairo_create (window);
	cairo_set_line_width (cr, 1.0);
	cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE);
	cairo_set_line_join (cr, CAIRO_LINE_JOIN_MITER);

	if (area) 
	{
		cairo_rectangle (cr, area->x, area->y, area->width, area->height);
		cairo_clip_preserve (cr);
		cairo_new_path (cr);
	}

	return cr;
}

void 
ge_cairo_set_color (cairo_t *cr, const CairoColor *color)
{
	g_return_if_fail (cr && color);

	cairo_set_source_rgba (cr, color->r, color->g, color->b, color->a);	
}

void
ge_cairo_set_gdk_color_with_alpha (cairo_t *cr, const GdkColor *color, gdouble alpha)
{
	g_return_if_fail (cr && color);

	cairo_set_source_rgba (cr, color->red / 65535.0,
	                           color->green / 65535.0,
	                           color->blue / 65535.0,
	                           alpha);
}

void 
ge_cairo_pattern_add_color_stop_color (cairo_pattern_t *pattern, 
						gfloat offset, 
						const CairoColor *color)
{
	g_return_if_fail (pattern && color);

	cairo_pattern_add_color_stop_rgba (pattern, offset, color->r, color->g, color->b, color->a);	
}

void
ge_cairo_pattern_add_color_stop_shade(cairo_pattern_t *pattern, 
						gdouble offset, 
						const CairoColor *color, 
						gdouble shade)
{
	CairoColor shaded;

	g_return_if_fail (pattern && color && (shade >= 0) && (shade <= 3));

	shaded = *color;

	if (shade != 1)
	{
		ge_shade_color(color, shade, &shaded);
	}

	ge_cairo_pattern_add_color_stop_color(pattern, offset, &shaded);	
}

/* This function will draw a rounded corner at position x,y. If the radius
 * is very small (or negative) it will instead just do a line_to.
 * ge_cairo_rounded_corner assumes clockwise drawing. */
void
ge_cairo_rounded_corner (cairo_t      *cr,
                         double        x,
                         double        y,
                         double        radius,
                         CairoCorners  corner)
{
	if (radius < 0.0001)
	{
		cairo_line_to (cr, x, y);
	}
	else
	{
		switch (corner) {
		case CR_CORNER_NONE:
			cairo_line_to (cr, x, y);
			break;
		case CR_CORNER_TOPLEFT:
			cairo_arc (cr, x + radius, y + radius, radius, G_PI, G_PI * 3/2);
			break;
		case CR_CORNER_TOPRIGHT:
			cairo_arc (cr, x - radius, y + radius, radius, G_PI * 3/2, G_PI * 2);
			break;
		case CR_CORNER_BOTTOMRIGHT:
			cairo_arc (cr, x - radius, y - radius, radius, 0, G_PI * 1/2);
			break;
		case CR_CORNER_BOTTOMLEFT:
			cairo_arc (cr, x + radius, y - radius, radius, G_PI * 1/2, G_PI);
			break;

		default:
			/* A bitfield and not a sane value ... */
			g_assert_not_reached ();
			cairo_line_to (cr, x, y);
			return;
		}
	}
}

void
ge_cairo_rounded_rectangle (cairo_t *cr,
                                 double x, double y, double w, double h,
                                 double radius, CairoCorners corners)
{
	g_return_if_fail (cr != NULL);

	if (radius < 0.0001 || corners == CR_CORNER_NONE)
	{
		cairo_rectangle (cr, x, y, w, h);
		return;
	}
#ifdef DEVELOPMENT
	if ((corners == CR_CORNER_ALL) && (radius > w / 2.0 || radius > h / 2.0))
		g_warning ("Radius is too large for width/height in ge_rounded_rectangle.\n");
	else if (radius > w || radius > h) /* This isn't perfect. Assumes that only one corner is set. */
		g_warning ("Radius is too large for width/height in ge_rounded_rectangle.\n");
#endif

	if (corners & CR_CORNER_TOPLEFT)
		cairo_move_to (cr, x+radius, y);
	else
		cairo_move_to (cr, x, y);
	
	if (corners & CR_CORNER_TOPRIGHT)
		cairo_arc (cr, x+w-radius, y+radius, radius, G_PI * 1.5, G_PI * 2);
	else
		cairo_line_to (cr, x+w, y);
	
	if (corners & CR_CORNER_BOTTOMRIGHT)
		cairo_arc (cr, x+w-radius, y+h-radius, radius, 0, G_PI * 0.5);
	else
		cairo_line_to (cr, x+w, y+h);
	
	if (corners & CR_CORNER_BOTTOMLEFT)
		cairo_arc (cr, x+radius,   y+h-radius, radius, G_PI * 0.5, G_PI);
	else
		cairo_line_to (cr, x, y+h);
	
	if (corners & CR_CORNER_TOPLEFT)
		cairo_arc (cr, x+radius,   y+radius,   radius, G_PI, G_PI * 1.5);
	else
		cairo_line_to (cr, x, y);
}


/* ge_cairo_stroke_rectangle.
 *
 *  A simple function to stroke the rectangle { x, y, w, h}.
 *  (This function only exists because of a cairo performance bug that
 *    has been fixed and it may be a good idea to get rid of it again.)
 */
void
ge_cairo_stroke_rectangle (cairo_t *cr, double x, double y, double w, double h)
{
	cairo_rectangle (cr, x, y, w, h);
	cairo_stroke (cr);
}

/***********************************************
 * ge_cairo_simple_border -
 *  
 *   A simple routine to draw thin squared
 *   borders with a topleft and bottomright color.
 *    
 *   It originated in Smooth-Engine.
 ***********************************************/
void
ge_cairo_simple_border (cairo_t *cr,
				const CairoColor * tl, const CairoColor * br,
				gint x,	gint y, gint width, gint height, 
				gboolean topleft_overlap)
{
	gboolean solid_color;

	g_return_if_fail (cr != NULL);
	g_return_if_fail (tl != NULL);
	g_return_if_fail (br != NULL);
	

	solid_color = (tl == br) || ((tl->r == br->r) && (tl->g == br->g) && (tl->b == br->b) && (tl->a == br->a));

	topleft_overlap &= !solid_color;

	cairo_save(cr);

	cairo_set_line_width (cr, 1);

	if (topleft_overlap)
	{
		ge_cairo_set_color(cr, br);	

		cairo_move_to(cr, x + 0.5, y + height - 0.5);
		cairo_line_to(cr, x + width - 0.5, y + height - 0.5);
		cairo_line_to(cr, x + width - 0.5, y + 0.5);
		
		cairo_stroke (cr);
	}
 
	ge_cairo_set_color(cr, tl);	

	cairo_move_to(cr, x + 0.5, y + height - 0.5);
	cairo_line_to(cr, x + 0.5, y + 0.5);
	cairo_line_to(cr, x + width - 0.5, y + 0.5);

	if (!topleft_overlap)
	{
		if (!solid_color)
		{
			cairo_stroke(cr);
			ge_cairo_set_color(cr, br);	
		}

		cairo_move_to(cr, x + 0.5, y + height - 0.5);
		cairo_line_to(cr, x + width - 0.5, y + height - 0.5);
		cairo_line_to(cr, x + width - 0.5, y + 0.5);
	}

	cairo_stroke(cr);

	cairo_restore(cr);
}

void ge_cairo_polygon (cairo_t *cr,
				const CairoColor *color,
				GdkPoint *points,
				gint npoints)
{
	int i = 0;

	cairo_save(cr);

	ge_cairo_set_color(cr, color);	
	cairo_move_to(cr, points[0].x, points[0].y);

	for (i = 1; i < npoints; i++)
	{
		if (!((points[i].x == points[i + 1].x) &&
		    (points[i].y == points[i + 1].y))) 
		{
			cairo_line_to(cr, points[i].x, points[i].y);
		}
	}
	
	if ((points[npoints-1].x != points[0].x) ||
		(points[npoints-1].y != points[0].y))
	{
		cairo_line_to(cr, points[0].x, points[0].y);
	}

	cairo_fill(cr);

	cairo_restore(cr);
}

void ge_cairo_line (cairo_t *cr,
			const CairoColor *color,
			gint x1,
			gint y1,
			gint x2,
			gint y2)
{ 
	cairo_save(cr);

	ge_cairo_set_color(cr, color);	
	cairo_set_line_width (cr, 1);

	cairo_move_to(cr, x1 + 0.5, y1 + 0.5);
	cairo_line_to(cr, x2 + 0.5, y2 + 0.5);

	cairo_stroke(cr);

	cairo_restore(cr);
}

void
ge_cairo_mirror (cairo_t     *cr,
                 CairoMirror  mirror,
                 gint        *x,
                 gint        *y,
                 gint        *width,
                 gint        *height)
{
	cairo_matrix_t matrix;
	
	cairo_matrix_init_identity (&matrix);
	
	cairo_translate (cr, *x, *y);
	*x = 0;
	*y = 0;
	
	if (mirror & CR_MIRROR_HORIZONTAL)
	{
		cairo_matrix_scale (&matrix, -1, 1);
		*x = -*width;
	}
	if (mirror & CR_MIRROR_VERTICAL)
	{
		cairo_matrix_scale (&matrix, 1, -1);
		*y = -*height;
	}

	cairo_transform (cr, &matrix);
}

void
ge_cairo_exchange_axis (cairo_t  *cr,
                        gint     *x,
                        gint     *y,
                        gint     *width,
                        gint     *height)
{
	gint tmp;
	cairo_matrix_t matrix;

	cairo_translate (cr, *x, *y);
	cairo_matrix_init (&matrix, 0, 1, 1, 0, 0, 0);

	cairo_transform (cr, &matrix);
	
	/* swap width/height */
	tmp = *width;
	*x = 0;
	*y = 0;
	*width = *height;
	*height = tmp;
}


/***********************************************
 * ge_cairo_pattern_fill -
 *  
 *   Fill an area with some pattern
 *   Scaling or tiling if needed
 ***********************************************/
void 
ge_cairo_pattern_fill(cairo_t *canvas,
			CairoPattern *pattern,
			gint x,
			gint y,
			gint width,
			gint height)
{
	cairo_matrix_t original_matrix, current_matrix;

	if (pattern->operator == CAIRO_OPERATOR_DEST)
	{
		return;
	}

	cairo_pattern_get_matrix(pattern->handle, &original_matrix);
	current_matrix = original_matrix;

	if (pattern->scale != GE_DIRECTION_NONE)
	{
		gdouble scale_x = 1.0;
		gdouble scale_y = 1.0;

		if ((pattern->scale == GE_DIRECTION_VERTICAL) || (pattern->scale == GE_DIRECTION_BOTH))
		{
			scale_x = 1.0/width;
		}

		if ((pattern->scale == GE_DIRECTION_HORIZONTAL) || (pattern->scale == GE_DIRECTION_BOTH))
		{
			scale_y = 1.0/height;
		}

		cairo_matrix_scale(&current_matrix, scale_x, scale_y);
	}

	if (pattern->translate != GE_DIRECTION_NONE)
	{
		gdouble translate_x = 0;
		gdouble translate_y = 0;

		if ((pattern->translate == GE_DIRECTION_VERTICAL) || (pattern->translate == GE_DIRECTION_BOTH))
		{
			translate_x = 0.0-x;
		}

		if ((pattern->translate == GE_DIRECTION_HORIZONTAL) || (pattern->translate == GE_DIRECTION_BOTH))
		{
			translate_y = 0.0-y;
		}

		cairo_matrix_translate(&current_matrix, translate_x, translate_y);
	}

	cairo_pattern_set_matrix(pattern->handle, &current_matrix);

	cairo_save(canvas);

	cairo_set_source(canvas, pattern->handle);
        cairo_set_operator(canvas, pattern->operator);
	cairo_rectangle(canvas, x, y, width, height);

	cairo_fill (canvas);

	cairo_restore(canvas);

	cairo_pattern_set_matrix(pattern->handle, &original_matrix);
}

/***********************************************
 * ge_cairo_color_pattern -
 *  
 *   Create A Solid Color Pattern
 ***********************************************/
CairoPattern*
ge_cairo_color_pattern(CairoColor *base)
{	
	CairoPattern * result = g_new0(CairoPattern, 1);

	#if  ((CAIRO_VERSION_MAJOR < 1) || ((CAIRO_VERSION_MAJOR == 1) && (CAIRO_VERSION_MINOR < 2)))
		result->type = CAIRO_PATTERN_TYPE_SOLID;
	#endif

	result->scale = GE_DIRECTION_NONE;
	result->translate = GE_DIRECTION_NONE;

	result->handle = cairo_pattern_create_rgba(base->r, 
							base->g, 
							base->b, 
							base->a);

	result->operator = CAIRO_OPERATOR_SOURCE;
	
	return result;
}

/***********************************************
 * ge_cairo_pixbuf_pattern -
 *  
 *   Create A Tiled Pixbuf Pattern
 ***********************************************/
CairoPattern*
ge_cairo_pixbuf_pattern(GdkPixbuf *pixbuf)
{	
	CairoPattern * result = g_new0(CairoPattern, 1);

	cairo_t *canvas;
	cairo_surface_t * surface;
	gint width, height;

	#if  ((CAIRO_VERSION_MAJOR < 1) || ((CAIRO_VERSION_MAJOR == 1) && (CAIRO_VERSION_MINOR < 2)))
		result->type = CAIRO_PATTERN_TYPE_SURFACE;
	#endif

	result->scale = GE_DIRECTION_NONE;
	result->translate = GE_DIRECTION_BOTH;

	width = gdk_pixbuf_get_width(pixbuf);
	height = gdk_pixbuf_get_height(pixbuf);
	
	surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);

	canvas = cairo_create(surface);

	gdk_cairo_set_source_pixbuf (canvas, pixbuf, 0, 0);
	cairo_rectangle (canvas, 0, 0, width, height);
	cairo_fill (canvas);
	cairo_destroy(canvas);

	result->handle = cairo_pattern_create_for_surface (surface);
	cairo_surface_destroy(surface);

	cairo_pattern_set_extend (result->handle, CAIRO_EXTEND_REPEAT);

	result->operator = CAIRO_OPERATOR_SOURCE;

	return result;
}

/***********************************************
 * ge_cairo_pixmap_pattern -
 *  
 *   Create A Tiled Pixmap Pattern
 ***********************************************/
CairoPattern*
ge_cairo_pixmap_pattern(GdkPixmap *pixmap)
{	
	CairoPattern * result = NULL;

	GdkPixbuf * pixbuf;
	gint width, height;

	gdk_drawable_get_size (GDK_DRAWABLE (pixmap), &width, &height);

	pixbuf = gdk_pixbuf_get_from_drawable(NULL, GDK_DRAWABLE (pixmap), 
				gdk_drawable_get_colormap(GDK_DRAWABLE (pixmap)), 
				0, 0, 0, 0, width, height);

	result = ge_cairo_pixbuf_pattern(pixbuf);
	
	g_object_unref (pixbuf);

	return result;
}

/***********************************************
 * ge_cairo_linear_shade_gradient_pattern - 
 *  
 *   Create A Linear Shade Gradient Pattern
 *   Aka Smooth Shade Gradient, from/to gradient
 *   With End points defined as shades of the
 *   base color
 ***********************************************/
CairoPattern *
ge_cairo_linear_shade_gradient_pattern(CairoColor *base, 
						gdouble shade1, 
						gdouble shade2, 
						gboolean vertical)
{
	CairoPattern * result = g_new0(CairoPattern, 1);
	
	#if  ((CAIRO_VERSION_MAJOR < 1) || ((CAIRO_VERSION_MAJOR == 1) && (CAIRO_VERSION_MINOR < 2)))
		result->type = CAIRO_PATTERN_TYPE_LINEAR;
	#endif

	if (vertical)
	{
		result->scale = GE_DIRECTION_VERTICAL;

		result->handle = cairo_pattern_create_linear(0, 0, 1, 0);
	}
	else
	{
		result->scale = GE_DIRECTION_HORIZONTAL;

		result->handle = cairo_pattern_create_linear(0, 0, 0, 1);
	}

	result->translate = GE_DIRECTION_BOTH;
	result->operator = CAIRO_OPERATOR_SOURCE;

	ge_cairo_pattern_add_color_stop_shade(result->handle, 0, base, shade1);
	ge_cairo_pattern_add_color_stop_shade(result->handle, 1, base, shade2);

	return result;
}

void
ge_cairo_pattern_destroy(CairoPattern *pattern)
{
	if (pattern)
	{
		if (pattern->handle)
			cairo_pattern_destroy(pattern->handle);
			
		g_free(pattern);
	}
}

/* The following function will be called by GTK+ when the module
 * is loaded and checks to see if we are compatible with the
 * version of GTK+ that loads us.
 */
GE_EXPORT const gchar* g_module_check_init (GModule *module);
const gchar*
g_module_check_init (GModule *module)
{
	(void) module;
	
	return gtk_check_version (GTK_MAJOR_VERSION,
				  GTK_MINOR_VERSION,
				  GTK_MICRO_VERSION - GTK_INTERFACE_AGE);
}
