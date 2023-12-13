/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2007 John Stowers, Neil Jagdish Patel.
 * Copyright (C) 2009 Bastien Nocera, David Zeuthen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA  02111-1307, USA.
 *
 * Code adapted from egg-spinner
 * by Christian Hergert <christian.hergert@gmail.com>
 */

/*
 * Modified by the GTK+ Team and others 2007.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtkintl.h"
#include "gtkaccessible.h"
#include "gtkimage.h"
#include "gtkspinner.h"
#include "gtkstyle.h"
#include "gtkalias.h"


/**
 * SECTION:gtkspinner
 * @Short_description: Show a spinner animation
 * @Title: GtkSpinner
 * @See_also: #GtkCellRendererSpinner, #GtkProgressBar
 *
 * A GtkSpinner widget displays an icon-size spinning animation.
 * It is often used as an alternative to a #GtkProgressBar for
 * displaying indefinite activity, instead of actual progress.
 *
 * To start the animation, use gtk_spinner_start(), to stop it
 * use gtk_spinner_stop().
 */


#define GTK_SPINNER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_SPINNER, GtkSpinnerPrivate))

G_DEFINE_TYPE (GtkSpinner, gtk_spinner, GTK_TYPE_DRAWING_AREA);

enum {
  PROP_0,
  PROP_ACTIVE
};

struct _GtkSpinnerPrivate
{
  guint current;
  guint num_steps;
  guint cycle_duration;
  gboolean active;
  guint timeout;
};

static void gtk_spinner_class_init     (GtkSpinnerClass *klass);
static void gtk_spinner_init           (GtkSpinner      *spinner);
static void gtk_spinner_dispose        (GObject         *gobject);
static void gtk_spinner_realize        (GtkWidget       *widget);
static void gtk_spinner_unrealize      (GtkWidget       *widget);
static gboolean gtk_spinner_expose     (GtkWidget       *widget,
                                        GdkEventExpose  *event);
static void gtk_spinner_screen_changed (GtkWidget       *widget,
                                        GdkScreen       *old_screen);
static void gtk_spinner_style_set      (GtkWidget       *widget,
                                        GtkStyle        *prev_style);
static void gtk_spinner_get_property   (GObject         *object,
                                        guint            param_id,
                                        GValue          *value,
                                        GParamSpec      *pspec);
static void gtk_spinner_set_property   (GObject         *object,
                                        guint            param_id,
                                        const GValue    *value,
                                        GParamSpec      *pspec);
static void gtk_spinner_set_active     (GtkSpinner      *spinner,
                                        gboolean         active);
static AtkObject *gtk_spinner_get_accessible      (GtkWidget *widget);
static GType      gtk_spinner_accessible_get_type (void);

