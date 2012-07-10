/*
     Copyright (C) 2000-2002 Paul Davis

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published by
     the Free Software Foundation; either version 2 of the License, or
     (at your option) any later version.

     This program is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
     GNU General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with this program; if not, write to the Free Software
     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

     $Id$
*/

#include <stdio.h>
#include <math.h>
#include <libgnomecanvas/libgnomecanvas.h>
#include <cairo.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

#include "ardour/dB.h"

#include "logmeter.h"
#include "canvas-waveview.h"
#include "rgb_macros.h"

/* POSIX guarantees casting between void* and function pointers, ISO C doesn't
 * We can work around warnings by going one step deeper in our casts
 */
#ifdef _POSIX_VERSION
#define POSIX_FUNC_PTR_CAST(type, object) *((type*) &(object))
#endif // _POSIX_VERSION

extern void c_stacktrace();

enum {
	 PROP_0,
	 PROP_DATA_SRC,
	 PROP_CHANNEL,
	 PROP_LENGTH_FUNCTION,
	 PROP_SOURCEFILE_LENGTH_FUNCTION,
	 PROP_PEAK_FUNCTION,
	 PROP_GAIN_FUNCTION,
	 PROP_GAIN_SRC,
	 PROP_CACHE,
	 PROP_CACHE_UPDATER,
	 PROP_SAMPLES_PER_UNIT,
	 PROP_AMPLITUDE_ABOVE_AXIS,
	 PROP_X,
	 PROP_Y,
	 PROP_HEIGHT,
	 PROP_WAVE_COLOR,
	 PROP_CLIP_COLOR,
	 PROP_ZERO_COLOR,
	 PROP_FILL_COLOR,
	 PROP_FILLED,
	 PROP_RECTIFIED,
	 PROP_ZERO_LINE,
	 PROP_REGION_START,
	 PROP_LOGSCALED,
};

static void gnome_canvas_waveview_class_init     (GnomeCanvasWaveViewClass *class);

static void gnome_canvas_waveview_init           (GnomeCanvasWaveView      *waveview);

static void gnome_canvas_waveview_destroy        (GtkObject            *object);

static void gnome_canvas_waveview_set_property   (GObject        *object,
						   guint           prop_id,
						   const GValue   *value,
						   GParamSpec     *pspec);
static void gnome_canvas_waveview_get_property   (GObject        *object,
						   guint           prop_id,
						   GValue         *value,
						   GParamSpec     *pspec);

static void   gnome_canvas_waveview_update       (GnomeCanvasItem *item,
						   double          *affine,
						   ArtSVP          *clip_path,
						   int              flags);

static void   gnome_canvas_waveview_bounds       (GnomeCanvasItem *item,
						   double          *x1,
						   double          *y1,
						   double          *x2,
						   double          *y2);

static double gnome_canvas_waveview_point        (GnomeCanvasItem  *item,
						   double            x,
						   double            y,
						   int               cx,
						   int               cy,
						   GnomeCanvasItem **actual_item);

static void gnome_canvas_waveview_render         (GnomeCanvasItem *item,
						   GnomeCanvasBuf  *buf);

static void gnome_canvas_waveview_draw           (GnomeCanvasItem *item,
						   GdkDrawable     *drawable,
						   int              x,
						   int              y,
						   int              w,
						   int              h);

static void gnome_canvas_waveview_set_data_src   (GnomeCanvasWaveView *,
						   void *);

static void gnome_canvas_waveview_set_channel    (GnomeCanvasWaveView *,
						   guint32);

static guint32 gnome_canvas_waveview_ensure_cache (GnomeCanvasWaveView *waveview,
						   gulong               start_sample,
						   gulong               end_sample);


static int _gradient_rendering = 0;

static GnomeCanvasItemClass *parent_class;

GType
gnome_canvas_waveview_get_type (void)
{
	 static GType waveview_type;

	 if (!waveview_type) {
		 static const GTypeInfo object_info = {
			 sizeof (GnomeCanvasWaveViewClass),
			 (GBaseInitFunc) NULL,
			 (GBaseFinalizeFunc) NULL,
			 (GClassInitFunc) gnome_canvas_waveview_class_init,
			 (GClassFinalizeFunc) NULL,
			 NULL,			/* class_data */
			 sizeof (GnomeCanvasWaveView),
			 0,			/* n_preallocs */
			 (GInstanceInitFunc) gnome_canvas_waveview_init,
			 NULL			/* value_table */
		 };

		 waveview_type = g_type_register_static (GNOME_TYPE_CANVAS_ITEM, "GnomeCanvasWaveView",
							 &object_info, 0);
	 }

	 return waveview_type;
 }

