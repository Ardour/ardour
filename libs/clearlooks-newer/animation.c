/* Clearlooks theme engine
 *
 * Copyright (C) 2006 Kulyk Nazar <schamane@myeburg.net>
 * Copyright (C) 2006 Benjamin Berg <benjamin@sipsolutions.net>
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

/* This code is responsible for the clearlooks animation support. The code
 * works by forcing a redraw on the animated widget.
 */

#include "animation.h"

#ifdef HAVE_ANIMATION
#include <glib.h>

struct _AnimationInfo {
	GTimer *timer;
	
	gdouble start_modifier;
	gdouble stop_time;
	GtkWidget *widget;
};
typedef struct _AnimationInfo AnimationInfo;

struct _SignalInfo {
	GtkWidget *widget;
	gulong handler_id;
};
typedef struct _SignalInfo SignalInfo;

static GSList     *connected_widgets  = NULL;
static GHashTable *animated_widgets   = NULL;
static int         animation_timer_id = 0;


static gboolean animation_timeout_handler (gpointer data);

/* This forces a redraw on a widget */
static void
force_widget_redraw (GtkWidget *widget)
{
	if (GE_IS_PROGRESS_BAR (widget))
		gtk_widget_queue_resize (widget);
	else
		gtk_widget_queue_draw (widget);
}

/* ensures that the timer is running */
static void
start_timer ()
{
	if (animation_timer_id == 0)
		animation_timer_id = g_timeout_add (ANIMATION_DELAY, animation_timeout_handler, NULL);
}

/* ensures that the timer is stopped */
static void
stop_timer ()
{
	if (animation_timer_id != 0)
	{
		g_source_remove(animation_timer_id);
		animation_timer_id = 0;
	}
}


/* destroys an AnimationInfo structure including the GTimer */
static void
animation_info_destroy (AnimationInfo *animation_info)
{
	g_timer_destroy (animation_info->timer);
	g_free (animation_info);
}


/* This function does not unref the weak reference, because the object
 * is beeing destroyed currently. */
static void
on_animated_widget_destruction (gpointer data, GObject *object)
{
	/* steal the animation info from the hash table (destroying it would
	 * result in the weak reference to be unrefed, which does not work
	 * as the widget is already destroyed. */
	g_hash_table_steal (animated_widgets, object);
	animation_info_destroy ((AnimationInfo*) data);
}

/* This function also needs to unref the weak reference. */
static void
destroy_animation_info_and_weak_unref (gpointer data)
{
	AnimationInfo *animation_info = data;
	
	/* force a last redraw. This is so that if the animation is removed,
	 * the widget is left in a sane state. */
	force_widget_redraw (animation_info->widget);
	
	g_object_weak_unref (G_OBJECT (animation_info->widget), on_animated_widget_destruction, data);
	animation_info_destroy (animation_info);
}

/* Find and return a pointer to the data linked to this widget, if it exists */
static AnimationInfo*
lookup_animation_info (const GtkWidget *widget)
{
	if (animated_widgets)
		return g_hash_table_lookup (animated_widgets, widget);
	
	return NULL;
}

/* Create all the relevant information for the animation, and insert it into the hash table. */
static void
add_animation (const GtkWidget *widget, gdouble stop_time)
{
	AnimationInfo *value;
	
	/* object already in the list, do not add it twice */
	if (lookup_animation_info (widget))
		return;
	
	if (animated_widgets == NULL)
		animated_widgets = g_hash_table_new_full (g_direct_hash, g_direct_equal,
		                                          NULL, destroy_animation_info_and_weak_unref);
	
	value = g_new(AnimationInfo, 1);
	
	value->widget = (GtkWidget*) widget;
	
	value->timer = g_timer_new ();
	value->stop_time= stop_time;
	value->start_modifier = 0.0;

	g_object_weak_ref (G_OBJECT (widget), on_animated_widget_destruction, value);
	g_hash_table_insert (animated_widgets, (GtkWidget*) widget, value);
	
	start_timer ();
}

/* update the animation information for each widget. This will also queue a redraw
 * and stop the animation if it is done. */
static gboolean
update_animation_info (gpointer key, gpointer value, gpointer user_data)
{
	AnimationInfo *animation_info = value;
	GtkWidget *widget = key;
	
	g_assert ((widget != NULL) && (animation_info != NULL));
	
	/* remove the widget from the hash table if it is not drawable */
	if (!GTK_WIDGET_DRAWABLE (widget))
	{
		return TRUE;
	}
	
	if (GE_IS_PROGRESS_BAR (widget))
	{
		gfloat fraction = gtk_progress_bar_get_fraction (GTK_PROGRESS_BAR (widget));
		
		/* stop animation for filled/not filled progress bars */
		if (fraction <= 0.0 || fraction >= 1.0)
			return TRUE;
	}
	
	force_widget_redraw (widget);
	
	/* stop at stop_time */
	if (animation_info->stop_time != 0 &&
	    g_timer_elapsed (animation_info->timer, NULL) > animation_info->stop_time)
		return TRUE;
	
	return FALSE;
}

