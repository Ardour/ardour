#ifndef CLEARLOOKS_DRAW_H
#define CLEARLOOKS_DRAW_H

#include <gtk/gtk.h>
#include <gdk/gdk.h>

typedef struct 
{
	GdkColor *from;
	GdkColor *to;
} CLGradient;

typedef enum
{
	CL_GRADIENT_NONE,
	CL_GRADIENT_HORIZONTAL,
	CL_GRADIENT_VERTICAL
} CLGradientType;

typedef struct
{
	CLGradient      fill_gradient;
	CLGradient      border_gradient;
	
	CLGradientType  gradient_type;
	
	GdkGC          *bordergc;
	GdkGC          *fillgc;

	guint8          corners[4];
	
	GdkGC          *topleft;		/* top + left shadow */
	GdkGC          *bottomright;	/* bottom + right shadow */
	
	GdkColor        tmp_color;		/* used for gradient */
} CLRectangle;

typedef enum /* DON'T CHANGE THE ORDER! */
{
	CL_CORNER_TOPRIGHT,
	CL_CORNER_BOTTOMRIGHT,
	CL_CORNER_BOTTOMLEFT,
	CL_CORNER_TOPLEFT
} CLCornerSide;

typedef enum /* DON'T CHANGE THE ORDER! */
{
	CL_BORDER_TOP,
	CL_BORDER_RIGHT,
	CL_BORDER_BOTTOM,
	CL_BORDER_LEFT
} CLBorderType;

typedef enum
{
	CL_CORNER_NONE   = 0,
	CL_CORNER_NARROW = 1,
	CL_CORNER_ROUND  = 2
} CLCornerSharpness;



CLRectangle *cl_rectangle_new(GdkGC *fillgc, GdkGC *bordergc,
                              int tl, int tr, int bl, int br);

void cl_draw_rectangle (GdkWindow *window, GtkWidget *widget, GtkStyle *style,
                        int x, int y, int width, int height, CLRectangle *r);

void cl_rectangle_set_color (CLGradient *g, GdkColor *color);
void cl_rectangle_set_gradient (CLGradient *g, GdkColor *from, GdkColor *to);

void cl_rectangle_set_button (CLRectangle *r, GtkStyle *style,
                             GtkStateType state_type, gboolean hasdefault, gboolean has_focus,
                             CLBorderType tl, CLBorderType tr,
                             CLBorderType bl, CLBorderType br);

void cl_rectangle_set_entry (CLRectangle *r, GtkStyle *style,
                            GtkStateType state_type,
                            CLBorderType tl, CLBorderType tr,
                            CLBorderType bl, CLBorderType br,
                            gboolean has_focus);
							
void cl_draw_shadow(GdkWindow *window, GtkWidget *widget, GtkStyle *style,
                    int x, int y, int width, int height, CLRectangle *r);

void cl_rectangle_set_clip_rectangle (CLRectangle *r, GdkRectangle *area);
void cl_rectangle_reset_clip_rectangle (CLRectangle *r);

void cl_set_corner_sharpness (const gchar *detail, GtkWidget *widget, CLRectangle *r);


void cl_rectangle_set_corners (CLRectangle *r, int tl, int tr, int bl, int br);

void cl_rectangle_init (CLRectangle *r, GdkGC *fillgc, GdkGC *bordergc,
                        int tl, int tr, int bl, int br);

void cl_rectangle_reset (CLRectangle *r, GtkStyle *style);


GdkPixmap* cl_progressbar_tile_new (GdkDrawable *drawable, GtkWidget *widget,
                              GtkStyle *style, gint height, gint offset);
				
void cl_progressbar_fill (GdkDrawable *drawable, GtkWidget *widget,
                          GtkStyle *style, GdkGC *gc,
                          gint x, gint y, gint width, gint height,
						  guint8 offset, GdkRectangle *area);
						  
GdkColor cl_gc_set_fg_color_shade (GdkGC *gc, GdkColormap *colormap, 
                                   GdkColor *from, gfloat s);

void cl_draw_spinbutton(GtkStyle *style, GdkWindow *window,
                        GtkStateType state_type, GtkShadowType shadow_type,
                        GdkRectangle *area,
                        GtkWidget *widget, const gchar *detail,
                        gint x, gint y, gint width, gint height);
						
void cl_draw_button(GtkStyle *style, GdkWindow *window,
                    GtkStateType state_type, GtkShadowType shadow_type,
                    GdkRectangle *area,
                    GtkWidget *widget, const gchar *detail,
                    gint x, gint y, gint width, gint height);
					
void cl_draw_entry (GtkStyle *style, GdkWindow *window,
                    GtkStateType state_type, GtkShadowType shadow_type,
                    GdkRectangle *area,
                    GtkWidget *widget, const gchar *detail,
                    gint x, gint y, gint width, gint height);
					
void cl_draw_combobox_entry (GtkStyle *style, GdkWindow *window,
                             GtkStateType state_type, GtkShadowType shadow_type,
                             GdkRectangle *area,
                             GtkWidget *widget, const gchar *detail,
                             gint x, gint y, gint width, gint height);
		
void cl_draw_combobox_button (GtkStyle *style, GdkWindow *window,
                             GtkStateType state_type, GtkShadowType shadow_type,
                             GdkRectangle *area,
                             GtkWidget *widget, const gchar *detail,
                             gint x, gint y, gint width, gint height);
							 
void cl_draw_menuitem_button (GdkDrawable *window, GtkWidget *widget, GtkStyle *style,
                              GdkRectangle *area, GtkStateType state_type, 
                              int x, int y, int wiidth, int height, CLRectangle *r);

void cl_draw_menuitem_flat (GdkDrawable *window, GtkWidget *widget, GtkStyle *style,
                            GdkRectangle *area, GtkStateType state_type, 
                            int x, int y, int wiidth, int height, CLRectangle *r);
                            
void cl_draw_menuitem_gradient (GdkDrawable *window, GtkWidget *widget, GtkStyle *style,
                                GdkRectangle *area, GtkStateType state_type, 
                                int x, int y, int wiidth, int height, CLRectangle *r);
							  
void cl_draw_treeview_header (GtkStyle *style, GdkWindow *window,
                              GtkStateType state_type, GtkShadowType shadow_type,
                              GdkRectangle *area,
                              GtkWidget *widget, const gchar *detail,
                              gint x, gint y, gint width, gint height);
							  
#endif /* CLEARLOOKS_DRAW_H */
