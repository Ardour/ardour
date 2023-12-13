/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc.
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2001.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"
#include <math.h>
#include <string.h>

#include "gdkconfig.h"
#include "gdk/gdkkeysyms.h"
#include "gtkcolorsel.h"
#include "gtkhsv.h"
#include "gtkwindow.h"
#include "gtkselection.h"
#include "gtkdnd.h"
#include "gtkdrawingarea.h"
#include "gtkhbox.h"
#include "gtkhbbox.h"
#include "gtkrc.h"
#include "gtkframe.h"
#include "gtktable.h"
#include "gtklabel.h"
#include "gtkmarshalers.h"
#include "gtkimage.h"
#include "gtkspinbutton.h"
#include "gtkrange.h"
#include "gtkhscale.h"
#include "gtkentry.h"
#include "gtkbutton.h"
#include "gtkhseparator.h"
#include "gtkinvisible.h"
#include "gtkmenuitem.h"
#include "gtkmain.h"
#include "gtksettings.h"
#include "gtkstock.h"
#include "gtkaccessible.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkalias.h"

/* Keep it in sync with gtksettings.c:default_color_palette */
#define DEFAULT_COLOR_PALETTE   "black:white:gray50:red:purple:blue:light blue:green:yellow:orange:lavender:brown:goldenrod4:dodger blue:pink:light green:gray10:gray30:gray75:gray90"

/* Number of elements in the custom palatte */
#define GTK_CUSTOM_PALETTE_WIDTH 10
#define GTK_CUSTOM_PALETTE_HEIGHT 2

#define CUSTOM_PALETTE_ENTRY_WIDTH   20
#define CUSTOM_PALETTE_ENTRY_HEIGHT  20

/* The cursor for the dropper */
#define DROPPER_WIDTH 17
#define DROPPER_HEIGHT 17
#define DROPPER_STRIDE 4
#define DROPPER_X_HOT 2
#define DROPPER_Y_HOT 16

#define SAMPLE_WIDTH  64
#define SAMPLE_HEIGHT 28
#define CHECK_SIZE 16  
#define BIG_STEP 20

/* Conversion between 0->1 double and and guint16. See
 * scale_round() below for more general conversions
 */
#define SCALE(i) (i / 65535.)
#define UNSCALE(d) ((guint16)(d * 65535 + 0.5))
#define INTENSITY(r, g, b) ((r) * 0.30 + (g) * 0.59 + (b) * 0.11)

enum {
  COLOR_CHANGED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_HAS_PALETTE,
  PROP_HAS_OPACITY_CONTROL,
  PROP_CURRENT_COLOR,
  PROP_CURRENT_ALPHA
};

enum {
  COLORSEL_RED = 0,
  COLORSEL_GREEN = 1,
  COLORSEL_BLUE = 2,
  COLORSEL_OPACITY = 3,
  COLORSEL_HUE,
  COLORSEL_SATURATION,
  COLORSEL_VALUE,
  COLORSEL_NUM_CHANNELS
};

typedef struct _ColorSelectionPrivate ColorSelectionPrivate;

struct _ColorSelectionPrivate
{
  guint has_opacity : 1;
  guint has_palette : 1;
  guint changing : 1;
  guint default_set : 1;
  guint default_alpha_set : 1;
  guint has_grab : 1;
  
  gdouble color[COLORSEL_NUM_CHANNELS];
  gdouble old_color[COLORSEL_NUM_CHANNELS];
  
  GtkWidget *triangle_colorsel;
  GtkWidget *hue_spinbutton;
  GtkWidget *sat_spinbutton;
  GtkWidget *val_spinbutton;
  GtkWidget *red_spinbutton;
  GtkWidget *green_spinbutton;
  GtkWidget *blue_spinbutton;
  GtkWidget *opacity_slider;
  GtkWidget *opacity_label;
  GtkWidget *opacity_entry;
  GtkWidget *palette_frame;
  GtkWidget *hex_entry;
  
  /* The Palette code */
  GtkWidget *custom_palette [GTK_CUSTOM_PALETTE_WIDTH][GTK_CUSTOM_PALETTE_HEIGHT];
  
  /* The color_sample stuff */
  GtkWidget *sample_area;
  GtkWidget *old_sample;
  GtkWidget *cur_sample;
  GtkWidget *colorsel;

  /* Window for grabbing on */
  GtkWidget *dropper_grab_widget;
  guint32    grab_time;

  /* Connection to settings */
  gulong settings_connection;
};


static void gtk_color_selection_destroy		(GtkObject		 *object);
static void gtk_color_selection_finalize        (GObject		 *object);
static void update_color			(GtkColorSelection	 *colorsel);
static void gtk_color_selection_set_property    (GObject                 *object,
					         guint                    prop_id,
					         const GValue            *value,
					         GParamSpec              *pspec);
static void gtk_color_selection_get_property    (GObject                 *object,
					         guint                    prop_id,
					         GValue                  *value,
					         GParamSpec              *pspec);

static void gtk_color_selection_realize         (GtkWidget               *widget);
static void gtk_color_selection_unrealize       (GtkWidget               *widget);
static void gtk_color_selection_show_all        (GtkWidget               *widget);
static gboolean gtk_color_selection_grab_broken (GtkWidget               *widget,
						 GdkEventGrabBroken      *event);

static void     gtk_color_selection_set_palette_color   (GtkColorSelection *colorsel,
                                                         gint               index,
                                                         GdkColor          *color);
static void     set_focus_line_attributes               (GtkWidget         *drawing_area,
							 cairo_t           *cr,
							 gint              *focus_width);
static void     default_noscreen_change_palette_func    (const GdkColor    *colors,
							 gint               n_colors);
static void     default_change_palette_func             (GdkScreen	   *screen,
							 const GdkColor    *colors,
							 gint               n_colors);
static void     make_control_relations                  (AtkObject         *atk_obj,
                                                         GtkWidget         *widget);
static void     make_all_relations                      (AtkObject         *atk_obj,
                                                         ColorSelectionPrivate *priv);

static void 	hsv_changed                             (GtkWidget         *hsv,
							 gpointer           data);
static void 	get_screen_color                        (GtkWidget         *button);
static void 	adjustment_changed                      (GtkAdjustment     *adjustment,
							 gpointer           data);
static void 	opacity_entry_changed                   (GtkWidget 	   *opacity_entry,
							 gpointer  	    data);
static void 	hex_changed                             (GtkWidget 	   *hex_entry,
							 gpointer  	    data);
static gboolean hex_focus_out                           (GtkWidget     	   *hex_entry, 
							 GdkEventFocus 	   *event,
							 gpointer      	    data);
static void 	color_sample_new                        (GtkColorSelection *colorsel);
static void 	make_label_spinbutton     		(GtkColorSelection *colorsel,
	    				  		 GtkWidget        **spinbutton,
	    				  		 gchar             *text,
	    				  		 GtkWidget         *table,
	    				  		 gint               i,
	    				  		 gint               j,
	    				  		 gint               channel_type,
	    				  		 const gchar       *tooltip);
static void 	make_palette_frame                      (GtkColorSelection *colorsel,
							 GtkWidget         *table,
							 gint               i,
							 gint               j);
static void 	set_selected_palette                    (GtkColorSelection *colorsel,
							 int                x,
							 int                y);
static void 	set_focus_line_attributes               (GtkWidget 	   *drawing_area,
							 cairo_t   	   *cr,
							 gint      	   *focus_width);
static gboolean mouse_press 		     	       	(GtkWidget         *invisible,
                            		     	       	 GdkEventButton    *event,
                            		     	       	 gpointer           data);
static void  palette_change_notify_instance (GObject    *object,
					     GParamSpec *pspec,
					     gpointer    data);
static void update_palette (GtkColorSelection *colorsel);
static void shutdown_eyedropper (GtkWidget *widget);

static guint color_selection_signals[LAST_SIGNAL] = { 0 };

static GtkColorSelectionChangePaletteFunc noscreen_change_palette_hook = default_noscreen_change_palette_func;
static GtkColorSelectionChangePaletteWithScreenFunc change_palette_hook = default_change_palette_func;

static const guchar dropper_bits[] = {
  0xff, 0x8f, 0x01, 0x00,  0xff, 0x77, 0x01, 0x00,
  0xff, 0xfb, 0x00, 0x00,  0xff, 0xf8, 0x00, 0x00,
  0x7f, 0xff, 0x00, 0x00,  0xff, 0x7e, 0x01, 0x00,
  0xff, 0x9d, 0x01, 0x00,  0xff, 0xd8, 0x01, 0x00,
  0x7f, 0xd4, 0x01, 0x00,  0x3f, 0xee, 0x01, 0x00,
  0x1f, 0xff, 0x01, 0x00,  0x8f, 0xff, 0x01, 0x00,
  0xc7, 0xff, 0x01, 0x00,  0xe3, 0xff, 0x01, 0x00,
  0xf3, 0xff, 0x01, 0x00,  0xfd, 0xff, 0x01, 0x00,
  0xff, 0xff, 0x01, 0x00 };

static const guchar dropper_mask[] = {
  0x00, 0x70, 0x00, 0x00,  0x00, 0xf8, 0x00, 0x00,
  0x00, 0xfc, 0x01, 0x00,  0x00, 0xff, 0x01, 0x00,
  0x80, 0xff, 0x01, 0x00,  0x00, 0xff, 0x00, 0x00,
  0x00, 0x7f, 0x00, 0x00,  0x80, 0x3f, 0x00, 0x00,
  0xc0, 0x3f, 0x00, 0x00,  0xe0, 0x13, 0x00, 0x00,
  0xf0, 0x01, 0x00, 0x00,  0xf8, 0x00, 0x00, 0x00,
  0x7c, 0x00, 0x00, 0x00,  0x3e, 0x00, 0x00, 0x00,
  0x1e, 0x00, 0x00, 0x00,  0x0d, 0x00, 0x00, 0x00,
  0x02, 0x00, 0x00, 0x00 };

G_DEFINE_TYPE (GtkColorSelection, gtk_color_selection, GTK_TYPE_VBOX)