/* This gets called by the glib main loop every once in a while. */
static gboolean
animation_timeout_handler (gpointer data)
{
	/*g_print("** TICK **\n");*/
	
	/* enter threads as update_animation_info will use gtk/gdk. */
	gdk_threads_enter ();
	g_hash_table_foreach_remove (animated_widgets, update_animation_info, NULL);
	/* leave threads again */
	gdk_threads_leave ();
	
	if(g_hash_table_size(animated_widgets)==0)
	{
		stop_timer ();
		return FALSE;
	}
	
	return TRUE;
}

static void
on_checkbox_toggle (GtkWidget *widget, gpointer data)
{
	AnimationInfo *animation_info = lookup_animation_info (widget);
	
	if (animation_info != NULL)
	{
		gfloat elapsed = g_timer_elapsed (animation_info->timer, NULL);
		
		animation_info->start_modifier = elapsed - animation_info->start_modifier;
	}
	else
	{
		add_animation (widget, CHECK_ANIMATION_TIME);
	}
}

static void
on_connected_widget_destruction (gpointer data, GObject *widget)
{
	connected_widgets = g_slist_remove (connected_widgets, data);
	g_free (data);
}

static void
disconnect_all_signals ()
{
	GSList * item = connected_widgets;
	while (item != NULL)
	{
		SignalInfo *signal_info = (SignalInfo*) item->data;
		
		g_signal_handler_disconnect (signal_info->widget, signal_info->handler_id);
		g_object_weak_unref (G_OBJECT (signal_info->widget), on_connected_widget_destruction, signal_info);
		g_free (signal_info);
		
		item = g_slist_next (item);
	}
	
	g_slist_free (connected_widgets);
	connected_widgets = NULL;
}

/* helper function for clearlooks_animation_connect_checkbox */
static gint
find_signal_info (gconstpointer signal_info, gconstpointer widget)
{
	if (((SignalInfo*)signal_info)->widget == widget)
		return 0;
	else
		return 1;
}


/* external interface */

/* adds a progress bar */
void
clearlooks_animation_progressbar_add (GtkWidget *progressbar)
{
	gdouble fraction = gtk_progress_bar_get_fraction (GTK_PROGRESS_BAR (progressbar));
	
	if (fraction < 1.0 && fraction > 0.0)
		add_animation ((GtkWidget*) progressbar, 0.0);
}

/* hooks up the signals for check and radio buttons */
void
clearlooks_animation_connect_checkbox (GtkWidget *widget)
{
	if (GE_IS_CHECK_BUTTON (widget))
	{
		if (!g_slist_find_custom (connected_widgets, widget, find_signal_info))
		{
			SignalInfo * signal_info = g_new (SignalInfo, 1);
			
			signal_info->widget = widget;
			signal_info->handler_id = g_signal_connect ((GObject*)widget, "toggled", G_CALLBACK (on_checkbox_toggle), NULL);
			
			connected_widgets = g_slist_append (connected_widgets, signal_info);
			g_object_weak_ref (G_OBJECT (widget), on_connected_widget_destruction, signal_info);
		}
	}
}

/* returns TRUE if the widget is animated, and FALSE otherwise */
gboolean
clearlooks_animation_is_animated (GtkWidget *widget)
{
	return lookup_animation_info (widget) != NULL ? TRUE : FALSE;
}

/* returns the elapsed time for the animation */
gdouble
clearlooks_animation_elapsed (gpointer data)
{
	AnimationInfo *animation_info = lookup_animation_info (data);
	
	if (animation_info)
		return   g_timer_elapsed (animation_info->timer, NULL)
		       - animation_info->start_modifier;
	else
		return 0.0;
}

/* cleans up all resources of the animation system */
void
clearlooks_animation_cleanup ()
{
	disconnect_all_signals ();
	
	if (animated_widgets != NULL)
	{
		g_hash_table_destroy (animated_widgets);
		animated_widgets = NULL;
	}
	
	stop_timer ();
}
#else /* !HAVE_ANIMATION */
static void clearlooks_animation_dummy_function_so_wall_shuts_up_when_animations_is_disabled()
{
	clearlooks_animation_dummy_function_so_wall_shuts_up_when_animations_is_disabled();
}
#endif /* HAVE_ANIMATION */
