#include "support.h"

/* #define ALWAYS_DITHER_GRADIENTS */

GtkTextDirection
get_direction (GtkWidget *widget)
{
	GtkTextDirection dir;
	
	if (widget)
		dir = gtk_widget_get_direction (widget);
	else
		dir = GTK_TEXT_DIR_LTR;
	
	return dir;
}

GdkPixbuf *
generate_bit (unsigned char alpha[], GdkColor *color, double mult)
{
	guint r, g, b;
	GdkPixbuf *pixbuf;
	unsigned char *pixels;
	int w, h, rs;
	int x, y;
	
	r = (color->red >> 8) * mult;
	r = MIN(r, 255);
	g = (color->green >> 8) * mult;
	g = MIN(g, 255);
	b = (color->blue >> 8) * mult;
	b = MIN(b, 255);
	
	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, RADIO_SIZE, RADIO_SIZE);
	
	w = gdk_pixbuf_get_width (pixbuf);
	h = gdk_pixbuf_get_height (pixbuf);
	rs = gdk_pixbuf_get_rowstride (pixbuf);
	pixels = gdk_pixbuf_get_pixels (pixbuf);
	
	
	for (y=0; y < h; y++)
	{
		for (x=0; x < w; x++)
		{
			pixels[y*rs + x*4 + 0] = r;
			pixels[y*rs + x*4 + 1] = g;
			pixels[y*rs + x*4 + 2] = b;
			if (alpha)
				pixels[y*rs + x*4 + 3] = alpha[y*w + x];
			else
				pixels[y*rs + x*4 + 3] = 255;
		}
	}
	
	return pixbuf;
}

#define CLAMP_UCHAR(v) ((guchar) (CLAMP (((int)v), (int)0, (int)255)))

GdkPixbuf *
colorize_bit (unsigned char *bit,
              unsigned char *alpha,
              GdkColor  *new_color)
{
	GdkPixbuf *pixbuf;
	double intensity;
	int x, y;
	const guchar *src, *asrc;
	guchar *dest;
	int dest_rowstride;
	int width, height;
	guchar *dest_pixels;
	
	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, RADIO_SIZE, RADIO_SIZE);
	
	if (pixbuf == NULL)
		return NULL;
	
	dest_rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	dest_pixels = gdk_pixbuf_get_pixels (pixbuf);
	
	for (y = 0; y < RADIO_SIZE; y++)
	{
		src = bit + y * RADIO_SIZE;
		asrc = alpha + y * RADIO_SIZE;
		dest = dest_pixels + y * dest_rowstride;
	
		for (x = 0; x < RADIO_SIZE; x++)
		{
			double dr, dg, db;
	
			intensity = (src[x] + 0 )/ 255.0;
	
			if (intensity <= 0.5)
			{
				/* Go from black at intensity = 0.0 to new_color at intensity = 0.5 */
				dr = (new_color->red * intensity * 2.0) / 65535.0;
				dg = (new_color->green * intensity * 2.0) / 65535.0;
				db = (new_color->blue * intensity * 2.0) / 65535.0;
			}
			else
			{
				/* Go from new_color at intensity = 0.5 to white at intensity = 1.0 */
				dr = (new_color->red + (65535 - new_color->red) * (intensity - 0.5) * 2.0) / 65535.0;
				dg = (new_color->green + (65535 - new_color->green) * (intensity - 0.5) * 2.0) / 65535.0;
				db = (new_color->blue + (65535 - new_color->blue) * (intensity - 0.5) * 2.0) / 65535.0;
			}
	
			dest[0] = CLAMP_UCHAR (255 * dr);
			dest[1] = CLAMP_UCHAR (255 * dg);
			dest[2] = CLAMP_UCHAR (255 * db);
	
			dest[3] = asrc[x];
			dest += 4;
		}
	}
	
	return pixbuf;
}

GdkPixmap *
pixbuf_to_pixmap (GtkStyle  *style,
                  GdkPixbuf *pixbuf,
                  GdkScreen *screen)
{
	GdkGC *tmp_gc;
	GdkPixmap *pixmap;
	
	pixmap = gdk_pixmap_new (gdk_screen_get_root_window (screen),
	                         gdk_pixbuf_get_width (pixbuf),
	                         gdk_pixbuf_get_height (pixbuf),
	                         style->depth);
							 
	gdk_drawable_set_colormap (pixmap, style->colormap);
	
	tmp_gc = gdk_gc_new (pixmap);
	
	gdk_pixbuf_render_to_drawable (pixbuf, pixmap, tmp_gc, 0, 0, 0, 0,
	                               gdk_pixbuf_get_width (pixbuf),
	                               gdk_pixbuf_get_height (pixbuf),
	                               GDK_RGB_DITHER_NORMAL, 0, 0);
	
	gdk_gc_unref (tmp_gc);
	
	return pixmap;
}