static void
gtk_spinner_class_init (GtkSpinnerClass *klass)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS(klass);
  g_type_class_add_private (gobject_class, sizeof (GtkSpinnerPrivate));
  gobject_class->dispose = gtk_spinner_dispose;
  gobject_class->get_property = gtk_spinner_get_property;
  gobject_class->set_property = gtk_spinner_set_property;

  widget_class = GTK_WIDGET_CLASS(klass);
  widget_class->expose_event = gtk_spinner_expose;
  widget_class->realize = gtk_spinner_realize;
  widget_class->unrealize = gtk_spinner_unrealize;
  widget_class->screen_changed = gtk_spinner_screen_changed;
  widget_class->style_set = gtk_spinner_style_set;
  widget_class->get_accessible = gtk_spinner_get_accessible;

  /* GtkSpinner:active:
   *
   * Whether the spinner is active
   *
   * Since: 2.20
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ACTIVE,
                                   g_param_spec_boolean ("active",
                                                         P_("Active"),
                                                         P_("Whether the spinner is active"),
                                                         FALSE,
                                                         G_PARAM_READWRITE));
  /**
   * GtkSpinner:num-steps:
   *
   * The number of steps for the spinner to complete a full loop.
   * The animation will complete a full cycle in one second by default
   * (see the #GtkSpinner:cycle-duration style property).
   *
   * Since: 2.20
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_uint ("num-steps",
                                                             P_("Number of steps"),
                                                             P_("The number of steps for the spinner to complete a full loop. The animation will complete a full cycle in one second by default (see #GtkSpinner:cycle-duration)."),
                                                             1,
                                                             G_MAXUINT,
                                                             12,
                                                             G_PARAM_READABLE));

  /**
   * GtkSpinner:cycle-duration:
   *
   * The duration in milliseconds for the spinner to complete a full cycle.
   *
   * Since: 2.20
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_uint ("cycle-duration",
                                                             P_("Animation duration"),
                                                             P_("The length of time in milliseconds for the spinner to complete a full loop"),
                                                             500,
                                                             G_MAXUINT,
                                                             1000,
                                                             G_PARAM_READABLE));
}

static void
gtk_spinner_get_property (GObject    *object,
                          guint       param_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GtkSpinnerPrivate *priv;

  priv = GTK_SPINNER (object)->priv;

  switch (param_id)
    {
      case PROP_ACTIVE:
        g_value_set_boolean (value, priv->active);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
gtk_spinner_set_property (GObject      *object,
                          guint         param_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  switch (param_id)
    {
      case PROP_ACTIVE:
        gtk_spinner_set_active (GTK_SPINNER (object), g_value_get_boolean (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
gtk_spinner_init (GtkSpinner *spinner)
{
  GtkSpinnerPrivate *priv;

  priv = GTK_SPINNER_GET_PRIVATE (spinner);
  priv->current = 0;
  priv->timeout = 0;

  spinner->priv = priv;

  gtk_widget_set_has_window (GTK_WIDGET (spinner), FALSE);
}

static gboolean
gtk_spinner_expose (GtkWidget      *widget,
                    GdkEventExpose *event)
{
  GtkStateType state_type;
  GtkSpinnerPrivate *priv;
  int width, height;

  priv = GTK_SPINNER (widget)->priv;

  width = widget->allocation.width;
  height = widget->allocation.height;

  if ((width < 12) || (height <12))
    gtk_widget_set_size_request (widget, 12, 12);

  state_type = GTK_STATE_NORMAL;
  if (!gtk_widget_is_sensitive (widget))
   state_type = GTK_STATE_INSENSITIVE;

  gtk_paint_spinner (widget->style,
                     widget->window,
                     state_type,
                     &event->area,
                     widget,
                     "spinner",
                     priv->current,
                     event->area.x, event->area.y,
                     event->area.width, event->area.height);

  return FALSE;
}

static gboolean
gtk_spinner_timeout (gpointer data)
{
  GtkSpinnerPrivate *priv;

  priv = GTK_SPINNER (data)->priv;

  if (priv->current + 1 >= priv->num_steps)
    priv->current = 0;
  else
    priv->current++;

  gtk_widget_queue_draw (GTK_WIDGET (data));

  return TRUE;
}

static void
gtk_spinner_add_timeout (GtkSpinner *spinner)
{
  GtkSpinnerPrivate *priv;

  priv = spinner->priv;

  priv->timeout = gdk_threads_add_timeout ((guint) priv->cycle_duration / priv->num_steps, gtk_spinner_timeout, spinner);
}

static void
gtk_spinner_remove_timeout (GtkSpinner *spinner)
{
  GtkSpinnerPrivate *priv;

  priv = spinner->priv;

  g_source_remove (priv->timeout);
  priv->timeout = 0;
}

static void
gtk_spinner_realize (GtkWidget *widget)
{
  GtkSpinnerPrivate *priv;

  priv = GTK_SPINNER (widget)->priv;

  GTK_WIDGET_CLASS (gtk_spinner_parent_class)->realize (widget);

  if (priv->active)
    gtk_spinner_add_timeout (GTK_SPINNER (widget));
}

static void
gtk_spinner_unrealize (GtkWidget *widget)
{
  GtkSpinnerPrivate *priv;

  priv = GTK_SPINNER (widget)->priv;

  if (priv->timeout != 0)
    {
      gtk_spinner_remove_timeout (GTK_SPINNER (widget));
    }

  GTK_WIDGET_CLASS (gtk_spinner_parent_class)->unrealize (widget);
}

static void
gtk_spinner_screen_changed (GtkWidget* widget, GdkScreen* old_screen)
{
  GtkSpinner *spinner;
  GdkScreen* new_screen;
  GdkColormap* colormap;

  spinner = GTK_SPINNER (widget);

  new_screen = gtk_widget_get_screen (widget);
  colormap = gdk_screen_get_rgba_colormap (new_screen);

  if (!colormap)
    {
      colormap = gdk_screen_get_rgb_colormap (new_screen);
    }

  gtk_widget_set_colormap (widget, colormap);
}

static void
gtk_spinner_style_set (GtkWidget *widget,
                       GtkStyle  *prev_style)
{
  GtkSpinnerPrivate *priv;

  priv = GTK_SPINNER (widget)->priv;

  gtk_widget_style_get (GTK_WIDGET (widget),
                        "num-steps", &(priv->num_steps),
                        "cycle-duration", &(priv->cycle_duration),
                        NULL);

  if (priv->current > priv->num_steps)
    priv->current = 0;
}

static void
gtk_spinner_dispose (GObject *gobject)
{
  GtkSpinnerPrivate *priv;

  priv = GTK_SPINNER (gobject)->priv;

  if (priv->timeout != 0)
    {
      gtk_spinner_remove_timeout (GTK_SPINNER (gobject));
    }

  G_OBJECT_CLASS (gtk_spinner_parent_class)->dispose (gobject);
}

static void
gtk_spinner_set_active (GtkSpinner *spinner, gboolean active)
{
  GtkSpinnerPrivate *priv;

  active = active != FALSE;

  priv = GTK_SPINNER (spinner)->priv;

  if (priv->active != active)
    {
      priv->active = active;
      g_object_notify (G_OBJECT (spinner), "active");

      if (active && gtk_widget_get_realized (GTK_WIDGET (spinner)) && priv->timeout == 0)
        {
          gtk_spinner_add_timeout (spinner);
        }
      else if (!active && priv->timeout != 0)
        {
          gtk_spinner_remove_timeout (spinner);
        }
    }
}

static GType
gtk_spinner_accessible_factory_get_accessible_type (void)
{
  return gtk_spinner_accessible_get_type ();
}

static AtkObject *
gtk_spinner_accessible_new (GObject *obj)
{
  AtkObject *accessible;

  g_return_val_if_fail (GTK_IS_WIDGET (obj), NULL);

  accessible = g_object_new (gtk_spinner_accessible_get_type (), NULL);
  atk_object_initialize (accessible, obj);

  return accessible;
}

static AtkObject*
gtk_spinner_accessible_factory_create_accessible (GObject *obj)
{
  return gtk_spinner_accessible_new (obj);
}

static void
gtk_spinner_accessible_factory_class_init (AtkObjectFactoryClass *klass)
{
  klass->create_accessible = gtk_spinner_accessible_factory_create_accessible;
  klass->get_accessible_type = gtk_spinner_accessible_factory_get_accessible_type;
}

static GType
gtk_spinner_accessible_factory_get_type (void)
{
  static GType type = 0;

  if (!type)
    {
      const GTypeInfo tinfo =
      {
        sizeof (AtkObjectFactoryClass),
        NULL,           /* base_init */
        NULL,           /* base_finalize */
        (GClassInitFunc) gtk_spinner_accessible_factory_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (AtkObjectFactory),
        0,             /* n_preallocs */
        NULL, NULL
      };

      type = g_type_register_static (ATK_TYPE_OBJECT_FACTORY,
                                    I_("GtkSpinnerAccessibleFactory"),
                                    &tinfo, 0);
    }
  return type;
}

