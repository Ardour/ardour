#include <gtk/gtk.h>

#include "clearlooks_style.h"
#include "clearlooks_rc_style.h"
#include "clearlooks_draw.h"

#include <math.h>
#include <string.h>

#include "bits.c"
#include "support.h"
//#include "config.h"

/* #define DEBUG 1 */

#define SCALE_SIZE 5

#define DETAIL(xx)   ((detail) && (!strcmp(xx, detail)))
#define COMPARE_COLORS(a,b) (a.red == b.red && a.green == b.green && a.blue == b.blue)

#define DRAW_ARGS    GtkStyle       *style, \
                     GdkWindow      *window, \
                     GtkStateType    state_type, \
                     GtkShadowType   shadow_type, \
                     GdkRectangle   *area, \
                     GtkWidget      *widget, \
                     const gchar    *detail, \
                     gint            x, \
                     gint            y, \
                     gint            width, \
                     gint            height

static GdkGC *realize_color (GtkStyle * style, GdkColor * color);
static GtkStyleClass *parent_class;
static GList *progressbars = NULL;
static gint8 pboffset = 10;
static int timer_id = 0;

static void cl_progressbar_remove (gpointer data)
{
	if (g_list_find (progressbars, data) == NULL)
		return;

	progressbars = g_list_remove (progressbars, data);
	g_object_unref (data);
	
	if (g_list_first(progressbars) == NULL) {
		g_source_remove(timer_id);
		timer_id = 0;
	}
}

static void update_progressbar (gpointer data, gpointer user_data)
{
	gfloat fraction;

	if (data == NULL)
		return;

	fraction = gtk_progress_bar_get_fraction (GTK_PROGRESS_BAR (data));
	
	/* update only if not filled */
	if (fraction < 1.0)
		gtk_widget_queue_resize ((GtkWidget*)data);
	
	if (fraction >= 1.0 || GTK_PROGRESS (data)->activity_mode)
		cl_progressbar_remove (data);
}

static gboolean timer_func (gpointer data)
{
	g_list_foreach (progressbars, update_progressbar, NULL);
	if (--pboffset < 0) pboffset = 9;
	return (g_list_first(progressbars) != NULL);
}

static gboolean cl_progressbar_known(gconstpointer data)
{
	return (g_list_find (progressbars, data) != NULL);
}


static void cl_progressbar_add (gpointer data)
{
	if (!GTK_IS_PROGRESS_BAR (data))
		return;

	progressbars = g_list_append (progressbars, data);

	g_object_ref (data);
	g_signal_connect ((GObject*)data, "unrealize", G_CALLBACK (cl_progressbar_remove), data);
	
	if (timer_id == 0)
		timer_id = g_timeout_add (100, timer_func, NULL);
}

static GdkColor *
clearlooks_get_spot_color (ClearlooksRcStyle *clearlooks_rc)
{
	GtkRcStyle *rc = GTK_RC_STYLE (clearlooks_rc);
	
	if (clearlooks_rc->has_spot_color)
		return &clearlooks_rc->spot_color;
	else
		return &rc->base[GTK_STATE_SELECTED];
}

/**************************************************************************/

/* used for optionmenus... */
static void
draw_tab (GtkStyle      *style,
	  GdkWindow     *window,
	  GtkStateType   state_type,
	  GtkShadowType  shadow_type,
	  GdkRectangle  *area,
	  GtkWidget     *widget,
	  const gchar   *detail,
	  gint           x,
	  gint           y,
	  gint           width,
	  gint           height)
{
#define ARROW_SPACE 2
#define ARROW_LINE_HEIGHT 2
#define ARROW_LINE_WIDTH 5
	ClearlooksStyle *clearlooks_style = CLEARLOOKS_STYLE (style);
	GtkRequisition indicator_size;
	GtkBorder indicator_spacing;
	gint arrow_height;
	
	option_menu_get_props (widget, &indicator_size, &indicator_spacing);
	
	indicator_size.width += (indicator_size.width % 2) - 1;
	arrow_height = indicator_size.width / 2 + 2;
	
	x += (width - indicator_size.width) / 2;
	y += height/2;

	if (state_type == GTK_STATE_INSENSITIVE)
	{
		draw_arrow (window, style->light_gc[state_type], area,
		            GTK_ARROW_UP, 1+x, 1+y-arrow_height,
		            indicator_size.width, arrow_height);

		draw_arrow (window, style->light_gc[state_type], area,
		            GTK_ARROW_DOWN, 1+x, 1+y+1,
		            indicator_size.width, arrow_height);
	}
	
	draw_arrow (window, style->fg_gc[state_type], area,
	            GTK_ARROW_UP, x, y-arrow_height,
	            indicator_size.width, arrow_height);
	
	draw_arrow (window, style->fg_gc[state_type], area,
	            GTK_ARROW_DOWN, x, y+1,
	            indicator_size.width, arrow_height);
}

static void
clearlooks_draw_arrow (GtkStyle      *style,
                       GdkWindow     *window,
                       GtkStateType   state,
                       GtkShadowType  shadow,
                       GdkRectangle  *area,
                       GtkWidget     *widget,
                       const gchar   *detail,
                       GtkArrowType   arrow_type,
                       gboolean       fill,
                       gint           x,
                       gint           y,
                       gint           width,
                       gint           height)
{
	ClearlooksStyle *clearlooks_style = CLEARLOOKS_STYLE (style);
	gint original_width, original_x;
	GdkGC *gc;
	
	sanitize_size (window, &width, &height);
	
	if (is_combo_box (widget))
	{
		width = 7;
		height = 5;
		x+=2;
		y+=4;
		if (state == GTK_STATE_INSENSITIVE)
		{
			draw_arrow (window, style->light_gc[state], area,
			            GTK_ARROW_UP, 1+x, 1+y-height,
			            width, height);

			draw_arrow (window, style->light_gc[state], area,
			            GTK_ARROW_DOWN, 1+x, 1+y+1,
			            width, height);
		}
	
		draw_arrow (window, style->fg_gc[state], area,
		            GTK_ARROW_UP, x, y-height,
		            width, height);
	
		draw_arrow (window, style->fg_gc[state], area,
		            GTK_ARROW_DOWN, x, y+1,
		            width, height);
		
		return;
	}
	
	original_width = width;
	original_x = x;
	
	/* Make spinbutton arrows and arrows in menus
	* slightly larger to get the right pixels drawn */
	if (DETAIL ("spinbutton"))
		height += 1;
	
	if (DETAIL("menuitem"))
	{
		width = 6;
		height = 7;
	}
	
	/* Compensate arrow position for "sunken" look */ 
	if (DETAIL ("spinbutton") && arrow_type == GTK_ARROW_DOWN &&
	    style->xthickness > 2 && style->ythickness > 2)
			y -= 1;
	
	if (widget && widget->parent && GTK_IS_COMBO (widget->parent->parent))
	{
		width -= 2;
		height -=2;
		x++;
	}
	
	calculate_arrow_geometry (arrow_type, &x, &y, &width, &height);
	
	if (DETAIL ("menuitem"))
		x = original_x + original_width - width;
	
	if (DETAIL ("spinbutton") && (arrow_type == GTK_ARROW_DOWN))
		y += 1;
	
	if (state == GTK_STATE_INSENSITIVE)
		draw_arrow (window, style->light_gc[state], area, arrow_type, x + 1, y + 1, width, height);

	gc = style->fg_gc[state];
		
	draw_arrow (window, gc, area, arrow_type, x, y, width, height);
}


static void
draw_flat_box (DRAW_ARGS)
{
	ClearlooksStyle *clearlooks_style = CLEARLOOKS_STYLE (style);

	g_return_if_fail (GTK_IS_STYLE (style));
	g_return_if_fail (window != NULL);

	sanitize_size (window, &width, &height);

	if (detail && 	
	    clearlooks_style->listviewitemstyle == 1 && 
	    state_type == GTK_STATE_SELECTED && (
	    !strncmp ("cell_even", detail, strlen ("cell_even")) ||
	    !strncmp ("cell_odd", detail, strlen ("cell_odd"))))
	{
		GdkGC    *gc;
		GdkColor  lower_color;
		GdkColor *upper_color;

		if (GTK_WIDGET_HAS_FOCUS (widget))
		{
			gc = style->base_gc[state_type];
			upper_color = &style->base[state_type];
		}
		else
		{
			gc = style->base_gc[GTK_STATE_ACTIVE];
			upper_color = &style->base[GTK_STATE_ACTIVE];
		}
		
		if (GTK_IS_TREE_VIEW (widget) && 0)
		{
			GtkTreeSelection *sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));

			if (gtk_tree_selection_count_selected_rows (sel) > 1)
			{
				parent_class->draw_flat_box (style, window, state_type, shadow_type,
				                             area, widget, detail,
				                             x, y, width, height);
				return;
			}
		}
		
		shade (upper_color, &lower_color, 0.8);

		if (area)
			gdk_gc_set_clip_rectangle (gc, area);
		
		draw_hgradient (window, gc, style,
				x, y, width, height, upper_color, &lower_color);

		if (area)
			gdk_gc_set_clip_rectangle (gc, NULL);
	}
	else
	{
		parent_class->draw_flat_box (style, window, state_type,
				             shadow_type,
			                     area, widget, detail,
			                     x, y, width, height);
	}
}
/**************************************************************************/