void
rgb_to_hls (gdouble *r,
            gdouble *g,
            gdouble *b)
{
	gdouble min;
	gdouble max;
	gdouble red;
	gdouble green;
	gdouble blue;
	gdouble h, l, s;
	gdouble delta;
	
	red = *r;
	green = *g;
	blue = *b;

	if (red > green)
	{
		if (red > blue)
			max = red;
		else
			max = blue;
	
		if (green < blue)
			min = green;
		else
			min = blue;
	}
	else
	{
		if (green > blue)
			max = green;
		else
			max = blue;
	
		if (red < blue)
			min = red;
		else
			min = blue;
	}

	l = (max + min) / 2;
	s = 0;
	h = 0;

	if (max != min)
	{
		if (l <= 0.5)
			s = (max - min) / (max + min);
		else
			s = (max - min) / (2 - max - min);
	
		delta = max -min;
		if (red == max)
			h = (green - blue) / delta;
		else if (green == max)
			h = 2 + (blue - red) / delta;
		else if (blue == max)
			h = 4 + (red - green) / delta;
	
		h *= 60;
		if (h < 0.0)
			h += 360;
	}

	*r = h;
	*g = l;
	*b = s;
}

void
hls_to_rgb (gdouble *h,
            gdouble *l,
            gdouble *s)
{
	gdouble hue;
	gdouble lightness;
	gdouble saturation;
	gdouble m1, m2;
	gdouble r, g, b;
	
	lightness = *l;
	saturation = *s;

	if (lightness <= 0.5)
		m2 = lightness * (1 + saturation);
	else
		m2 = lightness + saturation - lightness * saturation;
		
	m1 = 2 * lightness - m2;

	if (saturation == 0)
	{
		*h = lightness;
		*l = lightness;
		*s = lightness;
	}
	else
	{
		hue = *h + 120;
		while (hue > 360)
			hue -= 360;
		while (hue < 0)
			hue += 360;
	
		if (hue < 60)
			r = m1 + (m2 - m1) * hue / 60;
		else if (hue < 180)
			r = m2;
		else if (hue < 240)
			r = m1 + (m2 - m1) * (240 - hue) / 60;
		else
			r = m1;
	
		hue = *h;
		while (hue > 360)
			hue -= 360;
		while (hue < 0)
			hue += 360;
	
		if (hue < 60)
			g = m1 + (m2 - m1) * hue / 60;
		else if (hue < 180)
			g = m2;
		else if (hue < 240)
			g = m1 + (m2 - m1) * (240 - hue) / 60;
		else
			g = m1;
	
		hue = *h - 120;
		while (hue > 360)
			hue -= 360;
		while (hue < 0)
			hue += 360;
	
		if (hue < 60)
			b = m1 + (m2 - m1) * hue / 60;
		else if (hue < 180)
			b = m2;
		else if (hue < 240)
			b = m1 + (m2 - m1) * (240 - hue) / 60;
		else
			b = m1;
	
		*h = r;
		*l = g;
		*s = b;
	}
}

void
shade (GdkColor * a, GdkColor * b, float k)
{
	gdouble red;
	gdouble green;
	gdouble blue;
	
	red = (gdouble) a->red / 65535.0;
	green = (gdouble) a->green / 65535.0;
	blue = (gdouble) a->blue / 65535.0;
	
	rgb_to_hls (&red, &green, &blue);
	
	green *= k;
	if (green > 1.0)
		green = 1.0;
	else if (green < 0.0)
		green = 0.0;
	
	blue *= k;
	if (blue > 1.0)
		blue = 1.0;
	else if (blue < 0.0)
		blue = 0.0;
	
	hls_to_rgb (&red, &green, &blue);
	
	b->red = red * 65535.0;
	b->green = green * 65535.0;
	b->blue = blue * 65535.0;
}


/**************************************************************************/