static void
gtk_color_selection_class_init (GtkColorSelectionClass *klass)
{
  GObjectClass *gobject_class;
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  
  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = gtk_color_selection_finalize;
  gobject_class->set_property = gtk_color_selection_set_property;
  gobject_class->get_property = gtk_color_selection_get_property;

  object_class = GTK_OBJECT_CLASS (klass);
  object_class->destroy = gtk_color_selection_destroy;
  
  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->realize = gtk_color_selection_realize;
  widget_class->unrealize = gtk_color_selection_unrealize;
  widget_class->show_all = gtk_color_selection_show_all;
  widget_class->grab_broken_event = gtk_color_selection_grab_broken;
  
  g_object_class_install_property (gobject_class,
                                   PROP_HAS_OPACITY_CONTROL,
                                   g_param_spec_boolean ("has-opacity-control",
							 P_("Has Opacity Control"),
							 P_("Whether the color selector should allow setting opacity"),
							 FALSE,
							 GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_HAS_PALETTE,
                                   g_param_spec_boolean ("has-palette",
							 P_("Has palette"),
							 P_("Whether a palette should be used"),
							 FALSE,
							 GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_CURRENT_COLOR,
                                   g_param_spec_boxed ("current-color",
                                                       P_("Current Color"),
                                                       P_("The current color"),
                                                       GDK_TYPE_COLOR,
                                                       GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_CURRENT_ALPHA,
                                   g_param_spec_uint ("current-alpha",
						      P_("Current Alpha"),
						      P_("The current opacity value (0 fully transparent, 65535 fully opaque)"),
						      0, 65535, 65535,
						      GTK_PARAM_READWRITE));
  
  color_selection_signals[COLOR_CHANGED] =
    g_signal_new (I_("color-changed"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkColorSelectionClass, color_changed),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  g_type_class_add_private (gobject_class, sizeof (ColorSelectionPrivate));
}

static void
gtk_color_selection_init (GtkColorSelection *colorsel)
{
  GtkWidget *top_hbox;
  GtkWidget *top_right_vbox;
  GtkWidget *table, *label, *hbox, *frame, *vbox, *button;
  GtkAdjustment *adjust;
  GtkWidget *picker_image;
  gint i, j;
  ColorSelectionPrivate *priv;
  AtkObject *atk_obj;
  GList *focus_chain = NULL;
  
  gtk_widget_push_composite_child ();

  priv = colorsel->private_data = G_TYPE_INSTANCE_GET_PRIVATE (colorsel, GTK_TYPE_COLOR_SELECTION, ColorSelectionPrivate);
  priv->changing = FALSE;
  priv->default_set = FALSE;
  priv->default_alpha_set = FALSE;
  
  top_hbox = gtk_hbox_new (FALSE, 12);
  gtk_box_pack_start (GTK_BOX (colorsel), top_hbox, FALSE, FALSE, 0);
  
  vbox = gtk_vbox_new (FALSE, 6);
  priv->triangle_colorsel = gtk_hsv_new ();
  g_signal_connect (priv->triangle_colorsel, "changed",
                    G_CALLBACK (hsv_changed), colorsel);
  gtk_hsv_set_metrics (GTK_HSV (priv->triangle_colorsel), 174, 15);
  gtk_box_pack_start (GTK_BOX (top_hbox), vbox, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), priv->triangle_colorsel, FALSE, FALSE, 0);
  gtk_widget_set_tooltip_text (priv->triangle_colorsel,
                        _("Select the color you want from the outer ring. Select the darkness or lightness of that color using the inner triangle."));
  
  hbox = gtk_hbox_new (FALSE, 6);
  gtk_box_pack_end (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  
  frame = gtk_frame_new (NULL);
  gtk_widget_set_size_request (frame, -1, 30);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  color_sample_new (colorsel);
  gtk_container_add (GTK_CONTAINER (frame), priv->sample_area);
  gtk_box_pack_start (GTK_BOX (hbox), frame, TRUE, TRUE, 0);
  
  button = gtk_button_new ();

  gtk_widget_set_events (button, GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);
  g_object_set_data (G_OBJECT (button), I_("COLORSEL"), colorsel); 
  g_signal_connect (button, "clicked",
                    G_CALLBACK (get_screen_color), NULL);
  picker_image = gtk_image_new_from_stock (GTK_STOCK_COLOR_PICKER, GTK_ICON_SIZE_BUTTON);
  gtk_container_add (GTK_CONTAINER (button), picker_image);
  gtk_widget_show (GTK_WIDGET (picker_image));
  gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);

  gtk_widget_set_tooltip_text (button,
                        _("Click the eyedropper, then click a color anywhere on your screen to select that color."));
  
  top_right_vbox = gtk_vbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (top_hbox), top_right_vbox, FALSE, FALSE, 0);
  table = gtk_table_new (8, 6, FALSE);
  gtk_box_pack_start (GTK_BOX (top_right_vbox), table, FALSE, FALSE, 0);
  gtk_table_set_row_spacings (GTK_TABLE (table), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table), 12);
  
  make_label_spinbutton (colorsel, &priv->hue_spinbutton, _("_Hue:"), table, 0, 0, COLORSEL_HUE,
                         _("Position on the color wheel."));
  gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (priv->hue_spinbutton), TRUE);
  make_label_spinbutton (colorsel, &priv->sat_spinbutton, _("_Saturation:"), table, 0, 1, COLORSEL_SATURATION,
                         _("\"Deepness\" of the color."));
  make_label_spinbutton (colorsel, &priv->val_spinbutton, _("_Value:"), table, 0, 2, COLORSEL_VALUE,
                         _("Brightness of the color."));
  make_label_spinbutton (colorsel, &priv->red_spinbutton, _("_Red:"), table, 6, 0, COLORSEL_RED,
                         _("Amount of red light in the color."));
  make_label_spinbutton (colorsel, &priv->green_spinbutton, _("_Green:"), table, 6, 1, COLORSEL_GREEN,
                         _("Amount of green light in the color."));
  make_label_spinbutton (colorsel, &priv->blue_spinbutton, _("_Blue:"), table, 6, 2, COLORSEL_BLUE,
                         _("Amount of blue light in the color."));
  gtk_table_attach_defaults (GTK_TABLE (table), gtk_hseparator_new (), 0, 8, 3, 4); 

  priv->opacity_label = gtk_label_new_with_mnemonic (_("Op_acity:")); 
  gtk_misc_set_alignment (GTK_MISC (priv->opacity_label), 0.0, 0.5); 
  gtk_table_attach_defaults (GTK_TABLE (table), priv->opacity_label, 0, 1, 4, 5); 
  adjust = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 255.0, 1.0, 1.0, 0.0)); 
  g_object_set_data (G_OBJECT (adjust), I_("COLORSEL"), colorsel); 
  priv->opacity_slider = gtk_hscale_new (adjust);
  gtk_widget_set_tooltip_text (priv->opacity_slider,
                        _("Transparency of the color."));
  gtk_label_set_mnemonic_widget (GTK_LABEL (priv->opacity_label),
                                 priv->opacity_slider);
  gtk_scale_set_draw_value (GTK_SCALE (priv->opacity_slider), FALSE);
  g_signal_connect (adjust, "value-changed",
                    G_CALLBACK (adjustment_changed),
                    GINT_TO_POINTER (COLORSEL_OPACITY));
  gtk_table_attach_defaults (GTK_TABLE (table), priv->opacity_slider, 1, 7, 4, 5); 
  priv->opacity_entry = gtk_entry_new (); 
  gtk_widget_set_tooltip_text (priv->opacity_entry,
                        _("Transparency of the color."));
  gtk_widget_set_size_request (priv->opacity_entry, 40, -1); 

  g_signal_connect (priv->opacity_entry, "activate",
                    G_CALLBACK (opacity_entry_changed), colorsel);
  gtk_table_attach_defaults (GTK_TABLE (table), priv->opacity_entry, 7, 8, 4, 5);
  
  label = gtk_label_new_with_mnemonic (_("Color _name:"));
  gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 5, 6);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  priv->hex_entry = gtk_entry_new ();

  gtk_label_set_mnemonic_widget (GTK_LABEL (label), priv->hex_entry);

  g_signal_connect (priv->hex_entry, "activate",
                    G_CALLBACK (hex_changed), colorsel);

  g_signal_connect (priv->hex_entry, "focus-out-event",
                    G_CALLBACK (hex_focus_out), colorsel);

  gtk_widget_set_tooltip_text (priv->hex_entry,
                        _("You can enter an HTML-style hexadecimal color value, or simply a color name such as 'orange' in this entry."));
  
  gtk_entry_set_width_chars (GTK_ENTRY (priv->hex_entry), 7);
  gtk_table_attach_defaults (GTK_TABLE (table), priv->hex_entry, 1, 5, 5, 6);

  focus_chain = g_list_append (focus_chain, priv->hue_spinbutton);
  focus_chain = g_list_append (focus_chain, priv->sat_spinbutton);
  focus_chain = g_list_append (focus_chain, priv->val_spinbutton);
  focus_chain = g_list_append (focus_chain, priv->red_spinbutton);
  focus_chain = g_list_append (focus_chain, priv->green_spinbutton);
  focus_chain = g_list_append (focus_chain, priv->blue_spinbutton);
  focus_chain = g_list_append (focus_chain, priv->opacity_slider);
  focus_chain = g_list_append (focus_chain, priv->opacity_entry);
  focus_chain = g_list_append (focus_chain, priv->hex_entry);
  gtk_container_set_focus_chain (GTK_CONTAINER (table), focus_chain);
  g_list_free (focus_chain);

  /* Set up the palette */
  table = gtk_table_new (GTK_CUSTOM_PALETTE_HEIGHT, GTK_CUSTOM_PALETTE_WIDTH, TRUE);
  gtk_table_set_row_spacings (GTK_TABLE (table), 1);
  gtk_table_set_col_spacings (GTK_TABLE (table), 1);
  for (i = 0; i < GTK_CUSTOM_PALETTE_WIDTH; i++)
    {
      for (j = 0; j < GTK_CUSTOM_PALETTE_HEIGHT; j++)
	{
	  make_palette_frame (colorsel, table, i, j);
	}
    }
  set_selected_palette (colorsel, 0, 0);
  priv->palette_frame = gtk_vbox_new (FALSE, 6);
  label = gtk_label_new_with_mnemonic (_("_Palette:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_box_pack_start (GTK_BOX (priv->palette_frame), label, FALSE, FALSE, 0);

  gtk_label_set_mnemonic_widget (GTK_LABEL (label),
                                 priv->custom_palette[0][0]);
  
  gtk_box_pack_end (GTK_BOX (top_right_vbox), priv->palette_frame, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (priv->palette_frame), table, FALSE, FALSE, 0);
  
  gtk_widget_show_all (top_hbox);

  /* hide unused stuff */
  
  if (priv->has_opacity == FALSE)
    {
      gtk_widget_hide (priv->opacity_label);
      gtk_widget_hide (priv->opacity_slider);
      gtk_widget_hide (priv->opacity_entry);
    }
  
  if (priv->has_palette == FALSE)
    {
      gtk_widget_hide (priv->palette_frame);
    }

  atk_obj = gtk_widget_get_accessible (priv->triangle_colorsel);
  if (GTK_IS_ACCESSIBLE (atk_obj))
    {
      atk_object_set_name (atk_obj, _("Color Wheel"));
      atk_object_set_role (gtk_widget_get_accessible (GTK_WIDGET (colorsel)), ATK_ROLE_COLOR_CHOOSER);
      make_all_relations (atk_obj, priv);
    } 

  gtk_widget_pop_composite_child ();
}

/* GObject methods */
static void
gtk_color_selection_finalize (GObject *object)
{
  G_OBJECT_CLASS (gtk_color_selection_parent_class)->finalize (object);
}

static void
gtk_color_selection_set_property (GObject         *object,
				  guint            prop_id,
				  const GValue    *value,
				  GParamSpec      *pspec)
{
  GtkColorSelection *colorsel = GTK_COLOR_SELECTION (object);
  
  switch (prop_id)
    {
    case PROP_HAS_OPACITY_CONTROL:
      gtk_color_selection_set_has_opacity_control (colorsel, 
						   g_value_get_boolean (value));
      break;
    case PROP_HAS_PALETTE:
      gtk_color_selection_set_has_palette (colorsel, 
					   g_value_get_boolean (value));
      break;
    case PROP_CURRENT_COLOR:
      gtk_color_selection_set_current_color (colorsel, g_value_get_boxed (value));
      break;
    case PROP_CURRENT_ALPHA:
      gtk_color_selection_set_current_alpha (colorsel, g_value_get_uint (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
  
}

static void
gtk_color_selection_get_property (GObject     *object,
				  guint        prop_id,
				  GValue      *value,
				  GParamSpec  *pspec)
{
  GtkColorSelection *colorsel = GTK_COLOR_SELECTION (object);
  GdkColor color;
  
  switch (prop_id)
    {
    case PROP_HAS_OPACITY_CONTROL:
      g_value_set_boolean (value, gtk_color_selection_get_has_opacity_control (colorsel));
      break;
    case PROP_HAS_PALETTE:
      g_value_set_boolean (value, gtk_color_selection_get_has_palette (colorsel));
      break;
    case PROP_CURRENT_COLOR:
      gtk_color_selection_get_current_color (colorsel, &color);
      g_value_set_boxed (value, &color);
      break;
    case PROP_CURRENT_ALPHA:
      g_value_set_uint (value, gtk_color_selection_get_current_alpha (colorsel));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/* GtkObject methods */

static void
gtk_color_selection_destroy (GtkObject *object)
{
  GtkColorSelection *cselection = GTK_COLOR_SELECTION (object);
  ColorSelectionPrivate *priv = cselection->private_data;

  if (priv->dropper_grab_widget)
    {
      gtk_widget_destroy (priv->dropper_grab_widget);
      priv->dropper_grab_widget = NULL;
    }

  GTK_OBJECT_CLASS (gtk_color_selection_parent_class)->destroy (object);
}

/* GtkWidget methods */

static void
gtk_color_selection_realize (GtkWidget *widget)
{
  GtkColorSelection *colorsel = GTK_COLOR_SELECTION (widget);
  ColorSelectionPrivate *priv = colorsel->private_data;
  GtkSettings *settings = gtk_widget_get_settings (widget);

  priv->settings_connection =  g_signal_connect (settings,
						 "notify::gtk-color-palette",
						 G_CALLBACK (palette_change_notify_instance),
						 widget);
  update_palette (colorsel);

  GTK_WIDGET_CLASS (gtk_color_selection_parent_class)->realize (widget);
}

static void
gtk_color_selection_unrealize (GtkWidget *widget)
{
  GtkColorSelection *colorsel = GTK_COLOR_SELECTION (widget);
  ColorSelectionPrivate *priv = colorsel->private_data;
  GtkSettings *settings = gtk_widget_get_settings (widget);

  g_signal_handler_disconnect (settings, priv->settings_connection);

  GTK_WIDGET_CLASS (gtk_color_selection_parent_class)->unrealize (widget);
}

/* We override show-all since we have internal widgets that
 * shouldn't be shown when you call show_all(), like the
 * palette and opacity sliders.
 */
static void
gtk_color_selection_show_all (GtkWidget *widget)
{
  gtk_widget_show (widget);
}

static gboolean 
gtk_color_selection_grab_broken (GtkWidget          *widget,
				 GdkEventGrabBroken *event)
{
  shutdown_eyedropper (widget);

  return TRUE;
}

/*
 *
 * The Sample Color
 *
 */

static void color_sample_draw_sample (GtkColorSelection *colorsel, int which);
static void color_sample_update_samples (GtkColorSelection *colorsel);

static void
set_color_internal (GtkColorSelection *colorsel,
		    gdouble           *color)
{
  ColorSelectionPrivate *priv;
  gint i;
  
  priv = colorsel->private_data;
  priv->changing = TRUE;
  priv->color[COLORSEL_RED] = color[0];
  priv->color[COLORSEL_GREEN] = color[1];
  priv->color[COLORSEL_BLUE] = color[2];
  priv->color[COLORSEL_OPACITY] = color[3];
  gtk_rgb_to_hsv (priv->color[COLORSEL_RED],
		  priv->color[COLORSEL_GREEN],
		  priv->color[COLORSEL_BLUE],
		  &priv->color[COLORSEL_HUE],
		  &priv->color[COLORSEL_SATURATION],
		  &priv->color[COLORSEL_VALUE]);
  if (priv->default_set == FALSE)
    {
      for (i = 0; i < COLORSEL_NUM_CHANNELS; i++)
	priv->old_color[i] = priv->color[i];
    }
  priv->default_set = TRUE;
  priv->default_alpha_set = TRUE;
  update_color (colorsel);
}

static void
set_color_icon (GdkDragContext *context,
		gdouble        *colors)
{
  GdkPixbuf *pixbuf;
  guint32 pixel;

  pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE,
			   8, 48, 32);

  pixel = (((UNSCALE (colors[COLORSEL_RED])   & 0xff00) << 16) |
	   ((UNSCALE (colors[COLORSEL_GREEN]) & 0xff00) << 8) |
	   ((UNSCALE (colors[COLORSEL_BLUE])  & 0xff00)));

  gdk_pixbuf_fill (pixbuf, pixel);
  
  gtk_drag_set_icon_pixbuf (context, pixbuf, -2, -2);
  g_object_unref (pixbuf);
}

static void
color_sample_drag_begin (GtkWidget      *widget,
			 GdkDragContext *context,
			 gpointer        data)
{
  GtkColorSelection *colorsel = data;
  ColorSelectionPrivate *priv;
  gdouble *colsrc;
  
  priv = colorsel->private_data;
  
  if (widget == priv->old_sample)
    colsrc = priv->old_color;
  else
    colsrc = priv->color;

  set_color_icon (context, colsrc);
}

static void
color_sample_drag_end (GtkWidget      *widget,
		       GdkDragContext *context,
		       gpointer        data)
{
  g_object_set_data (G_OBJECT (widget), I_("gtk-color-selection-drag-window"), NULL);
}

static void
color_sample_drop_handle (GtkWidget        *widget,
			  GdkDragContext   *context,
			  gint              x,
			  gint              y,
			  GtkSelectionData *selection_data,
			  guint             info,
			  guint             time,
			  gpointer          data)
{
  GtkColorSelection *colorsel = data;
  ColorSelectionPrivate *priv;
  guint16 *vals;
  gdouble color[4];
  priv = colorsel->private_data;
  
  /* This is currently a guint16 array of the format:
   * R
   * G
   * B
   * opacity
   */
  
  if (selection_data->length < 0)
    return;
  
  /* We accept drops with the wrong format, since the KDE color
   * chooser incorrectly drops application/x-color with format 8.
   */
  if (selection_data->length != 8)
    {
      g_warning ("Received invalid color data\n");
      return;
    }
  
  vals = (guint16 *)selection_data->data;
  
  if (widget == priv->cur_sample)
    {
      color[0] = (gdouble)vals[0] / 0xffff;
      color[1] = (gdouble)vals[1] / 0xffff;
      color[2] = (gdouble)vals[2] / 0xffff;
      color[3] = (gdouble)vals[3] / 0xffff;
      
      set_color_internal (colorsel, color);
    }
}

static void
color_sample_drag_handle (GtkWidget        *widget,
			  GdkDragContext   *context,
			  GtkSelectionData *selection_data,
			  guint             info,
			  guint             time,
			  gpointer          data)
{
  GtkColorSelection *colorsel = data;
  ColorSelectionPrivate *priv;
  guint16 vals[4];
  gdouble *colsrc;
  
  priv = colorsel->private_data;
  
  if (widget == priv->old_sample)
    colsrc = priv->old_color;
  else
    colsrc = priv->color;
  
  vals[0] = colsrc[COLORSEL_RED] * 0xffff;
  vals[1] = colsrc[COLORSEL_GREEN] * 0xffff;
  vals[2] = colsrc[COLORSEL_BLUE] * 0xffff;
  vals[3] = priv->has_opacity ? colsrc[COLORSEL_OPACITY] * 0xffff : 0xffff;
  
  gtk_selection_data_set (selection_data,
			  gdk_atom_intern_static_string ("application/x-color"),
			  16, (guchar *)vals, 8);
}

/* which = 0 means draw old sample, which = 1 means draw new */
static void
color_sample_draw_sample (GtkColorSelection *colorsel, int which)
{
  GtkWidget *da;
  gint x, y, wid, heig, goff;
  ColorSelectionPrivate *priv;
  cairo_t *cr;
  
  g_return_if_fail (colorsel != NULL);
  priv = colorsel->private_data;
  
  g_return_if_fail (priv->sample_area != NULL);
  if (!gtk_widget_is_drawable (priv->sample_area))
    return;

  if (which == 0)
    {
      da = priv->old_sample;
      goff = 0;
    }
  else
    {
      da = priv->cur_sample;
      goff =  priv->old_sample->allocation.width % 32;
    }

  cr = gdk_cairo_create (da->window);
  
  wid = da->allocation.width;
  heig = da->allocation.height;

  /* Below needs tweaking for non-power-of-two */  
  
  if (priv->has_opacity)
    {
      /* Draw checks in background */

      cairo_set_source_rgb (cr, 0.5, 0.5, 0.5);
      cairo_rectangle (cr, 0, 0, wid, heig);
      cairo_fill (cr);

      cairo_set_source_rgb (cr, 0.75, 0.75, 0.75);
      for (x = goff & -CHECK_SIZE; x < goff + wid; x += CHECK_SIZE)
	for (y = 0; y < heig; y += CHECK_SIZE)
	  if ((x / CHECK_SIZE + y / CHECK_SIZE) % 2 == 0)
	    cairo_rectangle (cr, x - goff, y, CHECK_SIZE, CHECK_SIZE);
      cairo_fill (cr);
    }

  if (which == 0)
    {
      cairo_set_source_rgba (cr,
			     priv->old_color[COLORSEL_RED], 
			     priv->old_color[COLORSEL_GREEN], 
			     priv->old_color[COLORSEL_BLUE],
			     priv->has_opacity ?
			        priv->old_color[COLORSEL_OPACITY] : 1.0);
    }
  else
    {
      cairo_set_source_rgba (cr,
			     priv->color[COLORSEL_RED], 
			     priv->color[COLORSEL_GREEN], 
			     priv->color[COLORSEL_BLUE],
			     priv->has_opacity ?
			       priv->color[COLORSEL_OPACITY] : 1.0);
    }

  cairo_rectangle (cr, 0, 0, wid, heig);
  cairo_fill (cr);

  cairo_destroy (cr);
}


static void
color_sample_update_samples (GtkColorSelection *colorsel)
{
  ColorSelectionPrivate *priv = colorsel->private_data;
  gtk_widget_queue_draw (priv->old_sample);
  gtk_widget_queue_draw (priv->cur_sample);
}

static gboolean
color_old_sample_expose (GtkWidget         *da,
			 GdkEventExpose    *event,
			 GtkColorSelection *colorsel)
{
  color_sample_draw_sample (colorsel, 0);
  return FALSE;
}


static gboolean
color_cur_sample_expose (GtkWidget         *da,
			 GdkEventExpose    *event,
			 GtkColorSelection *colorsel)
{
  color_sample_draw_sample (colorsel, 1);
  return FALSE;
}

static void
color_sample_setup_dnd (GtkColorSelection *colorsel, GtkWidget *sample)
{
  static const GtkTargetEntry targets[] = {
    { "application/x-color", 0 }
  };
  ColorSelectionPrivate *priv;
  priv = colorsel->private_data;
  
  gtk_drag_source_set (sample,
		       GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
		       targets, 1,
		       GDK_ACTION_COPY | GDK_ACTION_MOVE);
  
  g_signal_connect (sample, "drag-begin",
		    G_CALLBACK (color_sample_drag_begin),
		    colorsel);
  if (sample == priv->cur_sample)
    {
      
      gtk_drag_dest_set (sample,
			 GTK_DEST_DEFAULT_HIGHLIGHT |
			 GTK_DEST_DEFAULT_MOTION |
			 GTK_DEST_DEFAULT_DROP,
			 targets, 1,
			 GDK_ACTION_COPY);
      
      g_signal_connect (sample, "drag-end",
			G_CALLBACK (color_sample_drag_end),
			colorsel);
    }
  
  g_signal_connect (sample, "drag-data-get",
		    G_CALLBACK (color_sample_drag_handle),
		    colorsel);
  g_signal_connect (sample, "drag-data-received",
		    G_CALLBACK (color_sample_drop_handle),
		    colorsel);
  
}

static void
update_tooltips (GtkColorSelection *colorsel)
{
  ColorSelectionPrivate *priv;

  priv = colorsel->private_data;

  if (priv->has_palette == TRUE)
    {
      gtk_widget_set_tooltip_text (priv->old_sample,
                            _("The previously-selected color, for comparison to the color you're selecting now. You can drag this color to a palette entry, or select this color as current by dragging it to the other color swatch alongside."));

      gtk_widget_set_tooltip_text (priv->cur_sample,
                            _("The color you've chosen. You can drag this color to a palette entry to save it for use in the future."));
    }
  else
    {
      gtk_widget_set_tooltip_text (priv->old_sample,
                            _("The previously-selected color, for comparison to the color you're selecting now."));

      gtk_widget_set_tooltip_text (priv->cur_sample,
                            _("The color you've chosen."));
    }
}

static void
color_sample_new (GtkColorSelection *colorsel)
{
  ColorSelectionPrivate *priv;
  
  priv = colorsel->private_data;
  
  priv->sample_area = gtk_hbox_new (FALSE, 0);
  priv->old_sample = gtk_drawing_area_new ();
  priv->cur_sample = gtk_drawing_area_new ();

  gtk_box_pack_start (GTK_BOX (priv->sample_area), priv->old_sample,
		      TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (priv->sample_area), priv->cur_sample,
		      TRUE, TRUE, 0);
  
  g_signal_connect (priv->old_sample, "expose-event",
		    G_CALLBACK (color_old_sample_expose),
		    colorsel);
  g_signal_connect (priv->cur_sample, "expose-event",
		    G_CALLBACK (color_cur_sample_expose),
		    colorsel);
  
  color_sample_setup_dnd (colorsel, priv->old_sample);
  color_sample_setup_dnd (colorsel, priv->cur_sample);

  update_tooltips (colorsel);

  gtk_widget_show_all (priv->sample_area);
}


/*
 *
 * The palette area code
 *
 */

static void
palette_get_color (GtkWidget *drawing_area, gdouble *color)
{
  gdouble *color_val;
  
  g_return_if_fail (color != NULL);
  
  color_val = g_object_get_data (G_OBJECT (drawing_area), "color_val");
  if (color_val == NULL)
    {
      /* Default to white for no good reason */
      color[0] = 1.0;
      color[1] = 1.0;
      color[2] = 1.0;
      color[3] = 1.0;
      return;
    }
  
  color[0] = color_val[0];
  color[1] = color_val[1];
  color[2] = color_val[2];
  color[3] = 1.0;
}

static void
palette_paint (GtkWidget    *drawing_area,
	       GdkRectangle *area,
	       gpointer      data)
{
  cairo_t *cr;
  gint focus_width;
    
  if (drawing_area->window == NULL)
    return;

  cr = gdk_cairo_create (drawing_area->window);

  gdk_cairo_set_source_color (cr, &drawing_area->style->bg[GTK_STATE_NORMAL]);
  gdk_cairo_rectangle (cr, area);
  cairo_fill (cr);
  
  if (gtk_widget_has_focus (drawing_area))
    {
      set_focus_line_attributes (drawing_area, cr, &focus_width);

      cairo_rectangle (cr,
		       focus_width / 2., focus_width / 2.,
		       drawing_area->allocation.width - focus_width,
		       drawing_area->allocation.height - focus_width);
      cairo_stroke (cr);
    }

  cairo_destroy (cr);
}

static void
set_focus_line_attributes (GtkWidget *drawing_area,
			   cairo_t   *cr,
			   gint      *focus_width)
{
  gdouble color[4];
  gint8 *dash_list;
  
  gtk_widget_style_get (drawing_area,
			"focus-line-width", focus_width,
			"focus-line-pattern", (gchar *)&dash_list,
			NULL);
      
  palette_get_color (drawing_area, color);

  if (INTENSITY (color[0], color[1], color[2]) > 0.5)
    cairo_set_source_rgb (cr, 0., 0., 0.);
  else
    cairo_set_source_rgb (cr, 1., 1., 1.);

  cairo_set_line_width (cr, *focus_width);

  if (dash_list[0])
    {
      gint n_dashes = strlen ((gchar *)dash_list);
      gdouble *dashes = g_new (gdouble, n_dashes);
      gdouble total_length = 0;
      gdouble dash_offset;
      gint i;

      for (i = 0; i < n_dashes; i++)
	{
	  dashes[i] = dash_list[i];
	  total_length += dash_list[i];
	}

      /* The dash offset here aligns the pattern to integer pixels
       * by starting the dash at the right side of the left border
       * Negative dash offsets in cairo don't work
       * (https://bugs.freedesktop.org/show_bug.cgi?id=2729)
       */
      dash_offset = - *focus_width / 2.;
      while (dash_offset < 0)
	dash_offset += total_length;
      
      cairo_set_dash (cr, dashes, n_dashes, dash_offset);
      g_free (dashes);
    }

  g_free (dash_list);
}

static void
palette_drag_begin (GtkWidget      *widget,
		    GdkDragContext *context,
		    gpointer        data)
{
  gdouble colors[4];
  
  palette_get_color (widget, colors);
  set_color_icon (context, colors);
}

static void
palette_drag_handle (GtkWidget        *widget,
		     GdkDragContext   *context,
		     GtkSelectionData *selection_data,
		     guint             info,
		     guint             time,
		     gpointer          data)
{
  guint16 vals[4];
  gdouble colsrc[4];
  
  palette_get_color (widget, colsrc);
  
  vals[0] = colsrc[COLORSEL_RED] * 0xffff;
  vals[1] = colsrc[COLORSEL_GREEN] * 0xffff;
  vals[2] = colsrc[COLORSEL_BLUE] * 0xffff;
  vals[3] = 0xffff;
  
  gtk_selection_data_set (selection_data,
			  gdk_atom_intern_static_string ("application/x-color"),
			  16, (guchar *)vals, 8);
}

static void
palette_drag_end (GtkWidget      *widget,
		  GdkDragContext *context,
		  gpointer        data)
{
  g_object_set_data (G_OBJECT (widget), I_("gtk-color-selection-drag-window"), NULL);
}

static GdkColor *
get_current_colors (GtkColorSelection *colorsel)
{
  GtkSettings *settings;
  GdkColor *colors = NULL;
  gint n_colors = 0;
  gchar *palette;

  settings = gtk_widget_get_settings (GTK_WIDGET (colorsel));
  g_object_get (settings, "gtk-color-palette", &palette, NULL);
  
  if (!gtk_color_selection_palette_from_string (palette, &colors, &n_colors))
    {
      gtk_color_selection_palette_from_string (DEFAULT_COLOR_PALETTE,
                                               &colors,
                                               &n_colors);
    }
  else
    {
      /* If there are less colors provided than the number of slots in the
       * color selection, we fill in the rest from the defaults.
       */
      if (n_colors < (GTK_CUSTOM_PALETTE_WIDTH * GTK_CUSTOM_PALETTE_HEIGHT))
	{
	  GdkColor *tmp_colors = colors;
	  gint tmp_n_colors = n_colors;
	  
	  gtk_color_selection_palette_from_string (DEFAULT_COLOR_PALETTE,
                                                   &colors,
                                                   &n_colors);
	  memcpy (colors, tmp_colors, sizeof (GdkColor) * tmp_n_colors);

	  g_free (tmp_colors);
	}
    }

  /* make sure that we fill every slot */
  g_assert (n_colors == GTK_CUSTOM_PALETTE_WIDTH * GTK_CUSTOM_PALETTE_HEIGHT);
  g_free (palette);
  
  return colors;
}

/* Changes the model color */
static void
palette_change_color (GtkWidget         *drawing_area,
                      GtkColorSelection *colorsel,
                      gdouble           *color)
{
  gint x, y;
  ColorSelectionPrivate *priv;
  GdkColor gdk_color;
  GdkColor *current_colors;
  GdkScreen *screen;

  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));
  g_return_if_fail (GTK_IS_DRAWING_AREA (drawing_area));
  
  priv = colorsel->private_data;
  
  gdk_color.red = UNSCALE (color[0]);
  gdk_color.green = UNSCALE (color[1]);
  gdk_color.blue = UNSCALE (color[2]);
  gdk_color.pixel = 0;

  x = 0;
  y = 0;			/* Quiet GCC */
  while (x < GTK_CUSTOM_PALETTE_WIDTH)
    {
      y = 0;
      while (y < GTK_CUSTOM_PALETTE_HEIGHT)
        {
          if (priv->custom_palette[x][y] == drawing_area)
            goto out;
          
          ++y;
        }

      ++x;
    }

 out:
  
  g_assert (x < GTK_CUSTOM_PALETTE_WIDTH || y < GTK_CUSTOM_PALETTE_HEIGHT);

  current_colors = get_current_colors (colorsel);
  current_colors[y * GTK_CUSTOM_PALETTE_WIDTH + x] = gdk_color;

  screen = gtk_widget_get_screen (GTK_WIDGET (colorsel));
  if (change_palette_hook != default_change_palette_func)
    (* change_palette_hook) (screen, current_colors, 
			     GTK_CUSTOM_PALETTE_WIDTH * GTK_CUSTOM_PALETTE_HEIGHT);
  else if (noscreen_change_palette_hook != default_noscreen_change_palette_func)
    {
      if (screen != gdk_screen_get_default ())
	g_warning ("gtk_color_selection_set_change_palette_hook used by widget is not on the default screen.");
      (* noscreen_change_palette_hook) (current_colors, 
					GTK_CUSTOM_PALETTE_WIDTH * GTK_CUSTOM_PALETTE_HEIGHT);
    }
  else
    (* change_palette_hook) (screen, current_colors, 
			     GTK_CUSTOM_PALETTE_WIDTH * GTK_CUSTOM_PALETTE_HEIGHT);

  g_free (current_colors);
}

/* Changes the view color */
static void
palette_set_color (GtkWidget         *drawing_area,
		   GtkColorSelection *colorsel,
		   gdouble           *color)
{
  gdouble *new_color = g_new (double, 4);
  GdkColor gdk_color;
  
  gdk_color.red = UNSCALE (color[0]);
  gdk_color.green = UNSCALE (color[1]);
  gdk_color.blue = UNSCALE (color[2]);

  gtk_widget_modify_bg (drawing_area, GTK_STATE_NORMAL, &gdk_color);
  
  if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (drawing_area), "color_set")) == 0)
    {
      static const GtkTargetEntry targets[] = {
	{ "application/x-color", 0 }
      };
      gtk_drag_source_set (drawing_area,
			   GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
			   targets, 1,
			   GDK_ACTION_COPY | GDK_ACTION_MOVE);
      
      g_signal_connect (drawing_area, "drag-begin",
			G_CALLBACK (palette_drag_begin),
			colorsel);
      g_signal_connect (drawing_area, "drag-data-get",
			G_CALLBACK (palette_drag_handle),
			colorsel);
      
      g_object_set_data (G_OBJECT (drawing_area), I_("color_set"),
			 GINT_TO_POINTER (1));
    }

  new_color[0] = color[0];
  new_color[1] = color[1];
  new_color[2] = color[2];
  new_color[3] = 1.0;
  
  g_object_set_data_full (G_OBJECT (drawing_area), I_("color_val"), new_color, (GDestroyNotify)g_free);
}

