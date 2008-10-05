/* Clearlooks theme engine
 * Copyright (C) 2005 Richard Stellingwerff
 * Copyright (C) 2007 Benjamin Berg
 * Copyright (C) 2007 Andrea Cimitan
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
 *
 */

#include <gtk/gtk.h>
#include <cairo.h>
#include <math.h>
#include <string.h>

#include <ge-support.h>
#include "clearlooks_style.h"
#include "clearlooks_rc_style.h"
#include "clearlooks_draw.h"
#include "support.h"

/* #define DEBUG 1 */

#define DETAIL(xx)   ((detail) && (!strcmp(xx, detail)))
#define CHECK_HINT(xx) (ge_check_hint ((xx), CLEARLOOKS_RC_STYLE ((style)->rc_style)->hint, widget))

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

#ifdef HAVE_ANIMATION
#include "animation.h"
#endif

#define STYLE_FUNCTION(function) (CLEARLOOKS_STYLE_GET_CLASS (style)->style_functions[CLEARLOOKS_STYLE (style)->style].function)

G_DEFINE_DYNAMIC_TYPE (ClearlooksStyle, clearlooks_style, GTK_TYPE_STYLE)

static void
clearlooks_set_widget_parameters (const GtkWidget      *widget,
                                  const GtkStyle       *style,
                                  GtkStateType          state_type,
                                  WidgetParameters     *params)
{
	params->style_functions = &(CLEARLOOKS_STYLE_GET_CLASS (style)->style_functions[CLEARLOOKS_STYLE (style)->style]);
	params->style_constants = &(CLEARLOOKS_STYLE_GET_CLASS (style)->style_constants[CLEARLOOKS_STYLE (style)->style]);

	params->active        = (state_type == GTK_STATE_ACTIVE);
	params->prelight      = (state_type == GTK_STATE_PRELIGHT);
	params->disabled      = (state_type == GTK_STATE_INSENSITIVE);
	params->state_type    = (ClearlooksStateType)state_type;
	params->corners       = CR_CORNER_ALL;
	params->ltr           = ge_widget_is_ltr ((GtkWidget*)widget);
	params->focus         = widget && GTK_WIDGET_HAS_FOCUS (widget);
	params->is_default    = widget && GE_WIDGET_HAS_DEFAULT (widget);
	params->enable_shadow = FALSE;
	params->radius        = CLEARLOOKS_STYLE (style)->radius;

	params->xthickness    = style->xthickness;
	params->ythickness    = style->ythickness;

	/* This is used in GtkEntry to fake transparency. The reason to do this
	 * is that the entry has it's entire background filled with base[STATE].
	 * This is not a very good solution as it will eg. fail if one changes
	 * the background color of a notebook. */
	params->parentbg = CLEARLOOKS_STYLE (style)->colors.bg[state_type];
	clearlooks_get_parent_bg (widget, &params->parentbg);
}

static void
clearlooks_style_draw_flat_box (DRAW_ARGS)
{
	if (detail &&
	    state_type == GTK_STATE_SELECTED && (
	    !strncmp ("cell_even", detail, 9) ||
	    !strncmp ("cell_odd", detail, 8)))
	{
		WidgetParameters params;
		ClearlooksStyle  *clearlooks_style;
		ClearlooksColors *colors;
		cairo_t          *cr;

		CHECK_ARGS
		SANITIZE_SIZE

		clearlooks_style = CLEARLOOKS_STYLE (style);
		clearlooks_set_widget_parameters (widget, style, state_type, &params);
		colors = &clearlooks_style->colors;
		cr = ge_gdk_drawable_to_cairo (window, area);

		/* XXX: We could expose the side details by setting params->corners accordingly
		 *      or adding another option. */
		STYLE_FUNCTION (draw_selected_cell) (cr, colors, &params, x, y, width, height);

		cairo_destroy (cr);
	}
	else if (DETAIL ("tooltip"))
	{
		WidgetParameters params;
		ClearlooksStyle  *clearlooks_style;
		ClearlooksColors *colors;
		cairo_t          *cr;

		CHECK_ARGS
		SANITIZE_SIZE

		clearlooks_style = CLEARLOOKS_STYLE (style);
		clearlooks_set_widget_parameters (widget, style, state_type, &params);
		colors = &clearlooks_style->colors;
		cr = ge_gdk_drawable_to_cairo (window, area);

		STYLE_FUNCTION (draw_tooltip) (cr, colors, &params, x, y, width, height);

		cairo_destroy (cr);
	}
	else if ((CLEARLOOKS_STYLE (style)->style == CL_STYLE_GLOSSY || CLEARLOOKS_STYLE (style)->style == CL_STYLE_GUMMY) &&
	         ((DETAIL("checkbutton") || DETAIL("radiobutton")) && state_type == GTK_STATE_PRELIGHT))
	{
		/* XXX: Don't draw any check/radiobutton bg in GLOSSY or GUMMY mode. */
	}
	else
	{
		GTK_STYLE_CLASS (clearlooks_style_parent_class)->draw_flat_box (style, window, state_type,
		                                        shadow_type,
		                                        area, widget, detail,
		                                        x, y, width, height);
	}
}

static void
clearlooks_style_draw_shadow (DRAW_ARGS)
{
	ClearlooksStyle  *clearlooks_style = CLEARLOOKS_STYLE (style);
	ClearlooksColors *colors = &clearlooks_style->colors;
	cairo_t          *cr     = ge_gdk_drawable_to_cairo (window, area);

	CHECK_ARGS
	SANITIZE_SIZE

	/* The "frame" thing is a hack because of GtkCombo. */
	if ((DETAIL ("entry") && !CHECK_HINT (GE_HINT_TREEVIEW)) ||
	    (DETAIL ("frame") && CHECK_HINT (GE_HINT_COMBOBOX_ENTRY)))
	{
		WidgetParameters params;

		/* Override the entries state type, because we are too lame to handle this via
		 * the focus ring, and GtkEntry doesn't even set the INSENSITIVE state ... */
		if (state_type == GTK_STATE_NORMAL && widget && GE_IS_ENTRY (widget))
			state_type = GTK_WIDGET_STATE (widget);

		clearlooks_set_widget_parameters (widget, style, state_type, &params);

		if (CHECK_HINT (GE_HINT_COMBOBOX_ENTRY) || CHECK_HINT (GE_HINT_SPINBUTTON))
		{
			width += style->xthickness;
			if (!params.ltr)
				x -= style->xthickness;

			if (params.ltr)
				params.corners = CR_CORNER_TOPLEFT | CR_CORNER_BOTTOMLEFT;
			else
				params.corners = CR_CORNER_TOPRIGHT | CR_CORNER_BOTTOMRIGHT;
		}
		
		/* Fill the background as it is initilized to base[NORMAL].
		 * Relevant GTK+ bug: http://bugzilla.gnome.org/show_bug.cgi?id=513471
		 * The fill only happens if no hint has been added by some application
		 * that is faking GTK+ widgets. */
		if (!widget || !g_object_get_data(G_OBJECT (widget), "transparent-bg-hint"))
		{
			cairo_rectangle (cr, 0, 0, width, height);
			ge_cairo_set_color (cr, &params.parentbg);
			cairo_fill (cr);
		}

		STYLE_FUNCTION (draw_entry) (cr, &clearlooks_style->colors, &params,
		                             x, y, width, height);
	}
	else if (DETAIL ("frame") && CHECK_HINT (GE_HINT_STATUSBAR))
	{
		WidgetParameters params;

		clearlooks_set_widget_parameters (widget, style, state_type, &params);

		gtk_style_apply_default_background (style, window, TRUE, state_type,
		                                    area, x, y, width, height);
		if (shadow_type != GTK_SHADOW_NONE)
			STYLE_FUNCTION (draw_statusbar) (cr, colors, &params,
			                                 x, y, width, height);
	}
	else if (DETAIL ("frame") || DETAIL ("calendar"))
	{
		WidgetParameters params;
		FrameParameters  frame;
		frame.shadow  = shadow_type;
		frame.gap_x   = -1;  /* No gap will be drawn */
		frame.border  = &colors->shade[4];

		clearlooks_set_widget_parameters (widget, style, state_type, &params);
		params.corners = CR_CORNER_NONE;

		if (widget && !g_str_equal ("XfcePanelWindow", gtk_widget_get_name (gtk_widget_get_toplevel (widget))))
			STYLE_FUNCTION(draw_frame) (cr, colors, &params, &frame,
			                            x, y, width, height);
	}
	else if (DETAIL ("scrolled_window") || DETAIL ("viewport") || detail == NULL)
	{
		CairoColor border;

		if (CLEARLOOKS_STYLE (style)->style == CL_STYLE_CLASSIC)
			ge_shade_color ((CairoColor*)&colors->bg[0], 0.78, &border);
		else
			border = colors->shade[5];

		cairo_rectangle (cr, x+0.5, y+0.5, width-1, height-1);
		ge_cairo_set_color (cr, &border);
		cairo_set_line_width (cr, 1);
		cairo_stroke (cr);
	}
	else
	{
		WidgetParameters params;
		FrameParameters frame;

		frame.shadow = shadow_type;
		frame.gap_x  = -1;
		frame.border = &colors->shade[5];
		clearlooks_set_widget_parameters (widget, style, state_type, &params);
		params.corners = CR_CORNER_ALL;

		STYLE_FUNCTION(draw_frame) (cr, colors, &params, &frame, x, y, width, height);
	}

	cairo_destroy (cr);
}