void
arrow_draw_hline (GdkWindow     *window,
		GdkGC         *gc,
		int            x1,
		int            x2,
		int            y,
		gboolean       last)
{
	if (x2 - x1 < 7 && !last) /* 7 to get garretts pixels, otherwise 6 */
	{
		gdk_draw_line (window, gc, x1, y, x2, y);
	}
	else if (last)
	{
		/* we don't draw "spikes" for very small arrows */
		if (x2 - x1 <= 9)
		{
			/*gdk_draw_line (window, gc, x1+1, y, x1+1, y);
			gdk_draw_line (window, gc, x2-1, y, x2-1, y);*/
		}
		else
		{
			gdk_draw_line (window, gc, x1+2, y, x1+2, y);
			gdk_draw_line (window, gc, x2-2, y, x2-2, y);
		}
	}
	else
	{
		gdk_draw_line (window, gc, x1, y, x1+2, y);
		gdk_draw_line (window, gc, x2-2, y, x2, y);
	}
}

void
arrow_draw_vline (GdkWindow     *window,
                  GdkGC         *gc,
                  int            y1,
                  int            y2,
                  int            x,
                  gboolean       last)
{
	if (y2 - y1 < 7 && !last) /* 7 to get garretts pixels */
		gdk_draw_line (window, gc, x, y1, x, y2);
	else if (last)
	{
		/* we don't draw "spikes" for very small arrows */
		if (y2 - y1 > 9) {
			gdk_draw_line (window, gc, x, y1+2, x, y1+2);
			gdk_draw_line (window, gc, x, y2-2, x, y2-2);
		}
	}
	else
	{
		gdk_draw_line (window, gc, x, y1, x, y1+2);
		gdk_draw_line (window, gc, x, y2-2, x, y2);
	}
}



void
draw_arrow (GdkWindow     *window,
            GdkGC         *gc,
            GdkRectangle  *area,
            GtkArrowType   arrow_type,
            gint           x,
            gint           y,
            gint           width,
            gint           height)
{
	gint i, j;

	if (area)
		gdk_gc_set_clip_rectangle (gc, area);

	if (arrow_type == GTK_ARROW_DOWN)
	{
		for (i = 0, j = -1; i < height; i++, j++)
			arrow_draw_hline (window, gc, x + j, x + width - j - 1, y + i, i == 0);

	}
	else if (arrow_type == GTK_ARROW_UP)
	{
		for (i = height - 1, j = -1; i >= 0; i--, j++)
			arrow_draw_hline (window, gc, x + j, x + width - j - 1, y + i, i == height - 1);
	}
	else if (arrow_type == GTK_ARROW_LEFT)
	{
		for (i = width - 1, j = -1; i >= 0; i--, j++)
			arrow_draw_vline (window, gc, y + j, y + height - j - 1, x + i, i == width - 1);
	}
	else if (arrow_type == GTK_ARROW_RIGHT)
	{
		for (i = 0, j = -1; i < width; i++, j++)
			arrow_draw_vline (window, gc, y + j, y + height - j - 1,  x + i, i == 0);
	}

	if (area)
		gdk_gc_set_clip_rectangle (gc, NULL);
}

void
calculate_arrow_geometry (GtkArrowType  arrow_type,
                          gint         *x,
                          gint         *y,
                          gint         *width,
                          gint         *height)
{
	gint w = *width;
	gint h = *height;

	switch (arrow_type)
	{
		case GTK_ARROW_UP:
		case GTK_ARROW_DOWN:
			w += (w % 2) - 1;
			h = (w / 2 + 1) + 1;
		
			if (h > *height)
			{
				h = *height;
				w = 2 * (h - 1) - 1;
			}
		
			if (arrow_type == GTK_ARROW_DOWN)
			{
				if (*height % 2 == 1 || h % 2 == 0)
					*height += 1;
			}
			else
			{
				if (*height % 2 == 0 || h % 2 == 0)
					*height -= 1;
			}
			break;
	
		case GTK_ARROW_RIGHT:
		case GTK_ARROW_LEFT:
			h += (h % 2) - 1;
			w = (h / 2 + 1) + 1;
		
			if (w > *width)
			{
				w = *width;
				h = 2 * (w - 1) - 1;
			}
		
			if (arrow_type == GTK_ARROW_RIGHT)
			{
				if (*width % 2 == 1 || w % 2 == 0)
					*width += 1;
			}
			else
			{
				if (*width % 2 == 0 || w % 2 == 0)
					*width -= 1;
			}
			break;
	
		default:
			/* should not be reached */
			break;
	}

	*x += (*width - w) / 2;
	*y += (*height - h) / 2;
	*height = h;
	*width = w;
}