static AtkObjectClass *a11y_parent_class = NULL;

static void
gtk_spinner_accessible_initialize (AtkObject *accessible,
                                   gpointer   widget)
{
  atk_object_set_name (accessible, C_("throbbing progress animation widget", "Spinner"));
  atk_object_set_description (accessible, _("Provides visual indication of progress"));

  a11y_parent_class->initialize (accessible, widget);
}

static void
gtk_spinner_accessible_class_init (AtkObjectClass *klass)
{
  a11y_parent_class = g_type_class_peek_parent (klass);

  klass->initialize = gtk_spinner_accessible_initialize;
}

static void
gtk_spinner_accessible_image_get_size (AtkImage *image,
                                       gint     *width,
                                       gint     *height)
{
  GtkWidget *widget;

  widget = GTK_ACCESSIBLE (image)->widget;
  if (!widget)
    {
      *width = *height = 0;
    }
  else
    {
      *width = widget->allocation.width;
      *height = widget->allocation.height;
    }
}

static void
gtk_spinner_accessible_image_interface_init (AtkImageIface *iface)
{
  iface->get_image_size = gtk_spinner_accessible_image_get_size;
}

static GType
gtk_spinner_accessible_get_type (void)
{
  static GType type = 0;

  /* Action interface
     Name etc. ... */
  if (G_UNLIKELY (type == 0))
    {
      const GInterfaceInfo atk_image_info = {
              (GInterfaceInitFunc) gtk_spinner_accessible_image_interface_init,
              (GInterfaceFinalizeFunc) NULL,
              NULL
      };
      GType parent_atk_type;
      GTypeInfo tinfo = { 0 };
      GTypeQuery query;
      AtkObjectFactory *factory;

      if ((type = g_type_from_name ("GtkSpinnerAccessible")))
        return type;

      factory = atk_registry_get_factory (atk_get_default_registry (),
                                          GTK_TYPE_IMAGE);
      if (!factory)
        return G_TYPE_INVALID;

      parent_atk_type = atk_object_factory_get_accessible_type (factory);
      if (!parent_atk_type)
        return G_TYPE_INVALID;

      /*
       * Figure out the size of the class and instance
       * we are deriving from
       */
      g_type_query (parent_atk_type, &query);

      tinfo.class_init = (GClassInitFunc) gtk_spinner_accessible_class_init;
      tinfo.class_size    = query.class_size;
      tinfo.instance_size = query.instance_size;

      /* Register the type */
      type = g_type_register_static (parent_atk_type,
                                     "GtkSpinnerAccessible",
                                     &tinfo, 0);

      g_type_add_interface_static (type, ATK_TYPE_IMAGE,
                                   &atk_image_info);
    }

  return type;
}