static gboolean
palette_expose (GtkWidget      *drawing_area,
		GdkEventExpose *event,
		gpointer        data)
{
  if (drawing_area->window == NULL)
    return FALSE;
  
  palette_paint (drawing_area, &(event->area), data);
  
  return FALSE;
}

static void
popup_position_func (GtkMenu   *menu,
                     gint      *x,
                     gint      *y,
                     gboolean  *push_in,
                     gpointer	user_data)
{
  GtkWidget *widget;
  GtkRequisition req;      
  gint root_x, root_y;
  GdkScreen *screen;
  
  widget = GTK_WIDGET (user_data);
  
  g_return_if_fail (gtk_widget_get_realized (widget));

  gdk_window_get_origin (widget->window, &root_x, &root_y);
  
  gtk_widget_size_request (GTK_WIDGET (menu), &req);

  /* Put corner of menu centered on color cell */
  *x = root_x + widget->allocation.width / 2;
  *y = root_y + widget->allocation.height / 2;

  /* Ensure sanity */
  screen = gtk_widget_get_screen (widget);
  *x = CLAMP (*x, 0, MAX (0, gdk_screen_get_width (screen) - req.width));
  *y = CLAMP (*y, 0, MAX (0, gdk_screen_get_height (screen) - req.height));
}