static void
draw_shadow (DRAW_ARGS)
{
	ClearlooksStyle *clearlooks_style = CLEARLOOKS_STYLE (style);
	CLRectangle r;
	
	GdkGC *outer_gc = clearlooks_style->shade_gc[4];
	GdkGC *gc1 = NULL;
	GdkGC *gc2 = NULL;
	gint thickness_light;
	gint thickness_dark;
	gboolean interior_focus = FALSE;

#if DEBUG
		printf("draw_shadow: %s %d %d %d %d\n", detail, x, y, width, height);
#endif

	if (widget == NULL)
	{
		gdk_draw_rectangle (window, outer_gc, FALSE,
		                    x, y, width - 1, height - 1);
		return;
	}

	if ((width == -1) && (height == -1))
		gdk_window_get_size (window, &width, &height);
	else if (width == -1)
		gdk_window_get_size (window, &width, NULL);
	else if (height == -1)
		gdk_window_get_size (window, NULL, &height);

	cl_rectangle_reset (&r, style);

	if (DETAIL ("frame") && widget->parent &&
	    GTK_IS_STATUSBAR (widget->parent))
	{
		gtk_style_apply_default_background (style, window,widget && !GTK_WIDGET_NO_WINDOW (widget),
		                                    state_type, area, x, y, width, height);
      
		if (area)
		{
			gdk_gc_set_clip_rectangle (clearlooks_style->shade_gc[3], area);
			gdk_gc_set_clip_rectangle (clearlooks_style->shade_gc[0], area);
		}
		
		gdk_draw_line (window, clearlooks_style->shade_gc[3],
		               x, y, x + width, y);
		gdk_draw_line (window, clearlooks_style->shade_gc[0],
		               x, y + 1, x + width, y + 1);
		
		if (area)
		{
			gdk_gc_set_clip_rectangle (clearlooks_style->shade_gc[3], NULL);
			gdk_gc_set_clip_rectangle (clearlooks_style->shade_gc[0], NULL);
		}
	}
	else if (detail && !strcmp (detail, "entry"))
	{
		if ( widget->parent && (GTK_IS_COMBO_BOX_ENTRY (widget->parent) ||
		                        GTK_IS_SPIN_BUTTON(widget) ||
		                        GTK_IS_COMBO (widget->parent)))
		{
			cl_draw_combobox_entry (style, window, GTK_WIDGET_STATE(widget), shadow_type, area, widget, detail, x, y, width, height);
		}
		else
		{
			cl_draw_entry (style, window, GTK_WIDGET_STATE(widget), shadow_type, area, widget, detail, x, y, width, height);
		}
	}
	else if (DETAIL ("viewport") || DETAIL ("scrolled_window"))
	{
		gdk_draw_rectangle (window, clearlooks_style->shade_gc[4], FALSE,
		                    x, y, width - 1, height - 1);
	}
	else
	{
		if (DETAIL ("menuitem"))
			outer_gc = clearlooks_style->spot3_gc;
		else
			outer_gc = clearlooks_style->shade_gc[4];

		if (shadow_type == GTK_SHADOW_IN)
			gdk_draw_rectangle (window, outer_gc, FALSE,
			                    x, y, width - 1, height - 1);
		else if (shadow_type == GTK_SHADOW_OUT)
		{
			gdk_draw_rectangle (window, outer_gc, FALSE,
			                    x, y, width - 1, height - 1);
			gdk_draw_line (window, style->light_gc[state_type],
			               x+1, y+1, x+width-2, y+1);
			gdk_draw_line (window, style->light_gc[state_type],
			               x+1, y+1, x+1, y+height-2);
		}
		else if (shadow_type == GTK_SHADOW_ETCHED_IN)
		{
			GdkGC *a = clearlooks_style->shade_gc[(shadow_type == GTK_SHADOW_ETCHED_IN) ? 0 : 3];
			GdkGC *b = clearlooks_style->shade_gc[(shadow_type == GTK_SHADOW_ETCHED_IN) ? 3 : 0];
	
			cl_rectangle_set_corners (&r, CL_CORNER_NONE, CL_CORNER_NONE,
			                              CL_CORNER_NONE, CL_CORNER_NONE);

			r.bordergc = a;
			cl_rectangle_set_clip_rectangle (&r, area);
			cl_draw_rectangle (window, widget, style, x+1, y+1, width-1, height-1, &r);
			cl_rectangle_reset_clip_rectangle (&r);
	
			r.bordergc = b;
			cl_rectangle_set_clip_rectangle (&r, area);
			cl_draw_rectangle (window, widget, style, x, y, width-1, height-1, &r);	
			cl_rectangle_reset_clip_rectangle (&r);
		}
		else if (shadow_type == GTK_SHADOW_ETCHED_IN)
		{
			GdkGC *a = clearlooks_style->shade_gc[(shadow_type == GTK_SHADOW_ETCHED_IN) ? 3 : 0];
			GdkGC *b = clearlooks_style->shade_gc[(shadow_type == GTK_SHADOW_ETCHED_IN) ? 0 : 3];
	
			cl_rectangle_set_corners (&r, CL_CORNER_NONE, CL_CORNER_NONE,
			                              CL_CORNER_NONE, CL_CORNER_NONE);

			r.bordergc = a;
			cl_rectangle_set_clip_rectangle (&r, area);
			cl_draw_rectangle (window, widget, style, x+1, y+1, width-1, height-1, &r);
			cl_rectangle_reset_clip_rectangle (&r);
	
			r.bordergc = b;
			cl_rectangle_set_clip_rectangle (&r, area);
			cl_draw_rectangle (window, widget, style, x, y, width-1, height-1, &r);	
			cl_rectangle_reset_clip_rectangle (&r);
		}		
		else
			parent_class->draw_shadow (style, window, state_type, shadow_type,
			                           area, widget, detail,
			                           x, y, width, height);
	}
}

#define GDK_RECTANGLE_SET(rect,a,b,c,d) rect.x = a; \
                                        rect.y = b; \
                                        rect.width = c; \
                                        rect.height = d;


static void 
draw_box_gap (DRAW_ARGS,
	      GtkPositionType gap_side,
	      gint            gap_x,
	      gint            gap_width)
{
	ClearlooksStyle *clearlooks_style = CLEARLOOKS_STYLE (style);
	CLRectangle r;
	
	GdkRegion *area_region = NULL,
	          *gap_region  = NULL;
	GdkRectangle light_rect;
	GdkRectangle dark_rect;

#if DEBUG
		printf("draw_box_gap: %s %d %d %d %d\n", detail, x, y, width, height);
#endif
	
	g_return_if_fail (GTK_IS_STYLE (style));
	g_return_if_fail (window != NULL);
	
	sanitize_size (window, &width, &height);

	cl_rectangle_reset (&r, style);

	r.bordergc = clearlooks_style->shade_gc[5];

	r.topleft     = style->light_gc[state_type];
	r.bottomright = clearlooks_style->shade_gc[1];

	if (area)
		area_region = gdk_region_rectangle (area);
	else
	{
		GdkRectangle tmp = { x, y, width, height };
		area_region = gdk_region_rectangle (&tmp);
	}
	
	switch (gap_side)
	{
		case GTK_POS_TOP:
		{
			GdkRectangle rect = { x+gap_x+1, y, gap_width-2, 2 };
			gap_region = gdk_region_rectangle (&rect);

			GDK_RECTANGLE_SET (light_rect, x+gap_x+1, y, x+gap_x+1, y+1);
			GDK_RECTANGLE_SET (dark_rect, x+gap_x+gap_width-2, y, x+gap_x+gap_width-2, y);
				
			cl_rectangle_set_corners (&r, CL_CORNER_NONE, CL_CORNER_NONE,
			                              CL_CORNER_ROUND, CL_CORNER_ROUND);

			break;
		}
		case GTK_POS_BOTTOM:
		{
			GdkRectangle rect = { x+gap_x+1, y+height-2, gap_width-2, 2 };
			gap_region = gdk_region_rectangle (&rect);
				
			GDK_RECTANGLE_SET (light_rect, x+gap_x+1, y+height-2, x+gap_x+1, y+height-1);
			GDK_RECTANGLE_SET (dark_rect, x+gap_x+gap_width-2, y+height-2, x+gap_x+gap_width-2, y+height-1);

			cl_rectangle_set_corners (&r, CL_CORNER_ROUND, CL_CORNER_ROUND,
			                              CL_CORNER_NONE, CL_CORNER_NONE);

			break;
		}
		case GTK_POS_LEFT:
		{
			GdkRectangle rect = { x, y+gap_x+1, 2, gap_width-2 };
			gap_region = gdk_region_rectangle (&rect);
				
			GDK_RECTANGLE_SET (light_rect, x, y+gap_x+1, x+1, y+gap_x+1);
			GDK_RECTANGLE_SET (dark_rect, x, y+gap_x+gap_width-2, x, y+gap_x+gap_width-2);

			cl_rectangle_set_corners (&r, CL_CORNER_NONE, CL_CORNER_ROUND,
			                              CL_CORNER_NONE, CL_CORNER_ROUND);
			break;
		}		
		case GTK_POS_RIGHT:
		{
			GdkRectangle rect = { x+width-2, y+gap_x+1, 2, gap_width-2 };
			gap_region = gdk_region_rectangle (&rect);
				
			GDK_RECTANGLE_SET (light_rect, x+width-2, y+gap_x+1, x+width-1, y+gap_x+1);
			GDK_RECTANGLE_SET (dark_rect, x+width-2, y+gap_x+gap_width-2, x+width-1, y+gap_x+gap_width-2);

			cl_rectangle_set_corners (&r, CL_CORNER_ROUND, CL_CORNER_NONE,
			                              CL_CORNER_ROUND, CL_CORNER_NONE);
			break;
		}		
	}
		
	gdk_region_subtract (area_region, gap_region);
		
	gdk_gc_set_clip_region (r.bordergc,    area_region);
	gdk_gc_set_clip_region (r.topleft,     area_region);
	gdk_gc_set_clip_region (r.bottomright, area_region);
	
	gdk_region_destroy (area_region);
	gdk_region_destroy (gap_region);
	
	gdk_draw_rectangle (window, style->bg_gc[state_type], TRUE, x, y, width, height);

	cl_draw_rectangle (window, widget, style, x, y, width, height, &r);

	cl_draw_shadow (window, widget, style, x, y, width, height, &r); 

	gdk_gc_set_clip_region (r.bordergc,    NULL);
	gdk_gc_set_clip_region (r.topleft,     NULL);
	gdk_gc_set_clip_region (r.bottomright, NULL);
	
	/* it's a semi hack */
	gdk_draw_line (window, style->light_gc[state_type],
	               light_rect.x, light_rect.y,
	               light_rect.width, light_rect.height);
		
	gdk_draw_line (window, clearlooks_style->shade_gc[1],
	               dark_rect.x, dark_rect.y,
	               dark_rect.width, dark_rect.height);
}

/**************************************************************************/

static void
draw_extension (DRAW_ARGS, GtkPositionType gap_side)
{
	ClearlooksStyle *clearlooks_style = CLEARLOOKS_STYLE (style);
	int              my_state_type = (state_type == GTK_STATE_ACTIVE) ? 2 : 0;
	CLRectangle      r;

#if DEBUG
		printf("draw_extension: %s %d %d %d %d\n", detail, x, y, width, height);
#endif

	g_return_if_fail (GTK_IS_STYLE (style));
	g_return_if_fail (window != NULL);
	
	sanitize_size (window, &width, &height);
	
	if (DETAIL ("tab"))
	{
		GdkRectangle new_area;
		GdkColor tmp_color;

		cl_rectangle_set_button (&r, style, state_type, FALSE, FALSE,
								CL_CORNER_ROUND, CL_CORNER_ROUND,
								CL_CORNER_ROUND, CL_CORNER_ROUND);
		
		if (state_type == GTK_STATE_ACTIVE)
			shade (&style->bg[state_type], &tmp_color, 1.08);
		else
			shade (&style->bg[state_type], &tmp_color, 1.05);
		
		if (area)
		{
			new_area = *area;
		}
		else
		{
			new_area.x = x;
			new_area.y = y;
			new_area.width = width;
			new_area.height = height;
		}
		
		switch (gap_side)
		{
			case GTK_POS_BOTTOM:
				height+=2;
				new_area.y = y;
				new_area.height = height-2;
				r.gradient_type = CL_GRADIENT_VERTICAL;
				cl_rectangle_set_gradient (&r.fill_gradient, &tmp_color, &style->bg[state_type]);
				cl_rectangle_set_gradient (&r.border_gradient,
	                                       &clearlooks_style->border[CL_BORDER_UPPER+my_state_type],
	                                       &clearlooks_style->border[CL_BORDER_LOWER+my_state_type]);
				break;
			case GTK_POS_TOP:
				y-=2;
				height+=2;
				new_area.y = y+2;
				new_area.height = height;
				r.gradient_type = CL_GRADIENT_VERTICAL;
				cl_rectangle_set_gradient (&r.fill_gradient, &style->bg[state_type], &tmp_color);
				cl_rectangle_set_gradient (&r.border_gradient,
	                                       &clearlooks_style->border[CL_BORDER_LOWER+my_state_type],
	                                       &clearlooks_style->border[CL_BORDER_UPPER+my_state_type]);
				break;
			case GTK_POS_LEFT:
				x-=2;
				width+=2;
				new_area.x = x+2;
				new_area.width = width;
				r.gradient_type = CL_GRADIENT_HORIZONTAL;
				cl_rectangle_set_gradient (&r.fill_gradient, &style->bg[state_type], &tmp_color);
				cl_rectangle_set_gradient (&r.border_gradient,
	                                       &clearlooks_style->border[CL_BORDER_LOWER+my_state_type],
	                                       &clearlooks_style->border[CL_BORDER_UPPER+my_state_type]);
				break;
			case GTK_POS_RIGHT:
				width+=2;
				new_area.x = x;
				new_area.width = width-2;
				r.gradient_type = CL_GRADIENT_HORIZONTAL;
				cl_rectangle_set_gradient (&r.fill_gradient, &tmp_color, &style->bg[state_type]);
				cl_rectangle_set_gradient (&r.border_gradient,
	                                       &clearlooks_style->border[CL_BORDER_UPPER+my_state_type],
	                                       &clearlooks_style->border[CL_BORDER_LOWER+my_state_type]);
				break;
		}
		
		r.topleft = style->light_gc[state_type];		
		r.bottomright = (state_type == GTK_STATE_NORMAL) ? clearlooks_style->shade_gc[1] : NULL;

		cl_rectangle_set_clip_rectangle (&r, &new_area);
		cl_draw_rectangle (window, widget, style, x, y, width, height, &r);
		cl_draw_shadow (window, widget, style, x, y, width, height, &r);
		cl_rectangle_reset_clip_rectangle (&r);
		
		/* draw the selection stripe */
		if (state_type != GTK_STATE_ACTIVE) {
			cl_rectangle_set_gradient (&r.fill_gradient, NULL, NULL);
			r.fillgc = clearlooks_style->spot2_gc;
			
			switch (gap_side)
			{
				case GTK_POS_BOTTOM:
					cl_rectangle_set_corners (&r, CL_CORNER_ROUND, CL_CORNER_ROUND,
					                              CL_CORNER_NONE, CL_CORNER_NONE);
					cl_rectangle_set_gradient (&r.border_gradient, &clearlooks_style->spot3, &clearlooks_style->spot2);
					r.gradient_type = CL_GRADIENT_VERTICAL;

					cl_rectangle_set_clip_rectangle (&r, &new_area);
					cl_draw_rectangle (window, widget, style, x, y, width, 3, &r);
					cl_rectangle_reset_clip_rectangle (&r);
					break;
				case GTK_POS_TOP:
					cl_rectangle_set_corners (&r, CL_CORNER_NONE, CL_CORNER_NONE,
					                              CL_CORNER_ROUND, CL_CORNER_ROUND);
					cl_rectangle_set_gradient (&r.border_gradient, &clearlooks_style->spot2, &clearlooks_style->spot3);
					r.gradient_type = CL_GRADIENT_VERTICAL;

					cl_rectangle_set_clip_rectangle (&r, &new_area);
					cl_draw_rectangle (window, widget, style, x, y + height - 3, width, 3, &r);
					cl_rectangle_reset_clip_rectangle (&r);
					break;
				case GTK_POS_LEFT:
					cl_rectangle_set_corners (&r, CL_CORNER_NONE, CL_CORNER_ROUND,
					                              CL_CORNER_NONE, CL_CORNER_ROUND);
					cl_rectangle_set_gradient (&r.border_gradient, &clearlooks_style->spot2, &clearlooks_style->spot3);
					r.gradient_type = CL_GRADIENT_HORIZONTAL;

					cl_rectangle_set_clip_rectangle (&r, &new_area);
					cl_draw_rectangle (window, widget, style, x + width - 3, y, 3, height, &r);
					cl_rectangle_reset_clip_rectangle (&r);
					break;
				case GTK_POS_RIGHT:
					cl_rectangle_set_corners (&r, CL_CORNER_ROUND, CL_CORNER_NONE,
					                              CL_CORNER_ROUND, CL_CORNER_NONE);
					cl_rectangle_set_gradient (&r.border_gradient, &clearlooks_style->spot3, &clearlooks_style->spot2);
					r.gradient_type = CL_GRADIENT_HORIZONTAL;
				
					cl_rectangle_set_clip_rectangle (&r, &new_area);
					cl_draw_rectangle (window, widget, style, x, y, 3, height, &r);
					cl_rectangle_reset_clip_rectangle (&r);
					break;
			}
		}
		

	}
	else
	{
		parent_class->draw_extension (style, window, state_type, shadow_type, area,
		                              widget, detail, x, y, width, height,
		                              gap_side);
	}
}
    