static void
clearlooks_style_draw_box_gap (DRAW_ARGS,
                               GtkPositionType gap_side,
                               gint            gap_x,
                               gint            gap_width)
{
	ClearlooksStyle  *clearlooks_style = CLEARLOOKS_STYLE (style);
	ClearlooksColors *colors = &clearlooks_style->colors;
	cairo_t          *cr;

	CHECK_ARGS
	SANITIZE_SIZE

	cr = ge_gdk_drawable_to_cairo (window, area);

	if (DETAIL ("notebook"))
	{
		WidgetParameters params;
		FrameParameters  frame;
		gboolean start, end;

		frame.shadow    = shadow_type;
		frame.gap_side  = gap_side;
		frame.gap_x     = gap_x;
		frame.gap_width = gap_width;
		frame.border    = &colors->shade[5];

		clearlooks_set_widget_parameters (widget, style, state_type, &params);

		clearlooks_get_notebook_tab_position (widget, &start, &end);

		params.corners = CR_CORNER_ALL;
		switch (gap_side)
		{
			case GTK_POS_TOP:
				if (ge_widget_is_ltr (widget))
				{
					if (start)
						params.corners ^= CR_CORNER_TOPLEFT;
					if (end)
						params.corners ^= CR_CORNER_TOPRIGHT;
				}
				else
				{
					if (start)
						params.corners ^= CR_CORNER_TOPRIGHT;
					if (end)
						params.corners ^= CR_CORNER_TOPLEFT;
				}
			break;
			case GTK_POS_BOTTOM:
				if (ge_widget_is_ltr (widget))
				{
					if (start)
						params.corners ^= CR_CORNER_BOTTOMLEFT;
					if (end)
						params.corners ^= CR_CORNER_BOTTOMRIGHT;
				}
				else
				{
					if (start)
						params.corners ^= CR_CORNER_BOTTOMRIGHT;
					if (end)
						params.corners ^= CR_CORNER_BOTTOMLEFT;
				}
			break;
			case GTK_POS_LEFT:
				if (start)
					params.corners ^= CR_CORNER_TOPLEFT;
				if (end)
					params.corners ^= CR_CORNER_BOTTOMLEFT;
			break;
			case GTK_POS_RIGHT:
				if (start)
					params.corners ^= CR_CORNER_TOPRIGHT;
				if (end)
					params.corners ^= CR_CORNER_BOTTOMRIGHT;
			break;
		}

		/* Fill the background with bg[NORMAL] */
		ge_cairo_rounded_rectangle (cr, x, y, width, height, params.radius, params.corners);
		ge_cairo_set_color (cr, &colors->bg[GTK_STATE_NORMAL]);
		cairo_fill (cr);

		STYLE_FUNCTION(draw_frame) (cr, colors, &params, &frame,
		                            x, y, width, height);
	}
	else
	{
		GTK_STYLE_CLASS (clearlooks_style_parent_class)->draw_box_gap (style, window, state_type, shadow_type,
		                                       area, widget, detail,
		                                       x, y, width, height,
		                                       gap_side, gap_x, gap_width);
	}

	cairo_destroy (cr);
}

static void
clearlooks_style_draw_extension (DRAW_ARGS, GtkPositionType gap_side)
{
	ClearlooksStyle  *clearlooks_style = CLEARLOOKS_STYLE (style);
	ClearlooksColors *colors = &clearlooks_style->colors;
	cairo_t          *cr;

	CHECK_ARGS
	SANITIZE_SIZE

	cr = ge_gdk_drawable_to_cairo (window, area);

	if (DETAIL ("tab"))
	{
		WidgetParameters params;
		TabParameters    tab;
		FocusParameters  focus;

		clearlooks_set_widget_parameters (widget, style, state_type, &params);

		tab.gap_side = (ClearlooksGapSide)gap_side;

		switch (gap_side)
		{
			case CL_GAP_TOP:
				params.corners = CR_CORNER_BOTTOMLEFT | CR_CORNER_BOTTOMRIGHT;
				break;
			case CL_GAP_BOTTOM:
				params.corners = CR_CORNER_TOPLEFT | CR_CORNER_TOPRIGHT;
				break;
			case CL_GAP_LEFT:
				params.corners = CR_CORNER_TOPRIGHT | CR_CORNER_BOTTOMRIGHT;
				break;
			case CL_GAP_RIGHT:
				params.corners = CR_CORNER_TOPLEFT | CR_CORNER_BOTTOMLEFT;
				break;
		}

		/* Focus color */
		if (clearlooks_style->has_focus_color)
		{
			ge_gdk_color_to_cairo (&clearlooks_style->focus_color, &focus.color);
			focus.has_color = TRUE;
		}
		else
			focus.color = colors->bg[GTK_STATE_SELECTED];

		tab.focus = focus;

		STYLE_FUNCTION(draw_tab) (cr, colors, &params, &tab,
		                          x, y, width, height);
	}
	else
	{
		GTK_STYLE_CLASS (clearlooks_style_parent_class)->draw_extension (style, window, state_type, shadow_type, area,
		                                         widget, detail, x, y, width, height,
		                                         gap_side);
	}

	cairo_destroy (cr);
}

static void
clearlooks_style_draw_handle (DRAW_ARGS, GtkOrientation orientation)
{
	ClearlooksStyle  *clearlooks_style = CLEARLOOKS_STYLE (style);
	ClearlooksColors *colors = &clearlooks_style->colors;
	cairo_t          *cr;

	CHECK_ARGS
	SANITIZE_SIZE

	cr = ge_gdk_drawable_to_cairo (window, area);

	if (DETAIL ("handlebox"))
	{
		WidgetParameters params;
		HandleParameters handle;

		clearlooks_set_widget_parameters (widget, style, state_type, &params);
		handle.type = CL_HANDLE_TOOLBAR;
		handle.horizontal = (orientation == GTK_ORIENTATION_HORIZONTAL);

		STYLE_FUNCTION(draw_handle) (cr, colors, &params, &handle,
		                             x, y, width, height);
	}
	else if (DETAIL ("paned"))
	{
		WidgetParameters params;
		HandleParameters handle;

		clearlooks_set_widget_parameters (widget, style, state_type, &params);
		handle.type = CL_HANDLE_SPLITTER;
		handle.horizontal = (orientation == GTK_ORIENTATION_HORIZONTAL);

		STYLE_FUNCTION(draw_handle) (cr, colors, &params, &handle,
		                             x, y, width, height);
	}
	else
	{
		WidgetParameters params;
		HandleParameters handle;

		clearlooks_set_widget_parameters (widget, style, state_type, &params);
		handle.type = CL_HANDLE_TOOLBAR;
		handle.horizontal = (orientation == GTK_ORIENTATION_HORIZONTAL);

		STYLE_FUNCTION(draw_handle) (cr, colors, &params, &handle,
		                             x, y, width, height);
	}

	cairo_destroy (cr);
}