static void
save_color_selected (GtkWidget *menuitem,
                     gpointer   data)
{
  GtkColorSelection *colorsel;
  GtkWidget *drawing_area;
  ColorSelectionPrivate *priv;

  drawing_area = GTK_WIDGET (data);
  
  colorsel = GTK_COLOR_SELECTION (g_object_get_data (G_OBJECT (drawing_area),
                                                     "gtk-color-sel"));

  priv = colorsel->private_data;
  
  palette_change_color (drawing_area, colorsel, priv->color);  
}

static void
do_popup (GtkColorSelection *colorsel,
          GtkWidget         *drawing_area,
          guint32            timestamp)
{
  GtkWidget *menu;
  GtkWidget *mi;
  
  g_object_set_data (G_OBJECT (drawing_area),
                     I_("gtk-color-sel"),
                     colorsel);
  
  menu = gtk_menu_new ();

  mi = gtk_menu_item_new_with_mnemonic (_("_Save color here"));

  g_signal_connect (mi, "activate",
                    G_CALLBACK (save_color_selected),
                    drawing_area);
  
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);

  gtk_widget_show_all (mi);

  gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
                  popup_position_func, drawing_area,
                  3, timestamp);
}


static gboolean
palette_enter (GtkWidget        *drawing_area,
	       GdkEventCrossing *event,
	       gpointer        data)
{
  g_object_set_data (G_OBJECT (drawing_area),
		     I_("gtk-colorsel-have-pointer"),
		     GUINT_TO_POINTER (TRUE));

  return FALSE;
}