/**************************************************************************/

static void 
draw_handle (DRAW_ARGS, GtkOrientation orientation)
{
	ClearlooksStyle *clearlooks_style = CLEARLOOKS_STYLE (style);
	gint xx, yy;
	gint xthick, ythick;
	GdkGC *light_gc, *dark_gc;
	GdkRectangle rect;
	GdkRectangle dest;
	gint intersect;
	gint h;
	int i;
	int n_lines;
	int offset;
	
#if DEBUG
		printf("draw_handle: %s %d %d %d %d\n", detail, x, y, width, height);
#endif

	g_return_if_fail (GTK_IS_STYLE (style));
	g_return_if_fail (window != NULL);
	
	sanitize_size (window, &width, &height);
	
	if (state_type == GTK_STATE_PRELIGHT)
		gtk_style_apply_default_background (style, window,
	                                  widget && !GTK_WIDGET_NO_WINDOW (widget),
	                                  state_type, area, x, y, width, height);

	/* orientation is totally bugged, but this actually works... */
	orientation = (width > height) ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;

	if (!strcmp (detail, "paned"))
	{
		/* we want to ignore the shadow border in paned widgets */
		xthick = 0;
		ythick = 0;
	}
	else
	{
		xthick = style->xthickness;
		ythick = style->ythickness;
	}

	if ( ((DETAIL ("handlebox") && widget && GTK_IS_HANDLE_BOX (widget)) || DETAIL ("dockitem")) &&
	     orientation == GTK_ORIENTATION_VERTICAL )
	{
		/* The line in the toolbar */
	
		light_gc = style->light_gc[state_type];
		dark_gc = clearlooks_style->shade_gc[3];
		
		if (area)
		{
			gdk_gc_set_clip_rectangle (light_gc, area);
			gdk_gc_set_clip_rectangle (dark_gc, area);
		}
     
		if (area)
		{
			gdk_gc_set_clip_rectangle (light_gc, NULL);
			gdk_gc_set_clip_rectangle (dark_gc, NULL);
		}

		if (area)
		{
			gdk_gc_set_clip_rectangle (clearlooks_style->shade_gc[0], area);
			gdk_gc_set_clip_rectangle (clearlooks_style->shade_gc[3], area);
		}
      
		gdk_draw_line (window, clearlooks_style->shade_gc[0], x, y, x + width, y);
		gdk_draw_line (window, clearlooks_style->shade_gc[3], x, y + height - 1, x + width, y + height - 1);
			
		if (area)
		{
			gdk_gc_set_clip_rectangle (clearlooks_style->shade_gc[0], NULL);
			gdk_gc_set_clip_rectangle (clearlooks_style->shade_gc[3], NULL);
		}
	}
	
	light_gc = clearlooks_style->shade_gc[0];
	dark_gc = clearlooks_style->shade_gc[4];

	rect.x = x + xthick;
	rect.y = y + ythick;
	rect.width = width - (xthick * 2);
	rect.height = height - (ythick * 2);
	
	if (area)
		intersect = gdk_rectangle_intersect (area, &rect, &dest);
	else
	{
		intersect = TRUE;
		dest = rect;
	}

	if (!intersect)
		return;

	gdk_gc_set_clip_rectangle (light_gc, &dest);
	gdk_gc_set_clip_rectangle (dark_gc, &dest);
	
	n_lines = (!strcmp (detail, "paned")) ? 21 : 11;

	if (orientation == GTK_ORIENTATION_VERTICAL)
	{	
		h = width - 2 * xthick;
		h = MAX (3, h - 6);
		
		xx = x + (width - h) / 2;
		offset = (height - 2*ythick - 2*n_lines)/2 + 1;
		if (offset < 0)
			offset = 0;
			
		for (i = 0, yy = y + ythick + offset; yy <= (y + height - ythick - 1) && i < n_lines; yy += 2, i++)
		{
			gdk_draw_line (window, dark_gc, xx, yy, xx + h, yy);
			gdk_draw_line (window, light_gc, xx, yy + 1, xx + h, yy + 1);
		}
	}
	else
	{
		h = height - 2 * ythick;
		h = MAX (3, h - 6);
		
		yy = y + (height - h) / 2;
		offset = (width - 2*xthick - 2*n_lines)/2 + 1;
		if (offset < 0)
			offset = 0;
		
		for (i = 0, xx = x + xthick + offset;  i < n_lines; xx += 2, i++)
		{
			gdk_draw_line (window, dark_gc, xx, yy, xx, yy + h);
			gdk_draw_line (window, light_gc, xx + 1, yy, xx + 1, yy + h);
		}
	}

	gdk_gc_set_clip_rectangle (light_gc, NULL);
	gdk_gc_set_clip_rectangle (dark_gc, NULL);
}

/**************************************************************************/