static void
clearlooks_style_draw_box (DRAW_ARGS)
{
	ClearlooksStyle *clearlooks_style = CLEARLOOKS_STYLE (style);
	const ClearlooksColors *colors;
	cairo_t *cr;

	cr     = ge_gdk_drawable_to_cairo (window, area);
	colors = &clearlooks_style->colors;

	CHECK_ARGS
	SANITIZE_SIZE

	if (DETAIL ("menubar"))
	{
		WidgetParameters params;
		MenuBarParameters menubar;
		gboolean horizontal;

		clearlooks_set_widget_parameters (widget, style, state_type, &params);

		menubar.style = clearlooks_style->menubarstyle;

		horizontal = height < 2*width;
		/* This is not that great. Ideally we would have a nice vertical menubar. */
		if ((shadow_type != GTK_SHADOW_NONE) && horizontal)
			STYLE_FUNCTION(draw_menubar) (cr, colors, &params, &menubar,
			                              x, y, width, height);
	}
	else if (DETAIL ("button") && CHECK_HINT (GE_HINT_TREEVIEW_HEADER))
	{
		WidgetParameters params;
		ListViewHeaderParameters header;

		gint columns, column_index;
		gboolean resizable = TRUE;

		/* XXX: This makes unknown treeview header "middle", in need for something nicer */
		columns = 3;
		column_index = 1;

		clearlooks_set_widget_parameters (widget, style, state_type, &params);

		params.corners = CR_CORNER_NONE;

		if (GE_IS_TREE_VIEW (widget->parent))
		{
			clearlooks_treeview_get_header_index (GTK_TREE_VIEW(widget->parent),
			                                      widget, &column_index, &columns,
			                                      &resizable);
		}
		else if (GE_IS_CLIST (widget->parent))
		{
			clearlooks_clist_get_header_index (GTK_CLIST(widget->parent),
			                                   widget, &column_index, &columns);
		}

		header.resizable = resizable;

		header.order = 0;
		if (column_index == 0)
			header.order |= params.ltr ? CL_ORDER_FIRST : CL_ORDER_LAST;
		if (column_index == columns-1)
			header.order |= params.ltr ? CL_ORDER_LAST : CL_ORDER_FIRST;

		gtk_style_apply_default_background (style, window, FALSE, state_type, area, x, y, width, height);

		STYLE_FUNCTION(draw_list_view_header) (cr, colors, &params, &header,
		                                       x, y, width, height);
	}
	else if (DETAIL ("button") || DETAIL ("buttondefault"))
	{
		WidgetParameters params;
		ShadowParameters shadow = { CR_CORNER_ALL, CL_SHADOW_NONE } ;
		clearlooks_set_widget_parameters (widget, style, state_type, &params);
		params.active = shadow_type == GTK_SHADOW_IN;

		if (CHECK_HINT (GE_HINT_COMBOBOX_ENTRY))
		{
			if (params.ltr)
				params.corners = CR_CORNER_TOPRIGHT | CR_CORNER_BOTTOMRIGHT;
			else
				params.corners = CR_CORNER_TOPLEFT | CR_CORNER_BOTTOMLEFT;

			shadow.shadow = CL_SHADOW_IN;

			if (params.xthickness > 2)
			{
				if (params.ltr)
					x--;
				width++;
			}
		}
		else
		{
			params.corners = CR_CORNER_ALL;
			if (clearlooks_style->reliefstyle != 0)
				params.enable_shadow = TRUE;
		}

		STYLE_FUNCTION(draw_button) (cr, &clearlooks_style->colors, &params,
		                             x, y, width, height);
	}
	else if (DETAIL ("spinbutton_up") || DETAIL ("spinbutton_down"))
	{
		if (state_type == GTK_STATE_ACTIVE)
		{
			WidgetParameters params;
			clearlooks_set_widget_parameters (widget, style, state_type, &params);

			if (style->xthickness == 3)
			{
				width++;
				if (params.ltr)
					x--;
			}

			if (DETAIL ("spinbutton_up"))
			{
				height+=2;
				if (params.ltr)
					params.corners = CR_CORNER_TOPRIGHT;
				else
					params.corners = CR_CORNER_TOPLEFT;
			}
			else
			{
				if (params.ltr)
					params.corners = CR_CORNER_BOTTOMRIGHT;
				else
					params.corners = CR_CORNER_BOTTOMLEFT;
			}

			STYLE_FUNCTION(draw_spinbutton_down) (cr, &clearlooks_style->colors, &params, x, y, width, height);
		}
	}
	else if (DETAIL ("spinbutton"))
	{
		WidgetParameters params;

		/* The "spinbutton" box is always drawn with state NORMAL, even if it is insensitive.
		 * So work around this here. */
		if (state_type == GTK_STATE_NORMAL && widget && GE_IS_ENTRY (widget))
			state_type = GTK_WIDGET_STATE (widget);

		clearlooks_set_widget_parameters (widget, style, state_type, &params);

		if (params.ltr)
			params.corners = CR_CORNER_TOPRIGHT | CR_CORNER_BOTTOMRIGHT;
		else
			params.corners = CR_CORNER_TOPLEFT | CR_CORNER_BOTTOMLEFT;

		if (style->xthickness == 3)
		{
			if (params.ltr)
				x--;
			width++;
		}

		STYLE_FUNCTION(draw_spinbutton) (cr, &clearlooks_style->colors, &params,
		                                 x, y, width, height);
	}
	else if (detail && g_str_has_prefix (detail, "trough") && CHECK_HINT (GE_HINT_SCALE))
	{
		WidgetParameters params;
		SliderParameters slider;

		clearlooks_set_widget_parameters (widget, style, state_type, &params);
		params.corners    = CR_CORNER_NONE;

		slider.lower = DETAIL ("trough-lower");
		slider.fill_level = DETAIL ("trough-fill-level") || DETAIL ("trough-fill-level-full");

		if (CHECK_HINT (GE_HINT_HSCALE))
			slider.horizontal = TRUE;
		else if (CHECK_HINT (GE_HINT_VSCALE))
			slider.horizontal = FALSE;
		else /* Fallback based on the size... */
			slider.horizontal = width >= height;

		STYLE_FUNCTION(draw_scale_trough) (cr, &clearlooks_style->colors,
		                                   &params, &slider,
		                                   x, y, width, height);
	}
	else if (DETAIL ("trough") && CHECK_HINT (GE_HINT_PROGRESSBAR))
	{
		WidgetParameters params;

		clearlooks_set_widget_parameters (widget, style, state_type, &params);

		/* Fill the background as it is initilized to base[NORMAL].
		 * Relevant GTK+ bug: http://bugzilla.gnome.org/show_bug.cgi?id=513476
		 * The fill only happens if no hint has been added by some application
		 * that is faking GTK+ widgets. */
		if (!widget || !g_object_get_data(G_OBJECT (widget), "transparent-bg-hint"))
		{
			cairo_rectangle (cr, 0, 0, width, height);
			ge_cairo_set_color (cr, &params.parentbg);
			cairo_fill (cr);
		}
		STYLE_FUNCTION(draw_progressbar_trough) (cr, colors, &params,
		                                         x, y, width, height);
	}
	else if (DETAIL ("trough") && CHECK_HINT (GE_HINT_SCROLLBAR))
	{
		WidgetParameters params;
		ScrollBarParameters scrollbar;
		gboolean trough_under_steppers = TRUE;
		ClearlooksStepper steppers;

		clearlooks_set_widget_parameters (widget, style, state_type, &params);
		params.corners = CR_CORNER_ALL;

		scrollbar.horizontal = TRUE;
		scrollbar.junction   = clearlooks_scrollbar_get_junction (widget);
		
		steppers = clearlooks_scrollbar_visible_steppers (widget);

		if (CHECK_HINT (GE_HINT_HSCROLLBAR))
			scrollbar.horizontal = TRUE;
		else if (CHECK_HINT (GE_HINT_VSCROLLBAR))
			scrollbar.horizontal = FALSE;
		else /* Fallback based on the size  ... */
			scrollbar.horizontal = width >= height;

		if (widget)
			gtk_widget_style_get (widget,
			                      "trough-under-steppers", &trough_under_steppers,
			                      NULL);

		if (trough_under_steppers)
		{
			/* If trough under steppers is set, then we decrease the size
			 * slightly. The size is decreased so that the trough is not
			 * visible underneath the steppers. This is not really needed
			 * as one can use the trough-under-steppers style property,
			 * but it needs to exist for backward compatibility. */
			if (scrollbar.horizontal)
			{
				if (steppers & (CL_STEPPER_A | CL_STEPPER_B))
				{
					x += 2;
					width -= 2;
				}
				if (steppers & (CL_STEPPER_C | CL_STEPPER_D))
				{
					width -= 2;
				}
			}
			else
			{
				if (steppers & (CL_STEPPER_A | CL_STEPPER_B))
				{
					y += 2;
					height -= 2;
				}
				if (steppers & (CL_STEPPER_C | CL_STEPPER_D))
				{
					height -= 2;
				}
			}
		}

		STYLE_FUNCTION(draw_scrollbar_trough) (cr, colors, &params, &scrollbar,
		                                       x, y, width, height);
	}
	else if (DETAIL ("bar"))
	{
		WidgetParameters      params;
		ProgressBarParameters progressbar;
		gdouble               elapsed = 0.0;

#ifdef HAVE_ANIMATION
		if(clearlooks_style->animation && CL_IS_PROGRESS_BAR (widget))
		{
			gboolean activity_mode = GTK_PROGRESS (widget)->activity_mode;

			if (!activity_mode)
				clearlooks_animation_progressbar_add ((gpointer)widget);
		}

		elapsed = clearlooks_animation_elapsed (widget);
#endif

		clearlooks_set_widget_parameters (widget, style, state_type, &params);

		if (widget && GE_IS_PROGRESS_BAR (widget))
		{
			progressbar.orientation = gtk_progress_bar_get_orientation (GTK_PROGRESS_BAR (widget));
			progressbar.value = gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(widget));
			progressbar.pulsing = GTK_PROGRESS (widget)->activity_mode;
		}
		else
		{
			progressbar.orientation = CL_ORIENTATION_LEFT_TO_RIGHT;
			progressbar.value = 0;
			progressbar.pulsing = FALSE;
		}

		if (!params.ltr)
		{
			if (progressbar.orientation == GTK_PROGRESS_LEFT_TO_RIGHT)
				progressbar.orientation = GTK_PROGRESS_RIGHT_TO_LEFT;
			else if (progressbar.orientation == GTK_PROGRESS_RIGHT_TO_LEFT)
				progressbar.orientation = GTK_PROGRESS_LEFT_TO_RIGHT;
		}

		/* Following is a hack to have a larger clip area, the one passed in
		 * does not allow for the shadow. */
		if (area)
		{
			GdkRectangle tmp = *area;
			if (!progressbar.pulsing)
			{
				switch (progressbar.orientation)
				{
					case GTK_PROGRESS_RIGHT_TO_LEFT:
						tmp.x -= 1;
					case GTK_PROGRESS_LEFT_TO_RIGHT:
						tmp.width += 1;
						break;
					case GTK_PROGRESS_BOTTOM_TO_TOP:
						tmp.y -= 1;
					case GTK_PROGRESS_TOP_TO_BOTTOM:
						tmp.height += 1;
						break;
				}
			}
			else
			{
				if (progressbar.orientation == GTK_PROGRESS_RIGHT_TO_LEFT ||
				    progressbar.orientation == GTK_PROGRESS_LEFT_TO_RIGHT)
				{
					tmp.x -= 1;
					tmp.width += 2;
				}
				else
				{
					tmp.y -= 1;
					tmp.height += 2;
				}
			}

			cairo_reset_clip (cr);
			gdk_cairo_rectangle (cr, &tmp);
			cairo_clip (cr);
		}

		STYLE_FUNCTION(draw_progressbar_fill) (cr, colors, &params, &progressbar,
		                                       x, y, width, height,
		                                       10 - (int)(elapsed * 10.0) % 10);
	}
	else if (DETAIL ("optionmenu"))
	{
		WidgetParameters params;
		OptionMenuParameters optionmenu;

		GtkRequisition indicator_size;
		GtkBorder indicator_spacing;

		clearlooks_set_widget_parameters (widget, style, state_type, &params);

		if (clearlooks_style->reliefstyle != 0)
			params.enable_shadow = TRUE;

		ge_option_menu_get_props (widget, &indicator_size, &indicator_spacing);

		if (ge_widget_is_ltr (widget))
			optionmenu.linepos = width - (indicator_size.width + indicator_spacing.left + indicator_spacing.right) - 1;
		else
			optionmenu.linepos = (indicator_size.width + indicator_spacing.left + indicator_spacing.right) + 1;

		STYLE_FUNCTION(draw_optionmenu) (cr, colors, &params, &optionmenu,
		                                 x, y, width, height);
	}
	else if (DETAIL ("menuitem"))
	{
		WidgetParameters params;
		clearlooks_set_widget_parameters (widget, style, state_type, &params);

		if (CHECK_HINT (GE_HINT_MENUBAR))
		{
			params.corners = CR_CORNER_TOPLEFT | CR_CORNER_TOPRIGHT;
			height += 1;
			STYLE_FUNCTION(draw_menubaritem) (cr, colors, &params, x, y, width, height);
		}
		else
		{
			params.corners = CR_CORNER_ALL;
			STYLE_FUNCTION(draw_menuitem) (cr, colors, &params, x, y, width, height);
		}
	}
	else if (DETAIL ("hscrollbar") || DETAIL ("vscrollbar")) /* This can't be "stepper" for scrollbars ... */
	{
		WidgetParameters    params;
		ScrollBarParameters scrollbar;
		ScrollBarStepperParameters stepper;
		GdkRectangle this_rectangle;

		this_rectangle.x = x;
		this_rectangle.y = y;
		this_rectangle.width  = width;
		this_rectangle.height = height;

		clearlooks_set_widget_parameters (widget, style, state_type, &params);
		params.corners = CR_CORNER_NONE;

		scrollbar.has_color  = FALSE;
		scrollbar.horizontal = TRUE;
		scrollbar.junction   = clearlooks_scrollbar_get_junction (widget);

		if (clearlooks_style->colorize_scrollbar || clearlooks_style->has_scrollbar_color)
			scrollbar.has_color = TRUE;

		scrollbar.horizontal = DETAIL ("hscrollbar");

		stepper.stepper = clearlooks_scrollbar_get_stepper (widget, &this_rectangle);

		STYLE_FUNCTION(draw_scrollbar_stepper) (cr, colors, &params, &scrollbar, &stepper,
		                                        x, y, width, height);
	}
	else if (DETAIL ("toolbar") || DETAIL ("handlebox_bin") || DETAIL ("dockitem_bin"))
	{
		WidgetParameters  params;
		ToolbarParameters toolbar;
		gboolean horizontal;

		clearlooks_set_widget_parameters (widget, style, state_type, &params);
		clearlooks_set_toolbar_parameters (&toolbar, widget, window, x, y);

		toolbar.style = clearlooks_style->toolbarstyle;

		if ((DETAIL ("handlebox_bin") || DETAIL ("dockitem_bin")) && GE_IS_BIN (widget))
		{
			GtkWidget* child = gtk_bin_get_child ((GtkBin*) widget);
			/* This is to draw the correct shadow on the handlebox.
			 * We need to draw it here, as otherwise the handle will not get the
			 * background. */
			if (GE_IS_TOOLBAR (child))
				gtk_widget_style_get (child, "shadow-type", &shadow_type, NULL);
		}
		
		horizontal = height < 2*width;
		/* This is not that great. Ideally we would have a nice vertical toolbar. */
		if ((shadow_type != GTK_SHADOW_NONE) && horizontal)
			STYLE_FUNCTION(draw_toolbar) (cr, colors, &params, &toolbar, x, y, width, height);
	}
	else if (DETAIL ("trough"))
	{
		/* Nothing? Why benjamin? */
	}
	else if (DETAIL ("menu"))
	{
		WidgetParameters params;

		clearlooks_set_widget_parameters (widget, style, state_type, &params);

		STYLE_FUNCTION(draw_menu_frame) (cr, colors, &params, x, y, width, height);
	}
	else if (DETAIL ("hseparator") || DETAIL ("vseparator"))
	{
		gchar *new_detail = (gchar*) detail;
		/* Draw a normal separator, we just use this because it gives more control
		 * over sizing (currently). */

		/* This isn't nice ... but it seems like the best cleanest way to me right now.
		 * It will get slightly nicer in the future hopefully. */
		if (GE_IS_MENU_ITEM (widget))
			new_detail = "menuitem";

		if (DETAIL ("hseparator"))
		{
			gtk_paint_hline (style, window, state_type, area, widget, new_detail,
			                 x, x + width - 1, y + height/2);
		}
		else
			gtk_paint_vline (style, window, state_type, area, widget, new_detail,
			                 y, y + height - 1, x + width/2);
	}
	else
	{
		GTK_STYLE_CLASS (clearlooks_style_parent_class)->draw_box (style, window, state_type, shadow_type, area,
		                                   widget, detail, x, y, width, height);
	}

	cairo_destroy (cr);
}

