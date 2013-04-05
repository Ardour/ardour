/* libgnomecanvas/gnome-canvas-waveview.h: GnomeCanvas item for displaying wave data
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

#ifndef __GNOME_CANVAS_WAVEVIEW_H__
#define __GNOME_CANVAS_WAVEVIEW_H__

#include <stdint.h>

#include <libgnomecanvas/libgnomecanvas.h>

G_BEGIN_DECLS

/* Wave viewer item for canvas.
 */

#define GNOME_TYPE_CANVAS_WAVEVIEW            (gnome_canvas_waveview_get_type ())
#define GNOME_CANVAS_WAVEVIEW(obj)            (GTK_CHECK_CAST ((obj), GNOME_TYPE_CANVAS_WAVEVIEW, GnomeCanvasWaveView))
#define GNOME_CANVAS_WAVEVIEW_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GNOME_TYPE_CANVAS_WAVEVIEW, GnomeCanvasWaveViewClass))
#define GNOME_IS_CANVAS_WAVEVIEW(obj)         (GTK_CHECK_TYPE ((obj), GNOME_TYPE_CANVAS_WAVEVIEW))
#define GNOME_IS_CANVAS_WAVEVIEW_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_CANVAS_WAVEVIEW))
#define GNOME_CANVAS_WAVEVIEW_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS ((obj), GNOME_TYPE_CANVAS_WAVEVIEW, GnomeCanvasWaveViewClass))

typedef struct _GnomeCanvasWaveView            GnomeCanvasWaveView;
typedef struct _GnomeCanvasWaveViewClass       GnomeCanvasWaveViewClass;
typedef struct _GnomeCanvasWaveViewChannelInfo GnomeCanvasWaveViewChannelInfo;
typedef struct _GnomeCanvasWaveViewCacheEntry  GnomeCanvasWaveViewCacheEntry;
typedef struct _GnomeCanvasWaveViewCache       GnomeCanvasWaveViewCache;

/* XXX this needs to be synced with ardour/source.h PeakData */

struct _GnomeCanvasWaveViewCacheEntry
{
    float  min;
    float  max;
};

struct _GnomeCanvasWaveViewCache
{
    GnomeCanvasWaveViewCacheEntry* data;
    guint32                       allocated;
    guint64                       data_size;
    gulong                        start;
    gulong                        end;
};

GnomeCanvasWaveViewCache* gnome_canvas_waveview_cache_new (void);
void                    gnome_canvas_waveview_cache_destroy (GnomeCanvasWaveViewCache*);

void gnome_canvas_waveview_set_gradient_waveforms (int);

typedef  gulong (*waveview_length_function_t)(void*);
typedef  gulong (*waveview_sourcefile_length_function_t)(void*, double);
typedef  void (*waveview_gain_curve_function_t)(void *arg, double start, double end, float* vector, gint64 veclen);
typedef  void (*waveview_peak_function_t)(void*,gulong,gulong,gulong,gpointer,guint32,double);

struct _GnomeCanvasWaveView
{
    GnomeCanvasItem item;

    GnomeCanvasWaveViewCache *cache;
    gboolean                cache_updater;
    gint                    screen_width;

    void *data_src;
    guint32 channel;
    waveview_peak_function_t peak_function;
    waveview_length_function_t length_function;
    waveview_sourcefile_length_function_t sourcefile_length_function;
    waveview_gain_curve_function_t gain_curve_function;
    void *gain_src;

    /** x-axis: samples per canvas unit. */
    double samples_per_unit;

    /** y-axis: amplitude_above_axis.
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
    uint32_t clip_color;
    uint32_t zero_color;
    uint32_t fill_color;

    char filled;
    char rectified;
    char zero_line;
    char logscaled;

    /* These are updated by the update() routine
       to optimize the render() routine, which may
       be called several times after a single update().
    */

    int32_t bbox_ulx;
    int32_t bbox_uly;
    int32_t bbox_lrx;
    int32_t bbox_lry;
    unsigned char wave_r, wave_g, wave_b, wave_a;
    unsigned char clip_r, clip_g, clip_b, clip_a;
    unsigned char fill_r, fill_g, fill_b, fill_a;
    uint32_t samples;
    uint32_t region_start;
    int32_t reload_cache_in_render;
};

struct _GnomeCanvasWaveViewClass {
	GnomeCanvasItemClass parent_class;
};

GType gnome_canvas_waveview_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __GNOME_CANVAS_WAVEVIEW_H__ */