static AtkObject *
gtk_spinner_get_accessible (GtkWidget *widget)
{
  static gboolean first_time = TRUE;

  if (first_time)
    {
      AtkObjectFactory *factory;
      AtkRegistry *registry;
      GType derived_type;
      GType derived_atk_type;

      /*
       * Figure out whether accessibility is enabled by looking at the
       * type of the accessible object which would be created for
       * the parent type of GtkSpinner.
       */
      derived_type = g_type_parent (GTK_TYPE_SPINNER);

      registry = atk_get_default_registry ();
      factory = atk_registry_get_factory (registry,
                                          derived_type);
      derived_atk_type = atk_object_factory_get_accessible_type (factory);
      if (g_type_is_a (derived_atk_type, GTK_TYPE_ACCESSIBLE))
        atk_registry_set_factory_type (registry,
                                       GTK_TYPE_SPINNER,
                                       gtk_spinner_accessible_factory_get_type ());
      first_time = FALSE;
    }
  return GTK_WIDGET_CLASS (gtk_spinner_parent_class)->get_accessible (widget);
}

/**
 * gtk_spinner_new:
 *
 * Returns a new spinner widget. Not yet started.
 *
 * Return value: a new #GtkSpinner
 *
 * Since: 2.20
 */
GtkWidget *
gtk_spinner_new (void)
{
  return g_object_new (GTK_TYPE_SPINNER, NULL);
}

/**
 * gtk_spinner_start:
 * @spinner: a #GtkSpinner
 *
 * Starts the animation of the spinner.
 *
 * Since: 2.20
 */
void
gtk_spinner_start (GtkSpinner *spinner)
{
  g_return_if_fail (GTK_IS_SPINNER (spinner));

  gtk_spinner_set_active (spinner, TRUE);
}

/**
 * gtk_spinner_stop:
 * @spinner: a #GtkSpinner
 *
 * Stops the animation of the spinner.
 *
 * Since: 2.20
 */
void
gtk_spinner_stop (GtkSpinner *spinner)
{
  g_return_if_fail (GTK_IS_SPINNER (spinner));

  gtk_spinner_set_active (spinner, FALSE);
}

#define __GTK_SPINNER_C__
#include "gtkaliasdef.c"