static void
draw_box (DRAW_ARGS)
{
	ClearlooksStyle *clearlooks_style = CLEARLOOKS_STYLE (style);
	CLRectangle r;
	gboolean false_size = FALSE;

#ifdef DEBUG
		printf("draw_box: %s %d %d %d %d\n", detail, x, y, width, height);
#endif
	
	g_return_if_fail (style != NULL);
	g_return_if_fail (window != NULL);

	if (width == -1 || height == -1)
		false_size = TRUE;
		
	if ((width == -1) && (height == -1))
		gdk_window_get_size (window, &width, &height);
	else if (width == -1)
		gdk_window_get_size (window, &width, NULL);
	else if (height == -1)
		gdk_window_get_size (window, NULL, &height);

	cl_rectangle_reset (&r, style);

	if (widget == NULL)
		return;
	
	/* listview headers */
	if (widget && DETAIL ("button") && widget->parent &&
	         (GTK_IS_TREE_VIEW(widget->parent) ||
	          GTK_IS_CLIST (widget->parent) ||
			  strcmp(G_OBJECT_TYPE_NAME (widget->parent), "ETree") == 0))
	{
		cl_draw_treeview_header (style, window, state_type, shadow_type,
		                         area, widget, detail, x, y, width, height);
	}
	else if (detail && (!strcmp (detail, "button") ||
		                !strcmp (detail, "buttondefault")))
	{
		if (GTK_IS_COMBO_BOX_ENTRY(widget->parent) || GTK_IS_COMBO(widget->parent))
		{
			cl_draw_combobox_button (style, window, state_type, shadow_type,
			                         area, widget,
		                             detail, x, y, width, height);
		}
		else
		{
			cl_draw_button (style, window, state_type, shadow_type, area, widget,
		                    detail, x, y, width, height);
		}
	}
	else if (detail && (
	         !strcmp (detail, "spinbutton_up") ||
	         !strcmp (detail, "spinbutton_down") ||
	         !strcmp (detail, "spinbutton")))
	{		
		cl_draw_spinbutton (style, window, state_type, shadow_type, area,
		                    widget, detail, x, y, width, height);
	}
	else if (detail && (
		     !strcmp (detail, "hscale") || !strcmp (detail, "vscale")))
	{
		cl_rectangle_set_button (&r, style, state_type,
		                GTK_WIDGET_HAS_DEFAULT  (widget), GTK_WIDGET_HAS_FOCUS (widget),
		                CL_CORNER_ROUND, CL_CORNER_ROUND,
		                CL_CORNER_ROUND, CL_CORNER_ROUND);

		if (!strcmp (detail, "hscale") || !strcmp (detail, "vscale"))
		{
			r.fill_gradient.to = &clearlooks_style->shade[2];
			r.bottomright = clearlooks_style->shade_gc[2];
		}
		
		cl_set_corner_sharpness (detail, widget, &r);

		if (!strcmp (detail, "spinbutton_up"))
		{
			r.border_gradient.to = r.border_gradient.from;
			height++;
			gtk_style_apply_default_background (style, window, FALSE, state_type,
			                                    area, x, y, width, height);
		}
		else if (!strcmp (detail, "spinbutton_down"))
		{
			r.border_gradient.to = r.border_gradient.from;
			gtk_style_apply_default_background (style, window, FALSE, state_type,
			                                    area, x, y, width, height);
		}
	
		cl_rectangle_set_clip_rectangle (&r, area);	
		cl_draw_rectangle (window, widget, style, x+1, y+1, width-2, height-2, &r);
		cl_draw_shadow (window, widget, style, x+1, y+1, width-2, height-2, &r);
		cl_rectangle_reset_clip_rectangle (&r);		
	}
	else if (DETAIL ("trough") && GTK_IS_PROGRESS_BAR (widget))
	{
		GdkPoint points[4] = { {x,y}, {x+width-1,y}, {x,y+height-1}, {x+width-1,y+height-1} };
		
		gdk_draw_points (window, style->bg_gc[state_type], points, 4);
		
		r.bordergc = clearlooks_style->shade_gc[5];
		r.fillgc   = clearlooks_style->shade_gc[2];
			
		cl_rectangle_set_corners (&r, CL_CORNER_NARROW, CL_CORNER_NARROW,
			                          CL_CORNER_NARROW, CL_CORNER_NARROW);
		
		cl_rectangle_set_clip_rectangle (&r, area);
		cl_draw_rectangle (window, widget, style, x, y, width, height, &r);
		cl_rectangle_reset_clip_rectangle (&r);
	}
	else if (DETAIL ("trough") &&
		     (GTK_IS_VSCALE (widget) || GTK_IS_HSCALE (widget)))
	{
		GdkGC   *inner  = clearlooks_style->shade_gc[3],
		        *outer  = clearlooks_style->shade_gc[5],
			    *shadow = clearlooks_style->shade_gc[4];
		GdkColor upper_color = *clearlooks_get_spot_color (CLEARLOOKS_RC_STYLE (style->rc_style)),
				 lower_color;
		
		GtkAdjustment *adjustment = gtk_range_get_adjustment (GTK_RANGE (widget));
		
		GtkOrientation orientation = GTK_RANGE (widget)->orientation;
		
		gint fill_size = (orientation ? height : width) * 
		                 (1 / ((adjustment->upper - adjustment->lower) / 
		                      (adjustment->value - adjustment->lower)));

		if (orientation == GTK_ORIENTATION_HORIZONTAL)
		{
			y += (height - SCALE_SIZE) / 2;
			height = SCALE_SIZE;
		}
		else
		{
			x += (width - SCALE_SIZE) / 2;
			width = SCALE_SIZE;
		}
		
		if (state_type == GTK_STATE_INSENSITIVE)
		{
			outer  = clearlooks_style->shade_gc[4];
			inner  = clearlooks_style->shade_gc[2];
			shadow = clearlooks_style->shade_gc[3];
		}
		
		cl_rectangle_init (&r, inner, outer, CL_CORNER_NONE, CL_CORNER_NONE, 
		                                     CL_CORNER_NONE, CL_CORNER_NONE );
		
		r.topleft = shadow;
		
		cl_rectangle_set_clip_rectangle (&r, area);	
		cl_draw_rectangle (window, widget, style, x, y, width, height, &r);
		cl_draw_shadow (window, widget, style, x, y, width, height, &r);
		cl_rectangle_reset_clip_rectangle (&r);	

		/* DRAW FILL */
		shade (&upper_color, &lower_color, 1.3);
		
		r.bordergc = clearlooks_style->spot3_gc;
		r.fillgc   = style->bg_gc[state_type];
		
		r.gradient_type = (orientation == GTK_ORIENTATION_HORIZONTAL ) ? CL_GRADIENT_VERTICAL
																	   : CL_GRADIENT_HORIZONTAL;
		
		cl_rectangle_set_gradient (&r.fill_gradient, &upper_color, &lower_color);
		
		cl_rectangle_set_clip_rectangle (&r, area);	
		if (orientation == GTK_ORIENTATION_HORIZONTAL && fill_size > 1)
		{
			if (gtk_range_get_inverted(GTK_RANGE(widget)) != (get_direction(widget) == GTK_TEXT_DIR_RTL))
				cl_draw_rectangle (window, widget, style, x+width-fill_size, y, fill_size, height, &r);
			else
				cl_draw_rectangle (window, widget, style, x, y, fill_size, height, &r);				
		}
		else if (fill_size > 1)
		{
			if (gtk_range_get_inverted (GTK_RANGE (widget)))
				cl_draw_rectangle (window, widget, style, x, y+height-fill_size, width, fill_size, &r);
			else
				cl_draw_rectangle (window, widget, style, x, y, width, fill_size, &r);			
		}
		cl_rectangle_reset_clip_rectangle (&r);	
	}
	else if (DETAIL ("trough"))
	{
		GdkGC *inner  = clearlooks_style->shade_gc[3],
		      *outer  = clearlooks_style->shade_gc[5];	
		
		cl_rectangle_init (&r, inner, outer, CL_CORNER_NONE, CL_CORNER_NONE, 
		                                     CL_CORNER_NONE, CL_CORNER_NONE );
		
		if (GTK_RANGE (widget)->orientation == GTK_ORIENTATION_VERTICAL)
		{
			y+=1;
			height-=2;
		}
		else
		{
			x+=1;
			width-=2;
		}
		
		cl_rectangle_set_clip_rectangle (&r, area);
		cl_draw_rectangle (window, widget, style, x, y, width, height, &r);
		cl_rectangle_reset_clip_rectangle (&r);
	}
	else if (detail && (!strcmp (detail, "vscrollbar") ||
	                    !strcmp (detail, "hscrollbar") ||
			    !strcmp (detail, "stepper")))
	{
		ClScrollButtonType button_type = CL_SCROLLBUTTON_OTHER;
		gboolean horizontal = TRUE;

		if (GTK_IS_VSCROLLBAR(widget))
		{
			if (y == widget->allocation.y)
				button_type = CL_SCROLLBUTTON_BEGIN;
			else if (y+height == widget->allocation.y+widget->allocation.height)
				button_type = CL_SCROLLBUTTON_END;

			horizontal = FALSE;
		}
		else if (GTK_IS_HSCROLLBAR(widget))
		{
			if (x == widget->allocation.x)
				button_type = CL_SCROLLBUTTON_BEGIN;
			else if (x+width == widget->allocation.x+widget->allocation.width)
				button_type = CL_SCROLLBUTTON_END;
		}

		cl_rectangle_set_button (&r, style, state_type, FALSE, FALSE, 0,0,0,0);
		
		cl_rectangle_set_gradient (&r.fill_gradient, NULL, NULL);
		cl_rectangle_set_gradient (&r.fill_gradient, &clearlooks_style->inset_light[state_type],
		                           &clearlooks_style->inset_dark[state_type]);
	
		
		r.gradient_type = horizontal ? CL_GRADIENT_VERTICAL
		                             : CL_GRADIENT_HORIZONTAL;
		
		r.bottomright = clearlooks_style->shade_gc[1];
		r.border_gradient.to = r.border_gradient.from;
		
		if (button_type == CL_SCROLLBUTTON_OTHER)
		{
			cl_rectangle_set_corners (&r, CL_CORNER_NONE, CL_CORNER_NONE,
			                              CL_CORNER_NONE, CL_CORNER_NONE);
		}
		else if (button_type == CL_SCROLLBUTTON_BEGIN)
		{
			if (horizontal)
				cl_rectangle_set_corners (&r, CL_CORNER_ROUND, CL_CORNER_NONE,
											  CL_CORNER_ROUND, CL_CORNER_NONE);
			else
				cl_rectangle_set_corners (&r, CL_CORNER_ROUND, CL_CORNER_ROUND,
											  CL_CORNER_NONE,  CL_CORNER_NONE);
		}
		else
		{
			if (horizontal)
				cl_rectangle_set_corners (&r, CL_CORNER_NONE,  CL_CORNER_ROUND,
											  CL_CORNER_NONE,  CL_CORNER_ROUND);
			else
				cl_rectangle_set_corners (&r, CL_CORNER_NONE,  CL_CORNER_NONE,
											  CL_CORNER_ROUND, CL_CORNER_ROUND);
		}
		
		cl_rectangle_set_clip_rectangle (&r, area);	
		cl_draw_rectangle (window, widget, style, x, y, width, height, &r);
		cl_draw_shadow (window, widget, style, x, y, width, height, &r);
		cl_rectangle_reset_clip_rectangle (&r);	

	}
	else if (DETAIL ("slider"))
	{
		if (DETAIL("slider") && widget && GTK_IS_RANGE (widget))
		{
			GtkAdjustment *adj = GTK_RANGE (widget)->adjustment;
			
			if (adj->value <= adj->lower &&
				(GTK_RANGE (widget)->has_stepper_a || GTK_RANGE (widget)->has_stepper_b))
			{
				if (GTK_IS_VSCROLLBAR (widget))
				{
					y-=1;
					height+=1;
				}
				else if (GTK_IS_HSCROLLBAR (widget))
				{
					x-=1;
					width+=1;
				}
			}
			if (adj->value >= adj->upper - adj->page_size &&
			    (GTK_RANGE (widget)->has_stepper_c || GTK_RANGE (widget)->has_stepper_d))
			{
				if (GTK_IS_VSCROLLBAR (widget))
					height+=1;
				else if (GTK_IS_HSCROLLBAR (widget))
					width+=1;
			}
		}
			
		cl_rectangle_set_button (&r, style, state_type, FALSE, GTK_WIDGET_HAS_FOCUS (widget),
		                        CL_CORNER_NONE, CL_CORNER_NONE,
		                        CL_CORNER_NONE, CL_CORNER_NONE);
		
		r.gradient_type = GTK_IS_HSCROLLBAR (widget) ? CL_GRADIENT_VERTICAL
		                                             : CL_GRADIENT_HORIZONTAL;

		cl_rectangle_set_gradient (&r.fill_gradient, &clearlooks_style->inset_light[state_type],
		                           &clearlooks_style->inset_dark[state_type]);

		r.bottomright = clearlooks_style->shade_gc[1];
		r.border_gradient.to = r.border_gradient.from;
		
		cl_rectangle_set_clip_rectangle (&r, area);	
		cl_draw_rectangle (window, widget, style, x, y, width, height, &r);
		cl_draw_shadow (window, widget, style, x, y, width, height, &r);
		cl_rectangle_reset_clip_rectangle (&r);	
	}
	else if (detail && !strcmp (detail, "optionmenu")) /* supporting deprecated */
	{
		cl_draw_optionmenu(style, window, state_type, shadow_type, area, widget, detail, x, y, width, height);
	}
	else if (DETAIL ("menuitem"))
	{
		if (clearlooks_style->menuitemstyle == 0)
		{
			cl_draw_menuitem_flat (window, widget, style, area, state_type,
			                       x, y, width, height, &r);			
		}
		else if (clearlooks_style->menuitemstyle == 1)
		{
			cl_draw_menuitem_gradient (window, widget, style, area, state_type,
			                           x, y, width, height, &r);
		}
		else
		{
			cl_draw_menuitem_button (window, widget, style, area, state_type,
			                         x, y, width, height, &r);
		}
	}
	else if (DETAIL ("menubar") && (clearlooks_style->sunkenmenubar || clearlooks_style->menubarstyle > 0))
	{
		GdkGC *dark = clearlooks_style->shade_gc[2];
		GdkColor upper_color, lower_color;
		
		/* don't draw sunken menubar on gnome panel
		   IT'S A HACK! HORRIBLE HACK! HIDEOUS HACK!
		   BUT IT WORKS FOR ME(tm)! */
		if (widget->parent &&
			strcmp(G_OBJECT_TYPE_NAME (widget->parent), "PanelWidget") == 0)
			return;
		
		shade(&style->bg[state_type], &upper_color, 1.0);
		shade(&style->bg[state_type], &lower_color, 0.95);
	
		cl_rectangle_set_corners (&r, CL_CORNER_NONE, CL_CORNER_NONE,
		                              CL_CORNER_NONE, CL_CORNER_NONE);
		
		r.fillgc = style->bg_gc[state_type];
		r.bordergc = clearlooks_style->shade_gc[2];
		r.gradient_type = CL_GRADIENT_VERTICAL;
		
		cl_rectangle_set_gradient (&r.border_gradient, &clearlooks_style->shade[2],
		                                               &clearlooks_style->shade[3]);
		cl_rectangle_set_gradient (&r.fill_gradient, &upper_color, &lower_color);
		
		/* make vertical and top borders invisible for style 2 */
		if (clearlooks_style->menubarstyle == 2) {
			x--; width+=2;
			y--; height+=1;
		}
		
		cl_rectangle_set_clip_rectangle (&r, area);
		cl_draw_rectangle (window, widget, style, x, y, width, height, &r);
		cl_rectangle_reset_clip_rectangle (&r);
	}
	else if (DETAIL ("menu") && widget->parent &&
	         GDK_IS_WINDOW (widget->parent->window))
	{	
		cl_rectangle_set_corners (&r, CL_CORNER_NONE, CL_CORNER_NONE,
		                              CL_CORNER_NONE, CL_CORNER_NONE);
		
		r.bordergc    = clearlooks_style->border_gc[CL_BORDER_UPPER];
		r.topleft     = style->light_gc[state_type];
		r.bottomright = clearlooks_style->shade_gc[1];
		
		cl_rectangle_set_clip_rectangle (&r, area);
		cl_draw_rectangle (window, widget, style, x, y, width, height, &r);
		cl_draw_shadow (window, widget, style, x, y, width, height, &r);
		cl_rectangle_reset_clip_rectangle (&r);	

		return;
	}
	else if (DETAIL ("bar") && widget && GTK_IS_PROGRESS_BAR (widget))
	{
		GdkColor upper_color = *clearlooks_get_spot_color (CLEARLOOKS_RC_STYLE (style->rc_style)),
		         lower_color,
		         prev_foreground;
		gboolean activity_mode = GTK_PROGRESS (widget)->activity_mode;
		
#ifdef HAVE_ANIMATION
		if (!activity_mode && gtk_progress_bar_get_fraction (widget) != 1.0 &&
			!cl_progressbar_known((gconstpointer)widget))
		{
			cl_progressbar_add ((gpointer)widget);
		}
#endif		
		cl_progressbar_fill (window, widget, style, style->black_gc,
		                     x, y, width, height,
#ifdef HAVE_ANIMATION
		                     activity_mode ? 0 : pboffset,
#else
		                     0,		
#endif
		                     area);
		
		cl_rectangle_set_corners (&r, CL_CORNER_NONE, CL_CORNER_NONE,
		                              CL_CORNER_NONE, CL_CORNER_NONE);
		
		r.bordergc = clearlooks_style->spot3_gc;
		r.topleft = clearlooks_style->spot2_gc;
		
		prev_foreground = cl_gc_set_fg_color_shade (clearlooks_style->spot2_gc,
		                                            style->colormap,
		                                            &clearlooks_style->spot2,
		                                            1.2);

		cl_rectangle_set_clip_rectangle (&r, area);
		cl_draw_rectangle (window, widget, style, x, y, width, height, &r);
		cl_draw_shadow (window, widget, style, x, y, width, height, &r);
		cl_rectangle_reset_clip_rectangle (&r);
		
		gdk_gc_set_foreground (clearlooks_style->spot2_gc, &prev_foreground);
	}

	else if ( widget && (DETAIL ("menubar") || DETAIL ("toolbar") || DETAIL ("dockitem_bin") || DETAIL ("handlebox_bin")) && shadow_type != GTK_SHADOW_NONE) /* Toolbars and menus */
	{
		if (area)
		{
			gdk_gc_set_clip_rectangle (clearlooks_style->shade_gc[0], area);
			gdk_gc_set_clip_rectangle (clearlooks_style->shade_gc[3], area);
		}
		
		gtk_style_apply_default_background (style, window,
				widget && !GTK_WIDGET_NO_WINDOW (widget),
				state_type, area, x, y, width, height);
		
		/* we only want the borders on horizontal toolbars */
		if ( DETAIL ("menubar") || height < 2*width ) { 
			if (!DETAIL ("menubar"))
				gdk_draw_line (window, clearlooks_style->shade_gc[0],
				               x, y, x + width, y); /* top */
							
			gdk_draw_line (window, clearlooks_style->shade_gc[3],
			               x, y + height - 1, x + width, y + height - 1); /* bottom */
		}
		
		if (area)
		{
			gdk_gc_set_clip_rectangle (clearlooks_style->shade_gc[0], NULL);
			gdk_gc_set_clip_rectangle (clearlooks_style->shade_gc[3], NULL);
		}
	}
	else
	{
		parent_class->draw_box (style, window, state_type, shadow_type, area,
		                        widget, detail, x, y, width, height);
	}
}