static void
gnome_canvas_waveview_class_init (GnomeCanvasWaveViewClass *class)
{
	 GObjectClass *gobject_class;
	 GtkObjectClass *object_class;
	 GnomeCanvasItemClass *item_class;

	 gobject_class = (GObjectClass *) class;
	 object_class = (GtkObjectClass *) class;
	 item_class = (GnomeCanvasItemClass *) class;

	 parent_class = g_type_class_peek_parent (class);

	 gobject_class->set_property = gnome_canvas_waveview_set_property;
	 gobject_class->get_property = gnome_canvas_waveview_get_property;

	 g_object_class_install_property
		 (gobject_class,
		  PROP_DATA_SRC,
		  g_param_spec_pointer ("data_src", NULL, NULL,
					(G_PARAM_READABLE | G_PARAM_WRITABLE)));

	 g_object_class_install_property
		 (gobject_class,
		  PROP_CHANNEL,
		  g_param_spec_uint ("channel", NULL, NULL,
				     0, G_MAXUINT, 0,
				     (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	 g_object_class_install_property
		 (gobject_class,
		  PROP_LENGTH_FUNCTION,
		  g_param_spec_pointer ("length_function", NULL, NULL,
					(G_PARAM_READABLE | G_PARAM_WRITABLE)));

	 g_object_class_install_property
                (gobject_class,
                 PROP_SOURCEFILE_LENGTH_FUNCTION,
                 g_param_spec_pointer ("sourcefile_length_function", NULL, NULL,
				       (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	 g_object_class_install_property
		 (gobject_class,
		  PROP_PEAK_FUNCTION,
		  g_param_spec_pointer ("peak_function", NULL, NULL,
					(G_PARAM_READABLE | G_PARAM_WRITABLE)));

	 g_object_class_install_property
		 (gobject_class,
		  PROP_GAIN_FUNCTION,
		  g_param_spec_pointer ("gain_function", NULL, NULL,
					(G_PARAM_READABLE | G_PARAM_WRITABLE)));

	 g_object_class_install_property
		 (gobject_class,
                 PROP_GAIN_SRC,
                 g_param_spec_pointer ("gain_src", NULL, NULL,
				       (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	 g_object_class_install_property
		 (gobject_class,
		  PROP_CACHE,
		  g_param_spec_pointer ("cache", NULL, NULL,
					(G_PARAM_READABLE | G_PARAM_WRITABLE)));

	 g_object_class_install_property
		 (gobject_class,
		  PROP_CACHE_UPDATER,
                 g_param_spec_boolean ("cache_updater", NULL, NULL,
				       FALSE,
				       (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	 g_object_class_install_property
		 (gobject_class,
		  PROP_SAMPLES_PER_UNIT,
		  g_param_spec_double ("samples_per_unit", NULL, NULL,
				       0.0, G_MAXDOUBLE, 0.0,
				       (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	 g_object_class_install_property
		 (gobject_class,
		  PROP_AMPLITUDE_ABOVE_AXIS,
		  g_param_spec_double ("amplitude_above_axis", NULL, NULL,
				       0.0, G_MAXDOUBLE, 0.0,
				       (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	 g_object_class_install_property
		 (gobject_class,
		  PROP_X,
		  g_param_spec_double ("x", NULL, NULL,
				       0.0, G_MAXDOUBLE, 0.0,
				       (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	 g_object_class_install_property
		 (gobject_class,
		  PROP_Y,
		  g_param_spec_double ("y", NULL, NULL,
				       0.0, G_MAXDOUBLE, 0.0,
				       (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	 g_object_class_install_property
		 (gobject_class,
		  PROP_HEIGHT,
		  g_param_spec_double ("height", NULL, NULL,
				       0.0, G_MAXDOUBLE, 0.0,
				       (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	 g_object_class_install_property
		 (gobject_class,
		  PROP_WAVE_COLOR,
		  g_param_spec_uint ("wave_color", NULL, NULL,
				     0, G_MAXUINT, 0,
				     (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	 g_object_class_install_property
		 (gobject_class,
		  PROP_CLIP_COLOR,
		  g_param_spec_uint ("clip_color", NULL, NULL,
				     0, G_MAXUINT, 0,
				     (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	 g_object_class_install_property
		 (gobject_class,
		  PROP_ZERO_COLOR,
		  g_param_spec_uint ("zero_color", NULL, NULL,
				     0, G_MAXUINT, 0,
				     (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	 g_object_class_install_property
		 (gobject_class,
		  PROP_FILL_COLOR,
		  g_param_spec_uint ("fill_color", NULL, NULL,
				     0, G_MAXUINT, 0,
				     (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	 g_object_class_install_property
		 (gobject_class,
		  PROP_FILLED,
		  g_param_spec_boolean ("filled", NULL, NULL,
					FALSE,
					(G_PARAM_READABLE | G_PARAM_WRITABLE)));

	 g_object_class_install_property
		 (gobject_class,
		  PROP_RECTIFIED,
		  g_param_spec_boolean ("rectified", NULL, NULL,
					FALSE,
					(G_PARAM_READABLE | G_PARAM_WRITABLE)));

	 g_object_class_install_property
		 (gobject_class,
		  PROP_ZERO_LINE,
		  g_param_spec_boolean ("zero_line", NULL, NULL,
					FALSE,
					(G_PARAM_READABLE | G_PARAM_WRITABLE)));

	 g_object_class_install_property
		 (gobject_class,
		  PROP_LOGSCALED,
		  g_param_spec_boolean ("logscaled", NULL, NULL,
					FALSE,
					(G_PARAM_READABLE | G_PARAM_WRITABLE)));

	 g_object_class_install_property
		 (gobject_class,
		  PROP_REGION_START,
		  g_param_spec_uint ("region_start", NULL, NULL,
				     0, G_MAXUINT, 0,
				     (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	 object_class->destroy = gnome_canvas_waveview_destroy;

	 item_class->update = gnome_canvas_waveview_update;
	 item_class->bounds = gnome_canvas_waveview_bounds;
	 item_class->point = gnome_canvas_waveview_point;
	 item_class->render = gnome_canvas_waveview_render;
	 item_class->draw = gnome_canvas_waveview_draw;
}

void 
gnome_canvas_waveview_set_gradient_waveforms (int yn)
{
	_gradient_rendering = yn;
}

GnomeCanvasWaveViewCache*
gnome_canvas_waveview_cache_new ()
{
	GnomeCanvasWaveViewCache *c;

	c = g_malloc (sizeof (GnomeCanvasWaveViewCache));

	c->allocated = 2048;
	c->data = g_malloc (sizeof (GnomeCanvasWaveViewCacheEntry) * c->allocated);
	c->data_size = 0;
	c->start = 0;
	c->end = 0;

	return c;
}

void
gnome_canvas_waveview_cache_destroy (GnomeCanvasWaveViewCache* cache)
{
	g_free (cache->data);
	g_free (cache);
}

static void
gnome_canvas_waveview_init (GnomeCanvasWaveView *waveview)
{
	waveview->x = 0.0;
	waveview->y = 0.0;
	waveview->cache = 0;
	waveview->cache_updater = FALSE;
	waveview->data_src = NULL;
	waveview->channel = 0;
	waveview->peak_function = NULL;
	waveview->length_function = NULL;
	waveview->sourcefile_length_function = NULL;
	waveview->gain_curve_function = NULL;
	waveview->gain_src = NULL;
	waveview->rectified = FALSE;
	waveview->logscaled = FALSE;
	waveview->filled = TRUE;
	waveview->zero_line = FALSE;
	waveview->region_start = 0;
	waveview->samples_per_unit = 1.0;
	waveview->amplitude_above_axis = 1.0;
	waveview->height = 100.0;
	waveview->screen_width = gdk_screen_width ();
	waveview->reload_cache_in_render = FALSE;

 	waveview->wave_color = 0;
 	waveview->clip_color = 0;
 	waveview->zero_color = 0;
 	waveview->fill_color = 0;
}

static void
gnome_canvas_waveview_destroy (GtkObject *object)
{
	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_WAVEVIEW (object));

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

#define DEBUG_CACHE 0
#undef CACHE_MEMMOVE_OPTIMIZATION

/** @return cache index of start_sample within the cache */
static guint32
gnome_canvas_waveview_ensure_cache (GnomeCanvasWaveView *waveview, gulong start_sample, gulong end_sample)
{
	gulong required_cache_entries;
	gulong rf1, rf2,rf3, required_frames;
	gulong new_cache_start, new_cache_end;
	gulong half_width;
	gulong npeaks;
	gulong offset;
	gulong ostart;
	gulong copied;
	GnomeCanvasWaveViewCache *cache;
	float* gain;
#ifdef CACHE_MEMMOVE_OPTIMIZATION
	gulong present_frames;
	gulong present_entries;
#endif

	cache = waveview->cache;

	start_sample = start_sample + waveview->region_start;
	end_sample = end_sample + waveview->region_start;
#if DEBUG_CACHE
	// printf("waveview->region_start == %lu\n",waveview->region_start);
	// c_stacktrace ();
	printf ("\n\n=> 0x%x cache @ 0x%x range: %lu - %lu request: %lu - %lu (%lu frames)\n",
		waveview, cache,
		cache->start, cache->end,
		start_sample, end_sample, end_sample - start_sample);
#endif

	if (cache->start <= start_sample && cache->end >= end_sample) {
#if DEBUG_CACHE
		// printf ("0x%x: cache hit for %lu-%lu (cache holds: %lu-%lu\n",
		// waveview, start_sample, end_sample, cache->start, cache->end);
#endif
		goto out;
	}

	/* make sure the cache is at least twice as wide as the screen width, and put the start sample
	   in the middle, ensuring that we cover the end_sample.
	*/

	/* Note the assumption that we have a 1:1 units:pixel ratio for the canvas. Its everywhere ... */

	half_width = (gulong) floor ((waveview->screen_width * waveview->samples_per_unit)/2.0 + 0.5);

	if (start_sample < half_width) {
		new_cache_start = 0;
	} else {
		new_cache_start = start_sample - half_width;
	}

	/* figure out how many frames we want */

	rf1 = end_sample - start_sample + 1;
	rf2 = (gulong) floor ((waveview->screen_width * waveview->samples_per_unit * 2.0f));
	required_frames = MAX(rf1,rf2);

	/* but make sure it doesn't extend beyond the end of the source material */

	rf3 = (gulong) (waveview->sourcefile_length_function (waveview->data_src, waveview->samples_per_unit)) + 1;
	if (rf3 < new_cache_start) {
		rf3 = 0;
	} else {
		rf3 -= new_cache_start;
	}

#if DEBUG_CACHE
	fprintf (stderr, "AVAILABLE FRAMES = %lu of %lu, start = %lu, sstart = %lu, cstart = %lu\n",
		 rf3, waveview->sourcefile_length_function (waveview->data_src, waveview->samples_per_unit),
		 waveview->region_start, start_sample, new_cache_start);
#endif

	required_frames = MIN(required_frames,rf3);

	new_cache_end = new_cache_start + required_frames - 1;

	required_cache_entries = (gulong) floor (required_frames / waveview->samples_per_unit );

#if DEBUG_CACHE
	fprintf (stderr, "new cache = %lu - %lu\n", new_cache_start, new_cache_end);
	fprintf(stderr,"required_cach_entries = %lu, samples_per_unit = %f req frames = %lu\n",
		required_cache_entries,waveview->samples_per_unit, required_frames);
#endif

	if (required_cache_entries > cache->allocated) {
		cache->data = g_realloc (cache->data, sizeof (GnomeCanvasWaveViewCacheEntry) * required_cache_entries);
		cache->allocated = required_cache_entries;
		// cache->start = 0;
		// cache->end = 0;
	}

	ostart = new_cache_start;

#ifdef CACHE_MEMMOVE_OPTIMIZATION

	/* data is not entirely in the cache, so go fetch it, making sure to fill the cache */

	/* some of the required cache entries are in the cache, but in the wrong
	   locations. use memmove to fix this.
	*/

	if (cache->start < new_cache_start && new_cache_start < cache->end) {

		/* case one: the common area is at the end of the existing cache. move it
		   to the beginning of the cache, and set up to refill whatever remains.


			   wv->cache_start                                        wv->cache_end
			   |-------------------------------------------------------| cache
			                                       |--------------------------------| requested
			                                       <------------------->
			                                             "present"
			                                    new_cache_start                      new_cache_end
		*/


		present_frames = cache->end - new_cache_start;
		present_entries = (gulong) floor (present_frames / waveview->samples_per_unit);

#if DEBUG_CACHE
		fprintf (stderr, "existing material at end of current cache, move to start of new cache\n"
			 "\tcopy from %lu to start\n", cache->data_size - present_entries);
#endif

		memmove (&cache->data[0],
			 &cache->data[cache->data_size - present_entries],
			 present_entries * sizeof (GnomeCanvasWaveViewCacheEntry));

#if DEBUG_CACHE
		fprintf (stderr, "satisfied %lu of %lu frames, offset = %lu, will start at %lu (ptr = 0x%x)\n",
			 present_frames, required_frames, present_entries, new_cache_start + present_entries,
			 cache->data + present_entries);
#endif

		copied = present_entries;
		offset = present_entries;
		new_cache_start += present_frames;
		required_frames -= present_frames;

	} else if (new_cache_end > cache->start && new_cache_end < cache->end) {

		/* case two: the common area lives at the beginning of the existing cache.

                                            wv->cache_start                                      wv->cache_end
			                     |-----------------------------------------------------|
                              |--------------------------------|
                                             <----------------->
					        "present"

                             new_cache_start                      new_cache_end
		*/

		present_frames = new_cache_end - cache->start;
		present_entries = (gulong) floor (present_frames / waveview->samples_per_unit);

		memmove (&cache->data[cache->data_size - present_entries],
			 &cache->data[0],
			 present_entries * sizeof (GnomeCanvasWaveViewCacheEntry));

#if DEBUG_CACHE
		fprintf (stderr, "existing material at start of current cache, move to start of end cache\n");
#endif

#if DEBUG_CACHE
		fprintf (stderr, "satisfied %lu of %lu frames, offset = %lu, will start at %lu (ptr = 0x%x)\n",
			 present_entries, required_frames, present_entries, new_cache_start + present_entries,
			 cache->data + present_entries);
#endif

		copied = present_entries;
		offset = 0;
		required_frames -= present_frames;


	} else {
		copied = 0;
		offset = 0;

	}

#else
	copied = 0;
	offset = 0;

#endif /* CACHE_MEMMOVE_OPTIMIZATION */

//	fprintf(stderr,"length == %lu\n",waveview->length_function (waveview->data_src));
//	required_frames = MIN (waveview->length_function (waveview->data_src) - new_cache_start, required_frames);

	npeaks = (gulong) floor (required_frames / waveview->samples_per_unit);
	required_frames = npeaks * waveview->samples_per_unit;

#if DEBUG_CACHE


	printf ("requesting %lu/%f to cover %lu-%lu at %f spu (request was %lu-%lu) into cache + %lu\n",
		required_frames, required_frames/waveview->samples_per_unit, new_cache_start, new_cache_end,
		waveview->samples_per_unit, start_sample, end_sample, offset);
#endif

#if DEBUG_CACHE
//	printf ("cache holds %lu entries, requesting %lu to cover %lu-%lu (request was %lu-%lu)\n",
//		cache->data_size, npeaks, new_cache_start, new_cache_end,
//		start_sample, end_sample);
#endif

	if (required_frames) {
		waveview->peak_function (waveview->data_src, npeaks, new_cache_start, required_frames, cache->data + offset, waveview->channel,waveview->samples_per_unit);

		/* take into account any copied peaks */

		npeaks += copied;
	} else {
		npeaks = copied;
	}

	if (npeaks < cache->allocated) {
#if DEBUG_CACHE
		fprintf (stderr, "zero fill cache for %lu at %lu\n", cache->allocated - npeaks, npeaks);
#endif
		memset (&cache->data[npeaks], 0, sizeof (GnomeCanvasWaveViewCacheEntry) * (cache->allocated - npeaks));
		cache->data_size = npeaks;
	} else {
		cache->data_size = cache->allocated;
	}

	if (waveview->gain_curve_function) {
		guint32 n;

		gain = (float*) malloc (sizeof (float) * cache->data_size);

		waveview->gain_curve_function (waveview->gain_src, new_cache_start, new_cache_end, gain, cache->data_size);

		for (n = 0; n < cache->data_size; ++n) {
			cache->data[n].min *= gain[n];
			cache->data[n].max *= gain[n];
		}

		free (gain);

	}

	/* do optional log scaling.  this implementation is not particularly efficient */

	if (waveview->logscaled) {
		guint32 n;
		GnomeCanvasWaveViewCacheEntry* buf = cache->data;

		for (n = 0; n < cache->data_size; ++n) {

			if (buf[n].max > 0.0f) {
				buf[n].max = alt_log_meter(fast_coefficient_to_dB(buf[n].max));
			} else if (buf[n].max < 0.0f) {
				buf[n].max = -alt_log_meter(fast_coefficient_to_dB(-buf[n].max));
			}

			if (buf[n].min > 0.0f) {
				buf[n].min = alt_log_meter(fast_coefficient_to_dB(buf[n].min));
			} else if (buf[n].min < 0.0f) {
				buf[n].min = -alt_log_meter(fast_coefficient_to_dB(-buf[n].min));
			}
		}
	}

	cache->start = ostart;
	cache->end = new_cache_end;

  out:
#if DEBUG_CACHE
	fprintf (stderr, "return cache index = %d\n",
		 (guint32) floor ((((double) (start_sample - cache->start)) / waveview->samples_per_unit) + 0.5));
#endif
	return (guint32) floor ((((double) (start_sample - cache->start)) / waveview->samples_per_unit) + 0.5);

}

void
gnome_canvas_waveview_set_data_src (GnomeCanvasWaveView *waveview, void *data_src)
{

	if (waveview->cache_updater) {
		if (waveview->data_src == data_src) {
			waveview->reload_cache_in_render = TRUE;
			return;
		}

		waveview->cache->start  = 0;
		waveview->cache->end = 0;
	}

	waveview->data_src = data_src;
}

void
gnome_canvas_waveview_set_channel (GnomeCanvasWaveView *waveview, guint32 chan)
{
	if (waveview->channel == chan) {
		return;
	}

	waveview->channel = chan;
}

static void
gnome_canvas_waveview_reset_bounds (GnomeCanvasItem *item)

{
	double x1, x2, y1, y2;
	ArtPoint i1, i2;
	ArtPoint w1, w2;
	int Ix1, Ix2, Iy1, Iy2;
	double i2w[6];

	gnome_canvas_waveview_bounds (item, &x1, &y1, &x2, &y2);

	i1.x = x1;
	i1.y = y1;
	i2.x = x2;
	i2.y = y2;

	gnome_canvas_item_i2w_affine (item, i2w);
	art_affine_point (&w1, &i1, i2w);
	art_affine_point (&w2, &i2, i2w);

	Ix1 = (int) rint(w1.x);
	Ix2 = (int) rint(w2.x);
	Iy1 = (int) rint(w1.y);
	Iy2 = (int) rint(w2.y);

	gnome_canvas_update_bbox (item, Ix1, Iy1, Ix2, Iy2);
}

/*
 * CANVAS CALLBACKS
 */

static void
gnome_canvas_waveview_set_property (GObject      *object,
				    guint         prop_id,
				    const GValue *value,
				    GParamSpec   *pspec)

{
	(void) pspec;

	GnomeCanvasItem *item;
	GnomeCanvasWaveView *waveview;
	int redraw = FALSE;
	int calc_bounds = FALSE;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_WAVEVIEW (object));

	item = GNOME_CANVAS_ITEM (object);
	waveview = GNOME_CANVAS_WAVEVIEW (object);

	void * ptr;
	switch (prop_id) {
	case PROP_DATA_SRC:
		gnome_canvas_waveview_set_data_src (waveview, g_value_get_pointer(value));
		redraw = TRUE;
		break;

	case PROP_CHANNEL:
		gnome_canvas_waveview_set_channel (waveview, g_value_get_uint(value));
		redraw = TRUE;
		break;

	case PROP_LENGTH_FUNCTION:
		ptr = g_value_get_pointer(value);
		waveview->length_function = POSIX_FUNC_PTR_CAST(waveview_length_function_t, ptr);
		redraw = TRUE;
		break;

	case PROP_SOURCEFILE_LENGTH_FUNCTION:
		ptr = g_value_get_pointer(value);
		waveview->sourcefile_length_function = POSIX_FUNC_PTR_CAST(waveview_sourcefile_length_function_t, ptr);
		redraw = TRUE;
		break;

	case PROP_PEAK_FUNCTION:
		ptr = g_value_get_pointer(value);
		waveview->peak_function = POSIX_FUNC_PTR_CAST(waveview_peak_function_t, ptr);
		redraw = TRUE;
		break;

	case PROP_GAIN_FUNCTION:
		ptr = g_value_get_pointer(value);
		waveview->gain_curve_function = POSIX_FUNC_PTR_CAST(waveview_gain_curve_function_t, ptr);
			 redraw = TRUE;
		break;

	case PROP_GAIN_SRC:
		waveview->gain_src = g_value_get_pointer(value);
		if (waveview->cache_updater) {
			waveview->cache->start = 0;
			waveview->cache->end = 0;
		}
		redraw = TRUE;
		calc_bounds = TRUE;
		break;

	case PROP_CACHE:
		waveview->cache = g_value_get_pointer(value);
		redraw = TRUE;
		break;


	case PROP_CACHE_UPDATER:
		waveview->cache_updater = g_value_get_boolean(value);
		redraw = TRUE;
		break;

	case PROP_SAMPLES_PER_UNIT:
		if ((waveview->samples_per_unit = g_value_get_double(value)) < 1.0) {
			waveview->samples_per_unit = 1.0;
		}
		if (waveview->cache_updater) {
			waveview->cache->start = 0;
			waveview->cache->end = 0;
		}
		redraw = TRUE;
		calc_bounds = TRUE;
		break;

	case PROP_AMPLITUDE_ABOVE_AXIS:
		waveview->amplitude_above_axis = g_value_get_double(value);
		redraw = TRUE;
		break;

	case PROP_X:
	        if (waveview->x != g_value_get_double (value)) {
		        waveview->x = g_value_get_double (value);
			calc_bounds = TRUE;
		}
		break;

	case PROP_Y:
	        if (waveview->y != g_value_get_double (value)) {
		        waveview->y = g_value_get_double (value);
			calc_bounds = TRUE;
		}
		break;

	case PROP_HEIGHT:
	        if (waveview->height != fabs (g_value_get_double (value))) {
		        waveview->height = fabs (g_value_get_double (value));
			redraw = TRUE;
		}
		break;

	case PROP_WAVE_COLOR:
		if (waveview->wave_color != g_value_get_uint(value)) {
		        waveview->wave_color = g_value_get_uint(value);
			redraw = TRUE;
		}
		break;

	case PROP_CLIP_COLOR:
		if (waveview->clip_color != g_value_get_uint(value)) {
		        waveview->clip_color = g_value_get_uint(value);
			redraw = TRUE;
		}
		break;

	case PROP_ZERO_COLOR:
		if (waveview->zero_color != g_value_get_uint(value)) {
		        waveview->zero_color = g_value_get_uint(value);
			redraw = TRUE;
		}
		break;

	case PROP_FILL_COLOR:
		if (waveview->fill_color != g_value_get_uint(value)) {
		        waveview->fill_color = g_value_get_uint(value);
			redraw = TRUE;
		}
		break;

	case PROP_FILLED:
		if (waveview->filled != g_value_get_boolean(value)) {
			waveview->filled = g_value_get_boolean(value);
			redraw = TRUE;
		}
		break;

	case PROP_RECTIFIED:
		if (waveview->rectified != g_value_get_boolean(value)) {
			waveview->rectified = g_value_get_boolean(value);
			redraw = TRUE;
		}
		break;

	case PROP_ZERO_LINE:
		if (waveview->zero_line != g_value_get_boolean(value)) {
			waveview->zero_line = g_value_get_boolean(value);
			redraw = TRUE;
		}
		break;

	case PROP_LOGSCALED:
		if (waveview->logscaled != g_value_get_boolean(value)) {
			waveview->logscaled = g_value_get_boolean(value);
			if (waveview->cache_updater) {
				waveview->cache->start = 0;
				waveview->cache->end = 0;
			}
			redraw = TRUE;
			calc_bounds = TRUE;
		}
		break;
	case PROP_REGION_START:
		waveview->region_start = g_value_get_uint(value);
		redraw = TRUE;
		calc_bounds = TRUE;
		break;


	default:
		break;
	}

	if (calc_bounds) {
		gnome_canvas_waveview_reset_bounds (item);
	}

	if (redraw) {
		gnome_canvas_item_request_update (item);
	}

}

static void
gnome_canvas_waveview_get_property (
		GObject      *object,
		guint         prop_id,
		GValue       *value,
		GParamSpec   *pspec)
{


	g_return_if_fail (object != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_WAVEVIEW (object));

	GnomeCanvasWaveView *waveview = GNOME_CANVAS_WAVEVIEW (object);

	switch (prop_id) {
	case PROP_DATA_SRC:
		g_value_set_pointer(value, waveview->data_src);
		break;

	case PROP_CHANNEL:
		g_value_set_uint(value, waveview->channel);
		break;

	case PROP_LENGTH_FUNCTION:
		g_value_set_pointer(value, POSIX_FUNC_PTR_CAST(void*, waveview->length_function));
		break;

	case PROP_SOURCEFILE_LENGTH_FUNCTION:
		g_value_set_pointer(value, POSIX_FUNC_PTR_CAST(void*, waveview->sourcefile_length_function));
		break;

	case PROP_PEAK_FUNCTION:
		g_value_set_pointer(value, POSIX_FUNC_PTR_CAST(void*, waveview->peak_function));
		break;

	case PROP_GAIN_FUNCTION:
		g_value_set_pointer(value, POSIX_FUNC_PTR_CAST(void*, waveview->gain_curve_function));
		break;

	case PROP_GAIN_SRC:
		g_value_set_pointer(value, waveview->gain_src);
		break;

	case PROP_CACHE:
		g_value_set_pointer(value, waveview->cache);
		break;

	case PROP_CACHE_UPDATER:
		g_value_set_boolean(value, waveview->cache_updater);
		break;

	case PROP_SAMPLES_PER_UNIT:
		g_value_set_double(value, waveview->samples_per_unit);
		break;

	case PROP_AMPLITUDE_ABOVE_AXIS:
		g_value_set_double(value, waveview->amplitude_above_axis);
		break;

	case PROP_X:
		g_value_set_double (value, waveview->x);
		break;

	case PROP_Y:
		g_value_set_double (value, waveview->y);
		break;

	case PROP_HEIGHT:
		g_value_set_double (value, waveview->height);
		break;

	case PROP_WAVE_COLOR:
		g_value_set_uint (value, waveview->wave_color);
		break;

	case PROP_CLIP_COLOR:
		g_value_set_uint (value, waveview->clip_color);
		break;

	case PROP_ZERO_COLOR:
		g_value_set_uint (value, waveview->zero_color);
		break;

	case PROP_FILL_COLOR:
		g_value_set_uint (value, waveview->fill_color);
		break;

	case PROP_FILLED:
		g_value_set_boolean (value, waveview->filled);
		break;

	case PROP_RECTIFIED:
		g_value_set_boolean (value, waveview->rectified);
		break;

	case PROP_ZERO_LINE:
		g_value_set_boolean (value, waveview->zero_line);
		break;

	case PROP_LOGSCALED:
		g_value_set_boolean (value, waveview->logscaled);
		break;

	case PROP_REGION_START:
		g_value_set_uint (value, waveview->region_start);
		break;

	default:
	        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gnome_canvas_waveview_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags)
{
	GnomeCanvasWaveView *waveview;
	double x, y;

	waveview = GNOME_CANVAS_WAVEVIEW (item);

//	check_cache (waveview, "start of update");

	if (parent_class->update)
		(* parent_class->update) (item, affine, clip_path, flags);

	gnome_canvas_waveview_reset_bounds (item);

	/* get the canvas coordinates of the view. Do NOT use affines
	   for this, because they do not round to the integer units used
	   by the canvas, resulting in subtle pixel-level errors later.
	*/

	x = waveview->x;
	y = waveview->y;

	gnome_canvas_item_i2w (item, &x, &y);
	gnome_canvas_w2c (GNOME_CANVAS(item->canvas), x, y, &waveview->bbox_ulx, &waveview->bbox_uly);

	waveview->samples = waveview->length_function (waveview->data_src);

	x = waveview->x + (waveview->samples / waveview->samples_per_unit);
	y = waveview->y + waveview->height;

	gnome_canvas_item_i2w (item, &x, &y);
	gnome_canvas_w2c (GNOME_CANVAS(item->canvas), x, y, &waveview->bbox_lrx, &waveview->bbox_lry);

	/* cache the half-height and the end point in canvas units */

	waveview->half_height = waveview->height / 2.0;

	/* parse the color */

	UINT_TO_RGBA (waveview->wave_color, &waveview->wave_r, &waveview->wave_g, &waveview->wave_b,
		      &waveview->wave_a);
	UINT_TO_RGBA (waveview->clip_color, &waveview->clip_r, &waveview->clip_g, &waveview->clip_b,
		      &waveview->clip_a);
	UINT_TO_RGBA (waveview->fill_color, &waveview->fill_r, &waveview->fill_g, &waveview->fill_b,
		      &waveview->fill_a);

//	check_cache (waveview, "end of update");
}

static void
gnome_canvas_waveview_gradient_render (GnomeCanvasItem *item,
				       GnomeCanvasBuf *buf)
{
	GnomeCanvasWaveView *waveview;
	gulong s1, s2;
	int clip_length = 0;
	int pymin, pymax;
	guint cache_index;
	double half_height;
	int x;
	char rectify;

	waveview = GNOME_CANVAS_WAVEVIEW (item);

//	check_cache (waveview, "start of render");

	if (parent_class->render) {
		(*parent_class->render) (item, buf);
	}

	if (buf->is_bg) {
		gnome_canvas_buf_ensure_buf (buf);
		buf->is_bg = FALSE;
	}

	/* a "unit" means a pixel */

	/* begin: render start x (units) */
	int const begin = MAX (waveview->bbox_ulx, buf->rect.x0);

        /* zbegin: start x for zero line (units) */
	int const zbegin = (begin == waveview->bbox_ulx) ? (begin + 1) : begin;

	/* end: render end x (units) */
	int const end = (waveview->bbox_lrx >= 0) ? MIN (waveview->bbox_lrx,buf->rect.x1) : buf->rect.x1;

	/* zend: end x for zero-line (units) */
	int const zend = (end == waveview->bbox_lrx) ? (end - 1) : end;

	if (begin == end) {
		return;
	}

	/* s1: start sample
	   s2: end sample
	*/

	s1 = floor ((begin - waveview->bbox_ulx) * waveview->samples_per_unit);

	// fprintf (stderr, "0x%x begins at sample %f\n", waveview, waveview->bbox_ulx * waveview->samples_per_unit);

	if (end == waveview->bbox_lrx) {
		/* This avoids minor rounding errors when we have the
		   entire region visible.
		*/
		s2 = waveview->samples;
	} else {
		s2 = s1 + floor ((end - begin) * waveview->samples_per_unit);
	}

#if 0
	printf ("0x%x r (%d..%d)(%d..%d) bbox (%d..%d)(%d..%d)"
		" b/e %d..%d s= %lu..%lu @ %f\n",
		waveview,
		buf->rect.x0,
		buf->rect.x1,
		buf->rect.y0,
		buf->rect.y1,
		waveview->bbox_ulx,
		waveview->bbox_lrx,
		waveview->bbox_uly,
		waveview->bbox_lry,
		begin, end, s1, s2,
		waveview->samples_per_unit);
#endif

	/* now ensure that the cache is full and properly
	   positioned.
	*/

//	check_cache (waveview, "pre-ensure");

	if (waveview->cache_updater && waveview->reload_cache_in_render) {
		waveview->cache->start = 0;
		waveview->cache->end = 0;
		waveview->reload_cache_in_render = FALSE;
	}

//	check_cache (waveview, "post-ensure");

	/* don't rectify at single-sample zoom */
	if (waveview->rectified && waveview->samples_per_unit > 1) {
		rectify = TRUE;
	}
	else {
		rectify = FALSE;
	}

	clip_length = MIN(5,(waveview->height/4));

	/*
	   Now draw each line, clipping it appropriately. The clipping
	   is done by the macros PAINT_FOO().
	*/

	half_height = waveview->half_height;

/* this makes it slightly easier to comprehend whats going on */
#define origin half_height

	if (waveview->filled && !rectify) {
		int prev_pymin = 1;
		int prev_pymax = 0;
		int last_pymin = 1;
		int last_pymax = 0;
		int next_pymin, next_pymax;
		double max, min;
		int next_clip_max = 0;
		int next_clip_min = 0;

		int wave_middle = (int) rint ((item->y1 + origin) * item->canvas->pixels_per_unit);
		int wave_top = (int) rint ((item->y1) * item->canvas->pixels_per_unit);

		if (s1 < waveview->samples_per_unit) {
			/* we haven't got a prev vars to compare with, so outline the whole line here */
			prev_pymax = (int) rint ((item->y1 + origin) * item->canvas->pixels_per_unit);
			prev_pymin = prev_pymax;
		}
		else {
			s1 -= waveview->samples_per_unit;
		}

		if(end == waveview->bbox_lrx) {
			/* we don't have the NEXT vars for the last sample */
			last_pymax = (int) rint ((item->y1 + origin) * item->canvas->pixels_per_unit);
			last_pymin = last_pymax;
		}
		else {
			s2 += waveview->samples_per_unit;
		}

		cache_index = gnome_canvas_waveview_ensure_cache (waveview, s1, s2);

		/*
		 * Compute the variables outside the rendering rect
		 */
		if(prev_pymax != prev_pymin) {

			prev_pymax = (int) rint ((item->y1 + origin - MIN(waveview->cache->data[cache_index].max, 1.0) * half_height) * item->canvas->pixels_per_unit);
			prev_pymin = (int) rint ((item->y1 + origin - MAX(waveview->cache->data[cache_index].min, -1.0) * half_height) * item->canvas->pixels_per_unit);
			++cache_index;
		}
		if(last_pymax != last_pymin) {
			/* take the index of one sample right of what we render */
			guint index = cache_index + (end - begin);

			if (index >= waveview->cache->data_size) {

				/* the data we want is off the end of the cache, which must mean its beyond
				   the end of the region's source; hence the peak values are 0 */
				last_pymax = (int) rint ((item->y1 + origin) * item->canvas->pixels_per_unit);
				last_pymin = (int) rint ((item->y1 + origin) * item->canvas->pixels_per_unit);

			} else {

				last_pymax = (int) rint ((item->y1 + origin - MIN(waveview->cache->data[index].max, 1.0) * half_height) * item->canvas->pixels_per_unit);
				last_pymin = (int) rint ((item->y1 + origin - MAX(waveview->cache->data[index].min, -1.0) * half_height) * item->canvas->pixels_per_unit);

			}

		}

		/*
		 * initialize NEXT* variables for the first run, duplicated in the loop for speed
		 */
		max = waveview->cache->data[cache_index].max;
		min = waveview->cache->data[cache_index].min;

		if (max >= 1.0) {
			max = 1.0;
			next_clip_max = 1;
		}

		if (min <= -1.0) {
			min = -1.0;
			next_clip_min = 1;
		}

		max *= half_height;
		min *= half_height;

		next_pymax = (int) rint ((item->y1 + origin - max) * item->canvas->pixels_per_unit);
		next_pymin = (int) rint ((item->y1 + origin - min) * item->canvas->pixels_per_unit);

		/*
		 * And now the loop
		 */
		for(x = begin; x < end; ++x) {
			int clip_max = next_clip_max;
			int clip_min = next_clip_min;
			int fill_max, fill_min;

			pymax = next_pymax;
			pymin = next_pymin;

			/* compute next */
			if(x == end - 1) {
				/*next is now the last column, which is outside the rendering rect, and possibly outside the region*/
				next_pymax = last_pymax;
				next_pymin = last_pymin;
			}
			else {
				++cache_index;

				if (cache_index < waveview->cache->data_size) {
					max = waveview->cache->data[cache_index].max;
					min = waveview->cache->data[cache_index].min;
				} else {
					max = min = 0;
				}

				next_clip_max = 0;
				next_clip_min = 0;

				if (max >= 1.0) {
					max = 1.0;
					next_clip_max = 1;
				}

				if (min <= -1.0) {
					min = -1.0;
					next_clip_min = 1;
				}

				max *= half_height;
				min *= half_height;

				next_pymax = (int) rint ((item->y1 + origin - max) * item->canvas->pixels_per_unit);
				next_pymin = (int) rint ((item->y1 + origin - min) * item->canvas->pixels_per_unit);
			}

			/* render */
			if (pymax == pymin) {
				PAINT_DOTA(buf, waveview->wave_r, waveview->wave_g, waveview->wave_b, waveview->wave_a, x, pymin);
			} else {
				if((prev_pymax < pymax && next_pymax < pymax) ||
				   (prev_pymax == pymax && next_pymax == pymax)) {
					fill_max = pymax + 1;
					PAINT_DOTA(buf, waveview->wave_r, waveview->wave_g, waveview->wave_b, waveview->wave_a, x, pymax);
				}
				else {
					fill_max = MAX(prev_pymax, next_pymax);
					if(pymax == fill_max) {
						PAINT_DOTA(buf, waveview->wave_r, waveview->wave_g, waveview->wave_b, waveview->wave_a, x, pymax);
						++fill_max;
					} else {
						PAINT_VERTA_GR(buf, waveview->wave_r, waveview->wave_g, waveview->wave_b, waveview->wave_a, x, pymax, fill_max, wave_middle, wave_top);
					}
					
				}

				if((prev_pymin > pymin && next_pymin > pymin) ||
				   (prev_pymin == pymin && next_pymin == pymin)) {
					fill_min = pymin - 1;
					PAINT_DOTA(buf, waveview->wave_r, waveview->wave_g, waveview->wave_b, waveview->wave_a, x, pymin-1);
				}
				else {
					fill_min = MIN(prev_pymin, next_pymin);
					if(pymin == fill_min) {
						PAINT_DOTA(buf, waveview->wave_r, waveview->wave_g, waveview->wave_b, waveview->wave_a, x, pymin);
					}
					else {
						PAINT_VERTA_GR(buf, waveview->wave_r, waveview->wave_g, waveview->wave_b, waveview->wave_a, x, fill_min, pymin, wave_middle, wave_top);
					}
				}

				if(fill_max < fill_min) {
					PAINT_VERTA_GR(buf, waveview->fill_r, waveview->fill_g, waveview->fill_b, waveview->fill_a, x, fill_max, fill_min, wave_middle, wave_top);
				}
				else if(fill_max == fill_min) {
					PAINT_DOTA(buf, waveview->fill_r, waveview->fill_g, waveview->fill_b, waveview->fill_a, x, fill_max);
				}
			}

			if (clip_max) {
				PAINT_VERTA(buf, waveview->clip_r, waveview->clip_g, waveview->clip_b, waveview->clip_a, x, pymax, pymax + clip_length);
				PAINT_VERTA(buf, waveview->clip_r, waveview->clip_g, waveview->clip_b, waveview->clip_a >> 1, x + 1, pymax, pymax + (clip_length -1));
				PAINT_VERTA(buf, waveview->clip_r, waveview->clip_g, waveview->clip_b, waveview->clip_a >> 1, x - 1, pymax, pymax + (clip_length - 1));

			}

			if (clip_min) {
				PAINT_VERTA(buf, waveview->clip_r, waveview->clip_g, waveview->clip_b, waveview->clip_a , x, pymin - clip_length, pymin);
				PAINT_VERTA(buf, waveview->clip_r, waveview->clip_g, waveview->clip_b, waveview->clip_a >> 1, x + 1, pymin - (clip_length - 1), pymin);
				PAINT_VERTA(buf, waveview->clip_r, waveview->clip_g, waveview->clip_b, waveview->clip_a >> 1, x - 1, pymin - (clip_length - 1), pymin);
			}

			prev_pymax = pymax;
			prev_pymin = pymin;
		}

	} else if (waveview->filled && rectify) {

		int prev_pymax = -1;
		int last_pymax = -1;
		int next_pymax;
		double max, min;
		int next_clip_max = 0;
		int next_clip_min = 0;

		int wave_middle = (int) rint ((item->y1 + waveview->height) * item->canvas->pixels_per_unit);
		int wave_top = (int) rint ((item->y1) * item->canvas->pixels_per_unit);

		// for rectified, this stays constant throughout the loop
		pymin = (int) rint ((item->y1 + waveview->height) * item->canvas->pixels_per_unit);

		if(s1 < waveview->samples_per_unit) {
			/* we haven't got a prev vars to compare with, so outline the whole line here */
			prev_pymax = pymin;
		}
		else {
			s1 -= waveview->samples_per_unit;
		}

		if(end == waveview->bbox_lrx) {
			/* we don't have the NEXT vars for the last sample */
			last_pymax = pymin;
		}
		else {
			s2 += waveview->samples_per_unit;
		}

		cache_index = gnome_canvas_waveview_ensure_cache (waveview, s1, s2);

		/*
		 * Compute the variables outside the rendering rect
		 */
		if(prev_pymax < 0) {
			max = MIN(waveview->cache->data[cache_index].max, 1.0);
			min = MAX(waveview->cache->data[cache_index].min, -1.0);

			if (fabs (min) > fabs (max)) {
				max = fabs (min);
			}

			prev_pymax = (int) rint ((item->y1 + waveview->height - max * waveview->height) * item->canvas->pixels_per_unit);
			++cache_index;
		}
		if(last_pymax < 0) {
			/* take the index of one sample right of what we render */
			int index = cache_index + (end - begin);

			max = MIN(waveview->cache->data[index].max, 1.0);
			min = MAX(waveview->cache->data[index].min, -1.0);

			if (fabs (min) > fabs (max)) {
				max = fabs (min);
			}

			last_pymax = (int) rint ((item->y1 + waveview->height - max * waveview->height) * item->canvas->pixels_per_unit);
		}

		/*
		 * initialize NEXT* variables for the first run, duplicated in the loop for speed
		 */
		max = waveview->cache->data[cache_index].max;
		min = waveview->cache->data[cache_index].min;

		if (max >= 1.0) {
			max = 1.0;
			next_clip_max = 1;
		}

		if (min <= -1.0) {
			min = -1.0;
			next_clip_min = 1;
		}

		if (fabs (min) > fabs (max)) {
			max = fabs (min);
		}

		next_pymax = (int) rint ((item->y1 + waveview->height - max * waveview->height) * item->canvas->pixels_per_unit);

		/*
		 * And now the loop
		 */
		for(x = begin; x < end; ++x) {
			int clip_max = next_clip_max;
			int clip_min = next_clip_min;
			int fill_max;

			pymax = next_pymax;

			/* compute next */
			if(x == end - 1) {
				/*next is now the last column, which is outside the rendering rect, and possibly outside the region*/
				next_pymax = last_pymax;
			}
			else {
				++cache_index;

				max = waveview->cache->data[cache_index].max;
				min = waveview->cache->data[cache_index].min;

				if (max >= 1.0) {
					max = 1.0;
					next_clip_max = 1;
				}

				if (min <= -1.0) {
					min = -1.0;
					next_clip_min = 1;
				}

				if (fabs (min) > fabs (max)) {
					max = fabs (min);
				}

				next_pymax = (int) rint ((item->y1 + waveview->height - max * waveview->height) * item->canvas->pixels_per_unit);
			}

			/* render */
			if (pymax == pymin) {
				PAINT_DOTA(buf, waveview->wave_r, waveview->wave_g, waveview->wave_b, waveview->wave_a, x, pymin);
			} else {
				if((prev_pymax < pymax && next_pymax < pymax) ||
				   (prev_pymax == pymax && next_pymax == pymax)) {
					fill_max = pymax + 1;
					PAINT_DOTA(buf, waveview->wave_r, waveview->wave_g, waveview->wave_b, waveview->wave_a, x, pymax);
				}
				else {
					fill_max = MAX(prev_pymax, next_pymax);
					if(pymax == fill_max) {
						PAINT_DOTA(buf, waveview->wave_r, waveview->wave_g, waveview->wave_b, waveview->wave_a, x, pymax);
						++fill_max;
					}
					else {
						PAINT_VERTA_GR(buf, waveview->wave_r, waveview->wave_g, waveview->wave_b, waveview->wave_a, x, pymax, fill_max, wave_middle, wave_top);
					}
				}

				if(fill_max < pymin) {
					PAINT_VERTA(buf, waveview->fill_r, waveview->fill_g, waveview->fill_b, waveview->fill_a, x, fill_max, pymin);
				}
				else if(fill_max == pymin) {
					PAINT_DOTA(buf, waveview->fill_r, waveview->fill_g, waveview->fill_b, waveview->fill_a, x, pymin);
				}
			}

			if (clip_max) {
				PAINT_VERTA(buf, waveview->clip_r, waveview->clip_g, waveview->clip_b, waveview->clip_a, x, pymax, pymax + clip_length);
				PAINT_VERTA(buf, waveview->clip_r, waveview->clip_g, waveview->clip_b, waveview->clip_a >> 1, x + 1, pymax, pymax + (clip_length -1));
				PAINT_VERTA(buf, waveview->clip_r, waveview->clip_g, waveview->clip_b, waveview->clip_a >> 1, x - 1, pymax, pymax + (clip_length - 1));
			}

			if (clip_min) {	
				PAINT_VERTA(buf, waveview->clip_r, waveview->clip_g, waveview->clip_b, waveview->clip_a , x, pymin - clip_length, pymin);
				PAINT_VERTA(buf, waveview->clip_r, waveview->clip_g, waveview->clip_b, waveview->clip_a >> 1, x + 1, pymin - (clip_length - 1), pymin);
				PAINT_VERTA(buf, waveview->clip_r, waveview->clip_g, waveview->clip_b, waveview->clip_a >> 1, x - 1, pymin - (clip_length - 1), pymin);
			}

			prev_pymax = pymax;
		}
	}
	else {
		cache_index = gnome_canvas_waveview_ensure_cache (waveview, s1, s2);

		for (x = begin; x < end; x++) {

			double max, min;
			int clip_max, clip_min;

			clip_max = 0;
			clip_min = 0;

			max = waveview->cache->data[cache_index].max;
			min = waveview->cache->data[cache_index].min;

			if (max >= 1.0) {
				max = 1.0;
				clip_max = 1;
			}

			if (min <= -1.0) {
				min = -1.0;
				clip_min = 1;
			}

			if (rectify) {

				if (fabs (min) > fabs (max)) {
					max = fabs (min);
				}

				max = max * waveview->height;

				pymax = (int) rint ((item->y1 + waveview->height - max) * item->canvas->pixels_per_unit);
				pymin = (int) rint ((item->y1 + waveview->height) * item->canvas->pixels_per_unit);

			} else {

				max = max * half_height;
				min = min * half_height;

				pymax = (int) rint ((item->y1 + origin - max) * item->canvas->pixels_per_unit);
				pymin = (int) rint ((item->y1 + origin - min) * item->canvas->pixels_per_unit);
			}

			/* OK, now fill the RGB buffer at x=i with a line between pymin and pymax,
			   or, if samples_per_unit == 1, then a dot at each location.
			*/

			if (pymax == pymin) {
				PAINT_DOTA(buf, waveview->wave_r, waveview->wave_g, waveview->wave_b, waveview->wave_a, x, pymin);
			} else {
				PAINT_VERTA(buf, waveview->wave_r, waveview->wave_g, waveview->wave_b, waveview->wave_a, x, pymax, pymin);
			}

			/* show clipped waveforms with small red lines */

			if (clip_max) {
				PAINT_VERTA(buf, waveview->clip_r, waveview->clip_g, waveview->clip_b, waveview->clip_a, x, pymax, pymax+clip_length);
			}

			if (clip_min) {
				PAINT_VERTA(buf, waveview->clip_r, waveview->clip_g, waveview->clip_b, waveview->clip_a, x, pymin-clip_length, pymin);
			}

			/* presto, we're done */

			cache_index++;
		}
	}

	if (!waveview->rectified && waveview->zero_line && waveview->height >= 100) {
		// Paint zeroline.

		unsigned char zero_r, zero_g, zero_b, zero_a;
		UINT_TO_RGBA( waveview->zero_color, &zero_r, &zero_g, &zero_b, &zero_a);
		int zeroline_y = (int) rint ((item->y1 + origin) * item->canvas->pixels_per_unit);
		PAINT_HORIZA(buf, zero_r, zero_g, zero_b, zero_a, zbegin, zend, zeroline_y);
	}
#undef origin

}

static void
gnome_canvas_waveview_flat_render (GnomeCanvasItem *item,
				   GnomeCanvasBuf *buf)
{
	GnomeCanvasWaveView *waveview;
	gulong s1, s2;
	int clip_length = 0;
	int pymin, pymax;
	guint cache_index;
	double half_height;
	int x;
	char rectify;

	waveview = GNOME_CANVAS_WAVEVIEW (item);

//	check_cache (waveview, "start of render");

	if (parent_class->render) {
		(*parent_class->render) (item, buf);
	}

	if (buf->is_bg) {
		gnome_canvas_buf_ensure_buf (buf);
		buf->is_bg = FALSE;
	}

	/* a "unit" means a pixel */

	/* begin: render start x (units) */
	int const begin = MAX (waveview->bbox_ulx, buf->rect.x0);

        /* zbegin: start x for zero line (units) */
	int const zbegin = (begin == waveview->bbox_ulx) ? (begin + 1) : begin;

	/* end: render end x (units) */
	int const end = (waveview->bbox_lrx >= 0) ? MIN (waveview->bbox_lrx,buf->rect.x1) : buf->rect.x1;

	/* zend: end x for zero-line (units) */
	int const zend = (end == waveview->bbox_lrx) ? (end - 1) : end;

	if (begin == end) {
		return;
	}

	/* s1: start sample
	   s2: end sample
	*/

	s1 = floor ((begin - waveview->bbox_ulx) * waveview->samples_per_unit);

	// fprintf (stderr, "0x%x begins at sample %f\n", waveview, waveview->bbox_ulx * waveview->samples_per_unit);

	if (end == waveview->bbox_lrx) {
		/* This avoids minor rounding errors when we have the
		   entire region visible.
		*/
		s2 = waveview->samples;
	} else {
		s2 = s1 + floor ((end - begin) * waveview->samples_per_unit);
	}

#if 0
	printf ("0x%x r (%d..%d)(%d..%d) bbox (%d..%d)(%d..%d)"
		" b/e %d..%d s= %lu..%lu @ %f\n",
		waveview,
		buf->rect.x0,
		buf->rect.x1,
		buf->rect.y0,
		buf->rect.y1,
		waveview->bbox_ulx,
		waveview->bbox_lrx,
		waveview->bbox_uly,
		waveview->bbox_lry,
		begin, end, s1, s2,
		waveview->samples_per_unit);
#endif

	/* now ensure that the cache is full and properly
	   positioned.
	*/

//	check_cache (waveview, "pre-ensure");

	if (waveview->cache_updater && waveview->reload_cache_in_render) {
		waveview->cache->start = 0;
		waveview->cache->end = 0;
		waveview->reload_cache_in_render = FALSE;
	}

//	check_cache (waveview, "post-ensure");

	/* don't rectify at single-sample zoom */
	if (waveview->rectified && waveview->samples_per_unit > 1) {
		rectify = TRUE;
	}
	else {
		rectify = FALSE;
	}

	clip_length = MIN(5,(waveview->height/4));

	/*
	   Now draw each line, clipping it appropriately. The clipping
	   is done by the macros PAINT_FOO().
	*/

	half_height = waveview->half_height;

/* this makes it slightly easier to comprehend whats going on */
#define origin half_height

	if (waveview->filled && !rectify) {
		int prev_pymin = 1;
		int prev_pymax = 0;
		int last_pymin = 1;
		int last_pymax = 0;
		int next_pymin, next_pymax;
		double max, min;
		int next_clip_max = 0;
		int next_clip_min = 0;

		if (s1 < waveview->samples_per_unit) {
			/* we haven't got a prev vars to compare with, so outline the whole line here */
			prev_pymax = (int) rint ((item->y1 + origin) * item->canvas->pixels_per_unit);
			prev_pymin = prev_pymax;
		}
		else {
			s1 -= waveview->samples_per_unit;
		}

		if(end == waveview->bbox_lrx) {
			/* we don't have the NEXT vars for the last sample */
			last_pymax = (int) rint ((item->y1 + origin) * item->canvas->pixels_per_unit);
			last_pymin = last_pymax;
		}
		else {
			s2 += waveview->samples_per_unit;
		}

		cache_index = gnome_canvas_waveview_ensure_cache (waveview, s1, s2);

		/*
		 * Compute the variables outside the rendering rect
		 */
		if(prev_pymax != prev_pymin) {

			prev_pymax = (int) rint ((item->y1 + origin - MIN(waveview->cache->data[cache_index].max, 1.0) * half_height) * item->canvas->pixels_per_unit);
			prev_pymin = (int) rint ((item->y1 + origin - MAX(waveview->cache->data[cache_index].min, -1.0) * half_height) * item->canvas->pixels_per_unit);
			++cache_index;
		}
		if(last_pymax != last_pymin) {
			/* take the index of one sample right of what we render */
			guint index = cache_index + (end - begin);

			if (index >= waveview->cache->data_size) {

				/* the data we want is off the end of the cache, which must mean its beyond
				   the end of the region's source; hence the peak values are 0 */
				last_pymax = (int) rint ((item->y1 + origin) * item->canvas->pixels_per_unit);
				last_pymin = (int) rint ((item->y1 + origin) * item->canvas->pixels_per_unit);

			} else {

				last_pymax = (int) rint ((item->y1 + origin - MIN(waveview->cache->data[index].max, 1.0) * half_height) * item->canvas->pixels_per_unit);
				last_pymin = (int) rint ((item->y1 + origin - MAX(waveview->cache->data[index].min, -1.0) * half_height) * item->canvas->pixels_per_unit);

			}

		}

		/*
		 * initialize NEXT* variables for the first run, duplicated in the loop for speed
		 */
		max = waveview->cache->data[cache_index].max;
		min = waveview->cache->data[cache_index].min;

		if (max >= 1.0) {
			max = 1.0;
			next_clip_max = 1;
		}

		if (min <= -1.0) {
			min = -1.0;
			next_clip_min = 1;
		}

		max *= half_height;
		min *= half_height;

		next_pymax = (int) rint ((item->y1 + origin - max) * item->canvas->pixels_per_unit);
		next_pymin = (int) rint ((item->y1 + origin - min) * item->canvas->pixels_per_unit);

		/*
		 * And now the loop
		 */
		for(x = begin; x < end; ++x) {
			int clip_max = next_clip_max;
			int clip_min = next_clip_min;
			int fill_max, fill_min;

			pymax = next_pymax;
			pymin = next_pymin;

			/* compute next */
			if(x == end - 1) {
				/*next is now the last column, which is outside the rendering rect, and possibly outside the region*/
				next_pymax = last_pymax;
				next_pymin = last_pymin;
			}
			else {
				++cache_index;

				if (cache_index < waveview->cache->data_size) {
					max = waveview->cache->data[cache_index].max;
					min = waveview->cache->data[cache_index].min;
				} else {
					max = min = 0;
				}

				next_clip_max = 0;
				next_clip_min = 0;

				if (max >= 1.0) {
					max = 1.0;
					next_clip_max = 1;
				}

				if (min <= -1.0) {
					min = -1.0;
					next_clip_min = 1;
				}

				max *= half_height;
				min *= half_height;

				next_pymax = (int) rint ((item->y1 + origin - max) * item->canvas->pixels_per_unit);
				next_pymin = (int) rint ((item->y1 + origin - min) * item->canvas->pixels_per_unit);
			}

			/* render */
			if (pymax == pymin) {
				PAINT_DOTA(buf, waveview->wave_r, waveview->wave_g, waveview->wave_b, waveview->wave_a, x, pymin);
			} else {
				if((prev_pymax < pymax && next_pymax < pymax) ||
				   (prev_pymax == pymax && next_pymax == pymax)) {
					fill_max = pymax + 1;
					PAINT_DOTA(buf, waveview->wave_r, waveview->wave_g, waveview->wave_b, waveview->wave_a, x, pymax);
				}
				else {
					fill_max = MAX(prev_pymax, next_pymax);
					if(pymax == fill_max) {
						PAINT_DOTA(buf, waveview->wave_r, waveview->wave_g, waveview->wave_b, waveview->wave_a, x, pymax);
						++fill_max;
					}
					else {
						PAINT_VERTA(buf, waveview->wave_r, waveview->wave_g, waveview->wave_b, waveview->wave_a, x, pymax, fill_max);
					}
				}

				if((prev_pymin > pymin && next_pymin > pymin) ||
				   (prev_pymin == pymin && next_pymin == pymin)) {
					fill_min = pymin - 1;
					PAINT_DOTA(buf, waveview->wave_r, waveview->wave_g, waveview->wave_b, waveview->wave_a, x, pymin-1);
				}
				else {
					fill_min = MIN(prev_pymin, next_pymin);
					if(pymin == fill_min) {
						PAINT_DOTA(buf, waveview->wave_r, waveview->wave_g, waveview->wave_b, waveview->wave_a, x, pymin);
					}
					else {
						PAINT_VERTA(buf, waveview->wave_r, waveview->wave_g, waveview->wave_b, waveview->wave_a, x, fill_min, pymin);
					}
				}

				if(fill_max < fill_min) {
					PAINT_VERTA(buf, waveview->fill_r, waveview->fill_g, waveview->fill_b, waveview->fill_a, x, fill_max, fill_min);
				}
				else if(fill_max == fill_min) {
					PAINT_DOTA(buf, waveview->fill_r, waveview->fill_g, waveview->fill_b, waveview->fill_a, x, fill_max);
				}
			}

			if (clip_max) {
				PAINT_VERTA(buf, waveview->clip_r, waveview->clip_g, waveview->clip_b, waveview->clip_a, x, pymax, pymax+clip_length);
			}

			if (clip_min) {
				PAINT_VERTA(buf, waveview->clip_r, waveview->clip_g, waveview->clip_b, waveview->clip_a, x, pymin-clip_length, pymin);
			}

			prev_pymax = pymax;
			prev_pymin = pymin;
		}

	} else if (waveview->filled && rectify) {

		int prev_pymax = -1;
		int last_pymax = -1;
		int next_pymax;
		double max, min;
		int next_clip_max = 0;
		int next_clip_min = 0;

		// for rectified, this stays constant throughout the loop
		pymin = (int) rint ((item->y1 + waveview->height) * item->canvas->pixels_per_unit);

		if(s1 < waveview->samples_per_unit) {
			/* we haven't got a prev vars to compare with, so outline the whole line here */
			prev_pymax = pymin;
		}
		else {
			s1 -= waveview->samples_per_unit;
		}

		if(end == waveview->bbox_lrx) {
			/* we don't have the NEXT vars for the last sample */
			last_pymax = pymin;
		}
		else {
			s2 += waveview->samples_per_unit;
		}

		cache_index = gnome_canvas_waveview_ensure_cache (waveview, s1, s2);

		/*
		 * Compute the variables outside the rendering rect
		 */
		if(prev_pymax < 0) {
			max = MIN(waveview->cache->data[cache_index].max, 1.0);
			min = MAX(waveview->cache->data[cache_index].min, -1.0);

			if (fabs (min) > fabs (max)) {
				max = fabs (min);
			}

			prev_pymax = (int) rint ((item->y1 + waveview->height - max * waveview->height) * item->canvas->pixels_per_unit);
			++cache_index;
		}
		if(last_pymax < 0) {
			/* take the index of one sample right of what we render */
			int index = cache_index + (end - begin);

			max = MIN(waveview->cache->data[index].max, 1.0);
			min = MAX(waveview->cache->data[index].min, -1.0);

			if (fabs (min) > fabs (max)) {
				max = fabs (min);
			}

			last_pymax = (int) rint ((item->y1 + waveview->height - max * waveview->height) * item->canvas->pixels_per_unit);
		}

		/*
		 * initialize NEXT* variables for the first run, duplicated in the loop for speed
		 */
		max = waveview->cache->data[cache_index].max;
		min = waveview->cache->data[cache_index].min;

		if (max >= 1.0) {
			max = 1.0;
			next_clip_max = 1;
		}

		if (min <= -1.0) {
			min = -1.0;
			next_clip_min = 1;
		}

		if (fabs (min) > fabs (max)) {
			max = fabs (min);
		}

		next_pymax = (int) rint ((item->y1 + waveview->height - max * waveview->height) * item->canvas->pixels_per_unit);

		/*
		 * And now the loop
		 */
		for(x = begin; x < end; ++x) {
			int clip_max = next_clip_max;
			int clip_min = next_clip_min;
			int fill_max;

			pymax = next_pymax;

			/* compute next */
			if(x == end - 1) {
				/*next is now the last column, which is outside the rendering rect, and possibly outside the region*/
				next_pymax = last_pymax;
			}
			else {
				++cache_index;

				max = waveview->cache->data[cache_index].max;
				min = waveview->cache->data[cache_index].min;

				if (max >= 1.0) {
					max = 1.0;
					next_clip_max = 1;
				}

				if (min <= -1.0) {
					min = -1.0;
					next_clip_min = 1;
				}

				if (fabs (min) > fabs (max)) {
					max = fabs (min);
				}

				next_pymax = (int) rint ((item->y1 + waveview->height - max * waveview->height) * item->canvas->pixels_per_unit);
			}

			/* render */
			if (pymax == pymin) {
				PAINT_DOTA(buf, waveview->wave_r, waveview->wave_g, waveview->wave_b, waveview->wave_a, x, pymin);
			} else {
				if((prev_pymax < pymax && next_pymax < pymax) ||
				   (prev_pymax == pymax && next_pymax == pymax)) {
					fill_max = pymax + 1;
					PAINT_DOTA(buf, waveview->wave_r, waveview->wave_g, waveview->wave_b, waveview->wave_a, x, pymax);
				}
				else {
					fill_max = MAX(prev_pymax, next_pymax);
					if(pymax == fill_max) {
						PAINT_DOTA(buf, waveview->wave_r, waveview->wave_g, waveview->wave_b, waveview->wave_a, x, pymax);
						++fill_max;
					}
					else {
						PAINT_VERTA(buf, waveview->wave_r, waveview->wave_g, waveview->wave_b, waveview->wave_a, x, pymax, fill_max);
					}
				}

				if(fill_max < pymin) {
					PAINT_VERTA(buf, waveview->fill_r, waveview->fill_g, waveview->fill_b, waveview->fill_a, x, fill_max, pymin);
				}
				else if(fill_max == pymin) {
					PAINT_DOTA(buf, waveview->fill_r, waveview->fill_g, waveview->fill_b, waveview->fill_a, x, pymin);
				}
			}

			if (clip_max) {
				PAINT_VERTA(buf, waveview->clip_r, waveview->clip_g, waveview->clip_b, waveview->clip_a, x, pymax, pymax+clip_length);
			}

			if (clip_min) {
				PAINT_VERTA(buf, waveview->clip_r, waveview->clip_g, waveview->clip_b, waveview->clip_a, x, pymin-clip_length, pymin);
			}

			prev_pymax = pymax;
		}
	}
	else {
		cache_index = gnome_canvas_waveview_ensure_cache (waveview, s1, s2);

		for (x = begin; x < end; x++) {

			double max, min;
			int clip_max, clip_min;

			clip_max = 0;
			clip_min = 0;

			max = waveview->cache->data[cache_index].max;
			min = waveview->cache->data[cache_index].min;

			if (max >= 1.0) {
				max = 1.0;
				clip_max = 1;
			}

			if (min <= -1.0) {
				min = -1.0;
				clip_min = 1;
			}

			if (rectify) {

				if (fabs (min) > fabs (max)) {
					max = fabs (min);
				}

				max = max * waveview->height;

				pymax = (int) rint ((item->y1 + waveview->height - max) * item->canvas->pixels_per_unit);
				pymin = (int) rint ((item->y1 + waveview->height) * item->canvas->pixels_per_unit);

			} else {

				max = max * half_height;
				min = min * half_height;

				pymax = (int) rint ((item->y1 + origin - max) * item->canvas->pixels_per_unit);
				pymin = (int) rint ((item->y1 + origin - min) * item->canvas->pixels_per_unit);
			}

			/* OK, now fill the RGB buffer at x=i with a line between pymin and pymax,
			   or, if samples_per_unit == 1, then a dot at each location.
			*/

			if (pymax == pymin) {
				PAINT_DOTA(buf, waveview->wave_r, waveview->wave_g, waveview->wave_b, waveview->wave_a, x, pymin);
			} else {
				PAINT_VERTA(buf, waveview->wave_r, waveview->wave_g, waveview->wave_b, waveview->wave_a, x, pymax, pymin);
			}

			/* show clipped waveforms with small red lines */

			if (clip_max) {
				PAINT_VERTA(buf, waveview->clip_r, waveview->clip_g, waveview->clip_b, waveview->clip_a, x, pymax, pymax+clip_length);
			}

			if (clip_min) {
				PAINT_VERTA(buf, waveview->clip_r, waveview->clip_g, waveview->clip_b, waveview->clip_a, x, pymin-clip_length, pymin);
			}

			/* presto, we're done */

			cache_index++;
		}
	}

	if (!waveview->rectified && waveview->zero_line && waveview->height >= 100) {
		// Paint zeroline.

		unsigned char zero_r, zero_g, zero_b, zero_a;
		UINT_TO_RGBA( waveview->zero_color, &zero_r, &zero_g, &zero_b, &zero_a);
		int zeroline_y = (int) rint ((item->y1 + origin) * item->canvas->pixels_per_unit);
		PAINT_HORIZA(buf, zero_r, zero_g, zero_b, zero_a, zbegin, zend, zeroline_y);
	}
#undef origin
}

static void
gnome_canvas_waveview_render (GnomeCanvasItem *item,
			      GnomeCanvasBuf *buf)
{
	if (_gradient_rendering) {
		gnome_canvas_waveview_gradient_render (item, buf);
	} else {
		gnome_canvas_waveview_flat_render (item, buf);
	}
}

static void
gnome_canvas_waveview_draw (GnomeCanvasItem *item,
			    GdkDrawable *drawable,
			    int x, int y,
			    int width, int height)
{
	GnomeCanvasWaveView *waveview;
	cairo_t* cr;
	gulong s1, s2;
	int cache_index;
	gboolean rectify;
	double origin;
	double xoff;
	double yoff = 0.0;
	double ulx;
	double uly;
	double lrx;
	double lry;

	waveview = GNOME_CANVAS_WAVEVIEW (item);

	/* compute intersection of Drawable area and waveview,
	   in canvas coordinate space
	*/

	if (x > waveview->bbox_ulx) {
		ulx = x;
	} else {
		ulx = waveview->bbox_ulx;
	}

	if (y > waveview->bbox_uly) {
		uly = y;
	} else {
		uly = waveview->bbox_uly;
	}

	if (x + width > waveview->bbox_lrx) {
		lrx = waveview->bbox_lrx;
	} else {
		lrx = x + width;
	}

	if (y + height > waveview->bbox_lry) {
		lry = waveview->bbox_lry;
	} else {
		lry = y + height;
	}

	/* figure out which samples we need for the resulting intersection */

	s1 = floor ((ulx - waveview->bbox_ulx) * waveview->samples_per_unit) ;

	if (lrx == waveview->bbox_lrx) {
		/* This avoids minor rounding errors when we have the
		   entire region visible.
		*/
		s2 = waveview->samples;
	} else {
		s2 = s1 + floor ((lrx - ulx) * waveview->samples_per_unit);
	}

	/* translate back to buffer coordinate space */

	ulx -= x;
	uly -= y;
	lrx -= x;
	lry -= y;

	/* don't rectify at single-sample zoom */
	if(waveview->rectified && waveview->samples_per_unit > 1.0) {
		rectify = TRUE;
	} else {
		rectify = FALSE;
	}

	cr = gdk_cairo_create (drawable);
	cairo_set_line_width (cr, 0.5);

	origin = waveview->bbox_uly - y + waveview->half_height;

	cairo_rectangle (cr, ulx, uly, lrx - ulx, lry - uly);
	cairo_clip (cr);

	if (waveview->cache_updater && waveview->reload_cache_in_render) {
		waveview->cache->start = 0;
		waveview->cache->end = 0;
		waveview->reload_cache_in_render = FALSE;
	}

	cache_index = gnome_canvas_waveview_ensure_cache (waveview, s1, s2);

#if 0
	printf ("%p r (%d,%d)(%d,%d)[%d x %d] bbox (%d,%d)(%d,%d)[%d x %d]"
		" draw (%.1f,%.1f)(%.1f,%.1f)[%.1f x %.1f] s= %lu..%lu\n",
		waveview,
		x, y,
		x + width,
		y + height,
		width,
		height,
		waveview->bbox_ulx,
		waveview->bbox_uly,
		waveview->bbox_lrx,
		waveview->bbox_lry,
		waveview->bbox_lrx - waveview->bbox_ulx,
		waveview->bbox_lry - waveview->bbox_uly,
		ulx, uly,
		lrx, lry,
		lrx - ulx,
		lry - uly,
		s1, s2);
#endif

	/* draw the top half */

	for (xoff = ulx; xoff < lrx; xoff++) {
		double max, min;

		max = waveview->cache->data[cache_index].max;
		min = waveview->cache->data[cache_index].min;

		if (min <= -1.0) {
			min = -1.0;
		}

		if (max >= 1.0) {
			max = 1.0;
		}

		if (rectify) {
			if (fabs (min) > fabs (max)) {
				max = fabs (min);
			}
		}

		yoff = origin - (waveview->half_height * max) + 0.5;

		if (xoff == ulx) {
			/* first point */
			cairo_move_to (cr, xoff+0.5, yoff);
		} else {
			cairo_line_to (cr, xoff+0.5, yoff);
		}

		cache_index++;
	}

	/* from the final top point, move out of the clip zone */

	cairo_line_to (cr, xoff + 10, yoff);

	/* now draw the bottom half */

	for (--xoff, --cache_index; xoff >= ulx; --xoff) {
		double min;

		min = waveview->cache->data[cache_index].min;

		if (min <= -1.0) {
			min = -1.0;
		}

		yoff = origin - (waveview->half_height * min) + 0.5;

		cairo_line_to (cr, xoff+0.5, yoff);
		cache_index--;
	}

	/* from the final lower point, move out of the clip zone */

	cairo_line_to (cr, xoff - 10, yoff);

	/* close path to fill */

	cairo_close_path (cr);

	/* fill and stroke */

	cairo_set_source_rgba (cr,
			       (waveview->fill_r/255.0),
			       (waveview->fill_g/255.0),
			       (waveview->fill_b/255.0),
			       (waveview->fill_a/255.0));
	cairo_fill_preserve (cr);
	cairo_set_source_rgba (cr,
			       (waveview->wave_r/255.0),
			       (waveview->wave_g/255.0),
			       (waveview->wave_b/255.0),
			       (waveview->wave_a/255.0));
	cairo_stroke (cr);

	cairo_destroy (cr);
}

#if 0
		if (clip_max || clip_min) {
			cairo_set_source_rgba (cr, waveview->clip_r, waveview->clip_g, waveview->clip_b, waveview->clip_a);
		}

		if (clip_max) {
			cairo_move_to (cr, xoff, yoff1);
			cairo_line_to (cr, xoff, yoff1 + clip_length);
			cairo_stroke (cr);
		}

		if (clip_min) {
			cairo_move_to (cr, xoff, yoff2);
			cairo_line_to (cr, xoff, yoff2 - clip_length);
			cairo_stroke (cr);
		}

#endif

static void
gnome_canvas_waveview_bounds (GnomeCanvasItem *item, double *x1, double *y1, double *x2, double *y2)
{
	GnomeCanvasWaveView *waveview = GNOME_CANVAS_WAVEVIEW (item);

	*x1 = waveview->x;
	*y1 = waveview->y;

	*x2 = ceil (*x1 + (waveview->length_function (waveview->data_src) / waveview->samples_per_unit));
	*y2 = *y1 + waveview->height;

#if 0
	x = 0; y = 0;
	gnome_canvas_item_i2w (item, &x, &y);
	gnome_canvas_w2c_d (GNOME_CANVAS(item->canvas), x, y, &a, &b);
	x = *x2;
	y = *y2;
	gnome_canvas_item_i2w (item, &x, &y);
	gnome_canvas_w2c_d (GNOME_CANVAS(item->canvas), x, y, &c, &d);
	printf ("item bounds now (%g,%g),(%g,%g)\n", a, b, c, d);
#endif

}

static double
gnome_canvas_waveview_point (GnomeCanvasItem *item, double x, double y, int cx, int cy, GnomeCanvasItem **actual_item)
{
	(void) item;
	(void) x;
	(void) y;
	(void) cx;
	(void) cy;
	(void) actual_item;

	/* XXX for now, point is never inside the wave
	GnomeCanvasWaveView *waveview;
	double x1, y1, x2, y2;
	double dx, dy;
	*/

	return DBL_MAX;

#if 0
	waveview = GNOME_CANVAS_WAVEVIEW (item);

	*actual_item = item;

	/* Find the bounds for the rectangle plus its outline width */

	gnome_canvas_waveview_bounds (item, &x1, &y1, &x2, &y2);

	/* Is point inside rectangle */

	if ((x >= x1) && (y >= y1) && (x <= x2) && (y <= y2)) {
		return 0.0;
	}

	/* Point is outside rectangle */

	if (x < x1)
		dx = x1 - x;
	else if (x > x2)
		dx = x - x2;
	else
		dx = 0.0;

	if (y < y1)
		dy = y1 - y;
	else if (y > y2)
		dy = y - y2;
	else
		dy = 0.0;

	return sqrt (dx * dx + dy * dy);
#endif
}