static gboolean
palette_leave (GtkWidget        *drawing_area,
	       GdkEventCrossing *event,
	       gpointer        data)
{
  g_object_set_data (G_OBJECT (drawing_area),
		     I_("gtk-colorsel-have-pointer"),
		     NULL);

  return FALSE;
}

static gboolean
palette_press (GtkWidget      *drawing_area,
	       GdkEventButton *event,
	       gpointer        data)
{
  GtkColorSelection *colorsel = GTK_COLOR_SELECTION (data);

  gtk_widget_grab_focus (drawing_area);

  if (_gtk_button_event_triggers_context_menu (event))
    {
      do_popup (colorsel, drawing_area, event->time);
      return TRUE;
    }

  return FALSE;
}

static gboolean
palette_release (GtkWidget      *drawing_area,
		 GdkEventButton *event,
		 gpointer        data)
{
  GtkColorSelection *colorsel = GTK_COLOR_SELECTION (data);

  gtk_widget_grab_focus (drawing_area);

  if (event->button == 1 &&
      g_object_get_data (G_OBJECT (drawing_area),
			 "gtk-colorsel-have-pointer") != NULL)
    {
      if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (drawing_area), "color_set")) != 0)
        {
          gdouble color[4];
          palette_get_color (drawing_area, color);
          set_color_internal (colorsel, color);
        }
    }

  return FALSE;
}

static void
palette_drop_handle (GtkWidget        *widget,
		     GdkDragContext   *context,
		     gint              x,
		     gint              y,
		     GtkSelectionData *selection_data,
		     guint             info,
		     guint             time,
		     gpointer          data)
{
  GtkColorSelection *colorsel = GTK_COLOR_SELECTION (data);
  guint16 *vals;
  gdouble color[4];
  
  if (selection_data->length < 0)
    return;
  
  /* We accept drops with the wrong format, since the KDE color
   * chooser incorrectly drops application/x-color with format 8.
   */
  if (selection_data->length != 8)
    {
      g_warning ("Received invalid color data\n");
      return;
    }
  
  vals = (guint16 *)selection_data->data;
  
  color[0] = (gdouble)vals[0] / 0xffff;
  color[1] = (gdouble)vals[1] / 0xffff;
  color[2] = (gdouble)vals[2] / 0xffff;
  color[3] = (gdouble)vals[3] / 0xffff;
  palette_change_color (widget, colorsel, color);
  set_color_internal (colorsel, color);
}

static gint
palette_activate (GtkWidget   *widget,
		  GdkEventKey *event,
		  gpointer     data)
{
  /* should have a drawing area subclass with an activate signal */
  if ((event->keyval == GDK_space) ||
      (event->keyval == GDK_Return) ||
      (event->keyval == GDK_ISO_Enter) ||
      (event->keyval == GDK_KP_Enter) ||
      (event->keyval == GDK_KP_Space))
    {
      if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "color_set")) != 0)
        {
          gdouble color[4];
          palette_get_color (widget, color);
          set_color_internal (GTK_COLOR_SELECTION (data), color);
        }
      return TRUE;
    }
  
  return FALSE;
}

static gboolean
palette_popup (GtkWidget *widget,
               gpointer   data)
{
  GtkColorSelection *colorsel = GTK_COLOR_SELECTION (data);

  do_popup (colorsel, widget, GDK_CURRENT_TIME);
  return TRUE;
}
               

static GtkWidget*
palette_new (GtkColorSelection *colorsel)
{
  GtkWidget *retval;
  ColorSelectionPrivate *priv;
  
  static const GtkTargetEntry targets[] = {
    { "application/x-color", 0 }
  };

  priv = colorsel->private_data;
  
  retval = gtk_drawing_area_new ();

  gtk_widget_set_can_focus (retval, TRUE);
  
  g_object_set_data (G_OBJECT (retval), I_("color_set"), GINT_TO_POINTER (0)); 
  gtk_widget_set_events (retval, GDK_BUTTON_PRESS_MASK
                         | GDK_BUTTON_RELEASE_MASK
                         | GDK_EXPOSURE_MASK
                         | GDK_ENTER_NOTIFY_MASK
                         | GDK_LEAVE_NOTIFY_MASK);
  
  g_signal_connect (retval, "expose-event",
		    G_CALLBACK (palette_expose), colorsel);
  g_signal_connect (retval, "button-press-event",
		    G_CALLBACK (palette_press), colorsel);
  g_signal_connect (retval, "button-release-event",
		    G_CALLBACK (palette_release), colorsel);
  g_signal_connect (retval, "enter-notify-event",
		    G_CALLBACK (palette_enter), colorsel);
  g_signal_connect (retval, "leave-notify-event",
		    G_CALLBACK (palette_leave), colorsel);
  g_signal_connect (retval, "key-press-event",
		    G_CALLBACK (palette_activate), colorsel);
  g_signal_connect (retval, "popup-menu",
		    G_CALLBACK (palette_popup), colorsel);
  
  gtk_drag_dest_set (retval,
		     GTK_DEST_DEFAULT_HIGHLIGHT |
		     GTK_DEST_DEFAULT_MOTION |
		     GTK_DEST_DEFAULT_DROP,
		     targets, 1,
		     GDK_ACTION_COPY);
  
  g_signal_connect (retval, "drag-end",
                    G_CALLBACK (palette_drag_end), NULL);
  g_signal_connect (retval, "drag-data-received",
                    G_CALLBACK (palette_drop_handle), colorsel);

  gtk_widget_set_tooltip_text (retval,
                        _("Click this palette entry to make it the current color. To change this entry, drag a color swatch here or right-click it and select \"Save color here.\""));
  return retval;
}


/*
 *
 * The actual GtkColorSelection widget
 *
 */

static GdkCursor *
make_picker_cursor (GdkScreen *screen)
{
  GdkCursor *cursor;

  cursor = gdk_cursor_new_from_name (gdk_screen_get_display (screen),
				     "color-picker");

  if (!cursor)
    {
      GdkColor bg = { 0, 0xffff, 0xffff, 0xffff };
      GdkColor fg = { 0, 0x0000, 0x0000, 0x0000 };
      GdkWindow *window;
      GdkPixmap *pixmap, *mask;
      cairo_surface_t *image;
      cairo_t *cr;

      window = gdk_screen_get_root_window (screen);
      

      pixmap = gdk_pixmap_new (window, DROPPER_WIDTH, DROPPER_HEIGHT, 1);

      cr = gdk_cairo_create (pixmap);
      cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
      image = cairo_image_surface_create_for_data ((guchar *) dropper_bits,
                                                   CAIRO_FORMAT_A1,
                                                   DROPPER_WIDTH,
                                                   DROPPER_HEIGHT,
                                                   DROPPER_STRIDE);
      cairo_set_source_surface (cr, image, 0, 0);
      cairo_surface_destroy (image);
      cairo_paint (cr);
      cairo_destroy (cr);
      

      mask = gdk_pixmap_new (window, DROPPER_WIDTH, DROPPER_HEIGHT, 1);

      cr = gdk_cairo_create (mask);
      cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
      image = cairo_image_surface_create_for_data ((guchar *) dropper_mask,
                                                   CAIRO_FORMAT_A1,
                                                   DROPPER_WIDTH,
                                                   DROPPER_HEIGHT,
                                                   DROPPER_STRIDE);
      cairo_set_source_surface (cr, image, 0, 0);
      cairo_surface_destroy (image);
      cairo_paint (cr);
      cairo_destroy (cr);
      
      cursor = gdk_cursor_new_from_pixmap (pixmap, mask, &fg, &bg,
					   DROPPER_X_HOT, DROPPER_Y_HOT);
      
      g_object_unref (pixmap);
      g_object_unref (mask);
    }
      
  return cursor;
}

static void
grab_color_at_mouse (GdkScreen *screen,
		     gint       x_root,
		     gint       y_root,
		     gpointer   data)
{
  GdkPixbuf *pixbuf;
  guchar *pixels;
  GtkColorSelection *colorsel = data;
  ColorSelectionPrivate *priv;
  GdkColor color;
  GdkWindow *root_window = gdk_screen_get_root_window (screen);
  
  priv = colorsel->private_data;
  
  pixbuf = gdk_pixbuf_get_from_drawable (NULL, root_window, NULL,
                                         x_root, y_root,
                                         0, 0,
                                         1, 1);
  if (!pixbuf)
    {
      gint x, y;
      GdkDisplay *display = gdk_screen_get_display (screen);
      GdkWindow *window = gdk_display_get_window_at_pointer (display, &x, &y);
      if (!window)
	return;
      pixbuf = gdk_pixbuf_get_from_drawable (NULL, window, NULL,
                                             x, y,
                                             0, 0,
                                             1, 1);
      if (!pixbuf)
	return;
    }
  pixels = gdk_pixbuf_get_pixels (pixbuf);
  color.red = pixels[0] * 0x101;
  color.green = pixels[1] * 0x101;
  color.blue = pixels[2] * 0x101;
  g_object_unref (pixbuf);

  priv->color[COLORSEL_RED] = SCALE (color.red);
  priv->color[COLORSEL_GREEN] = SCALE (color.green);
  priv->color[COLORSEL_BLUE] = SCALE (color.blue);
  
  gtk_rgb_to_hsv (priv->color[COLORSEL_RED],
		  priv->color[COLORSEL_GREEN],
		  priv->color[COLORSEL_BLUE],
		  &priv->color[COLORSEL_HUE],
		  &priv->color[COLORSEL_SATURATION],
		  &priv->color[COLORSEL_VALUE]);

  update_color (colorsel);
}

static void
shutdown_eyedropper (GtkWidget *widget)
{
  GtkColorSelection *colorsel;
  ColorSelectionPrivate *priv;
  GdkDisplay *display = gtk_widget_get_display (widget);

  colorsel = GTK_COLOR_SELECTION (widget);
  priv = colorsel->private_data;    

  if (priv->has_grab)
    {
      gdk_display_keyboard_ungrab (display, priv->grab_time);
      gdk_display_pointer_ungrab (display, priv->grab_time);
      gtk_grab_remove (priv->dropper_grab_widget);

      priv->has_grab = FALSE;
    }
}

static void
mouse_motion (GtkWidget      *invisible,
	      GdkEventMotion *event,
	      gpointer        data)
{
  grab_color_at_mouse (gdk_event_get_screen ((GdkEvent *)event),
		       event->x_root, event->y_root, data); 
}

static gboolean
mouse_release (GtkWidget      *invisible,
	       GdkEventButton *event,
	       gpointer        data)
{
  /* GtkColorSelection *colorsel = data; */

  if (event->button != 1)
    return FALSE;

  grab_color_at_mouse (gdk_event_get_screen ((GdkEvent *)event),
		       event->x_root, event->y_root, data);

  shutdown_eyedropper (GTK_WIDGET (data));
  
  g_signal_handlers_disconnect_by_func (invisible,
					mouse_motion,
					data);
  g_signal_handlers_disconnect_by_func (invisible,
					mouse_release,
					data);

  return TRUE;
}

/* Helper Functions */