void gtk_treeview_get_header_index (GtkTreeView *tv, GtkWidget *header,
                                           gint *column_index, gint *columns,
gboolean *resizable)
{
	GList *list;
	*column_index = *columns = 0;
	list = gtk_tree_view_get_columns (tv);

	do
	{
		GtkTreeViewColumn *column = GTK_TREE_VIEW_COLUMN(list->data);
		if ( column->button == header )
		{
			*column_index = *columns;
			*resizable = column->resizable;
		}
		if ( column->visible )
			(*columns)++;
	} while ((list = g_list_next(list)));
}

void gtk_clist_get_header_index (GtkCList *clist, GtkWidget *button,
                                 gint *column_index, gint *columns)
{
	*columns = clist->columns;
	int i;
	
	for (i=0; i<*columns; i++)
	{
		if (clist->column[i].button == button)
		{
			*column_index = i;
			break;
		}
	}
}

gboolean
sanitize_size (GdkWindow      *window,
               gint           *width,
               gint           *height)
{
	gboolean set_bg = FALSE;
	
	if ((*width == -1) && (*height == -1))
	{
		set_bg = GDK_IS_WINDOW (window);
		gdk_window_get_size (window, width, height);
	}
	else if (*width == -1)
		gdk_window_get_size (window, width, NULL);
	else if (*height == -1)
		gdk_window_get_size (window, NULL, height);
	
	return set_bg;
}

static GtkRequisition default_option_indicator_size = { 7, 13 };
static GtkBorder default_option_indicator_spacing = { 7, 5, 2, 2 };

void
option_menu_get_props (GtkWidget      *widget,
                       GtkRequisition *indicator_size,
                       GtkBorder      *indicator_spacing)
{
	GtkRequisition *tmp_size = NULL;
	GtkBorder *tmp_spacing = NULL;
	
	if (widget)
		gtk_widget_style_get (widget, "indicator_size", &tmp_size,
		                      "indicator_spacing", &tmp_spacing, NULL);
	
	if (tmp_size)
	{
		*indicator_size = *tmp_size;
		g_free (tmp_size);
	}
	else
		*indicator_size = default_option_indicator_size;
	
	if (tmp_spacing)
	{
		*indicator_spacing = *tmp_spacing;
		g_free (tmp_spacing);
	}
	else
		*indicator_spacing = default_option_indicator_spacing;
}

GtkWidget *special_get_ancestor(GtkWidget * widget,
				       GType widget_type)
{
	g_return_val_if_fail(GTK_IS_WIDGET(widget), NULL);

	while (widget && widget->parent
	       && !g_type_is_a(GTK_WIDGET_TYPE(widget->parent),
			       widget_type))
		widget = widget->parent;

	if (!
	    (widget && widget->parent
	     && g_type_is_a(GTK_WIDGET_TYPE(widget->parent), widget_type)))
		return NULL;

	return widget;
}

/* Dithered Gradient Buffers */
static void
internel_image_buffer_free_pixels (guchar *pixels, gpointer data)
{
	g_free (pixels);
}

static GdkPixbuf*
internal_image_buffer_new (gint width, gint height)
{
	guchar *buf;
	int rowstride;

	g_return_val_if_fail (width > 0, NULL);
	g_return_val_if_fail (height > 0, NULL);

	rowstride = width * 3;

	buf = g_try_malloc (height * rowstride);

	if (!buf)
		return NULL;

	return gdk_pixbuf_new_from_data(buf, GDK_COLORSPACE_RGB,
					FALSE, 8,
					width, height, rowstride,
					internel_image_buffer_free_pixels, NULL);
}

static void 
internal_color_get_as_uchars(GdkColor *color, 
				guchar *red, 
				guchar *green, 
				guchar *blue)
{
	*red = (guchar) (color->red / 256.0);
	*green = (guchar) (color->green / 256.0);
	*blue = (guchar) (color->blue / 256.0);
}				

