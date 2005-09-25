/* gtk-canvas-waveview.h: GtkCanvas item for displaying wave data
 *
 * Copyright (C) 2001 Paul Davis <pbd@op.net>
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

#ifndef __GTK_CANVAS_WAVEVIEW_H__
#define __GTK_CANVAS_WAVEVIEW_H__

#include <stdint.h>

#include <gtk-canvas/gtk-canvas-defs.h>
#include "gtk-canvas/gtk-canvas.h"

BEGIN_GTK_CANVAS_DECLS

/* Wave viewer item for canvas.
 */

#define GTK_CANVAS_TYPE_CANVAS_WAVEVIEW            (gtk_canvas_waveview_get_type ())
#define GTK_CANVAS_WAVEVIEW(obj)                   (GTK_CHECK_CAST ((obj), GTK_CANVAS_TYPE_CANVAS_WAVEVIEW, GtkCanvasWaveView))
#define GTK_CANVAS_WAVEVIEW_CLASS(klass)           (GTK_CHECK_CLASS_CAST ((klass), GTK_CANVAS_TYPE_CANVAS_WAVEVIEW, GtkCanvasWaveViewClass))
#define GTK_CANVAS_IS_CANVAS_WAVEVIEW(obj)         (GTK_CHECK_TYPE ((obj), GTK_CANVAS_TYPE_CANVAS_WAVEVIEW))
#define GTK_CANVAS_IS_CANVAS_WAVEVIEW_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_CANVAS_TYPE_CANVAS_WAVEVIEW))

typedef struct _GtkCanvasWaveView            GtkCanvasWaveView;
typedef struct _GtkCanvasWaveViewClass       GtkCanvasWaveViewClass;
typedef struct _GtkCanvasWaveViewChannelInfo GtkCanvasWaveViewChannelInfo;
typedef struct _GtkCanvasWaveViewCacheEntry  GtkCanvasWaveViewCacheEntry;
typedef struct _GtkCanvasWaveViewCache       GtkCanvasWaveViewCache;

/* XXX this needs to be synced with ardour/source.h PeakData */

struct _GtkCanvasWaveViewCacheEntry
{
    float  min;
    float  max;
};

struct _GtkCanvasWaveViewCache
{
    GtkCanvasWaveViewCacheEntry* data;
    gint32                       allocated;
    gint32                       data_size;
    gulong                       start;
    gulong                       end;
};    

GtkCanvasWaveViewCache* gtk_canvas_waveview_cache_new ();
void                    gtk_canvas_waveview_cache_destroy (GtkCanvasWaveViewCache*);

struct _GtkCanvasWaveView
{
    GtkCanvasItem item;
    
    GtkCanvasWaveViewCache *cache;
    gboolean                cache_updater;
    gint                    screen_width;

    void *data_src;
    guint32 channel;
	void (*peak_function)(void*,gulong,gulong,gulong,gpointer,guint32,double);
    gulong (*length_function)(void *);
    gulong (*sourcefile_length_function)(void*);
    void (*gain_curve_function)(void *arg, double start, double end, float* vector, guint32 veclen);
    void *gain_src;

    /* x-axis: samples per canvas unit. */
    double samples_per_unit;
    
    /* y-axis: amplitude_above_axis.
     * 
     * the default is that an (scaled, normalized -1.0 ... +1.0) amplitude of 1.0
     * corresponds to the top of the area assigned to the waveview.
     *
     * larger values will expand the vertical scale, cutting off the peaks/troughs.
     * smaller values will decrease the vertical scale, moving peaks/troughs toward
     * the middle of the area assigned to the waveview.
     */

    double amplitude_above_axis;
    double x;
    double y;
    double height;
    double half_height;
    uint32_t wave_color;

    char rectified;

    /* These are updated by the update() routine
       to optimize the render() routine, which may
       be called several times after a single update().
    */

    int32_t bbox_ulx;
    int32_t bbox_uly;
    int32_t bbox_lrx;
    int32_t bbox_lry;
    unsigned char wave_r, wave_g, wave_b, wave_a;
    uint32_t samples;
    uint32_t region_start;
    int32_t reload_cache_in_render;
};

struct _GtkCanvasWaveViewClass {
	GtkCanvasItemClass parent_class;
};

GtkType gtk_canvas_waveview_get_type (void);

END_GTK_CANVAS_DECLS

#endif /* __GTK_CANVAS_WAVEVIEW_H__ */