static void
clearlooks_style_draw_slider (DRAW_ARGS, GtkOrientation orientation)
{
	ClearlooksStyle *clearlooks_style = CLEARLOOKS_STYLE (style);
	const ClearlooksColors *colors;
	cairo_t *cr;

	cr     = ge_gdk_drawable_to_cairo (window, area);
	colors = &clearlooks_style->colors;

	CHECK_ARGS
	SANITIZE_SIZE

	if (DETAIL ("hscale") || DETAIL ("vscale"))
	{
		WidgetParameters params;
		SliderParameters slider;

		clearlooks_set_widget_parameters (widget, style, state_type, &params);

		slider.horizontal = (orientation == GTK_ORIENTATION_HORIZONTAL);
		slider.lower = FALSE;
		slider.fill_level = FALSE;

		if (clearlooks_style->style == CL_STYLE_GLOSSY) /* XXX! */
			params.corners = CR_CORNER_ALL;

		STYLE_FUNCTION(draw_slider_button) (cr, &clearlooks_style->colors,
		                                    &params, &slider,
		                                    x, y, width, height);
	}
	else if (DETAIL ("slider"))
	{
		WidgetParameters    params;
		ScrollBarParameters scrollbar;

		clearlooks_set_widget_parameters (widget, style, state_type, &params);
		params.corners = CR_CORNER_NONE;

		scrollbar.has_color  = FALSE;
		scrollbar.horizontal = (orientation == GTK_ORIENTATION_HORIZONTAL);
		scrollbar.junction   = clearlooks_scrollbar_get_junction (widget);

		if (clearlooks_style->colorize_scrollbar)
		{
			scrollbar.color = colors->spot[1];
			scrollbar.has_color = TRUE;
		}

		/* Set scrollbar color */
		if (clearlooks_style->has_scrollbar_color)
		{
			ge_gdk_color_to_cairo (&clearlooks_style->scrollbar_color, &scrollbar.color);
			scrollbar.has_color = TRUE;
		}

		if ((clearlooks_style->style == CL_STYLE_GLOSSY || clearlooks_style->style == CL_STYLE_GUMMY)
			&& !scrollbar.has_color)
			scrollbar.color = colors->bg[0];

		STYLE_FUNCTION(draw_scrollbar_slider) (cr, colors, &params, &scrollbar,
		                                       x, y, width, height);
	}
	else
	{
		GTK_STYLE_CLASS (clearlooks_style_parent_class)->draw_slider (style, window, state_type, shadow_type, area,
		                                      widget, detail, x, y, width, height, orientation);
	}

	cairo_destroy (cr);
}