static GdkPixbuf*
internal_create_horizontal_gradient_image_buffer (gint width, gint height,
							GdkColor *from,
							GdkColor *to)
{    
	int i;
	long r, g, b, dr, dg, db;
	GdkPixbuf* buffer;
	guchar *ptr;
	guchar *pixels;
	guchar r0, g0, b0;
	guchar rf, gf, bf;
	int rowstride;

	buffer = internal_image_buffer_new (width, height);

	if (buffer == NULL)
		return NULL;
    
	pixels = gdk_pixbuf_get_pixels (buffer);
	ptr = pixels;
	rowstride = gdk_pixbuf_get_rowstride (buffer);
  
	internal_color_get_as_uchars(from, &r0, &g0, &b0);
	internal_color_get_as_uchars(to, &rf, &gf, &bf);
  
	r = r0 << 16;
	g = g0 << 16;
	b = b0 << 16;
    
	dr = ((rf-r0)<<16)/width;
	dg = ((gf-g0)<<16)/width;
	db = ((bf-b0)<<16)/width;

	/* render the first line */
	for (i=0; i<width; i++)
	{
		*(ptr++) = (guchar)(r>>16);
		*(ptr++) = (guchar)(g>>16);
		*(ptr++) = (guchar)(b>>16);

		r += dr;
		g += dg;
		b += db;
	}

	/* copy the first line to the other lines */
	for (i=1; i<height; i++)
	{
		memcpy (&(pixels[i*rowstride]), pixels, rowstride);
	}
    
	return buffer;
}

static GdkPixbuf*
internal_create_vertical_gradient_image_buffer (gint width, gint height,
						GdkColor *from,
						GdkColor *to)
{
	gint i, j, max_block, last_block;
	long r, g, b, dr, dg, db;
	GdkPixbuf *buffer;
	
	guchar *ptr;
	guchar point[4];

	guchar r0, g0, b0;
	guchar rf, gf, bf;

	gint rowstride;
	guchar *pixels;
  
	buffer = internal_image_buffer_new (width, height);

	if (buffer == NULL)
		return NULL;
    
	pixels = gdk_pixbuf_get_pixels (buffer);
	rowstride = gdk_pixbuf_get_rowstride (buffer);
  
	internal_color_get_as_uchars(from, &r0, &g0, &b0);
	internal_color_get_as_uchars(to, &rf, &gf, &bf);

	r = r0<<16;
	g = g0<<16;
	b = b0<<16;

	dr = ((rf-r0)<<16)/height;
	dg = ((gf-g0)<<16)/height;
	db = ((bf-b0)<<16)/height;

	max_block = width/2;

	for (i=0; i < height; i++)
	{
		ptr = pixels + i * rowstride;
      
		ptr[0] = r>>16;
		ptr[1] = g>>16;
		ptr[2] = b>>16;
		
		if (width > 1)
		{
			last_block = 0;

			for (j=1; j <= max_block; j *= 2)
			{
				memcpy (&(ptr[j*3]), ptr, j*3);

				if ((j*2) >= max_block)
				{
					last_block = j*2;
				}
			}

			if ((last_block < width) && (last_block > 0))
			{
				memcpy (&(ptr[last_block*3]), ptr, (width - last_block)*3);
			}
		}

		r += dr;
		g += dg;
		b += db;
	}
	
	return buffer;
}

void
draw_vgradient (GdkDrawable *drawable, GdkGC *gc, GtkStyle *style,
                int x, int y, int width, int height,
                GdkColor *left_color, GdkColor *right_color)
{
	#ifndef ALWAYS_DITHER_GRADIENTS
	gboolean dither = ((style->depth > 0) && (style->depth <= 16));
	#endif

	if ((width <= 0) || (height <= 0))
		return;

	if ( left_color == NULL || right_color == NULL )
	{
		gdk_draw_rectangle (drawable, gc, TRUE, x, y, width, height);
		return;
	}

	#ifndef ALWAYS_DITHER_GRADIENTS
	if (dither)
	#endif
	{
		GdkPixbuf *image_buffer = NULL;
				
		image_buffer = internal_create_horizontal_gradient_image_buffer (width, height, left_color, right_color);
	
		if (image_buffer)
		{
			gdk_draw_pixbuf(drawable, gc, image_buffer, 0, 0, x, y, width, height, GDK_RGB_DITHER_MAX, 0, 0);

			g_object_unref(image_buffer);
		}	
	}
	#ifndef ALWAYS_DITHER_GRADIENTS
	else
	{
		int i;
		GdkColor col;
		int dr, dg, db;
		GdkGCValues old_values;

		gdk_gc_get_values (gc, &old_values);
	
		if (left_color == right_color )
		{
			col = *left_color;
			gdk_rgb_find_color (style->colormap, &col);
			gdk_gc_set_foreground (gc, &col);
			gdk_draw_rectangle (drawable, gc, TRUE, x, y, width, height);
			gdk_gc_set_foreground (gc, &old_values.foreground);
			return;
		}
	
		col = *left_color;
		dr = (right_color->red - left_color->red) / width;
		dg = (right_color->green - left_color->green) / width;
		db = (right_color->blue - left_color->blue) / width;

		for (i = 0; i < width; i++)
		{
			gdk_rgb_find_color (style->colormap, &col);
	
			gdk_gc_set_foreground (gc, &col);
			gdk_draw_line (drawable, gc, x + i, y, x + i, y + height - 1);
		
			col.red += dr;
			col.green += dg;
			col.blue += db;
		}

		gdk_gc_set_foreground (gc, &old_values.foreground);
	}
	#endif
}