static gboolean
key_press (GtkWidget   *invisible,
           GdkEventKey *event,
           gpointer     data)
{  
  GdkDisplay *display = gtk_widget_get_display (invisible);
  GdkScreen *screen = gdk_event_get_screen ((GdkEvent *)event);
  guint state = event->state & gtk_accelerator_get_default_mod_mask ();
  gint x, y;
  gint dx, dy;

  gdk_display_get_pointer (display, NULL, &x, &y, NULL);

  dx = 0;
  dy = 0;

  switch (event->keyval) 
    {
    case GDK_space:
    case GDK_Return:
    case GDK_ISO_Enter:
    case GDK_KP_Enter:
    case GDK_KP_Space:
      grab_color_at_mouse (screen, x, y, data);
      /* fall through */

    case GDK_Escape:
      shutdown_eyedropper (data);
      
      g_signal_handlers_disconnect_by_func (invisible,
					    mouse_press,
					    data);
      g_signal_handlers_disconnect_by_func (invisible,
					    key_press,
					    data);
      
      return TRUE;

#if defined GDK_WINDOWING_X11 || defined GDK_WINDOWING_WIN32
    case GDK_Up:
    case GDK_KP_Up:
      dy = state == GDK_MOD1_MASK ? -BIG_STEP : -1;
      break;

    case GDK_Down:
    case GDK_KP_Down:
      dy = state == GDK_MOD1_MASK ? BIG_STEP : 1;
      break;

    case GDK_Left:
    case GDK_KP_Left:
      dx = state == GDK_MOD1_MASK ? -BIG_STEP : -1;
      break;

    case GDK_Right:
    case GDK_KP_Right:
      dx = state == GDK_MOD1_MASK ? BIG_STEP : 1;
      break;
#endif

    default:
      return FALSE;
    }

  gdk_display_warp_pointer (display, screen, x + dx, y + dy);
  
  return TRUE;

}

static gboolean
mouse_press (GtkWidget      *invisible,
	     GdkEventButton *event,
	     gpointer        data)
{
  /* GtkColorSelection *colorsel = data; */
  
  if (event->type == GDK_BUTTON_PRESS &&
      event->button == 1)
    {
      g_signal_connect (invisible, "motion-notify-event",
                        G_CALLBACK (mouse_motion),
                        data);
      g_signal_connect (invisible, "button-release-event",
                        G_CALLBACK (mouse_release),
                        data);
      g_signal_handlers_disconnect_by_func (invisible,
					    mouse_press,
					    data);
      g_signal_handlers_disconnect_by_func (invisible,
					    key_press,
					    data);
      return TRUE;
    }

  return FALSE;
}

/* when the button is clicked */
static void
get_screen_color (GtkWidget *button)
{
  GtkColorSelection *colorsel = g_object_get_data (G_OBJECT (button), "COLORSEL");
  ColorSelectionPrivate *priv = colorsel->private_data;
  GdkScreen *screen = gtk_widget_get_screen (GTK_WIDGET (button));
  GdkCursor *picker_cursor;
  GdkGrabStatus grab_status;
  GtkWidget *grab_widget, *toplevel;

  guint32 time = gtk_get_current_event_time ();
  
  if (priv->dropper_grab_widget == NULL)
    {
      grab_widget = gtk_window_new (GTK_WINDOW_POPUP);
      gtk_window_set_screen (GTK_WINDOW (grab_widget), screen);
      gtk_window_resize (GTK_WINDOW (grab_widget), 1, 1);
      gtk_window_move (GTK_WINDOW (grab_widget), -100, -100);
      gtk_widget_show (grab_widget);

      gtk_widget_add_events (grab_widget,
                             GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK);
      
      toplevel = gtk_widget_get_toplevel (GTK_WIDGET (colorsel));
  
      if (GTK_IS_WINDOW (toplevel))
	{
	  if (GTK_WINDOW (toplevel)->group)
	    gtk_window_group_add_window (GTK_WINDOW (toplevel)->group, 
					 GTK_WINDOW (grab_widget));
	}

      priv->dropper_grab_widget = grab_widget;
    }

  if (gdk_keyboard_grab (priv->dropper_grab_widget->window,
                         FALSE, time) != GDK_GRAB_SUCCESS)
    return;
  
  picker_cursor = make_picker_cursor (screen);
  grab_status = gdk_pointer_grab (priv->dropper_grab_widget->window,
				  FALSE,
				  GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK,
				  NULL,
				  picker_cursor,
				  time);
  gdk_cursor_unref (picker_cursor);
  
  if (grab_status != GDK_GRAB_SUCCESS)
    {
      gdk_display_keyboard_ungrab (gtk_widget_get_display (button), time);
      return;
    }

  gtk_grab_add (priv->dropper_grab_widget);
  priv->grab_time = time;
  priv->has_grab = TRUE;
  
  g_signal_connect (priv->dropper_grab_widget, "button-press-event",
                    G_CALLBACK (mouse_press), colorsel);
  g_signal_connect (priv->dropper_grab_widget, "key-press-event",
                    G_CALLBACK (key_press), colorsel);
}

static void
hex_changed (GtkWidget *hex_entry,
	     gpointer   data)
{
  GtkColorSelection *colorsel;
  ColorSelectionPrivate *priv;
  GdkColor color;
  gchar *text;
  
  colorsel = GTK_COLOR_SELECTION (data);
  priv = colorsel->private_data;
  
  if (priv->changing)
    return;
  
  text = gtk_editable_get_chars (GTK_EDITABLE (priv->hex_entry), 0, -1);
  if (gdk_color_parse (text, &color))
    {
      priv->color[COLORSEL_RED] = CLAMP (color.red/65535.0, 0.0, 1.0);
      priv->color[COLORSEL_GREEN] = CLAMP (color.green/65535.0, 0.0, 1.0);
      priv->color[COLORSEL_BLUE] = CLAMP (color.blue/65535.0, 0.0, 1.0);
      gtk_rgb_to_hsv (priv->color[COLORSEL_RED],
		      priv->color[COLORSEL_GREEN],
		      priv->color[COLORSEL_BLUE],
		      &priv->color[COLORSEL_HUE],
		      &priv->color[COLORSEL_SATURATION],
		      &priv->color[COLORSEL_VALUE]);
      update_color (colorsel);
    }
  g_free (text);
}

static gboolean
hex_focus_out (GtkWidget     *hex_entry, 
	       GdkEventFocus *event,
	       gpointer       data)
{
  hex_changed (hex_entry, data);
  
  return FALSE;
}

static void
hsv_changed (GtkWidget *hsv,
	     gpointer   data)
{
  GtkColorSelection *colorsel;
  ColorSelectionPrivate *priv;
  
  colorsel = GTK_COLOR_SELECTION (data);
  priv = colorsel->private_data;
  
  if (priv->changing)
    return;
  
  gtk_hsv_get_color (GTK_HSV (hsv),
		     &priv->color[COLORSEL_HUE],
		     &priv->color[COLORSEL_SATURATION],
		     &priv->color[COLORSEL_VALUE]);
  gtk_hsv_to_rgb (priv->color[COLORSEL_HUE],
		  priv->color[COLORSEL_SATURATION],
		  priv->color[COLORSEL_VALUE],
		  &priv->color[COLORSEL_RED],
		  &priv->color[COLORSEL_GREEN],
		  &priv->color[COLORSEL_BLUE]);
  update_color (colorsel);
}

static void
adjustment_changed (GtkAdjustment *adjustment,
		    gpointer       data)
{
  GtkColorSelection *colorsel;
  ColorSelectionPrivate *priv;
  
  colorsel = GTK_COLOR_SELECTION (g_object_get_data (G_OBJECT (adjustment), "COLORSEL"));
  priv = colorsel->private_data;
  
  if (priv->changing)
    return;
  
  switch (GPOINTER_TO_INT (data))
    {
    case COLORSEL_SATURATION:
    case COLORSEL_VALUE:
      priv->color[GPOINTER_TO_INT (data)] = adjustment->value / 100;
      gtk_hsv_to_rgb (priv->color[COLORSEL_HUE],
		      priv->color[COLORSEL_SATURATION],
		      priv->color[COLORSEL_VALUE],
		      &priv->color[COLORSEL_RED],
		      &priv->color[COLORSEL_GREEN],
		      &priv->color[COLORSEL_BLUE]);
      break;
    case COLORSEL_HUE:
      priv->color[GPOINTER_TO_INT (data)] = adjustment->value / 360;
      gtk_hsv_to_rgb (priv->color[COLORSEL_HUE],
		      priv->color[COLORSEL_SATURATION],
		      priv->color[COLORSEL_VALUE],
		      &priv->color[COLORSEL_RED],
		      &priv->color[COLORSEL_GREEN],
		      &priv->color[COLORSEL_BLUE]);
      break;
    case COLORSEL_RED:
    case COLORSEL_GREEN:
    case COLORSEL_BLUE:
      priv->color[GPOINTER_TO_INT (data)] = adjustment->value / 255;
      
      gtk_rgb_to_hsv (priv->color[COLORSEL_RED],
		      priv->color[COLORSEL_GREEN],
		      priv->color[COLORSEL_BLUE],
		      &priv->color[COLORSEL_HUE],
		      &priv->color[COLORSEL_SATURATION],
		      &priv->color[COLORSEL_VALUE]);
      break;
    default:
      priv->color[GPOINTER_TO_INT (data)] = adjustment->value / 255;
      break;
    }
  update_color (colorsel);
}

static void 
opacity_entry_changed (GtkWidget *opacity_entry,
		       gpointer   data)
{
  GtkColorSelection *colorsel;
  ColorSelectionPrivate *priv;
  GtkAdjustment *adj;
  gchar *text;
  
  colorsel = GTK_COLOR_SELECTION (data);
  priv = colorsel->private_data;
  
  if (priv->changing)
    return;
  
  text = gtk_editable_get_chars (GTK_EDITABLE (priv->opacity_entry), 0, -1);
  adj = gtk_range_get_adjustment (GTK_RANGE (priv->opacity_slider));
  gtk_adjustment_set_value (adj, g_strtod (text, NULL)); 
  
  update_color (colorsel);
  
  g_free (text);
}

static void
make_label_spinbutton (GtkColorSelection *colorsel,
		       GtkWidget        **spinbutton,
		       gchar             *text,
		       GtkWidget         *table,
		       gint               i,
		       gint               j,
		       gint               channel_type,
                       const gchar       *tooltip)
{
  GtkWidget *label;
  GtkAdjustment *adjust;

  if (channel_type == COLORSEL_HUE)
    {
      adjust = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 360.0, 1.0, 1.0, 0.0));
    }
  else if (channel_type == COLORSEL_SATURATION ||
	   channel_type == COLORSEL_VALUE)
    {
      adjust = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 100.0, 1.0, 1.0, 0.0));
    }
  else
    {
      adjust = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 255.0, 1.0, 1.0, 0.0));
    }
  g_object_set_data (G_OBJECT (adjust), I_("COLORSEL"), colorsel);
  *spinbutton = gtk_spin_button_new (adjust, 10.0, 0);

  gtk_widget_set_tooltip_text (*spinbutton, tooltip);  

  g_signal_connect (adjust, "value-changed",
                    G_CALLBACK (adjustment_changed),
                    GINT_TO_POINTER (channel_type));
  label = gtk_label_new_with_mnemonic (text);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), *spinbutton);

  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_table_attach_defaults (GTK_TABLE (table), label, i, i+1, j, j+1);
  gtk_table_attach_defaults (GTK_TABLE (table), *spinbutton, i+1, i+2, j, j+1);
}

static void
make_palette_frame (GtkColorSelection *colorsel,
		    GtkWidget         *table,
		    gint               i,
		    gint               j)
{
  GtkWidget *frame;
  ColorSelectionPrivate *priv;
  
  priv = colorsel->private_data;
  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  priv->custom_palette[i][j] = palette_new (colorsel);
  gtk_widget_set_size_request (priv->custom_palette[i][j], CUSTOM_PALETTE_ENTRY_WIDTH, CUSTOM_PALETTE_ENTRY_HEIGHT);
  gtk_container_add (GTK_CONTAINER (frame), priv->custom_palette[i][j]);
  gtk_table_attach_defaults (GTK_TABLE (table), frame, i, i+1, j, j+1);
}

/* Set the palette entry [x][y] to be the currently selected one. */
static void 
set_selected_palette (GtkColorSelection *colorsel, int x, int y)
{
  ColorSelectionPrivate *priv = colorsel->private_data; 

  gtk_widget_grab_focus (priv->custom_palette[x][y]);
}

static double
scale_round (double val, double factor)
{
  val = floor (val * factor + 0.5);
  val = MAX (val, 0);
  val = MIN (val, factor);
  return val;
}