static void
clearlooks_style_draw_option (DRAW_ARGS)
{
	ClearlooksStyle *clearlooks_style = CLEARLOOKS_STYLE (style);
	const ClearlooksColors *colors;
	WidgetParameters params;
	CheckboxParameters checkbox;
	cairo_t *cr;

	CHECK_ARGS
	SANITIZE_SIZE

	cr = ge_gdk_drawable_to_cairo (window, area);
	colors = &clearlooks_style->colors;

	checkbox.shadow_type = shadow_type;
	checkbox.in_menu = (widget && GTK_IS_MENU(widget->parent));

	clearlooks_set_widget_parameters (widget, style, state_type, &params);

	STYLE_FUNCTION(draw_radiobutton) (cr, colors, &params, &checkbox, x, y, width, height);

	cairo_destroy (cr);
}

static void
clearlooks_style_draw_check (DRAW_ARGS)
{
	ClearlooksStyle *clearlooks_style = CLEARLOOKS_STYLE (style);
	WidgetParameters params;
	CheckboxParameters checkbox;
	cairo_t *cr;

	CHECK_ARGS
	SANITIZE_SIZE

	cr = ge_gdk_drawable_to_cairo (window, area);

	clearlooks_set_widget_parameters (widget, style, state_type, &params);

	params.corners = CR_CORNER_ALL;

	checkbox.shadow_type = shadow_type;
	checkbox.in_cell = DETAIL("cellcheck");

	checkbox.in_menu = (widget && widget->parent && GTK_IS_MENU(widget->parent));

	STYLE_FUNCTION(draw_checkbox) (cr, &clearlooks_style->colors, &params, &checkbox,
	                               x, y, width, height);

	cairo_destroy (cr);
}

static void
clearlooks_style_draw_vline (GtkStyle               *style,
                             GdkWindow              *window,
                             GtkStateType            state_type,
                             GdkRectangle           *area,
                             GtkWidget              *widget,
                             const gchar            *detail,
                             gint                    y1,
                             gint                    y2,
                             gint                    x)
{
	ClearlooksStyle *clearlooks_style = CLEARLOOKS_STYLE (style);
	const ClearlooksColors *colors;
	SeparatorParameters separator = { FALSE };
	cairo_t *cr;

	CHECK_ARGS

	colors = &clearlooks_style->colors;

	cr = ge_gdk_drawable_to_cairo (window, area);

	/* There is no such thing as a vertical menu separator
	 * (and even if, a normal one should be better on menu bars) */
	STYLE_FUNCTION(draw_separator) (cr, colors, NULL, &separator,
	                                x, y1, 2, y2-y1+1);

	cairo_destroy (cr);
}