void
draw_hgradient (GdkDrawable *drawable, GdkGC *gc, GtkStyle *style,
                int x, int y, int width, int height,
                GdkColor *top_color, GdkColor *bottom_color)
{	
	#ifndef ALWAYS_DITHER_GRADIENTS
	gboolean dither = ((style->depth > 0) && (style->depth <= 16));
	#endif
	
	if ((width <= 0) || (height <= 0))
		return;

	#ifndef ALWAYS_DITHER_GRADIENTS
	if (dither)
	#endif
	{
		GdkPixbuf *image_buffer = NULL;
				
		image_buffer = internal_create_vertical_gradient_image_buffer (width, height, top_color, bottom_color);
	
		if (image_buffer)
		{
			gdk_draw_pixbuf(drawable, gc, image_buffer, 0, 0, x, y, width, height, GDK_RGB_DITHER_MAX, 0, 0);

			g_object_unref(image_buffer);
		}	
	}
	#ifndef ALWAYS_DITHER_GRADIENTS
	else
	{
		int i;
		GdkColor col;
		int dr, dg, db;
		GdkGCValues old_values;
	
		gdk_gc_get_values (gc, &old_values);

		if (top_color == bottom_color )
		{
				col = *top_color;
				gdk_rgb_find_color (style->colormap, &col);
				gdk_gc_set_foreground (gc, &col);
				gdk_draw_rectangle (drawable, gc, TRUE, x, y, width, height);
				gdk_gc_set_foreground (gc, &old_values.foreground);
				return;
		}

		col = *top_color;
		dr = (bottom_color->red - top_color->red) / height;
		dg = (bottom_color->green - top_color->green) / height;
		db = (bottom_color->blue - top_color->blue) / height;
	
		for (i = 0; i < height; i++)
		{
			gdk_rgb_find_color (style->colormap, &col);
	
			gdk_gc_set_foreground (gc, &col);
			gdk_draw_line (drawable, gc, x, y + i, x + width - 1, y + i);
		
			col.red += dr;
			col.green += dg;
			col.blue += db;
		}

		gdk_gc_set_foreground (gc, &old_values.foreground);
	}
	#endif
}

void blend (GdkColormap *colormap,
                       GdkColor *a, GdkColor *b, GdkColor *c, int alpha)
{
	int inAlpha = 100-alpha;
	c->red   = (a->red   * alpha + b->red   * inAlpha) / 100;
	c->green = (a->green * alpha + b->green * inAlpha) / 100;
	c->blue  = (a->blue  * alpha + b->blue  * inAlpha) / 100;
	
	gdk_rgb_find_color (colormap, c);
}

GtkWidget *get_parent_window (GtkWidget *widget)
{
        GtkWidget *parent = widget->parent;

        while (parent && GTK_WIDGET_NO_WINDOW (parent))
                parent = parent->parent;

        return parent;
}

GdkColor *get_parent_bgcolor (GtkWidget *widget)
{
        GtkWidget *parent = get_parent_window (widget);

        if (parent && parent->style)
                return &parent->style->bg[GTK_STATE_NORMAL];

        return NULL;
}

GtkWidget *
find_combo_box_widget (GtkWidget * widget)
{
	GtkWidget *result = NULL;
	
	if (widget && !GTK_IS_COMBO_BOX_ENTRY (widget))
	{
		if (GTK_IS_COMBO_BOX (widget))
			result = widget;
		else
			result = find_combo_box_widget(widget->parent);
	}
	
	return result;
}

gboolean
is_combo_box (GtkWidget * widget)
{
	return (find_combo_box_widget(widget) != NULL);
}