/**************************************************************************/

static void
ensure_check_pixmaps (GtkStyle     *style,
                      GtkStateType  state,
                      GdkScreen    *screen,
                      gboolean      treeview)
{
	ClearlooksStyle *clearlooks_style = CLEARLOOKS_STYLE (style);
	ClearlooksRcStyle *clearlooks_rc = CLEARLOOKS_RC_STYLE (style->rc_style);
	GdkPixbuf *check, *base, *inconsistent, *composite;
	GdkColor *spot_color = clearlooks_get_spot_color (clearlooks_rc);
	
	if (clearlooks_style->check_pixmap_nonactive[state] != NULL)
		return;
	
	if (state == GTK_STATE_ACTIVE || state == GTK_STATE_SELECTED) {
		check = generate_bit (check_alpha, &style->text[GTK_STATE_NORMAL], 1.0);
		inconsistent = generate_bit (check_inconsistent_alpha, &style->text[GTK_STATE_NORMAL], 1.0);
	} else {
		check = generate_bit (check_alpha, &style->text[state], 1.0);
		inconsistent = generate_bit (check_inconsistent_alpha, &style->text[state], 1.0);
	}
	
	if (state == GTK_STATE_ACTIVE && !treeview)
		base = generate_bit (check_base_alpha, &style->bg[state], 1.0);
	else
		base = generate_bit (check_base_alpha, &style->base[GTK_STATE_NORMAL], 1.0);
	
	if (treeview)
		composite = generate_bit (NULL, &clearlooks_style->shade[6], 1.0);
	else
		composite = generate_bit (NULL, &clearlooks_style->shade[5], 1.0);
	
	gdk_pixbuf_composite (base, composite,
	                      0, 0, RADIO_SIZE, RADIO_SIZE, 0, 0,
	                      1.0, 1.0, GDK_INTERP_NEAREST, 255);

	clearlooks_style->check_pixmap_nonactive[state] =
		pixbuf_to_pixmap (style, composite, screen);
	
	gdk_pixbuf_composite (check, composite,
	                      0, 0, RADIO_SIZE, RADIO_SIZE, 0, 0,
	                      1.0, 1.0, GDK_INTERP_NEAREST, 255);

	clearlooks_style->check_pixmap_active[state] =
		pixbuf_to_pixmap (style, composite, screen);

	g_object_unref (composite);

	composite = generate_bit (NULL, &clearlooks_style->shade[6], 1.0);

	gdk_pixbuf_composite (base, composite,
	                      0, 0, RADIO_SIZE, RADIO_SIZE, 0, 0,
	                      1.0, 1.0, GDK_INTERP_NEAREST, 255);
 
	gdk_pixbuf_composite (inconsistent, composite,
	                      0, 0, RADIO_SIZE, RADIO_SIZE, 0, 0,
	                      1.0, 1.0, GDK_INTERP_NEAREST, 255);

	clearlooks_style->check_pixmap_inconsistent[state] =
		pixbuf_to_pixmap (style, composite, screen);

	g_object_unref (composite);
	g_object_unref (base);
	g_object_unref (check);
	g_object_unref (inconsistent);
}

static void
draw_check (DRAW_ARGS)
{
	ClearlooksStyle *clearlooks_style = CLEARLOOKS_STYLE (style);
	GdkGC *gc = style->base_gc[state_type];
	GdkPixmap *pixmap;
	gboolean treeview;

	if (DETAIL ("check")) /* Menu item */
	{
		parent_class->draw_check (style, window, state_type, shadow_type, area,
					widget, detail, x, y, width, height);
		return;
	}

	treeview = widget && GTK_IS_TREE_VIEW(widget);
	ensure_check_pixmaps (style, state_type, gtk_widget_get_screen (widget), treeview);

	if (area)
		gdk_gc_set_clip_rectangle (gc, area);

	if (shadow_type == GTK_SHADOW_IN)
		pixmap = clearlooks_style->check_pixmap_active[state_type];
	else if (shadow_type == GTK_SHADOW_ETCHED_IN) /* inconsistent */
		pixmap = clearlooks_style->check_pixmap_inconsistent[state_type];
	else
		pixmap = clearlooks_style->check_pixmap_nonactive[state_type];

	x += (width - CHECK_SIZE)/2;
	y += (height - CHECK_SIZE)/2;

	gdk_draw_drawable (window, gc, pixmap, 0, 0, x, y, CHECK_SIZE, CHECK_SIZE);
	
	if (area)
		gdk_gc_set_clip_rectangle (gc, NULL);
}

/**************************************************************************/
static void
draw_slider (DRAW_ARGS, GtkOrientation orientation)
{
	ClearlooksStyle *clearlooks_style = CLEARLOOKS_STYLE (style);
	GdkGC *shade_gc = clearlooks_style->shade_gc[4];
	GdkGC *white_gc = clearlooks_style->shade_gc[0];
	int x1, y1;

#if DEBUG
		printf("draw_slider: %s %d %d %d %d\n", detail, x, y, width, height);
#endif

	g_return_if_fail (GTK_IS_STYLE (style));
	g_return_if_fail (window != NULL);

	sanitize_size (window, &width, &height);

	gtk_paint_box (style, window, state_type, shadow_type,
	               area, widget, detail, x, y, width, height);

	if ((orientation == GTK_ORIENTATION_VERTICAL && height < 20) ||
		(orientation == GTK_ORIENTATION_HORIZONTAL && width < 20))
		return;
	
	if (detail && strcmp ("slider", detail) == 0)
	{
		if (area)
		{
			gdk_gc_set_clip_rectangle (shade_gc, area);
			gdk_gc_set_clip_rectangle (white_gc, area);
		}
		if (orientation == GTK_ORIENTATION_HORIZONTAL)
		{
			x1 = x + width / 2 - 4;
			y1 = y + (height - 6) / 2;
			gdk_draw_line (window, shade_gc, x1, y1, x1, y1 + 6);
			gdk_draw_line (window, white_gc, x1 + 1, y1, x1 + 1, y1 + 6);
			gdk_draw_line (window, shade_gc, x1 + 3, y1, x1 + 3, y1 + 6);
			gdk_draw_line (window, white_gc, x1 + 3 + 1, y1, x1 + 3 + 1, y1 + 6);
			gdk_draw_line (window, shade_gc, x1 + 3*2, y1, x1 + 3*2, y1 + 6);
			gdk_draw_line (window, white_gc, x1 + 3*2 + 1, y1, x1 + 3*2 + 1, y1 + 6);
		}
		else
		{
			x1 = x + (width - 6) / 2;
			y1 = y + height / 2 - 4;
			gdk_draw_line (window, shade_gc, x1 + 6, y1, x1, y1);
			gdk_draw_line (window, white_gc, x1 + 6, y1 + 1, x1, y1 + 1);
			gdk_draw_line (window, shade_gc, x1 + 6, y1 + 3, x1, y1 + 3);
			gdk_draw_line (window, white_gc, x1 + 6, y1 + 3 + 1, x1, y1 + 3 + 1);
			gdk_draw_line (window, shade_gc, x1 + 6, y1 + 3*2, x1, y1 + 3*2);
			gdk_draw_line (window, white_gc, x1 + 6, y1 + 3*2 + 1, x1, y1 + 3*2 + 1);
		}
		if (area)
		{
			gdk_gc_set_clip_rectangle (shade_gc, NULL);
			gdk_gc_set_clip_rectangle (white_gc, NULL);
		}
	}
	else if (detail && (strcmp ("hscale", detail) == 0 || strcmp ("vscale", detail) == 0))
	{
		if (area)
		{
			gdk_gc_set_clip_rectangle (shade_gc, area);
			gdk_gc_set_clip_rectangle (white_gc, area);
		}

		if (orientation == GTK_ORIENTATION_HORIZONTAL)
		{
			x1 = x + width / 2 - 3;
			y1 = y + (height - 7) / 2;
			gdk_draw_line (window, shade_gc, x1 + 0, y1 + 5, x1 + 0, y1 + 1);
			gdk_draw_line (window, white_gc, x1 + 1, y1 + 5, x1 + 1, y1 + 1);
			gdk_draw_line (window, shade_gc, x1 + 3, y1 + 5, x1 + 3, y1 + 1);
			gdk_draw_line (window, white_gc, x1 + 4, y1 + 5, x1 + 4, y1 + 1);
			gdk_draw_line (window, shade_gc, x1 + 6,  y1 + 5, x1 + 6, y1 + 1);
			gdk_draw_line (window, white_gc, x1 + 7,  y1 + 5, x1 + 7, y1 + 1);
		}
		else
		{
			x1 = x + (width - 7) / 2;
			y1 = y + height / 2 - 3;
			gdk_draw_line (window, shade_gc, x1 + 5, y1 + 0, x1 + 1, y1 + 0);
			gdk_draw_line (window, white_gc, x1 + 5, y1 + 1, x1 + 1, y1 + 1);
			gdk_draw_line (window, shade_gc, x1 + 5, y1 + 3, x1 + 1, y1 + 3);
			gdk_draw_line (window, white_gc, x1 + 5, y1 + 4, x1 + 1, y1 + 4);
			gdk_draw_line (window, shade_gc, x1 + 5, y1 + 6, x1 + 1, y1 + 6);
			gdk_draw_line (window, white_gc, x1 + 5, y1 + 7, x1 + 1, y1 + 7);
		}
		if (area)
		{
			gdk_gc_set_clip_rectangle (shade_gc, NULL);
			gdk_gc_set_clip_rectangle (white_gc, NULL);
		}
	}
}