static void
clearlooks_style_draw_hline (GtkStyle               *style,
                             GdkWindow              *window,
                             GtkStateType            state_type,
                             GdkRectangle           *area,
                             GtkWidget              *widget,
                             const gchar            *detail,
                             gint                    x1,
                             gint                    x2,
                             gint                    y)
{
	ClearlooksStyle *clearlooks_style = CLEARLOOKS_STYLE (style);
	const ClearlooksColors *colors;
	cairo_t *cr;
	SeparatorParameters separator;

	CHECK_ARGS

	colors = &clearlooks_style->colors;

	cr = ge_gdk_drawable_to_cairo (window, area);

	separator.horizontal = TRUE;

	if (!DETAIL ("menuitem"))
		STYLE_FUNCTION(draw_separator) (cr, colors, NULL, &separator,
		                                x1, y, x2-x1+1, 2);
	else
		STYLE_FUNCTION(draw_menu_item_separator) (cr, colors, NULL, &separator,
		                                          x1, y, x2-x1+1, 2);

	cairo_destroy (cr);
}

static void
clearlooks_style_draw_shadow_gap (DRAW_ARGS,
                                  GtkPositionType gap_side,
                                  gint            gap_x,
                                  gint            gap_width)
{
	ClearlooksStyle *clearlooks_style = CLEARLOOKS_STYLE (style);
	const ClearlooksColors *colors;
	cairo_t *cr;

	CHECK_ARGS
	SANITIZE_SIZE

	cr     = ge_gdk_drawable_to_cairo (window, area);
	colors = &clearlooks_style->colors;

	if (DETAIL ("frame"))
	{
		WidgetParameters params;
		FrameParameters  frame;

		frame.shadow    = shadow_type;
		frame.gap_side  = gap_side;
		frame.gap_x     = gap_x;
		frame.gap_width = gap_width;
		frame.border    = &colors->shade[5];

		clearlooks_set_widget_parameters (widget, style, state_type, &params);

		params.corners = CR_CORNER_ALL;

		STYLE_FUNCTION(draw_frame) (cr, colors, &params, &frame,
		                            x, y, width, height);
	}
	else
	{
		GTK_STYLE_CLASS (clearlooks_style_parent_class)->draw_shadow_gap (style, window, state_type, shadow_type, area,
		                                          widget, detail, x, y, width, height,
		                                          gap_side, gap_x, gap_width);
	}

	cairo_destroy (cr);
}

static void
clearlooks_style_draw_resize_grip (GtkStyle       *style,
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
	ClearlooksColors *colors = &clearlooks_style->colors;
	cairo_t *cr;
	WidgetParameters params;
	ResizeGripParameters grip;

	CHECK_ARGS
	SANITIZE_SIZE

	grip.edge = (ClearlooksWindowEdge)edge;

	g_return_if_fail (window != NULL);

	cr = ge_gdk_drawable_to_cairo (window, area);

	clearlooks_set_widget_parameters (widget, style, state_type, &params);

	STYLE_FUNCTION(draw_resize_grip) (cr, colors, &params, &grip,
	                                  x, y, width, height);

	cairo_destroy (cr);
}

static void
clearlooks_style_draw_tab (DRAW_ARGS)
{
	ClearlooksStyle *clearlooks_style = CLEARLOOKS_STYLE (style);
	ClearlooksColors *colors = &clearlooks_style->colors;
	WidgetParameters params;
	ArrowParameters  arrow;
	cairo_t *cr;

	CHECK_ARGS
	SANITIZE_SIZE

	cr = ge_gdk_drawable_to_cairo (window, area);

	clearlooks_set_widget_parameters (widget, style, state_type, &params);
	arrow.type      = CL_ARROW_COMBO;
	arrow.direction = CL_DIRECTION_DOWN;

	STYLE_FUNCTION(draw_arrow) (cr, colors, &params, &arrow, x, y, width, height);

	cairo_destroy (cr);
}

static void
clearlooks_style_draw_arrow (GtkStyle  *style,
                             GdkWindow     *window,
                             GtkStateType   state_type,
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
	ClearlooksStyle  *clearlooks_style = CLEARLOOKS_STYLE (style);
	ClearlooksColors *colors = &clearlooks_style->colors;
	WidgetParameters params;
	ArrowParameters  arrow;
	cairo_t *cr = ge_gdk_drawable_to_cairo (window, area);

	CHECK_ARGS
	SANITIZE_SIZE

	if (arrow_type == GTK_ARROW_NONE)
	{
		cairo_destroy (cr);
		return;
	}

	clearlooks_set_widget_parameters (widget, style, state_type, &params);
	arrow.type = CL_ARROW_NORMAL;
	arrow.direction = (ClearlooksDirection)arrow_type;

	if (ge_is_combo_box (widget, FALSE) && !ge_is_combo_box_entry (widget))
	{
		arrow.type = CL_ARROW_COMBO;
	}

	/* I have no idea why, but the arrow of GtkCombo is larger than in other places.
	 * Subtracting 3 seems to fix this. */
	if (widget && widget->parent && GE_IS_COMBO (widget->parent->parent))
	{
		if (params.ltr)
			x += 1;
		else
			x += 2;
		width -= 3;
	}

	STYLE_FUNCTION(draw_arrow) (cr, colors, &params, &arrow, x, y, width, height);

	cairo_destroy (cr);
}

static void
clearlooks_style_init_from_rc (GtkStyle * style,
                               GtkRcStyle * rc_style)
{
	ClearlooksStyle *clearlooks_style = CLEARLOOKS_STYLE (style);

	GTK_STYLE_CLASS (clearlooks_style_parent_class)->init_from_rc (style, rc_style);

	g_assert ((CLEARLOOKS_RC_STYLE (rc_style)->style >= 0) && (CLEARLOOKS_RC_STYLE (rc_style)->style < CL_NUM_STYLES));
	clearlooks_style->style               = CLEARLOOKS_RC_STYLE (rc_style)->style;

	clearlooks_style->reliefstyle         = CLEARLOOKS_RC_STYLE (rc_style)->reliefstyle;
	clearlooks_style->menubarstyle        = CLEARLOOKS_RC_STYLE (rc_style)->menubarstyle;
	clearlooks_style->toolbarstyle        = CLEARLOOKS_RC_STYLE (rc_style)->toolbarstyle;
	clearlooks_style->has_focus_color     = CLEARLOOKS_RC_STYLE (rc_style)->flags & CL_FLAG_FOCUS_COLOR;
	clearlooks_style->has_scrollbar_color = CLEARLOOKS_RC_STYLE (rc_style)->flags & CL_FLAG_SCROLLBAR_COLOR;
	clearlooks_style->colorize_scrollbar  = CLEARLOOKS_RC_STYLE (rc_style)->colorize_scrollbar;
	clearlooks_style->animation           = CLEARLOOKS_RC_STYLE (rc_style)->animation;
	clearlooks_style->radius              = CLAMP (CLEARLOOKS_RC_STYLE (rc_style)->radius, 0.0, 10.0);

	if (clearlooks_style->has_focus_color)
		clearlooks_style->focus_color     = CLEARLOOKS_RC_STYLE (rc_style)->focus_color;
	if (clearlooks_style->has_scrollbar_color)
		clearlooks_style->scrollbar_color = CLEARLOOKS_RC_STYLE (rc_style)->scrollbar_color;
}