static void
update_color (GtkColorSelection *colorsel)
{
  ColorSelectionPrivate *priv = colorsel->private_data;
  gchar entryval[12];
  gchar opacity_text[32];
  gchar *ptr;
  
  priv->changing = TRUE;
  color_sample_update_samples (colorsel);
  
  gtk_hsv_set_color (GTK_HSV (priv->triangle_colorsel),
		     priv->color[COLORSEL_HUE],
		     priv->color[COLORSEL_SATURATION],
		     priv->color[COLORSEL_VALUE]);
  gtk_adjustment_set_value (gtk_spin_button_get_adjustment
			    (GTK_SPIN_BUTTON (priv->hue_spinbutton)),
			    scale_round (priv->color[COLORSEL_HUE], 360));
  gtk_adjustment_set_value (gtk_spin_button_get_adjustment
			    (GTK_SPIN_BUTTON (priv->sat_spinbutton)),
			    scale_round (priv->color[COLORSEL_SATURATION], 100));
  gtk_adjustment_set_value (gtk_spin_button_get_adjustment
			    (GTK_SPIN_BUTTON (priv->val_spinbutton)),
			    scale_round (priv->color[COLORSEL_VALUE], 100));
  gtk_adjustment_set_value (gtk_spin_button_get_adjustment
			    (GTK_SPIN_BUTTON (priv->red_spinbutton)),
			    scale_round (priv->color[COLORSEL_RED], 255));
  gtk_adjustment_set_value (gtk_spin_button_get_adjustment
			    (GTK_SPIN_BUTTON (priv->green_spinbutton)),
			    scale_round (priv->color[COLORSEL_GREEN], 255));
  gtk_adjustment_set_value (gtk_spin_button_get_adjustment
			    (GTK_SPIN_BUTTON (priv->blue_spinbutton)),
			    scale_round (priv->color[COLORSEL_BLUE], 255));
  gtk_adjustment_set_value (gtk_range_get_adjustment
			    (GTK_RANGE (priv->opacity_slider)),
			    scale_round (priv->color[COLORSEL_OPACITY], 255));
  
  g_snprintf (opacity_text, 32, "%.0f", scale_round (priv->color[COLORSEL_OPACITY], 255));
  gtk_entry_set_text (GTK_ENTRY (priv->opacity_entry), opacity_text);
  
  g_snprintf (entryval, 11, "#%2X%2X%2X",
	      (guint) (scale_round (priv->color[COLORSEL_RED], 255)),
	      (guint) (scale_round (priv->color[COLORSEL_GREEN], 255)),
	      (guint) (scale_round (priv->color[COLORSEL_BLUE], 255)));
  
  for (ptr = entryval; *ptr; ptr++)
    if (*ptr == ' ')
      *ptr = '0';
  gtk_entry_set_text (GTK_ENTRY (priv->hex_entry), entryval);
  priv->changing = FALSE;

  g_object_ref (colorsel);
  
  g_signal_emit (colorsel, color_selection_signals[COLOR_CHANGED], 0);
  
  g_object_freeze_notify (G_OBJECT (colorsel));
  g_object_notify (G_OBJECT (colorsel), "current-color");
  g_object_notify (G_OBJECT (colorsel), "current-alpha");
  g_object_thaw_notify (G_OBJECT (colorsel));
  
  g_object_unref (colorsel);
}

static void
update_palette (GtkColorSelection *colorsel)
{
  GdkColor *current_colors;
  gint i, j;

  current_colors = get_current_colors (colorsel);
  
  for (i = 0; i < GTK_CUSTOM_PALETTE_HEIGHT; i++)
    {
      for (j = 0; j < GTK_CUSTOM_PALETTE_WIDTH; j++)
	{
          gint index;

          index = i * GTK_CUSTOM_PALETTE_WIDTH + j;
          
          gtk_color_selection_set_palette_color (colorsel,
                                                 index,
                                                 &current_colors[index]);
	}
    }

  g_free (current_colors);
}

static void
palette_change_notify_instance (GObject    *object,
                                GParamSpec *pspec,
                                gpointer    data)
{
  update_palette (GTK_COLOR_SELECTION (data));
}

static void
default_noscreen_change_palette_func (const GdkColor *colors,
				      gint            n_colors)
{
  default_change_palette_func (gdk_screen_get_default (), colors, n_colors);
}

static void
default_change_palette_func (GdkScreen	    *screen,
			     const GdkColor *colors,
                             gint            n_colors)
{
  gchar *str;
  
  str = gtk_color_selection_palette_to_string (colors, n_colors);

  gtk_settings_set_string_property (gtk_settings_get_for_screen (screen),
                                    "gtk-color-palette",
                                    str,
                                    "gtk_color_selection_palette_to_string");

  g_free (str);
}

/**
 * gtk_color_selection_new:
 * 
 * Creates a new GtkColorSelection.
 * 
 * Return value: a new #GtkColorSelection
 **/
GtkWidget *
gtk_color_selection_new (void)
{
  GtkColorSelection *colorsel;
  ColorSelectionPrivate *priv;
  gdouble color[4];
  color[0] = 1.0;
  color[1] = 1.0;
  color[2] = 1.0;
  color[3] = 1.0;
  
  colorsel = g_object_new (GTK_TYPE_COLOR_SELECTION, NULL);
  priv = colorsel->private_data;
  set_color_internal (colorsel, color);
  gtk_color_selection_set_has_opacity_control (colorsel, TRUE);
  
  /* We want to make sure that default_set is FALSE */
  /* This way the user can still set it */
  priv->default_set = FALSE;
  priv->default_alpha_set = FALSE;
  
  return GTK_WIDGET (colorsel);
}


void
gtk_color_selection_set_update_policy (GtkColorSelection *colorsel,
				       GtkUpdateType      policy)
{
  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));
}

/**
 * gtk_color_selection_get_has_opacity_control:
 * @colorsel: a #GtkColorSelection.
 * 
 * Determines whether the colorsel has an opacity control.
 * 
 * Return value: %TRUE if the @colorsel has an opacity control.  %FALSE if it does't.
 **/
gboolean
gtk_color_selection_get_has_opacity_control (GtkColorSelection *colorsel)
{
  ColorSelectionPrivate *priv;
  
  g_return_val_if_fail (GTK_IS_COLOR_SELECTION (colorsel), FALSE);
  
  priv = colorsel->private_data;
  
  return priv->has_opacity;
}

/**
 * gtk_color_selection_set_has_opacity_control:
 * @colorsel: a #GtkColorSelection.
 * @has_opacity: %TRUE if @colorsel can set the opacity, %FALSE otherwise.
 *
 * Sets the @colorsel to use or not use opacity.
 * 
 **/
void
gtk_color_selection_set_has_opacity_control (GtkColorSelection *colorsel,
					     gboolean           has_opacity)
{
  ColorSelectionPrivate *priv;
  
  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));
  
  priv = colorsel->private_data;
  has_opacity = has_opacity != FALSE;
  
  if (priv->has_opacity != has_opacity)
    {
      priv->has_opacity = has_opacity;
      if (has_opacity)
	{
	  gtk_widget_show (priv->opacity_slider);
	  gtk_widget_show (priv->opacity_label);
	  gtk_widget_show (priv->opacity_entry);
	}
      else
	{
	  gtk_widget_hide (priv->opacity_slider);
	  gtk_widget_hide (priv->opacity_label);
	  gtk_widget_hide (priv->opacity_entry);
	}
      color_sample_update_samples (colorsel);
      
      g_object_notify (G_OBJECT (colorsel), "has-opacity-control");
    }
}

/**
 * gtk_color_selection_get_has_palette:
 * @colorsel: a #GtkColorSelection.
 * 
 * Determines whether the color selector has a color palette.
 * 
 * Return value: %TRUE if the selector has a palette.  %FALSE if it hasn't.
 **/
gboolean
gtk_color_selection_get_has_palette (GtkColorSelection *colorsel)
{
  ColorSelectionPrivate *priv;
  
  g_return_val_if_fail (GTK_IS_COLOR_SELECTION (colorsel), FALSE);
  
  priv = colorsel->private_data;
  
  return priv->has_palette;
}

/**
 * gtk_color_selection_set_has_palette:
 * @colorsel: a #GtkColorSelection.
 * @has_palette: %TRUE if palette is to be visible, %FALSE otherwise.
 *
 * Shows and hides the palette based upon the value of @has_palette.
 * 
 **/
void
gtk_color_selection_set_has_palette (GtkColorSelection *colorsel,
				     gboolean           has_palette)
{
  ColorSelectionPrivate *priv;
  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));
  
  priv = colorsel->private_data;
  has_palette = has_palette != FALSE;
  
  if (priv->has_palette != has_palette)
    {
      priv->has_palette = has_palette;
      if (has_palette)
	gtk_widget_show (priv->palette_frame);
      else
	gtk_widget_hide (priv->palette_frame);

      update_tooltips (colorsel);

      g_object_notify (G_OBJECT (colorsel), "has-palette");
    }
}

/**
 * gtk_color_selection_set_current_color:
 * @colorsel: a #GtkColorSelection.
 * @color: A #GdkColor to set the current color with.
 *
 * Sets the current color to be @color.  The first time this is called, it will
 * also set the original color to be @color too.
 **/
void
gtk_color_selection_set_current_color (GtkColorSelection *colorsel,
				       const GdkColor    *color)
{
  ColorSelectionPrivate *priv;
  gint i;
  
  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));
  g_return_if_fail (color != NULL);

  priv = colorsel->private_data;
  priv->changing = TRUE;
  priv->color[COLORSEL_RED] = SCALE (color->red);
  priv->color[COLORSEL_GREEN] = SCALE (color->green);
  priv->color[COLORSEL_BLUE] = SCALE (color->blue);
  gtk_rgb_to_hsv (priv->color[COLORSEL_RED],
		  priv->color[COLORSEL_GREEN],
		  priv->color[COLORSEL_BLUE],
		  &priv->color[COLORSEL_HUE],
		  &priv->color[COLORSEL_SATURATION],
		  &priv->color[COLORSEL_VALUE]);
  if (priv->default_set == FALSE)
    {
      for (i = 0; i < COLORSEL_NUM_CHANNELS; i++)
	priv->old_color[i] = priv->color[i];
    }
  priv->default_set = TRUE;
  update_color (colorsel);
}

/**
 * gtk_color_selection_set_current_alpha:
 * @colorsel: a #GtkColorSelection.
 * @alpha: an integer between 0 and 65535.
 *
 * Sets the current opacity to be @alpha.  The first time this is called, it will
 * also set the original opacity to be @alpha too.
 **/
void
gtk_color_selection_set_current_alpha (GtkColorSelection *colorsel,
				       guint16            alpha)
{
  ColorSelectionPrivate *priv;
  gint i;
  
  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));
  
  priv = colorsel->private_data;
  priv->changing = TRUE;
  priv->color[COLORSEL_OPACITY] = SCALE (alpha);
  if (priv->default_alpha_set == FALSE)
    {
      for (i = 0; i < COLORSEL_NUM_CHANNELS; i++)
	priv->old_color[i] = priv->color[i];
    }
  priv->default_alpha_set = TRUE;
  update_color (colorsel);
}

/**
 * gtk_color_selection_set_color:
 * @colorsel: a #GtkColorSelection.
 * @color: an array of 4 doubles specifying the red, green, blue and opacity 
 *   to set the current color to.
 *
 * Sets the current color to be @color.  The first time this is called, it will
 * also set the original color to be @color too.
 *
 * Deprecated: 2.0: Use gtk_color_selection_set_current_color() instead.
 **/
void
gtk_color_selection_set_color (GtkColorSelection    *colorsel,
			       gdouble              *color)
{
  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));

  set_color_internal (colorsel, color);
}

/**
 * gtk_color_selection_get_current_color:
 * @colorsel: a #GtkColorSelection.
 * @color: (out): a #GdkColor to fill in with the current color.
 *
 * Sets @color to be the current color in the GtkColorSelection widget.
 **/
void
gtk_color_selection_get_current_color (GtkColorSelection *colorsel,
				       GdkColor          *color)
{
  ColorSelectionPrivate *priv;
  
  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));
  g_return_if_fail (color != NULL);
  
  priv = colorsel->private_data;
  color->red = UNSCALE (priv->color[COLORSEL_RED]);
  color->green = UNSCALE (priv->color[COLORSEL_GREEN]);
  color->blue = UNSCALE (priv->color[COLORSEL_BLUE]);
}