/**************************************************************************/
static void
ensure_radio_pixmaps (GtkStyle     *style,
		      GtkStateType  state,
		      GdkScreen    *screen)
{
	ClearlooksStyle *clearlooks_style = CLEARLOOKS_STYLE (style);
	ClearlooksRcStyle *clearlooks_rc = CLEARLOOKS_RC_STYLE (style->rc_style);
	GdkPixbuf *dot, *circle, *outline, *inconsistent, *composite;
	GdkColor *spot_color = clearlooks_get_spot_color (clearlooks_rc);
	GdkColor *composite_color;
	
	if (clearlooks_style->radio_pixmap_nonactive[state] != NULL)
		return;
	
	if (state == GTK_STATE_ACTIVE || state == GTK_STATE_SELECTED) {
		dot = colorize_bit (dot_intensity, dot_alpha, &style->text[GTK_STATE_NORMAL]);
		inconsistent = generate_bit (inconsistent_alpha, &style->text[GTK_STATE_NORMAL], 1.0);
	} else {
		dot = colorize_bit (dot_intensity, dot_alpha, &style->text[state]);
		inconsistent = generate_bit (inconsistent_alpha, &style->text[state], 1.0);
	}
	
	outline = generate_bit (outline_alpha, &clearlooks_style->shade[5], 1.0);
	
	if (clearlooks_style->radio_pixmap_mask == NULL)
	{
		gdk_pixbuf_render_pixmap_and_mask (outline,
		                                   NULL,
		                                   &clearlooks_style->radio_pixmap_mask,
		                                   1);
	}
	
	if (state == GTK_STATE_ACTIVE)
	{
		composite_color = &style->bg[GTK_STATE_PRELIGHT];
		circle = generate_bit (circle_alpha, &style->bg[state], 1.0);
	}
	else
	{
		composite_color = &style->bg[state];
		circle = generate_bit (circle_alpha, &style->base[GTK_STATE_NORMAL], 1.0);
	}
	
	composite = generate_bit (NULL, composite_color, 1.0);
	
	gdk_pixbuf_composite (outline, composite,
	                      0, 0, RADIO_SIZE, RADIO_SIZE, 0, 0,
	                      1.0, 1.0, GDK_INTERP_NEAREST, 255);
						  
	gdk_pixbuf_composite (circle, composite,
	                      0, 0, RADIO_SIZE, RADIO_SIZE, 0, 0,
	                      1.0, 1.0, GDK_INTERP_NEAREST, 255);
	
	clearlooks_style->radio_pixmap_nonactive[state] =
		pixbuf_to_pixmap (style, composite, screen);
	
	gdk_pixbuf_composite (dot, composite,
	                      0, 0, RADIO_SIZE, RADIO_SIZE, 0, 0,
	                      1.0, 1.0, GDK_INTERP_NEAREST, 255);
	
	clearlooks_style->radio_pixmap_active[state] =
		pixbuf_to_pixmap (style, composite, screen);
	
	g_object_unref (composite);
	
	composite = generate_bit (NULL, composite_color,1.0);
	
	gdk_pixbuf_composite (outline, composite,
	                      0, 0, RADIO_SIZE, RADIO_SIZE, 0, 0,
	                      1.0, 1.0, GDK_INTERP_NEAREST, 255);
	gdk_pixbuf_composite (circle, composite,
	                      0, 0, RADIO_SIZE, RADIO_SIZE, 0, 0,
	                      1.0, 1.0, GDK_INTERP_NEAREST, 255);
	gdk_pixbuf_composite (inconsistent, composite,
	                      0, 0, RADIO_SIZE, RADIO_SIZE, 0, 0,
	                      1.0, 1.0, GDK_INTERP_NEAREST, 255);
	
	clearlooks_style->radio_pixmap_inconsistent[state] =
		pixbuf_to_pixmap (style, composite, screen);
	
	g_object_unref (composite);
	g_object_unref (circle);
	g_object_unref (dot);
	g_object_unref (inconsistent);
	g_object_unref (outline);
}

static void
draw_option (DRAW_ARGS)
{
	ClearlooksStyle *clearlooks_style = CLEARLOOKS_STYLE (style);
	GdkGC *gc = style->base_gc[state_type];
	GdkPixmap *pixmap;
	
	if (DETAIL ("option")) /* Menu item */
	{
		parent_class->draw_option (style, window, state_type, shadow_type,
		                           area, widget, detail, x, y, width, height);
		return;
	}
	
	ensure_radio_pixmaps (style, state_type, gtk_widget_get_screen (widget));
	
	if (area)
		gdk_gc_set_clip_rectangle (gc, area);
	
	if (shadow_type == GTK_SHADOW_IN)
		pixmap = clearlooks_style->radio_pixmap_active[state_type];
	else if (shadow_type == GTK_SHADOW_ETCHED_IN) /* inconsistent */
		pixmap = clearlooks_style->radio_pixmap_inconsistent[state_type];
	else
		pixmap = clearlooks_style->radio_pixmap_nonactive[state_type];
	
	x += (width - RADIO_SIZE)/2;
	y += (height - RADIO_SIZE)/2;
	
	gdk_gc_set_clip_mask (gc, clearlooks_style->radio_pixmap_mask);
	gdk_gc_set_clip_origin (gc, x, y);
	
	gdk_draw_drawable (window, gc, pixmap, 0, 0, x, y,
	                   RADIO_SIZE, RADIO_SIZE);
	
	gdk_gc_set_clip_origin (gc, 0, 0);
	gdk_gc_set_clip_mask (gc, NULL);
	
	if (area)
		gdk_gc_set_clip_rectangle (gc, NULL);
}

/**************************************************************************/

static void 
draw_shadow_gap (DRAW_ARGS,
                 GtkPositionType gap_side,
                 gint            gap_x,
                 gint            gap_width)
{
	/* I need to improve this function. */
	ClearlooksStyle *clearlooks_style = CLEARLOOKS_STYLE (style);
	CLRectangle r;
	GdkRegion *area_region = NULL, 
	          *gap_region  = NULL;

#if DEBUG
		printf("draw_shadow_gap: %s %d %d %d %d\n", detail, x, y, width, height);
#endif

	g_return_if_fail (GTK_IS_STYLE (style));
	g_return_if_fail (window != NULL);
	
	sanitize_size (window, &width, &height);

	cl_rectangle_reset (&r, style);
	cl_rectangle_set_corners (&r, CL_CORNER_NONE, CL_CORNER_NONE,
								  CL_CORNER_NONE, CL_CORNER_NONE);
	
	if (area)
	{
		area_region = gdk_region_rectangle (area);
	
		switch (gap_side)
		{
			case GTK_POS_TOP:
			{
				GdkRectangle rect = { x+gap_x, y, gap_width, 2 };
				gap_region = gdk_region_rectangle (&rect);
				break;
			}
			case GTK_POS_BOTTOM:
			{
				GdkRectangle rect = { x+gap_x, y+height-2, gap_width, 2 };
				gap_region = gdk_region_rectangle (&rect);
				break;
			}
			case GTK_POS_LEFT:
			{
				GdkRectangle rect = { x, y+gap_x, 2, gap_width };
				gap_region = gdk_region_rectangle (&rect);
				break;
			}		
			case GTK_POS_RIGHT:
			{
				GdkRectangle rect = { x+width-2, y+gap_x, 2, gap_width };
				gap_region = gdk_region_rectangle (&rect);
				break;
			}		
		}
		
		gdk_region_subtract (area_region, gap_region);
	}
	
	if (shadow_type == GTK_SHADOW_ETCHED_IN ||
	    shadow_type == GTK_SHADOW_ETCHED_OUT)
	{
		GdkGC *a;
		GdkGC *b;

		if (shadow_type == GTK_SHADOW_ETCHED_IN)
		{
			a = style->light_gc[state_type];
			b = clearlooks_style->shade_gc[3];
		}
		else
		{
			a = clearlooks_style->shade_gc[3];
			b = style->light_gc[state_type];
		}
		
		gdk_gc_set_clip_region (a, area_region);		
		gdk_gc_set_clip_region (b, area_region);		

		r.bordergc = a;
		cl_draw_rectangle (window, widget, style, x+1, y+1, width-1, height-1, &r);

		r.bordergc = b;
		cl_draw_rectangle (window, widget, style, x, y, width-1, height-1, &r);

		gdk_gc_set_clip_region (a, NULL);	
		gdk_gc_set_clip_region (b, NULL);		
	}
	else if (shadow_type == GTK_SHADOW_IN || shadow_type == GTK_SHADOW_OUT)
	{
		r.topleft     = (shadow_type == GTK_SHADOW_OUT) ? style->light_gc[state_type] : clearlooks_style->shade_gc[1];
		r.bottomright = (shadow_type == GTK_SHADOW_OUT) ? clearlooks_style->shade_gc[1] : style->light_gc[state_type];
		r.bordergc    = clearlooks_style->shade_gc[5];
		
		gdk_gc_set_clip_region (r.bordergc,    area_region);		
		gdk_gc_set_clip_region (r.topleft,     area_region);		
		gdk_gc_set_clip_region (r.bottomright, area_region);		
				
		cl_draw_rectangle (window, widget, style, x, y, width, height, &r);
		
		cl_draw_shadow (window, widget, style, x, y, width, height, &r);

		gdk_gc_set_clip_region (r.bordergc,    NULL);
		gdk_gc_set_clip_region (r.topleft,     NULL);
		gdk_gc_set_clip_region (r.bottomright, NULL);
	}

	if (area_region)
		gdk_region_destroy (area_region);
}

/**************************************************************************/
static void
draw_hline (GtkStyle     *style,
            GdkWindow    *window,
            GtkStateType  state_type,
            GdkRectangle  *area,
            GtkWidget     *widget,
            const gchar   *detail,
            gint          x1,
            gint          x2,
            gint          y)
{
	ClearlooksStyle *clearlooks_style = CLEARLOOKS_STYLE (style);

#if DEBUG
		printf("draw_hline\n");
#endif

	g_return_if_fail (GTK_IS_STYLE (style));
	g_return_if_fail (window != NULL);
	
	if (area)
		gdk_gc_set_clip_rectangle (clearlooks_style->shade_gc[2], area);
	
	if (detail && !strcmp (detail, "label"))
	{
		if (state_type == GTK_STATE_INSENSITIVE)
			gdk_draw_line (window, style->light_gc[state_type], x1 + 1, y + 1, x2 + 1, y + 1);
			
		gdk_draw_line (window, style->fg_gc[state_type], x1, y, x2, y);
	}
	else
	{
		gdk_draw_line (window, clearlooks_style->shade_gc[2], x1, y, x2, y);
		
		/* if (DETAIL ("menuitem")) */
		gdk_draw_line (window, clearlooks_style->shade_gc[0], x1, y+1, x2, y+1);
	}
	
	if (area)
		gdk_gc_set_clip_rectangle (clearlooks_style->shade_gc[2], NULL);
}