static void
clearlooks_style_realize (GtkStyle * style)
{
	ClearlooksStyle *clearlooks_style = CLEARLOOKS_STYLE (style);
	double shades[] = {1.15, 0.95, 0.896, 0.82, 0.7, 0.665, 0.475, 0.45, 0.4};
	CairoColor spot_color;
	CairoColor bg_normal;
	double contrast;
	int i;

	GTK_STYLE_CLASS (clearlooks_style_parent_class)->realize (style);

	contrast = CLEARLOOKS_RC_STYLE (style->rc_style)->contrast;

	/* Lighter to darker */
	ge_gdk_color_to_cairo (&style->bg[GTK_STATE_NORMAL], &bg_normal);

	for (i = 0; i < 9; i++)
	{
		ge_shade_color (&bg_normal, (shades[i] < 1.0) ?
		                (shades[i]/contrast) : (shades[i]*contrast),
		                &clearlooks_style->colors.shade[i]);
	}

	ge_gdk_color_to_cairo (&style->bg[GTK_STATE_SELECTED], &spot_color);

	/* Andrea Cimitan wants something like the following to handle dark themes.
	 * However, these two lines are broken currently, as ge_hsb_from_color expects
	 * a CairoColor and not GdkColor
	 *  ge_hsb_from_color (&style->bg[GTK_STATE_SELECTED], &hue_spot, &saturation_spot, &brightness_spot);
	 *  ge_hsb_from_color (&style->bg[GTK_STATE_NORMAL],   &hue_bg,   &saturation_bg,   &brightness_bg);
	 */

	/* Here to place some checks for dark themes.
	 * We should use a different shade value for spot[2]. */

	ge_shade_color (&spot_color, 1.25, &clearlooks_style->colors.spot[0]);
	ge_shade_color (&spot_color, 1.05, &clearlooks_style->colors.spot[1]);
	ge_shade_color (&spot_color, 0.65, &clearlooks_style->colors.spot[2]);

	for (i=0; i<5; i++)
	{
		ge_gdk_color_to_cairo (&style->fg[i], &clearlooks_style->colors.fg[i]);
		ge_gdk_color_to_cairo (&style->bg[i], &clearlooks_style->colors.bg[i]);
		ge_gdk_color_to_cairo (&style->base[i], &clearlooks_style->colors.base[i]);
		ge_gdk_color_to_cairo (&style->text[i], &clearlooks_style->colors.text[i]);
	}
}

static void
clearlooks_style_draw_focus (GtkStyle *style, GdkWindow *window, GtkStateType state_type,
                             GdkRectangle *area, GtkWidget *widget, const gchar *detail,
                             gint x, gint y, gint width, gint height)
{
	ClearlooksStyle  *clearlooks_style = CLEARLOOKS_STYLE (style);
	ClearlooksColors *colors = &clearlooks_style->colors;
	WidgetParameters params;
	FocusParameters focus;
	guint8* dash_list;

	cairo_t *cr;

	CHECK_ARGS
	SANITIZE_SIZE

	cr = gdk_cairo_create (window);

	clearlooks_set_widget_parameters (widget, style, state_type, &params);

	/* Corners */
	params.corners = CR_CORNER_ALL;
	if (CHECK_HINT (GE_HINT_COMBOBOX_ENTRY))
	{
		if (params.ltr)
			params.corners = CR_CORNER_TOPRIGHT | CR_CORNER_BOTTOMRIGHT;
		else
			params.corners = CR_CORNER_TOPLEFT | CR_CORNER_BOTTOMLEFT;

		if (params.xthickness > 2)
		{
			if (params.ltr)
				x--;
			width++;
		}
	}

	focus.has_color = FALSE;
	focus.interior = FALSE;
	focus.line_width = 1;
	focus.padding = 1;
	dash_list = NULL;

	if (widget)
	{
		gtk_widget_style_get (widget,
		                      "focus-line-width", &focus.line_width,
		                      "focus-line-pattern", &dash_list,
		                      "focus-padding", &focus.padding,
		                      "interior-focus", &focus.interior,
		                      NULL);
	}
	if (dash_list)
		focus.dash_list = dash_list;
	else
		focus.dash_list = (guint8*) g_strdup ("\1\1");

	/* Focus type */
	if (DETAIL("button"))
	{
		if (CHECK_HINT (GE_HINT_TREEVIEW_HEADER))
		{
			focus.type = CL_FOCUS_TREEVIEW_HEADER;
		}
		else
		{
			GtkReliefStyle relief = GTK_RELIEF_NORMAL;
			/* Check for the shadow type. */
			if (widget && GTK_IS_BUTTON (widget))
				g_object_get (G_OBJECT (widget), "relief", &relief, NULL);

			if (relief == GTK_RELIEF_NORMAL)
				focus.type = CL_FOCUS_BUTTON;
			else
				focus.type = CL_FOCUS_BUTTON_FLAT;

			/* This is a workaround for the bogus focus handling that
			 * clearlooks has currently.
			 * I truely dislike putting it here, but I guess it is better
			 * then having such a visible bug. It should be removed in the
			 * next unstable release cycle.  -- Benjamin */
			if (ge_object_is_a (G_OBJECT (widget), "ButtonWidget"))
				focus.type = CL_FOCUS_LABEL;
		}
	}
	else if (detail && g_str_has_prefix (detail, "treeview"))
	{
		/* Focus in a treeview, and that means a lot of different detail strings. */
		if (g_str_has_prefix (detail, "treeview-drop-indicator"))
			focus.type = CL_FOCUS_TREEVIEW_DND;
		else
			focus.type = CL_FOCUS_TREEVIEW_ROW;

		if (g_str_has_suffix (detail, "left"))
		{
			focus.continue_side = CL_CONT_RIGHT;
		}
		else if (g_str_has_suffix (detail, "right"))
		{
			focus.continue_side = CL_CONT_LEFT;
		}
		else if (g_str_has_suffix (detail, "middle"))
		{
			focus.continue_side = CL_CONT_LEFT | CL_CONT_RIGHT;
		}
		else
		{
			/* This may either mean no continuation, or unknown ...
			 * if it is unknown we assume it continues on both sides */
			gboolean row_ending_details = FALSE;

			/* Try to get the style property. */
			if (widget)
				gtk_widget_style_get (widget,
				                      "row-ending-details", &row_ending_details,
				                      NULL);

			if (row_ending_details)
				focus.continue_side = CL_CONT_NONE;
			else
				focus.continue_side = CL_CONT_LEFT | CL_CONT_RIGHT;
		}

	}
	else if (detail && g_str_has_prefix (detail, "trough") && CHECK_HINT (GE_HINT_SCALE))
	{
		focus.type = CL_FOCUS_SCALE;
	}
	else if (DETAIL("tab"))
	{
		focus.type = CL_FOCUS_TAB;
	}
	else if (detail && g_str_has_prefix (detail, "colorwheel"))
	{
		if (DETAIL ("colorwheel_dark"))
			focus.type = CL_FOCUS_COLOR_WHEEL_DARK;
		else
			focus.type = CL_FOCUS_COLOR_WHEEL_LIGHT;
	}
	else if (DETAIL("checkbutton") || DETAIL("radiobutton"))
	{
		focus.type = CL_FOCUS_LABEL; /* Let's call it "LABEL" :) */
	}
	else if (CHECK_HINT (GE_HINT_TREEVIEW))
	{
		focus.type = CL_FOCUS_TREEVIEW; /* Treeview without content is focused. */
	}
	else
	{
		focus.type = CL_FOCUS_UNKNOWN; /* Custom widgets (Beagle) and something unknown */
	}

	/* Focus color */
	if (clearlooks_style->has_focus_color)
	{
		ge_gdk_color_to_cairo (&clearlooks_style->focus_color, &focus.color);
		focus.has_color = TRUE;
	}
	else
		focus.color = colors->bg[GTK_STATE_SELECTED];

	STYLE_FUNCTION(draw_focus) (cr, colors, &params, &focus, x, y, width, height);

	g_free (focus.dash_list);

	cairo_destroy (cr);
}