/**
 * gtk_color_selection_get_current_alpha:
 * @colorsel: a #GtkColorSelection.
 *
 * Returns the current alpha value.
 *
 * Return value: an integer between 0 and 65535.
 **/
guint16
gtk_color_selection_get_current_alpha (GtkColorSelection *colorsel)
{
  ColorSelectionPrivate *priv;
  
  g_return_val_if_fail (GTK_IS_COLOR_SELECTION (colorsel), 0);
  
  priv = colorsel->private_data;
  return priv->has_opacity ? UNSCALE (priv->color[COLORSEL_OPACITY]) : 65535;
}

/**
 * gtk_color_selection_get_color:
 * @colorsel: a #GtkColorSelection.
 * @color: an array of 4 #gdouble to fill in with the current color.
 *
 * Sets @color to be the current color in the GtkColorSelection widget.
 *
 * Deprecated: 2.0: Use gtk_color_selection_get_current_color() instead.
 **/
void
gtk_color_selection_get_color (GtkColorSelection *colorsel,
			       gdouble           *color)
{
  ColorSelectionPrivate *priv;
  
  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));
  
  priv = colorsel->private_data;
  color[0] = priv->color[COLORSEL_RED];
  color[1] = priv->color[COLORSEL_GREEN];
  color[2] = priv->color[COLORSEL_BLUE];
  color[3] = priv->has_opacity ? priv->color[COLORSEL_OPACITY] : 65535;
}

/**
 * gtk_color_selection_set_previous_color:
 * @colorsel: a #GtkColorSelection.
 * @color: a #GdkColor to set the previous color with.
 *
 * Sets the 'previous' color to be @color.  This function should be called with
 * some hesitations, as it might seem confusing to have that color change.
 * Calling gtk_color_selection_set_current_color() will also set this color the first
 * time it is called.
 **/
void
gtk_color_selection_set_previous_color (GtkColorSelection *colorsel,
					const GdkColor    *color)
{
  ColorSelectionPrivate *priv;
  
  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));
  g_return_if_fail (color != NULL);
  
  priv = colorsel->private_data;
  priv->changing = TRUE;
  priv->old_color[COLORSEL_RED] = SCALE (color->red);
  priv->old_color[COLORSEL_GREEN] = SCALE (color->green);
  priv->old_color[COLORSEL_BLUE] = SCALE (color->blue);
  gtk_rgb_to_hsv (priv->old_color[COLORSEL_RED],
		  priv->old_color[COLORSEL_GREEN],
		  priv->old_color[COLORSEL_BLUE],
		  &priv->old_color[COLORSEL_HUE],
		  &priv->old_color[COLORSEL_SATURATION],
		  &priv->old_color[COLORSEL_VALUE]);
  color_sample_update_samples (colorsel);
  priv->default_set = TRUE;
  priv->changing = FALSE;
}

/**
 * gtk_color_selection_set_previous_alpha:
 * @colorsel: a #GtkColorSelection.
 * @alpha: an integer between 0 and 65535.
 *
 * Sets the 'previous' alpha to be @alpha.  This function should be called with
 * some hesitations, as it might seem confusing to have that alpha change.
 **/
void
gtk_color_selection_set_previous_alpha (GtkColorSelection *colorsel,
					guint16            alpha)
{
  ColorSelectionPrivate *priv;
  
  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));
  
  priv = colorsel->private_data;
  priv->changing = TRUE;
  priv->old_color[COLORSEL_OPACITY] = SCALE (alpha);
  color_sample_update_samples (colorsel);
  priv->default_alpha_set = TRUE;
  priv->changing = FALSE;
}


/**
 * gtk_color_selection_get_previous_color:
 * @colorsel: a #GtkColorSelection.
 * @color: (out): a #GdkColor to fill in with the original color value.
 *
 * Fills @color in with the original color value.
 **/
void
gtk_color_selection_get_previous_color (GtkColorSelection *colorsel,
					GdkColor           *color)
{
  ColorSelectionPrivate *priv;
  
  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));
  g_return_if_fail (color != NULL);
  
  priv = colorsel->private_data;
  color->red = UNSCALE (priv->old_color[COLORSEL_RED]);
  color->green = UNSCALE (priv->old_color[COLORSEL_GREEN]);
  color->blue = UNSCALE (priv->old_color[COLORSEL_BLUE]);
}

/**
 * gtk_color_selection_get_previous_alpha:
 * @colorsel: a #GtkColorSelection.
 *
 * Returns the previous alpha value.
 *
 * Return value: an integer between 0 and 65535.
 **/
guint16
gtk_color_selection_get_previous_alpha (GtkColorSelection *colorsel)
{
  ColorSelectionPrivate *priv;
  
  g_return_val_if_fail (GTK_IS_COLOR_SELECTION (colorsel), 0);
  
  priv = colorsel->private_data;
  return priv->has_opacity ? UNSCALE (priv->old_color[COLORSEL_OPACITY]) : 65535;
}

/**
 * gtk_color_selection_set_palette_color:
 * @colorsel: a #GtkColorSelection.
 * @index: the color index of the palette.
 * @color: A #GdkColor to set the palette with.
 *
 * Sets the palette located at @index to have @color as its color.
 * 
 **/
static void
gtk_color_selection_set_palette_color (GtkColorSelection   *colorsel,
				       gint                 index,
				       GdkColor            *color)
{
  ColorSelectionPrivate *priv;
  gint x, y;
  gdouble col[3];
  
  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));
  g_return_if_fail (index >= 0  && index < GTK_CUSTOM_PALETTE_WIDTH*GTK_CUSTOM_PALETTE_HEIGHT);

  x = index % GTK_CUSTOM_PALETTE_WIDTH;
  y = index / GTK_CUSTOM_PALETTE_WIDTH;
  
  priv = colorsel->private_data;
  col[0] = SCALE (color->red);
  col[1] = SCALE (color->green);
  col[2] = SCALE (color->blue);
  
  palette_set_color (priv->custom_palette[x][y], colorsel, col);
}

/**
 * gtk_color_selection_is_adjusting:
 * @colorsel: a #GtkColorSelection.
 *
 * Gets the current state of the @colorsel.
 *
 * Return value: %TRUE if the user is currently dragging a color around, and %FALSE
 * if the selection has stopped.
 **/
gboolean
gtk_color_selection_is_adjusting (GtkColorSelection *colorsel)
{
  ColorSelectionPrivate *priv;
  
  g_return_val_if_fail (GTK_IS_COLOR_SELECTION (colorsel), FALSE);
  
  priv = colorsel->private_data;
  
  return (gtk_hsv_is_adjusting (GTK_HSV (priv->triangle_colorsel)));
}


/**
 * gtk_color_selection_palette_from_string:
 * @str: a string encoding a color palette.
 * @colors: (out) (array length=n_colors): return location for allocated
 *          array of #GdkColor.
 * @n_colors: return location for length of array.
 * 
 * Parses a color palette string; the string is a colon-separated
 * list of color names readable by gdk_color_parse().
 * 
 * Return value: %TRUE if a palette was successfully parsed.
 **/
gboolean
gtk_color_selection_palette_from_string (const gchar *str,
                                         GdkColor   **colors,
                                         gint        *n_colors)
{
  GdkColor *retval;
  gint count;
  gchar *p;
  gchar *start;
  gchar *copy;
  
  count = 0;
  retval = NULL;
  copy = g_strdup (str);

  start = copy;
  p = copy;
  while (TRUE)
    {
      if (*p == ':' || *p == '\0')
        {
          gboolean done = TRUE;

          if (start == p)
            {
              goto failed; /* empty entry */
            }
              
          if (*p)
            {
              *p = '\0';
              done = FALSE;
            }

          retval = g_renew (GdkColor, retval, count + 1);
          if (!gdk_color_parse (start, retval + count))
            {
              goto failed;
            }

          ++count;

          if (done)
            break;
          else
            start = p + 1;
        }

      ++p;
    }

  g_free (copy);
  
  if (colors)
    *colors = retval;
  else
    g_free (retval);

  if (n_colors)
    *n_colors = count;

  return TRUE;
  
 failed:
  g_free (copy);
  g_free (retval);

  if (colors)
    *colors = NULL;
  if (n_colors)
    *n_colors = 0;

  return FALSE;
}

/**
 * gtk_color_selection_palette_to_string:
 * @colors: (array length=n_colors): an array of colors.
 * @n_colors: length of the array.
 * 
 * Encodes a palette as a string, useful for persistent storage.
 * 
 * Return value: allocated string encoding the palette.
 **/
gchar*
gtk_color_selection_palette_to_string (const GdkColor *colors,
                                       gint            n_colors)
{
  gint i;
  gchar **strs = NULL;
  gchar *retval;
  
  if (n_colors == 0)
    return g_strdup ("");

  strs = g_new0 (gchar*, n_colors + 1);

  i = 0;
  while (i < n_colors)
    {
      gchar *ptr;
      
      strs[i] =
        g_strdup_printf ("#%2X%2X%2X",
                         colors[i].red / 256,
                         colors[i].green / 256,
                         colors[i].blue / 256);

      for (ptr = strs[i]; *ptr; ptr++)
        if (*ptr == ' ')
          *ptr = '0';
      
      ++i;
    }

  retval = g_strjoinv (":", strs);

  g_strfreev (strs);

  return retval;
}

/**
 * gtk_color_selection_set_change_palette_hook:
 * @func: a function to call when the custom palette needs saving.
 * 
 * Installs a global function to be called whenever the user tries to
 * modify the palette in a color selection. This function should save
 * the new palette contents, and update the GtkSettings property
 * "gtk-color-palette" so all GtkColorSelection widgets will be modified.
 *
 * Return value: the previous change palette hook (that was replaced).
 *
 * Deprecated: 2.4: This function does not work in multihead environments.
 *     Use gtk_color_selection_set_change_palette_with_screen_hook() instead. 
 * 
 **/
GtkColorSelectionChangePaletteFunc
gtk_color_selection_set_change_palette_hook (GtkColorSelectionChangePaletteFunc func)
{
  GtkColorSelectionChangePaletteFunc old;

  old = noscreen_change_palette_hook;

  noscreen_change_palette_hook = func;

  return old;
}

/**
 * gtk_color_selection_set_change_palette_with_screen_hook:
 * @func: a function to call when the custom palette needs saving.
 * 
 * Installs a global function to be called whenever the user tries to
 * modify the palette in a color selection. This function should save
 * the new palette contents, and update the GtkSettings property
 * "gtk-color-palette" so all GtkColorSelection widgets will be modified.
 * 
 * Return value: the previous change palette hook (that was replaced).
 *
 * Since: 2.2
 **/
GtkColorSelectionChangePaletteWithScreenFunc
gtk_color_selection_set_change_palette_with_screen_hook (GtkColorSelectionChangePaletteWithScreenFunc func)
{
  GtkColorSelectionChangePaletteWithScreenFunc old;

  old = change_palette_hook;

  change_palette_hook = func;

  return old;
}

static void
make_control_relations (AtkObject *atk_obj,
                        GtkWidget *widget)
{
  AtkObject *obj;

  obj = gtk_widget_get_accessible (widget);
  atk_object_add_relationship (atk_obj, ATK_RELATION_CONTROLLED_BY, obj);
  atk_object_add_relationship (obj, ATK_RELATION_CONTROLLER_FOR, atk_obj);
}

static void
make_all_relations (AtkObject *atk_obj,
                    ColorSelectionPrivate *priv)
{
  make_control_relations (atk_obj, priv->hue_spinbutton);
  make_control_relations (atk_obj, priv->sat_spinbutton);
  make_control_relations (atk_obj, priv->val_spinbutton);
  make_control_relations (atk_obj, priv->red_spinbutton);
  make_control_relations (atk_obj, priv->green_spinbutton);
  make_control_relations (atk_obj, priv->blue_spinbutton);
}

#define __GTK_COLOR_SELECTION_C__
#include "gtkaliasdef.c"