/**************************************************************************/
static void
draw_vline (GtkStyle     *style,
            GdkWindow    *window,
            GtkStateType  state_type,
            GdkRectangle  *area,
            GtkWidget     *widget,
            const gchar   *detail,
            gint          y1,
            gint          y2,
            gint          x)
{
	ClearlooksStyle *clearlooks_style = CLEARLOOKS_STYLE (style);
	gint thickness_light;
	gint thickness_dark;

#if DEBUG
		printf("draw_vline\n");
#endif

	g_return_if_fail (GTK_IS_STYLE (style));
	g_return_if_fail (window != NULL);
	
	thickness_light = style->xthickness / 2;
	thickness_dark = style->xthickness - thickness_light;
	
	if (area)
		gdk_gc_set_clip_rectangle (clearlooks_style->shade_gc[2], area);
	
	gdk_draw_line (window, clearlooks_style->shade_gc[2], x, y1, x, y2 - 1);
	gdk_draw_line (window, clearlooks_style->shade_gc[0], x+1, y1, x+1, y2 - 1);
	
	if (area)
		gdk_gc_set_clip_rectangle (clearlooks_style->shade_gc[2], NULL);
}

/**************************************************************************/
static void
draw_focus (GtkStyle      *style,
	    GdkWindow     *window,
	    GtkStateType   state_type,
	    GdkRectangle  *area,
	    GtkWidget     *widget,
	    const gchar   *detail,
	    gint           x,
	    gint           y,
	    gint           width,
	    gint           height)
{
	ClearlooksStyle *clearlooks_style = CLEARLOOKS_STYLE (style);
	GdkPoint points[5];
	GdkGC    *gc;
	gboolean free_dash_list = FALSE;
	gint line_width = 1;
	gchar *dash_list = "\1\1";
	gint dash_len;

#if DEBUG
		printf("draw_focus: %s %d %d %d %d\n", detail, x, y, width, height);
#endif
	
	gc = clearlooks_style->shade_gc[6];
	
	if (widget)
	{
		gtk_widget_style_get (widget,
		                      "focus-line-width", &line_width,
		                      "focus-line-pattern", (gchar *)&dash_list,
		                      NULL);
	
		free_dash_list = TRUE;
	}
	
	sanitize_size (window, &width, &height);
	
	if (area)
		gdk_gc_set_clip_rectangle (gc, area);
	
	gdk_gc_set_line_attributes (gc, line_width,
	                            dash_list[0] ? GDK_LINE_ON_OFF_DASH : GDK_LINE_SOLID,
	                            GDK_CAP_BUTT, GDK_JOIN_MITER);
	
	
	if (detail && !strcmp (detail, "add-mode"))
	{
		if (free_dash_list)
			g_free (dash_list);
		
		dash_list = "\4\4";
		free_dash_list = FALSE;
	}
	
	points[0].x = x + line_width / 2;
	points[0].y = y + line_width / 2;
	points[1].x = x + width - line_width + line_width / 2;
	points[1].y = y + line_width / 2;
	points[2].x = x + width - line_width + line_width / 2;
	points[2].y = y + height - line_width + line_width / 2;
	points[3].x = x + line_width / 2;
	points[3].y = y + height - line_width + line_width / 2;
	points[4] = points[0];
	
	if (!dash_list[0])
	{
		gdk_draw_lines (window, gc, points, 5);
	}
	else
	{
		dash_len = strlen (dash_list);
		
		if (dash_list[0])
			gdk_gc_set_dashes (gc, 0, dash_list, dash_len);
		
		gdk_draw_lines (window, gc, points, 3);
		
		points[2].x += 1;
		
		if (dash_list[0])
		{
			gint dash_pixels = 0;
			gint i;
		
			/* Adjust the dash offset for the bottom and left so we
			* match up at the upper left.
			*/
			for (i = 0; i < dash_len; i++)
				dash_pixels += dash_list[i];
			
			if (dash_len % 2 == 1)
				dash_pixels *= 2;
		
			gdk_gc_set_dashes (gc,
							dash_pixels - (width + height - 2 * line_width) % dash_pixels,
							dash_list, dash_len);
		}
		
		gdk_draw_lines (window, gc, points + 2, 3);
	}
	
	gdk_gc_set_line_attributes (gc, 0, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_MITER);
	
	if (area)
		gdk_gc_set_clip_rectangle (gc, NULL);
	
	if (free_dash_list)
		g_free (dash_list);
}

static void
draw_layout(GtkStyle * style,
            GdkWindow * window,
            GtkStateType state_type,
            gboolean use_text,
            GdkRectangle * area,
            GtkWidget * widget,
            const gchar * detail, gint x, gint y, PangoLayout * layout)
{
	ClearlooksStyle *clearlooks_style = CLEARLOOKS_STYLE (style);
	
	g_return_if_fail(GTK_IS_STYLE (style));
	g_return_if_fail(window != NULL);

	parent_class->draw_layout(style, window, state_type, use_text,
	                          area, widget, detail, x, y, layout);

	
}

/**************************************************************************/
static void
draw_resize_grip (GtkStyle       *style,
		  GdkWindow      *window,
		  GtkStateType    state_type,
		  GdkRectangle   *area,
		  GtkWidget      *widget,
		  const gchar    *detail,
		  GdkWindowEdge   edge,
		  gint            x,
		  gint            y,
		  gint            width,
		  gint            height)
{
  ClearlooksStyle *clearlooks_style = CLEARLOOKS_STYLE (style);
  g_return_if_fail (GTK_IS_STYLE (style));
  g_return_if_fail (window != NULL);
  
  if (area)
    {
      gdk_gc_set_clip_rectangle (style->light_gc[state_type], area);
      gdk_gc_set_clip_rectangle (style->dark_gc[state_type], area);
      gdk_gc_set_clip_rectangle (style->bg_gc[state_type], area);
    }

  switch (edge)
    {
    case GDK_WINDOW_EDGE_NORTH_WEST:
      /* make it square */
      if (width < height)
	{
	  height = width;
	}
      else if (height < width)
	{
	  width = height;
	}
      break;
    case GDK_WINDOW_EDGE_NORTH:
      if (width < height)
	{
	  height = width;
	}
      break;
    case GDK_WINDOW_EDGE_NORTH_EAST:
      /* make it square, aligning to top right */
      if (width < height)
	{
	  height = width;
	}
      else if (height < width)
	{
	  x += (width - height);
	  width = height;
	}
      break;
    case GDK_WINDOW_EDGE_WEST:
      if (height < width)
	{
	   width = height;
	}
      break;
    case GDK_WINDOW_EDGE_EAST:
      /* aligning to right */
      if (height < width)
	{
	  x += (width - height);
	  width = height;
	}
      break;
    case GDK_WINDOW_EDGE_SOUTH_WEST:
      /* make it square, aligning to bottom left */
      if (width < height)
	{
	  y += (height - width);
	  height = width;
	}
      else if (height < width)
	{
	  width = height;
	}
      break;
    case GDK_WINDOW_EDGE_SOUTH:
      /* align to bottom */
      if (width < height)
	{
	  y += (height - width);
	  height = width;
	}
      break;
    case GDK_WINDOW_EDGE_SOUTH_EAST:
      /* make it square, aligning to bottom right */
      if (width < height)
	{
	  y += (height - width);
	  height = width;
	}
      else if (height < width)
	{
	  x += (width - height);
	  width = height;
	}
      break;
    default:
      g_assert_not_reached ();
    }

  /* Clear background */
  gtk_style_apply_default_background (style, window, FALSE,
				      state_type, area,
				      x, y, width, height);   

  switch (edge)
    {
    case GDK_WINDOW_EDGE_WEST:
    case GDK_WINDOW_EDGE_EAST:
      {
	gint xi;

	xi = x;

	while (xi < x + width)
	  {
	    gdk_draw_line (window,
			   style->light_gc[state_type],
			   xi, y,
			   xi, y + height);

	    xi++;
	    gdk_draw_line (window,
			   clearlooks_style->shade_gc[4],
			   xi, y,
			   xi, y + height);

	    xi += 2;
	  }
      }
      break;
    case GDK_WINDOW_EDGE_NORTH:
    case GDK_WINDOW_EDGE_SOUTH:
      {
	gint yi;

	yi = y;

	while (yi < y + height)
	  {
	    gdk_draw_line (window,
			   style->light_gc[state_type],
			   x, yi,
			   x + width, yi);

	    yi++;
	    gdk_draw_line (window,
			   clearlooks_style->shade_gc[4],
			   x, yi,
			   x + width, yi);

	    yi+= 2;
	  }
      }
      break;
    case GDK_WINDOW_EDGE_NORTH_WEST:
      {
	gint xi, yi;

	xi = x + width;
	yi = y + height;

	while (xi > x + 3)
	  {
	    gdk_draw_line (window,
			   clearlooks_style->shade_gc[4],
			   xi, y,
			   x, yi);

	    --xi;
	    --yi;

	    gdk_draw_line (window,
			   style->light_gc[state_type],
			   xi, y,
			   x, yi);

	    xi -= 3;
	    yi -= 3;
	    
	  }
      }
      break;
    case GDK_WINDOW_EDGE_NORTH_EAST:
      {
        gint xi, yi;

        xi = x;
        yi = y + height;

        while (xi < (x + width - 3))
          {
            gdk_draw_line (window,
                           style->light_gc[state_type],
                           xi, y,
                           x + width, yi);                           

            ++xi;
            --yi;
            
            gdk_draw_line (window,
                           clearlooks_style->shade_gc[4],
                           xi, y,
                           x + width, yi);

            xi += 3;
            yi -= 3;
          }
      }
      break;
    case GDK_WINDOW_EDGE_SOUTH_WEST:
      {
	gint xi, yi;

	xi = x + width;
	yi = y;

	while (xi > x + 3)
	  {
	    gdk_draw_line (window,
			   clearlooks_style->shade_gc[4],
			   x, yi,
			   xi, y + height);

	    --xi;
	    ++yi;

	    gdk_draw_line (window,
			   style->light_gc[state_type],
			   x, yi,
			   xi, y + height);

	    xi -= 3;
	    yi += 3;
	    
	  }
      }
      break;
      
    case GDK_WINDOW_EDGE_SOUTH_EAST:
      {
        gint xi, yi;

        xi = x;
        yi = y;

        while (xi < (x + width - 3))
          {
            gdk_draw_line (window,
                           style->light_gc[state_type],
                           xi, y + height,
                           x + width, yi);                           

            ++xi;
            ++yi;
            
            gdk_draw_line (window,
                           clearlooks_style->shade_gc[4],
                           xi, y + height,
                           x + width, yi);                           
            
            xi += 3;
            yi += 3;
          }
      }
      break;
    default:
      g_assert_not_reached ();
      break;
    }
  
  if (area)
    {
      gdk_gc_set_clip_rectangle (style->light_gc[state_type], NULL);
      gdk_gc_set_clip_rectangle (style->dark_gc[state_type], NULL);
      gdk_gc_set_clip_rectangle (style->bg_gc[state_type], NULL);
    }
}

/**************************************************************************/

static void
clearlooks_style_init_from_rc (GtkStyle * style,
			       GtkRcStyle * rc_style)
{
	ClearlooksStyle *clearlooks_style = CLEARLOOKS_STYLE (style);
	GdkColor *spot_color;
	double shades[] = {1.065, 0.93, 0.896, 0.85, 0.768, 0.665, 0.4, 0.205};
	int i;
	double contrast;
	
	parent_class->init_from_rc (style, rc_style);
	
	contrast = CLEARLOOKS_RC_STYLE (rc_style)->contrast;
	
	clearlooks_style->sunkenmenubar = CLEARLOOKS_RC_STYLE (rc_style)->sunkenmenubar;
	clearlooks_style->progressbarstyle = CLEARLOOKS_RC_STYLE (rc_style)->progressbarstyle;
	clearlooks_style->menubarstyle = CLEARLOOKS_RC_STYLE (rc_style)->menubarstyle;
	clearlooks_style->menuitemstyle = CLEARLOOKS_RC_STYLE (rc_style)->menuitemstyle;
	clearlooks_style->listviewitemstyle = CLEARLOOKS_RC_STYLE (rc_style)->listviewitemstyle;
	
	/* Lighter to darker */
	for (i = 0; i < 8; i++)
	{
		shade (&style->bg[GTK_STATE_NORMAL], &clearlooks_style->shade[i],
		       (shades[i]-0.7) * contrast + 0.7);
	}
		
	spot_color = clearlooks_get_spot_color (CLEARLOOKS_RC_STYLE (rc_style));
	
	clearlooks_style->spot_color = *spot_color;
	shade (&clearlooks_style->spot_color, &clearlooks_style->spot1, 1.42);
	shade (&clearlooks_style->spot_color, &clearlooks_style->spot2, 1.05);
	shade (&clearlooks_style->spot_color, &clearlooks_style->spot3, 0.65);

	shade (&style->bg[GTK_STATE_NORMAL], &clearlooks_style->border[CL_BORDER_UPPER],        0.5);
	shade (&style->bg[GTK_STATE_NORMAL], &clearlooks_style->border[CL_BORDER_LOWER],        0.62);
	shade (&style->bg[GTK_STATE_ACTIVE], &clearlooks_style->border[CL_BORDER_UPPER_ACTIVE], 0.5);
	shade (&style->bg[GTK_STATE_ACTIVE], &clearlooks_style->border[CL_BORDER_LOWER_ACTIVE], 0.55);
}