static void
clearlooks_style_copy (GtkStyle * style, GtkStyle * src)
{
	ClearlooksStyle * cl_style = CLEARLOOKS_STYLE (style);
	ClearlooksStyle * cl_src = CLEARLOOKS_STYLE (src);

	cl_style->colors              = cl_src->colors;
	cl_style->reliefstyle         = cl_src->reliefstyle;
	cl_style->menubarstyle        = cl_src->menubarstyle;
	cl_style->toolbarstyle        = cl_src->toolbarstyle;
	cl_style->focus_color         = cl_src->focus_color;
	cl_style->has_focus_color     = cl_src->has_focus_color;
	cl_style->scrollbar_color     = cl_src->scrollbar_color;
	cl_style->has_scrollbar_color = cl_src->has_scrollbar_color;
	cl_style->colorize_scrollbar  = cl_src->colorize_scrollbar;
	cl_style->animation           = cl_src->animation;
	cl_style->radius              = cl_src->radius;
	cl_style->style               = cl_src->style;

	GTK_STYLE_CLASS (clearlooks_style_parent_class)->copy (style, src);
}

static void
clearlooks_style_unrealize (GtkStyle * style)
{
	GTK_STYLE_CLASS (clearlooks_style_parent_class)->unrealize (style);
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

	for (y = 0; y < height; y++)
	{
		for (x = 0; x < width; x++)
		{
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
	    height == gdk_pixbuf_get_height (src))
	{
		return g_object_ref (src);
	}
	else
	{
		return gdk_pixbuf_scale_simple (src,
		                                width, height,
		                                GDK_INTERP_BILINEAR);
	}
}

static void
clearlooks_style_draw_layout (GtkStyle * style,
                              GdkWindow * window,
                              GtkStateType state_type,
                              gboolean use_text,
                              GdkRectangle * area,
                              GtkWidget * widget,
                              const gchar * detail, gint x, gint y, PangoLayout * layout)
{
	GdkGC *gc;

	g_return_if_fail (GTK_IS_STYLE (style));
	g_return_if_fail (window != NULL);

	gc = use_text ? style->text_gc[state_type] : style->fg_gc[state_type];

	if (area)
		gdk_gc_set_clip_rectangle (gc, area);

	if (state_type == GTK_STATE_INSENSITIVE)
	{
		ClearlooksStyle *clearlooks_style = CLEARLOOKS_STYLE (style);
		ClearlooksColors *colors = &clearlooks_style->colors;

		WidgetParameters params;
		GdkColor etched;
		CairoColor temp;

		clearlooks_set_widget_parameters (widget, style, state_type, &params);

		if (GTK_WIDGET_NO_WINDOW (widget))
			ge_shade_color (&params.parentbg, 1.2, &temp);
		else
			ge_shade_color (&colors->bg[widget->state], 1.2, &temp);

		etched.red = (int) (temp.r * 65535);
		etched.green = (int) (temp.g * 65535);
		etched.blue = (int) (temp.b * 65535);

		gdk_draw_layout_with_colors (window, gc, x + 1, y + 1, layout, &etched, NULL);
		gdk_draw_layout (window, gc, x, y, layout);
	}
	else
		gdk_draw_layout (window, gc, x, y, layout);

	if (area)
		gdk_gc_set_clip_rectangle (gc, NULL);
}

static GdkPixbuf *
clearlooks_style_draw_render_icon (GtkStyle            *style,
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

	if (widget && gtk_widget_has_screen (widget))
	{
		screen = gtk_widget_get_screen (widget);
		settings = gtk_settings_get_for_screen (screen);
	}
	else if (style->colormap)
	{
		screen = gdk_colormap_get_screen (style->colormap);
		settings = gtk_settings_get_for_screen (screen);
	}
	else
	{
		settings = gtk_settings_get_default ();
		GTK_NOTE (MULTIHEAD,
			  g_warning ("Using the default screen for gtk_default_render_icon()"));
	}

	if (size != (GtkIconSize) -1 && !gtk_icon_size_lookup_for_settings (settings, size, &width, &height))
	{
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
	if (gtk_icon_source_get_state_wildcarded (source))
	{
		if (state == GTK_STATE_INSENSITIVE)
		{
			stated = set_transparency (scaled, 0.3);
			gdk_pixbuf_saturate_and_pixelate (stated, stated, 0.1, FALSE);

			g_object_unref (scaled);
		}
		else if (state == GTK_STATE_PRELIGHT)
		{
			stated = gdk_pixbuf_copy (scaled);

			gdk_pixbuf_saturate_and_pixelate (scaled, stated, 1.2, FALSE);

			g_object_unref (scaled);
		}
		else
		{
			stated = scaled;
		}
	}
	else
		stated = scaled;

	return stated;
}

void
clearlooks_style_register_types (GTypeModule *module)
{
  clearlooks_style_register_type (module);
}

static void
clearlooks_style_init (ClearlooksStyle * style)
{
}

static void
clearlooks_style_class_init (ClearlooksStyleClass * klass)
{
	GtkStyleClass *style_class = GTK_STYLE_CLASS (klass);

	style_class->copy             = clearlooks_style_copy;
	style_class->realize          = clearlooks_style_realize;
	style_class->unrealize        = clearlooks_style_unrealize;
	style_class->init_from_rc     = clearlooks_style_init_from_rc;
	style_class->draw_handle      = clearlooks_style_draw_handle;
	style_class->draw_slider      = clearlooks_style_draw_slider;
	style_class->draw_shadow_gap  = clearlooks_style_draw_shadow_gap;
	style_class->draw_focus       = clearlooks_style_draw_focus;
	style_class->draw_box         = clearlooks_style_draw_box;
	style_class->draw_shadow      = clearlooks_style_draw_shadow;
	style_class->draw_box_gap     = clearlooks_style_draw_box_gap;
	style_class->draw_extension   = clearlooks_style_draw_extension;
	style_class->draw_option      = clearlooks_style_draw_option;
	style_class->draw_check       = clearlooks_style_draw_check;
	style_class->draw_flat_box    = clearlooks_style_draw_flat_box;
	style_class->draw_vline       = clearlooks_style_draw_vline;
	style_class->draw_hline       = clearlooks_style_draw_hline;
	style_class->draw_resize_grip = clearlooks_style_draw_resize_grip;
	style_class->draw_tab         = clearlooks_style_draw_tab;
	style_class->draw_arrow       = clearlooks_style_draw_arrow;
	style_class->draw_layout      = clearlooks_style_draw_layout;
	style_class->render_icon      = clearlooks_style_draw_render_icon;

	clearlooks_register_style_classic (&klass->style_functions[CL_STYLE_CLASSIC],
	                                   &klass->style_constants[CL_STYLE_CLASSIC]);

	klass->style_functions[CL_STYLE_GLOSSY] = klass->style_functions[CL_STYLE_CLASSIC];
	klass->style_constants[CL_STYLE_GLOSSY] = klass->style_constants[CL_STYLE_CLASSIC];
	clearlooks_register_style_glossy (&klass->style_functions[CL_STYLE_GLOSSY],
	                                  &klass->style_constants[CL_STYLE_GLOSSY]);

	klass->style_functions[CL_STYLE_INVERTED] = klass->style_functions[CL_STYLE_CLASSIC];
	klass->style_constants[CL_STYLE_INVERTED] = klass->style_constants[CL_STYLE_CLASSIC];
	clearlooks_register_style_inverted (&klass->style_functions[CL_STYLE_INVERTED],
	                                    &klass->style_constants[CL_STYLE_INVERTED]);

	klass->style_functions[CL_STYLE_GUMMY] = klass->style_functions[CL_STYLE_CLASSIC];
	klass->style_constants[CL_STYLE_GUMMY] = klass->style_constants[CL_STYLE_CLASSIC];
	clearlooks_register_style_gummy (&klass->style_functions[CL_STYLE_GUMMY],
	                                 &klass->style_constants[CL_STYLE_GUMMY]);
}

static void
clearlooks_style_class_finalize (ClearlooksStyleClass *klass)
{
}