static GdkGC *
realize_color (GtkStyle * style,
	       GdkColor * color)
{
	GdkGCValues gc_values;
	
	gdk_colormap_alloc_color (style->colormap, color, FALSE, TRUE);
	
	gc_values.foreground = *color;
	
	return gtk_gc_get (style->depth, style->colormap, &gc_values, GDK_GC_FOREGROUND);
}

static void
clearlooks_style_realize (GtkStyle * style)
{
	ClearlooksStyle *clearlooks_style = CLEARLOOKS_STYLE (style);
	int i;
	
	parent_class->realize (style);
	
	for (i = 0; i < 8; i++)
		clearlooks_style->shade_gc[i] = realize_color (style, &clearlooks_style->shade[i]);

	for (i=0; i < CL_BORDER_COUNT; i++)
		clearlooks_style->border_gc[i] = realize_color (style, &clearlooks_style->border[i]);
		
	clearlooks_style->spot1_gc = realize_color (style, &clearlooks_style->spot1);
	clearlooks_style->spot2_gc = realize_color (style, &clearlooks_style->spot2);
	clearlooks_style->spot3_gc = realize_color (style, &clearlooks_style->spot3);

	/* set light inset color */
	for (i=0; i<5; i++)
	{
		shade (&style->bg[i], &clearlooks_style->inset_dark[i], 0.93);
		gdk_rgb_find_color (style->colormap, &clearlooks_style->inset_dark[i]);

		shade (&style->bg[i], &clearlooks_style->inset_light[i], 1.055);
		gdk_rgb_find_color (style->colormap, &clearlooks_style->inset_light[i]);

		shade (&style->bg[i], &clearlooks_style->listview_bg[i], 1.015);
		gdk_rgb_find_color (style->colormap, &clearlooks_style->listview_bg[i]);

		/* CREATE GRADIENT FOR BUTTONS */		
		shade (&style->bg[i], &clearlooks_style->button_g1[i], 1.055);
		gdk_rgb_find_color (style->colormap, &clearlooks_style->button_g1[i]);
		
		shade (&style->bg[i], &clearlooks_style->button_g2[i], 1.005);
		gdk_rgb_find_color (style->colormap, &clearlooks_style->button_g2[i]);

		shade (&style->bg[i], &clearlooks_style->button_g3[i], 0.98);
		gdk_rgb_find_color (style->colormap, &clearlooks_style->button_g3[i]);

		shade (&style->bg[i], &clearlooks_style->button_g4[i], 0.91);
		gdk_rgb_find_color (style->colormap, &clearlooks_style->button_g4[i]);
	}

}

static void
clearlooks_style_unrealize (GtkStyle * style)
{
	ClearlooksStyle *clearlooks_style = CLEARLOOKS_STYLE (style);
	int i;
	
	/* We don't free the colors, because we don't know if
	* gtk_gc_release() actually freed the GC. FIXME - need
	* a way of ref'ing colors explicitely so GtkGC can
	* handle things properly.
	*/
	for (i=0; i < 8; i++)
		gtk_gc_release (clearlooks_style->shade_gc[i]);
	
	gtk_gc_release (clearlooks_style->spot1_gc);
	gtk_gc_release (clearlooks_style->spot2_gc);
	gtk_gc_release (clearlooks_style->spot3_gc);
	
	for (i = 0; i < 5; i++)
	{
		if (clearlooks_style->radio_pixmap_nonactive[i] != NULL)
		{
			g_object_unref (clearlooks_style->radio_pixmap_nonactive[i]);
			clearlooks_style->radio_pixmap_nonactive[i] = NULL;
			g_object_unref (clearlooks_style->radio_pixmap_active[i]);
			clearlooks_style->radio_pixmap_active[i] = NULL;
			g_object_unref (clearlooks_style->radio_pixmap_inconsistent[i]);
			clearlooks_style->radio_pixmap_inconsistent[i] = NULL;
		}
		
		if (clearlooks_style->check_pixmap_nonactive[i] != NULL)
		{
			g_object_unref (clearlooks_style->check_pixmap_nonactive[i]);
			clearlooks_style->check_pixmap_nonactive[i] = NULL;
			g_object_unref (clearlooks_style->check_pixmap_active[i]);
			clearlooks_style->check_pixmap_active[i] = NULL;
			g_object_unref (clearlooks_style->check_pixmap_inconsistent[i]);
			clearlooks_style->check_pixmap_inconsistent[i] = NULL;
		}
	}
	
	if (clearlooks_style->radio_pixmap_mask != NULL)
		g_object_unref (clearlooks_style->radio_pixmap_mask);
		
	clearlooks_style->radio_pixmap_mask = NULL;

	while (progressbars = g_list_first (progressbars))
		cl_progressbar_remove (progressbars->data);
	
	if (timer_id != 0)
	{
		g_source_remove(timer_id);
		timer_id = 0;		
	}
	
	parent_class->unrealize (style);
}

static GdkPixbuf *
set_transparency (const GdkPixbuf *pixbuf, gdouble alpha_percent)
{
	GdkPixbuf *target;
	guchar *data, *current;
	guint x, y, rowstride, height, width;

	g_return_val_if_fail (pixbuf != NULL, NULL);
	g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);

	/* Returns a copy of pixbuf with it's non-completely-transparent pixels to
	   have an alpha level "alpha_percent" of their original value. */

	target = gdk_pixbuf_add_alpha (pixbuf, FALSE, 0, 0, 0);

	if (alpha_percent == 1.0)
		return target;
	width = gdk_pixbuf_get_width (target);
	height = gdk_pixbuf_get_height (target);
	rowstride = gdk_pixbuf_get_rowstride (target);
	data = gdk_pixbuf_get_pixels (target);

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			/* The "4" is the number of chars per pixel, in this case, RGBA,
			   the 3 means "skip to the alpha" */
			current = data + (y * rowstride) + (x * 4) + 3; 
			*(current) = (guchar) (*(current) * alpha_percent);
		}
	}

	return target;
}

static GdkPixbuf*
scale_or_ref (GdkPixbuf *src,
              int width,
              int height)
{
	if (width == gdk_pixbuf_get_width (src) &&
	    height == gdk_pixbuf_get_height (src)) {
		return g_object_ref (src);
	} else {
		return gdk_pixbuf_scale_simple (src,
						width, height,
						GDK_INTERP_BILINEAR);
	}
}

static GdkPixbuf *
render_icon (GtkStyle            *style,
	     const GtkIconSource *source,
	     GtkTextDirection     direction,
	     GtkStateType         state,
	     GtkIconSize          size,
	     GtkWidget           *widget,
	     const char          *detail)
{
	int width = 1;
	int height = 1;
	GdkPixbuf *scaled;
	GdkPixbuf *stated;
	GdkPixbuf *base_pixbuf;
	GdkScreen *screen;
	GtkSettings *settings;
	
	/* Oddly, style can be NULL in this function, because
	 * GtkIconSet can be used without a style and if so
	 * it uses this function.
	 */
	
	base_pixbuf = gtk_icon_source_get_pixbuf (source);
	
	g_return_val_if_fail (base_pixbuf != NULL, NULL);
	
	if (widget && gtk_widget_has_screen (widget)) {
		screen = gtk_widget_get_screen (widget);
		settings = gtk_settings_get_for_screen (screen);
	} else if (style->colormap) {
		screen = gdk_colormap_get_screen (style->colormap);
		settings = gtk_settings_get_for_screen (screen);
	} else {
		settings = gtk_settings_get_default ();
		GTK_NOTE (MULTIHEAD,
			  g_warning ("Using the default screen for gtk_default_render_icon()"));
	}
	
  
	if (size != (GtkIconSize) -1 && !gtk_icon_size_lookup_for_settings (settings, size, &width, &height)) {
		g_warning (G_STRLOC ": invalid icon size '%d'", size);
		return NULL;
	}

	/* If the size was wildcarded, and we're allowed to scale, then scale; otherwise,
	 * leave it alone.
	 */
	if (size != (GtkIconSize)-1 && gtk_icon_source_get_size_wildcarded (source))
		scaled = scale_or_ref (base_pixbuf, width, height);
	else
		scaled = g_object_ref (base_pixbuf);
	
	/* If the state was wildcarded, then generate a state. */
	if (gtk_icon_source_get_state_wildcarded (source)) {
		if (state == GTK_STATE_INSENSITIVE) {
			stated = set_transparency (scaled, 0.3);
#if 0
			stated =
				gdk_pixbuf_composite_color_simple (scaled,
								   gdk_pixbuf_get_width (scaled),
								   gdk_pixbuf_get_height (scaled),
								   GDK_INTERP_BILINEAR, 128,
								   gdk_pixbuf_get_width (scaled),
								   style->bg[state].pixel,
								   style->bg[state].pixel);
#endif
			gdk_pixbuf_saturate_and_pixelate (stated, stated,
							  0.1, FALSE);
			
			g_object_unref (scaled);
		} else if (state == GTK_STATE_PRELIGHT) {
			stated = gdk_pixbuf_copy (scaled);      
			
			gdk_pixbuf_saturate_and_pixelate (scaled, stated,
							  1.2, FALSE);
			
			g_object_unref (scaled);
		} else {
			stated = scaled;
		}
	}
	else
		stated = scaled;
  
  return stated;
}

static void
clearlooks_style_init (ClearlooksStyle * style)
{
}

static void
clearlooks_style_class_init (ClearlooksStyleClass * klass)
{
	GtkStyleClass *style_class = GTK_STYLE_CLASS (klass);
	
	parent_class = g_type_class_peek_parent (klass);

	style_class->realize = clearlooks_style_realize;
	style_class->unrealize = clearlooks_style_unrealize;
	style_class->init_from_rc = clearlooks_style_init_from_rc;
	style_class->draw_focus = draw_focus;
	style_class->draw_resize_grip = draw_resize_grip;
	style_class->draw_handle = draw_handle;
	style_class->draw_vline = draw_vline;
	style_class->draw_hline = draw_hline;
	style_class->draw_slider = draw_slider;
	style_class->draw_shadow_gap = draw_shadow_gap;
	style_class->draw_arrow = clearlooks_draw_arrow;
	style_class->draw_check = draw_check;
	style_class->draw_tab = draw_tab;
	style_class->draw_box = draw_box;
	style_class->draw_shadow = draw_shadow;
	style_class->draw_box_gap = draw_box_gap;
	style_class->draw_extension = draw_extension;
	style_class->draw_option = draw_option;
	style_class->draw_layout = draw_layout;	
	style_class->render_icon = render_icon;
	style_class->draw_flat_box = draw_flat_box;
}

GType clearlooks_type_style = 0;

void
clearlooks_style_register_type (GTypeModule * module)
{
	static const GTypeInfo object_info =
	{
		sizeof (ClearlooksStyleClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) clearlooks_style_class_init,
		NULL,         /* class_finalize */
		NULL,         /* class_data */
		sizeof (ClearlooksStyle),
		0,            /* n_preallocs */
		(GInstanceInitFunc) clearlooks_style_init,
		NULL
	};

	clearlooks_type_style = g_type_module_register_type (module,
	                                                     GTK_TYPE_STYLE,
	                                                     "ClearlooksStyle",
	                                                     &object_info, 0);
}
