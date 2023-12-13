/* GTK - The GIMP Toolkit
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include <math.h>
#include <string.h>

#include "gtklabel.h"
#include "gtkaccellabel.h"
#include "gtkdnd.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkpango.h"
#include "gtkwindow.h"
#include "gdk/gdkkeysyms.h"
#include "gtkclipboard.h"
#include "gtkimagemenuitem.h"
#include "gtkintl.h"
#include "gtkseparatormenuitem.h"
#include "gtktextutil.h"
#include "gtkmenuitem.h"
#include "gtknotebook.h"
#include "gtkstock.h"
#include "gtkbindings.h"
#include "gtkbuildable.h"
#include "gtkimage.h"
#include "gtkshow.h"
#include "gtktooltip.h"
#include "gtkprivate.h"
#include "gtkalias.h"

#define GTK_LABEL_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_LABEL, GtkLabelPrivate))

typedef struct
{
  gint wrap_width;
  gint width_chars;
  gint max_width_chars;

  gboolean mnemonics_visible;
} GtkLabelPrivate;

/* Notes about the handling of links:
 *
 * Links share the GtkLabelSelectionInfo struct with selectable labels.
 * There are some new fields for links. The links field contains the list
 * of GtkLabelLink structs that describe the links which are embedded in
 * the label. The active_link field points to the link under the mouse
 * pointer. For keyboard navigation, the 'focus' link is determined by
 * finding the link which contains the selection_anchor position.
 * The link_clicked field is used with button press and release events
 * to ensure that pressing inside a link and releasing outside of it
 * does not activate the link.
 *
 * Links are rendered with the link-color/visited-link-color colors
 * that are determined by the style and with an underline. When the mouse
 * pointer is over a link, the pointer is changed to indicate the link,
 * and the background behind the link is rendered with the base[PRELIGHT]
 * color. While a button is pressed over a link, the background is rendered
 * with the base[ACTIVE] color.
 *
 * Labels with links accept keyboard focus, and it is possible to move
 * the focus between the embedded links using Tab/Shift-Tab. The focus
 * is indicated by a focus rectangle that is drawn around the link text.
 * Pressing Enter activates the focussed link, and there is a suitable
 * context menu for links that can be opened with the Menu key. Pressing
 * Control-C copies the link URI to the clipboard.
 *
 * In selectable labels with links, link functionality is only available
 * when the selection is empty.
 */
typedef struct
{
  gchar *uri;
  gchar *title;     /* the title attribute, used as tooltip */
  gboolean visited; /* get set when the link is activated; this flag
                     * gets preserved over later set_markup() calls
                     */
  gint start;       /* position of the link in the PangoLayout */
  gint end;
} GtkLabelLink;

struct _GtkLabelSelectionInfo
{
  GdkWindow *window;
  gint selection_anchor;
  gint selection_end;
  GtkWidget *popup_menu;

  GList *links;
  GtkLabelLink *active_link;

  gint drag_start_x;
  gint drag_start_y;

  guint in_drag      : 1;
  guint select_words : 1;
  guint selectable   : 1;
  guint link_clicked : 1;
};

enum {
  MOVE_CURSOR,
  COPY_CLIPBOARD,
  POPULATE_POPUP,
  ACTIVATE_LINK,
  ACTIVATE_CURRENT_LINK,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_LABEL,
  PROP_ATTRIBUTES,
  PROP_USE_MARKUP,
  PROP_USE_UNDERLINE,
  PROP_JUSTIFY,
  PROP_PATTERN,
  PROP_WRAP,
  PROP_WRAP_MODE,
  PROP_SELECTABLE,
  PROP_MNEMONIC_KEYVAL,
  PROP_MNEMONIC_WIDGET,
  PROP_CURSOR_POSITION,
  PROP_SELECTION_BOUND,
  PROP_ELLIPSIZE,
  PROP_WIDTH_CHARS,
  PROP_SINGLE_LINE_MODE,
  PROP_ANGLE,
  PROP_MAX_WIDTH_CHARS,
  PROP_TRACK_VISITED_LINKS
};

static guint signals[LAST_SIGNAL] = { 0 };

static const GdkColor default_link_color = { 0, 0, 0, 0xeeee };
static const GdkColor default_visited_link_color = { 0, 0x5555, 0x1a1a, 0x8b8b };

static void gtk_label_set_property      (GObject          *object,
					 guint             prop_id,
					 const GValue     *value,
					 GParamSpec       *pspec);
static void gtk_label_get_property      (GObject          *object,
					 guint             prop_id,
					 GValue           *value,
					 GParamSpec       *pspec);
static void gtk_label_destroy           (GtkObject        *object);
static void gtk_label_finalize          (GObject          *object);
static void gtk_label_size_request      (GtkWidget        *widget,
					 GtkRequisition   *requisition);
static void gtk_label_size_allocate     (GtkWidget        *widget,
                                         GtkAllocation    *allocation);
static void gtk_label_state_changed     (GtkWidget        *widget,
                                         GtkStateType      state);
static void gtk_label_style_set         (GtkWidget        *widget,
					 GtkStyle         *previous_style);
static void gtk_label_direction_changed (GtkWidget        *widget,
					 GtkTextDirection  previous_dir);
static gint gtk_label_expose            (GtkWidget        *widget,
					 GdkEventExpose   *event);
static gboolean gtk_label_focus         (GtkWidget         *widget,
                                         GtkDirectionType   direction);

static void gtk_label_realize           (GtkWidget        *widget);
static void gtk_label_unrealize         (GtkWidget        *widget);
static void gtk_label_map               (GtkWidget        *widget);
static void gtk_label_unmap             (GtkWidget        *widget);

static gboolean gtk_label_button_press      (GtkWidget        *widget,
					     GdkEventButton   *event);
static gboolean gtk_label_button_release    (GtkWidget        *widget,
					     GdkEventButton   *event);
static gboolean gtk_label_motion            (GtkWidget        *widget,
					     GdkEventMotion   *event);
static gboolean gtk_label_leave_notify      (GtkWidget        *widget,
                                             GdkEventCrossing *event);

static void     gtk_label_grab_focus        (GtkWidget        *widget);

static gboolean gtk_label_query_tooltip     (GtkWidget        *widget,
                                             gint              x,
                                             gint              y,
                                             gboolean          keyboard_tip,
                                             GtkTooltip       *tooltip);

static void gtk_label_set_text_internal          (GtkLabel      *label,
						  gchar         *str);
static void gtk_label_set_label_internal         (GtkLabel      *label,
						  gchar         *str);
static void gtk_label_set_use_markup_internal    (GtkLabel      *label,
						  gboolean       val);
static void gtk_label_set_use_underline_internal (GtkLabel      *label,
						  gboolean       val);
static void gtk_label_set_attributes_internal    (GtkLabel      *label,
						  PangoAttrList *attrs);
static void gtk_label_set_uline_text_internal    (GtkLabel      *label,
						  const gchar   *str);
static void gtk_label_set_pattern_internal       (GtkLabel      *label,
				                  const gchar   *pattern,
                                                  gboolean       is_mnemonic);
static void gtk_label_set_markup_internal        (GtkLabel      *label,
						  const gchar   *str,
						  gboolean       with_uline);
static void gtk_label_recalculate                (GtkLabel      *label);
static void gtk_label_hierarchy_changed          (GtkWidget     *widget,
						  GtkWidget     *old_toplevel);
static void gtk_label_screen_changed             (GtkWidget     *widget,
						  GdkScreen     *old_screen);
static gboolean gtk_label_popup_menu             (GtkWidget     *widget);

static void gtk_label_create_window       (GtkLabel *label);
static void gtk_label_destroy_window      (GtkLabel *label);
static void gtk_label_ensure_select_info  (GtkLabel *label);
static void gtk_label_clear_select_info   (GtkLabel *label);
static void gtk_label_update_cursor       (GtkLabel *label);
static void gtk_label_clear_layout        (GtkLabel *label);
static void gtk_label_ensure_layout       (GtkLabel *label);
static void gtk_label_invalidate_wrap_width (GtkLabel *label);
static void gtk_label_select_region_index (GtkLabel *label,
                                           gint      anchor_index,
                                           gint      end_index);

static gboolean gtk_label_mnemonic_activate (GtkWidget         *widget,
					     gboolean           group_cycling);
static void     gtk_label_setup_mnemonic    (GtkLabel          *label,
					     guint              last_key);
static void     gtk_label_drag_data_get     (GtkWidget         *widget,
					     GdkDragContext    *context,
					     GtkSelectionData  *selection_data,
					     guint              info,
					     guint              time);

static void     gtk_label_buildable_interface_init     (GtkBuildableIface *iface);
static gboolean gtk_label_buildable_custom_tag_start   (GtkBuildable     *buildable,
							GtkBuilder       *builder,
							GObject          *child,
							const gchar      *tagname,
							GMarkupParser    *parser,
							gpointer         *data);

static void     gtk_label_buildable_custom_finished    (GtkBuildable     *buildable,
							GtkBuilder       *builder,
							GObject          *child,
							const gchar      *tagname,
							gpointer          user_data);


static void connect_mnemonics_visible_notify    (GtkLabel   *label);
static gboolean      separate_uline_pattern     (const gchar  *str,
                                                 guint        *accel_key,
                                                 gchar       **new_str,
                                                 gchar       **pattern);


/* For selectable labels: */
static void gtk_label_move_cursor        (GtkLabel        *label,
					  GtkMovementStep  step,
					  gint             count,
					  gboolean         extend_selection);
static void gtk_label_copy_clipboard     (GtkLabel        *label);
static void gtk_label_select_all         (GtkLabel        *label);
static void gtk_label_do_popup           (GtkLabel        *label,
					  GdkEventButton  *event);
static gint gtk_label_move_forward_word  (GtkLabel        *label,
					  gint             start);
static gint gtk_label_move_backward_word (GtkLabel        *label,
					  gint             start);

/* For links: */
static void          gtk_label_rescan_links     (GtkLabel  *label);
static void          gtk_label_clear_links      (GtkLabel  *label);
static gboolean      gtk_label_activate_link    (GtkLabel    *label,
                                                 const gchar *uri);
static void          gtk_label_activate_current_link (GtkLabel *label);
static GtkLabelLink *gtk_label_get_current_link (GtkLabel  *label);
static void          gtk_label_get_link_colors  (GtkWidget  *widget,
                                                 GdkColor  **link_color,
                                                 GdkColor  **visited_link_color);
static void          emit_activate_link         (GtkLabel     *label,
                                                 GtkLabelLink *link);

static GQuark quark_angle = 0;

static GtkBuildableIface *buildable_parent_iface = NULL;

G_DEFINE_TYPE_WITH_CODE (GtkLabel, gtk_label, GTK_TYPE_MISC,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
						gtk_label_buildable_interface_init));

static void
add_move_binding (GtkBindingSet  *binding_set,
		  guint           keyval,
		  guint           modmask,
		  GtkMovementStep step,
		  gint            count)
{
  g_return_if_fail ((modmask & GDK_SHIFT_MASK) == 0);
  
  gtk_binding_entry_add_signal (binding_set, keyval, modmask,
				"move-cursor", 3,
				G_TYPE_ENUM, step,
				G_TYPE_INT, count,
				G_TYPE_BOOLEAN, FALSE);

  /* Selection-extending version */
  gtk_binding_entry_add_signal (binding_set, keyval, modmask | GDK_SHIFT_MASK,
				"move-cursor", 3,
				G_TYPE_ENUM, step,
				G_TYPE_INT, count,
				G_TYPE_BOOLEAN, TRUE);
}

static void
gtk_label_class_init (GtkLabelClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkObjectClass *object_class = GTK_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkBindingSet *binding_set;

  quark_angle = g_quark_from_static_string ("angle");

  gobject_class->set_property = gtk_label_set_property;
  gobject_class->get_property = gtk_label_get_property;
  gobject_class->finalize = gtk_label_finalize;

  object_class->destroy = gtk_label_destroy;

  widget_class->size_request = gtk_label_size_request;
  widget_class->size_allocate = gtk_label_size_allocate;
  widget_class->state_changed = gtk_label_state_changed;
  widget_class->style_set = gtk_label_style_set;
  widget_class->query_tooltip = gtk_label_query_tooltip;
  widget_class->direction_changed = gtk_label_direction_changed;
  widget_class->expose_event = gtk_label_expose;
  widget_class->realize = gtk_label_realize;
  widget_class->unrealize = gtk_label_unrealize;
  widget_class->map = gtk_label_map;
  widget_class->unmap = gtk_label_unmap;
  widget_class->button_press_event = gtk_label_button_press;
  widget_class->button_release_event = gtk_label_button_release;
  widget_class->motion_notify_event = gtk_label_motion;
  widget_class->leave_notify_event = gtk_label_leave_notify;
  widget_class->hierarchy_changed = gtk_label_hierarchy_changed;
  widget_class->screen_changed = gtk_label_screen_changed;
  widget_class->mnemonic_activate = gtk_label_mnemonic_activate;
  widget_class->drag_data_get = gtk_label_drag_data_get;
  widget_class->grab_focus = gtk_label_grab_focus;
  widget_class->popup_menu = gtk_label_popup_menu;
  widget_class->focus = gtk_label_focus;

  class->move_cursor = gtk_label_move_cursor;
  class->copy_clipboard = gtk_label_copy_clipboard;
  class->activate_link = gtk_label_activate_link;

  /**
   * GtkLabel::move-cursor:
   * @entry: the object which received the signal
   * @step: the granularity of the move, as a #GtkMovementStep
   * @count: the number of @step units to move
   * @extend_selection: %TRUE if the move should extend the selection
   *
   * The ::move-cursor signal is a
   * <link linkend="keybinding-signals">keybinding signal</link>
   * which gets emitted when the user initiates a cursor movement.
   * If the cursor is not visible in @entry, this signal causes
   * the viewport to be moved instead.
   *
   * Applications should not connect to it, but may emit it with
   * g_signal_emit_by_name() if they need to control the cursor
   * programmatically.
   *
   * The default bindings for this signal come in two variants,
   * the variant with the Shift modifier extends the selection,
   * the variant without the Shift modifer does not.
   * There are too many key combinations to list them all here.
   * <itemizedlist>
   * <listitem>Arrow keys move by individual characters/lines</listitem>
   * <listitem>Ctrl-arrow key combinations move by words/paragraphs</listitem>
   * <listitem>Home/End keys move to the ends of the buffer</listitem>
   * </itemizedlist>
   */
  signals[MOVE_CURSOR] = 
    g_signal_new (I_("move-cursor"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkLabelClass, move_cursor),
		  NULL, NULL,
		  _gtk_marshal_VOID__ENUM_INT_BOOLEAN,
		  G_TYPE_NONE, 3,
		  GTK_TYPE_MOVEMENT_STEP,
		  G_TYPE_INT,
		  G_TYPE_BOOLEAN);

   /**
   * GtkLabel::copy-clipboard:
   * @label: the object which received the signal
   *
   * The ::copy-clipboard signal is a
   * <link linkend="keybinding-signals">keybinding signal</link>
   * which gets emitted to copy the selection to the clipboard.
   *
   * The default binding for this signal is Ctrl-c.
   */ 
  signals[COPY_CLIPBOARD] =
    g_signal_new (I_("copy-clipboard"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkLabelClass, copy_clipboard),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  
  /**
   * GtkLabel::populate-popup:
   * @label: The label on which the signal is emitted
   * @menu: the menu that is being populated
   *
   * The ::populate-popup signal gets emitted before showing the
   * context menu of the label. Note that only selectable labels
   * have context menus.
   *
   * If you need to add items to the context menu, connect
   * to this signal and append your menuitems to the @menu.
   */
  signals[POPULATE_POPUP] =
    g_signal_new (I_("populate-popup"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkLabelClass, populate_popup),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_MENU);

    /**
     * GtkLabel::activate-current-link:
     * @label: The label on which the signal was emitted
     *
     * A <link linkend="keybinding-signals">keybinding signal</link>
     * which gets emitted when the user activates a link in the label.
     *
     * Applications may also emit the signal with g_signal_emit_by_name()
     * if they need to control activation of URIs programmatically.
     *
     * The default bindings for this signal are all forms of the Enter key.
     *
     * Since: 2.18
     */
    signals[ACTIVATE_CURRENT_LINK] =
      g_signal_new_class_handler ("activate-current-link",
                                  G_TYPE_FROM_CLASS (object_class),
                                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                  G_CALLBACK (gtk_label_activate_current_link),
                                  NULL, NULL,
                                  _gtk_marshal_VOID__VOID,
                                  G_TYPE_NONE, 0);

    /**
     * GtkLabel::activate-link:
     * @label: The label on which the signal was emitted
     * @uri: the URI that is activated
     *
     * The signal which gets emitted to activate a URI.
     * Applications may connect to it to override the default behaviour,
     * which is to call gtk_show_uri().
     *
     * Returns: %TRUE if the link has been activated
     *
     * Since: 2.18
     */
    signals[ACTIVATE_LINK] =
      g_signal_new ("activate-link",
                    G_TYPE_FROM_CLASS (object_class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GtkLabelClass, activate_link),
                    _gtk_boolean_handled_accumulator, NULL,
                    _gtk_marshal_BOOLEAN__STRING,
                    G_TYPE_BOOLEAN, 1, G_TYPE_STRING);

  g_object_class_install_property (gobject_class,
                                   PROP_LABEL,
                                   g_param_spec_string ("label",
                                                        P_("Label"),
                                                        P_("The text of the label"),
                                                        "",
                                                        GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_ATTRIBUTES,
				   g_param_spec_boxed ("attributes",
						       P_("Attributes"),
						       P_("A list of style attributes to apply to the text of the label"),
						       PANGO_TYPE_ATTR_LIST,
						       GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_USE_MARKUP,
                                   g_param_spec_boolean ("use-markup",
							 P_("Use markup"),
							 P_("The text of the label includes XML markup. See pango_parse_markup()"),
                                                        FALSE,
                                                        GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_USE_UNDERLINE,
                                   g_param_spec_boolean ("use-underline",
							 P_("Use underline"),
							 P_("If set, an underline in the text indicates the next character should be used for the mnemonic accelerator key"),
                                                        FALSE,
                                                        GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
				   PROP_JUSTIFY,
                                   g_param_spec_enum ("justify",
                                                      P_("Justification"),
                                                      P_("The alignment of the lines in the text of the label relative to each other. This does NOT affect the alignment of the label within its allocation. See GtkMisc::xalign for that"),
						      GTK_TYPE_JUSTIFICATION,
						      GTK_JUSTIFY_LEFT,
                                                      GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_PATTERN,
                                   g_param_spec_string ("pattern",
                                                        P_("Pattern"),
                                                        P_("A string with _ characters in positions correspond to characters in the text to underline"),
                                                        NULL,
                                                        GTK_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class,
                                   PROP_WRAP,
                                   g_param_spec_boolean ("wrap",
                                                        P_("Line wrap"),
                                                        P_("If set, wrap lines if the text becomes too wide"),
                                                        FALSE,
                                                        GTK_PARAM_READWRITE));
  /**
   * GtkLabel:wrap-mode:
   *
   * If line wrapping is on (see the #GtkLabel:wrap property) this controls 
   * how the line wrapping is done. The default is %PANGO_WRAP_WORD, which 
   * means wrap on word boundaries.
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
                                   PROP_WRAP_MODE,
                                   g_param_spec_enum ("wrap-mode",
						      P_("Line wrap mode"),
						      P_("If wrap is set, controls how linewrapping is done"),
						      PANGO_TYPE_WRAP_MODE,
						      PANGO_WRAP_WORD,
						      GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_SELECTABLE,
                                   g_param_spec_boolean ("selectable",
                                                        P_("Selectable"),
                                                        P_("Whether the label text can be selected with the mouse"),
                                                        FALSE,
                                                        GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_MNEMONIC_KEYVAL,
                                   g_param_spec_uint ("mnemonic-keyval",
						      P_("Mnemonic key"),
						      P_("The mnemonic accelerator key for this label"),
						      0,
						      G_MAXUINT,
						      GDK_VoidSymbol,
						      GTK_PARAM_READABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_MNEMONIC_WIDGET,
                                   g_param_spec_object ("mnemonic-widget",
							P_("Mnemonic widget"),
							P_("The widget to be activated when the label's mnemonic "
							  "key is pressed"),
							GTK_TYPE_WIDGET,
							GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_CURSOR_POSITION,
                                   g_param_spec_int ("cursor-position",
                                                     P_("Cursor Position"),
                                                     P_("The current position of the insertion cursor in chars"),
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     GTK_PARAM_READABLE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_SELECTION_BOUND,
                                   g_param_spec_int ("selection-bound",
                                                     P_("Selection Bound"),
                                                     P_("The position of the opposite end of the selection from the cursor in chars"),
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     GTK_PARAM_READABLE));
  
  /**
   * GtkLabel:ellipsize:
   *
   * The preferred place to ellipsize the string, if the label does 
   * not have enough room to display the entire string, specified as a 
   * #PangoEllisizeMode. 
   *
   * Note that setting this property to a value other than 
   * %PANGO_ELLIPSIZE_NONE has the side-effect that the label requests 
   * only enough space to display the ellipsis "...". In particular, this 
   * means that ellipsizing labels do not work well in notebook tabs, unless 
   * the tab's #GtkNotebook:tab-expand property is set to %TRUE. Other ways
   * to set a label's width are gtk_widget_set_size_request() and
   * gtk_label_set_width_chars().
   *
   * Since: 2.6
   */
  g_object_class_install_property (gobject_class,
				   PROP_ELLIPSIZE,
                                   g_param_spec_enum ("ellipsize",
                                                      P_("Ellipsize"),
                                                      P_("The preferred place to ellipsize the string, if the label does not have enough room to display the entire string"),
						      PANGO_TYPE_ELLIPSIZE_MODE,
						      PANGO_ELLIPSIZE_NONE,
                                                      GTK_PARAM_READWRITE));

  /**
   * GtkLabel:width-chars:
   * 
   * The desired width of the label, in characters. If this property is set to
   * -1, the width will be calculated automatically, otherwise the label will
   * request either 3 characters or the property value, whichever is greater.
   * If the "width-chars" property is set to a positive value, then the 
   * #GtkLabel:max-width-chars property is ignored. 
   *
   * Since: 2.6
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_WIDTH_CHARS,
                                   g_param_spec_int ("width-chars",
                                                     P_("Width In Characters"),
                                                     P_("The desired width of the label, in characters"),
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     GTK_PARAM_READWRITE));
  
  /**
   * GtkLabel:single-line-mode:
   * 
   * Whether the label is in single line mode. In single line mode,
   * the height of the label does not depend on the actual text, it
   * is always set to ascent + descent of the font. This can be an
   * advantage in situations where resizing the label because of text 
   * changes would be distracting, e.g. in a statusbar.
   *
   * Since: 2.6
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_SINGLE_LINE_MODE,
                                   g_param_spec_boolean ("single-line-mode",
                                                        P_("Single Line Mode"),
                                                        P_("Whether the label is in single line mode"),
                                                        FALSE,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkLabel:angle:
   * 
   * The angle that the baseline of the label makes with the horizontal,
   * in degrees, measured counterclockwise. An angle of 90 reads from
   * from bottom to top, an angle of 270, from top to bottom. Ignored
   * if the label is selectable, wrapped, or ellipsized.
   *
   * Since: 2.6
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ANGLE,
                                   g_param_spec_double ("angle",
							P_("Angle"),
							P_("Angle at which the label is rotated"),
							0.0,
							360.0,
							0.0, 
							GTK_PARAM_READWRITE));
  
  /**
   * GtkLabel:max-width-chars:
   * 
   * The desired maximum width of the label, in characters. If this property 
   * is set to -1, the width will be calculated automatically, otherwise the 
   * label will request space for no more than the requested number of 
   * characters. If the #GtkLabel:width-chars property is set to a positive 
   * value, then the "max-width-chars" property is ignored.
   * 
   * Since: 2.6
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MAX_WIDTH_CHARS,
                                   g_param_spec_int ("max-width-chars",
                                                     P_("Maximum Width In Characters"),
                                                     P_("The desired maximum width of the label, in characters"),
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     GTK_PARAM_READWRITE));

  /**
   * GtkLabel:track-visited-links:
   *
   * Set this property to %TRUE to make the label track which links
   * have been clicked. It will then apply the ::visited-link-color
   * color, instead of ::link-color.
   *
   * Since: 2.18
   */
  g_object_class_install_property (gobject_class,
                                   PROP_TRACK_VISITED_LINKS,
                                   g_param_spec_boolean ("track-visited-links",
                                                         P_("Track visited links"),
                                                         P_("Whether visited links should be tracked"),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE));
  /*
   * Key bindings
   */

  binding_set = gtk_binding_set_by_class (class);

  /* Moving the insertion point */
  add_move_binding (binding_set, GDK_Right, 0,
		    GTK_MOVEMENT_VISUAL_POSITIONS, 1);
  
  add_move_binding (binding_set, GDK_Left, 0,
		    GTK_MOVEMENT_VISUAL_POSITIONS, -1);

  add_move_binding (binding_set, GDK_KP_Right, 0,
		    GTK_MOVEMENT_VISUAL_POSITIONS, 1);
  
  add_move_binding (binding_set, GDK_KP_Left, 0,
		    GTK_MOVEMENT_VISUAL_POSITIONS, -1);
  
  add_move_binding (binding_set, GDK_f, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_LOGICAL_POSITIONS, 1);
  
  add_move_binding (binding_set, GDK_b, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_LOGICAL_POSITIONS, -1);
  
  add_move_binding (binding_set, GDK_Right, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_WORDS, 1);

  add_move_binding (binding_set, GDK_Left, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_WORDS, -1);

  add_move_binding (binding_set, GDK_KP_Right, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_WORDS, 1);

  add_move_binding (binding_set, GDK_KP_Left, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_WORDS, -1);

  /* select all */
  gtk_binding_entry_add_signal (binding_set, GDK_a, GDK_CONTROL_MASK,
				"move-cursor", 3,
				G_TYPE_ENUM, GTK_MOVEMENT_PARAGRAPH_ENDS,
				G_TYPE_INT, -1,
				G_TYPE_BOOLEAN, FALSE);

  gtk_binding_entry_add_signal (binding_set, GDK_a, GDK_CONTROL_MASK,
				"move-cursor", 3,
				G_TYPE_ENUM, GTK_MOVEMENT_PARAGRAPH_ENDS,
				G_TYPE_INT, 1,
				G_TYPE_BOOLEAN, TRUE);

  gtk_binding_entry_add_signal (binding_set, GDK_slash, GDK_CONTROL_MASK,
				"move-cursor", 3,
				G_TYPE_ENUM, GTK_MOVEMENT_PARAGRAPH_ENDS,
				G_TYPE_INT, -1,
				G_TYPE_BOOLEAN, FALSE);

  gtk_binding_entry_add_signal (binding_set, GDK_slash, GDK_CONTROL_MASK,
				"move-cursor", 3,
				G_TYPE_ENUM, GTK_MOVEMENT_PARAGRAPH_ENDS,
				G_TYPE_INT, 1,
				G_TYPE_BOOLEAN, TRUE);

  /* unselect all */
  gtk_binding_entry_add_signal (binding_set, GDK_a, GDK_SHIFT_MASK | GDK_CONTROL_MASK,
				"move-cursor", 3,
				G_TYPE_ENUM, GTK_MOVEMENT_PARAGRAPH_ENDS,
				G_TYPE_INT, 0,
				G_TYPE_BOOLEAN, FALSE);

  gtk_binding_entry_add_signal (binding_set, GDK_backslash, GDK_CONTROL_MASK,
				"move-cursor", 3,
				G_TYPE_ENUM, GTK_MOVEMENT_PARAGRAPH_ENDS,
				G_TYPE_INT, 0,
				G_TYPE_BOOLEAN, FALSE);

  add_move_binding (binding_set, GDK_f, GDK_MOD1_MASK,
		    GTK_MOVEMENT_WORDS, 1);

  add_move_binding (binding_set, GDK_b, GDK_MOD1_MASK,
		    GTK_MOVEMENT_WORDS, -1);

  add_move_binding (binding_set, GDK_Home, 0,
		    GTK_MOVEMENT_DISPLAY_LINE_ENDS, -1);

  add_move_binding (binding_set, GDK_End, 0,
		    GTK_MOVEMENT_DISPLAY_LINE_ENDS, 1);

  add_move_binding (binding_set, GDK_KP_Home, 0,
		    GTK_MOVEMENT_DISPLAY_LINE_ENDS, -1);

  add_move_binding (binding_set, GDK_KP_End, 0,
		    GTK_MOVEMENT_DISPLAY_LINE_ENDS, 1);
  
  add_move_binding (binding_set, GDK_Home, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_BUFFER_ENDS, -1);

  add_move_binding (binding_set, GDK_End, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_BUFFER_ENDS, 1);

  add_move_binding (binding_set, GDK_KP_Home, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_BUFFER_ENDS, -1);

  add_move_binding (binding_set, GDK_KP_End, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_BUFFER_ENDS, 1);

  /* copy */
  gtk_binding_entry_add_signal (binding_set, GDK_c, GDK_CONTROL_MASK,
				"copy-clipboard", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_Return, 0,
				"activate-current-link", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_ISO_Enter, 0,
				"activate-current-link", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Enter, 0,
				"activate-current-link", 0);

  g_type_class_add_private (class, sizeof (GtkLabelPrivate));
}

static void 
gtk_label_set_property (GObject      *object,
			guint         prop_id,
			const GValue *value,
			GParamSpec   *pspec)
{
  GtkLabel *label;

  label = GTK_LABEL (object);
  
  switch (prop_id)
    {
    case PROP_LABEL:
      gtk_label_set_label (label, g_value_get_string (value));
      break;
    case PROP_ATTRIBUTES:
      gtk_label_set_attributes (label, g_value_get_boxed (value));
      break;
    case PROP_USE_MARKUP:
      gtk_label_set_use_markup (label, g_value_get_boolean (value));
      break;
    case PROP_USE_UNDERLINE:
      gtk_label_set_use_underline (label, g_value_get_boolean (value));
      break;
    case PROP_JUSTIFY:
      gtk_label_set_justify (label, g_value_get_enum (value));
      break;
    case PROP_PATTERN:
      gtk_label_set_pattern (label, g_value_get_string (value));
      break;
    case PROP_WRAP:
      gtk_label_set_line_wrap (label, g_value_get_boolean (value));
      break;	  
    case PROP_WRAP_MODE:
      gtk_label_set_line_wrap_mode (label, g_value_get_enum (value));
      break;	  
    case PROP_SELECTABLE:
      gtk_label_set_selectable (label, g_value_get_boolean (value));
      break;	  
    case PROP_MNEMONIC_WIDGET:
      gtk_label_set_mnemonic_widget (label, (GtkWidget*) g_value_get_object (value));
      break;
    case PROP_ELLIPSIZE:
      gtk_label_set_ellipsize (label, g_value_get_enum (value));
      break;
    case PROP_WIDTH_CHARS:
      gtk_label_set_width_chars (label, g_value_get_int (value));
      break;
    case PROP_SINGLE_LINE_MODE:
      gtk_label_set_single_line_mode (label, g_value_get_boolean (value));
      break;	  
    case PROP_ANGLE:
      gtk_label_set_angle (label, g_value_get_double (value));
      break;
    case PROP_MAX_WIDTH_CHARS:
      gtk_label_set_max_width_chars (label, g_value_get_int (value));
      break;
    case PROP_TRACK_VISITED_LINKS:
      gtk_label_set_track_visited_links (label, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_label_get_property (GObject     *object,
			guint        prop_id,
			GValue      *value,
			GParamSpec  *pspec)
{
  GtkLabel *label;
  
  label = GTK_LABEL (object);
  
  switch (prop_id)
    {
    case PROP_LABEL:
      g_value_set_string (value, label->label);
      break;
    case PROP_ATTRIBUTES:
      g_value_set_boxed (value, label->attrs);
      break;
    case PROP_USE_MARKUP:
      g_value_set_boolean (value, label->use_markup);
      break;
    case PROP_USE_UNDERLINE:
      g_value_set_boolean (value, label->use_underline);
      break;
    case PROP_JUSTIFY:
      g_value_set_enum (value, label->jtype);
      break;
    case PROP_WRAP:
      g_value_set_boolean (value, label->wrap);
      break;
    case PROP_WRAP_MODE:
      g_value_set_enum (value, label->wrap_mode);
      break;
    case PROP_SELECTABLE:
      g_value_set_boolean (value, gtk_label_get_selectable (label));
      break;
    case PROP_MNEMONIC_KEYVAL:
      g_value_set_uint (value, label->mnemonic_keyval);
      break;
    case PROP_MNEMONIC_WIDGET:
      g_value_set_object (value, (GObject*) label->mnemonic_widget);
      break;
    case PROP_CURSOR_POSITION:
      if (label->select_info && label->select_info->selectable)
	{
	  gint offset = g_utf8_pointer_to_offset (label->text,
						  label->text + label->select_info->selection_end);
	  g_value_set_int (value, offset);
	}
      else
	g_value_set_int (value, 0);
      break;
    case PROP_SELECTION_BOUND:
      if (label->select_info && label->select_info->selectable)
	{
	  gint offset = g_utf8_pointer_to_offset (label->text,
						  label->text + label->select_info->selection_anchor);
	  g_value_set_int (value, offset);
	}
      else
	g_value_set_int (value, 0);
      break;
    case PROP_ELLIPSIZE:
      g_value_set_enum (value, label->ellipsize);
      break;
    case PROP_WIDTH_CHARS:
      g_value_set_int (value, gtk_label_get_width_chars (label));
      break;
    case PROP_SINGLE_LINE_MODE:
      g_value_set_boolean (value, gtk_label_get_single_line_mode (label));
      break;
    case PROP_ANGLE:
      g_value_set_double (value, gtk_label_get_angle (label));
      break;
    case PROP_MAX_WIDTH_CHARS:
      g_value_set_int (value, gtk_label_get_max_width_chars (label));
      break;
    case PROP_TRACK_VISITED_LINKS:
      g_value_set_boolean (value, gtk_label_get_track_visited_links (label));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_label_init (GtkLabel *label)
{
  GtkLabelPrivate *priv;

  gtk_widget_set_has_window (GTK_WIDGET (label), FALSE);

  priv = GTK_LABEL_GET_PRIVATE (label);
  priv->width_chars = -1;
  priv->max_width_chars = -1;
  priv->wrap_width = -1;
  label->label = NULL;

  label->jtype = GTK_JUSTIFY_LEFT;
  label->wrap = FALSE;
  label->wrap_mode = PANGO_WRAP_WORD;
  label->ellipsize = PANGO_ELLIPSIZE_NONE;

  label->use_underline = FALSE;
  label->use_markup = FALSE;
  label->pattern_set = FALSE;
  label->track_links = TRUE;

  label->mnemonic_keyval = GDK_VoidSymbol;
  label->layout = NULL;
  label->text = NULL;
  label->attrs = NULL;

  label->mnemonic_widget = NULL;
  label->mnemonic_window = NULL;

  priv->mnemonics_visible = TRUE;

  gtk_label_set_text (label, "");
}


static void
gtk_label_buildable_interface_init (GtkBuildableIface *iface)
{
  buildable_parent_iface = g_type_interface_peek_parent (iface);

  iface->custom_tag_start = gtk_label_buildable_custom_tag_start;
  iface->custom_finished = gtk_label_buildable_custom_finished;
}

typedef struct {
  GtkBuilder    *builder;
  GObject       *object;
  PangoAttrList *attrs;
} PangoParserData;

static PangoAttribute *
attribute_from_text (GtkBuilder   *builder,
		     const gchar  *name, 
		     const gchar  *value,
		     GError      **error)
{
  PangoAttribute *attribute = NULL;
  PangoAttrType   type;
  PangoLanguage  *language;
  PangoFontDescription *font_desc;
  GdkColor       *color;
  GValue          val = { 0, };

  if (!gtk_builder_value_from_string_type (builder, PANGO_TYPE_ATTR_TYPE, name, &val, error))
    return NULL;

  type = g_value_get_enum (&val);
  g_value_unset (&val);

  switch (type)
    {
      /* PangoAttrLanguage */
    case PANGO_ATTR_LANGUAGE:
      if ((language = pango_language_from_string (value)))
	{
	  attribute = pango_attr_language_new (language);
	  g_value_init (&val, G_TYPE_INT);
	}
      break;
      /* PangoAttrInt */
    case PANGO_ATTR_STYLE:
      if (gtk_builder_value_from_string_type (builder, PANGO_TYPE_STYLE, value, &val, error))
	attribute = pango_attr_style_new (g_value_get_enum (&val));
      break;
    case PANGO_ATTR_WEIGHT:
      if (gtk_builder_value_from_string_type (builder, PANGO_TYPE_WEIGHT, value, &val, error))
	attribute = pango_attr_weight_new (g_value_get_enum (&val));
      break;
    case PANGO_ATTR_VARIANT:
      if (gtk_builder_value_from_string_type (builder, PANGO_TYPE_VARIANT, value, &val, error))
	attribute = pango_attr_variant_new (g_value_get_enum (&val));
      break;
    case PANGO_ATTR_STRETCH:
      if (gtk_builder_value_from_string_type (builder, PANGO_TYPE_STRETCH, value, &val, error))
	attribute = pango_attr_stretch_new (g_value_get_enum (&val));
      break;
    case PANGO_ATTR_UNDERLINE:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, value, &val, error))
	attribute = pango_attr_underline_new (g_value_get_boolean (&val));
      break;
    case PANGO_ATTR_STRIKETHROUGH:	
      if (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, value, &val, error))
	attribute = pango_attr_strikethrough_new (g_value_get_boolean (&val));
      break;
    case PANGO_ATTR_GRAVITY:
      if (gtk_builder_value_from_string_type (builder, PANGO_TYPE_GRAVITY, value, &val, error))
	attribute = pango_attr_gravity_new (g_value_get_enum (&val));
      break;
    case PANGO_ATTR_GRAVITY_HINT:
      if (gtk_builder_value_from_string_type (builder, PANGO_TYPE_GRAVITY_HINT, 
					      value, &val, error))
	attribute = pango_attr_gravity_hint_new (g_value_get_enum (&val));
      break;

      /* PangoAttrString */	  
    case PANGO_ATTR_FAMILY:
      attribute = pango_attr_family_new (value);
      g_value_init (&val, G_TYPE_INT);
      break;

      /* PangoAttrSize */	  
    case PANGO_ATTR_SIZE:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_INT, 
					      value, &val, error))
	attribute = pango_attr_size_new (g_value_get_int (&val));
      break;
    case PANGO_ATTR_ABSOLUTE_SIZE:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_INT, 
					      value, &val, error))
	attribute = pango_attr_size_new_absolute (g_value_get_int (&val));
      break;
    
      /* PangoAttrFontDesc */
    case PANGO_ATTR_FONT_DESC:
      if ((font_desc = pango_font_description_from_string (value)))
	{
	  attribute = pango_attr_font_desc_new (font_desc);
	  pango_font_description_free (font_desc);
	  g_value_init (&val, G_TYPE_INT);
	}
      break;

      /* PangoAttrColor */
    case PANGO_ATTR_FOREGROUND:
      if (gtk_builder_value_from_string_type (builder, GDK_TYPE_COLOR, 
					      value, &val, error))
	{
	  color = g_value_get_boxed (&val);
	  attribute = pango_attr_foreground_new (color->red, color->green, color->blue);
	}
      break;
    case PANGO_ATTR_BACKGROUND: 
      if (gtk_builder_value_from_string_type (builder, GDK_TYPE_COLOR, 
					      value, &val, error))
	{
	  color = g_value_get_boxed (&val);
	  attribute = pango_attr_background_new (color->red, color->green, color->blue);
	}
      break;
    case PANGO_ATTR_UNDERLINE_COLOR:
      if (gtk_builder_value_from_string_type (builder, GDK_TYPE_COLOR, 
					      value, &val, error))
	{
	  color = g_value_get_boxed (&val);
	  attribute = pango_attr_underline_color_new (color->red, color->green, color->blue);
	}
      break;
    case PANGO_ATTR_STRIKETHROUGH_COLOR:
      if (gtk_builder_value_from_string_type (builder, GDK_TYPE_COLOR, 
					      value, &val, error))
	{
	  color = g_value_get_boxed (&val);
	  attribute = pango_attr_strikethrough_color_new (color->red, color->green, color->blue);
	}
      break;
      
      /* PangoAttrShape */
    case PANGO_ATTR_SHAPE:
      /* Unsupported for now */
      break;
      /* PangoAttrFloat */
    case PANGO_ATTR_SCALE:
      if (gtk_builder_value_from_string_type (builder, G_TYPE_DOUBLE, 
					      value, &val, error))
	attribute = pango_attr_scale_new (g_value_get_double (&val));
      break;

    case PANGO_ATTR_INVALID:
    case PANGO_ATTR_LETTER_SPACING:
    case PANGO_ATTR_RISE:
    case PANGO_ATTR_FALLBACK:
    default:
      break;
    }

  g_value_unset (&val);

  return attribute;
}


static void
pango_start_element (GMarkupParseContext *context,
		     const gchar         *element_name,
		     const gchar        **names,
		     const gchar        **values,
		     gpointer             user_data,
		     GError             **error)
{
  PangoParserData *data = (PangoParserData*)user_data;
  GValue val = { 0, };
  guint i;
  gint line_number, char_number;

  if (strcmp (element_name, "attribute") == 0)
    {
      PangoAttribute *attr = NULL;
      const gchar *name = NULL;
      const gchar *value = NULL;
      const gchar *start = NULL;
      const gchar *end = NULL;
      guint start_val = 0;
      guint end_val   = G_MAXUINT;

      for (i = 0; names[i]; i++)
	{
	  if (strcmp (names[i], "name") == 0)
	    name = values[i];
	  else if (strcmp (names[i], "value") == 0)
	    value = values[i];
	  else if (strcmp (names[i], "start") == 0)
	    start = values[i];
	  else if (strcmp (names[i], "end") == 0)
	    end = values[i];
	  else
	    {
	      g_markup_parse_context_get_position (context,
						   &line_number,
						   &char_number);
	      g_set_error (error,
			   GTK_BUILDER_ERROR,
			   GTK_BUILDER_ERROR_INVALID_ATTRIBUTE,
			   "%s:%d:%d '%s' is not a valid attribute of <%s>",
			   "<input>",
			   line_number, char_number, names[i], "attribute");
	      return;
	    }
	}

      if (!name || !value)
	{
	  g_markup_parse_context_get_position (context,
					       &line_number,
					       &char_number);
	  g_set_error (error,
		       GTK_BUILDER_ERROR,
		       GTK_BUILDER_ERROR_MISSING_ATTRIBUTE,
		       "%s:%d:%d <%s> requires attribute \"%s\"",
		       "<input>",
		       line_number, char_number, "attribute",
		       name ? "value" : "name");
	  return;
	}

      if (start)
	{
	  if (!gtk_builder_value_from_string_type (data->builder, G_TYPE_UINT, 
						   start, &val, error))
	    return;
	  start_val = g_value_get_uint (&val);
	  g_value_unset (&val);
	}

      if (end)
	{
	  if (!gtk_builder_value_from_string_type (data->builder, G_TYPE_UINT, 
						   end, &val, error))
	    return;
	  end_val = g_value_get_uint (&val);
	  g_value_unset (&val);
	}

      attr = attribute_from_text (data->builder, name, value, error);

      if (attr)
	{
          attr->start_index = start_val;
          attr->end_index   = end_val;

	  if (!data->attrs)
	    data->attrs = pango_attr_list_new ();

	  pango_attr_list_insert (data->attrs, attr);
	}
    }
  else if (strcmp (element_name, "attributes") == 0)
    ;
  else
    g_warning ("Unsupported tag for GtkLabel: %s\n", element_name);
}

static const GMarkupParser pango_parser =
  {
    pango_start_element,
  };

static gboolean
gtk_label_buildable_custom_tag_start (GtkBuildable     *buildable,
				      GtkBuilder       *builder,
				      GObject          *child,
				      const gchar      *tagname,
				      GMarkupParser    *parser,
				      gpointer         *data)
{
  if (buildable_parent_iface->custom_tag_start (buildable, builder, child, 
						tagname, parser, data))
    return TRUE;

  if (strcmp (tagname, "attributes") == 0)
    {
      PangoParserData *parser_data;

      parser_data = g_slice_new0 (PangoParserData);
      parser_data->builder = g_object_ref (builder);
      parser_data->object = g_object_ref (buildable);
      *parser = pango_parser;
      *data = parser_data;
      return TRUE;
    }
  return FALSE;
}

static void
gtk_label_buildable_custom_finished (GtkBuildable *buildable,
				     GtkBuilder   *builder,
				     GObject      *child,
				     const gchar  *tagname,
				     gpointer      user_data)
{
  PangoParserData *data;

  buildable_parent_iface->custom_finished (buildable, builder, child, 
					   tagname, user_data);

  if (strcmp (tagname, "attributes") == 0)
    {
      data = (PangoParserData*)user_data;

      if (data->attrs)
	{
	  gtk_label_set_attributes (GTK_LABEL (buildable), data->attrs);
	  pango_attr_list_unref (data->attrs);
	}

      g_object_unref (data->object);
      g_object_unref (data->builder);
      g_slice_free (PangoParserData, data);
    }
}


/**
 * gtk_label_new:
 * @str: The text of the label
 *
 * Creates a new label with the given text inside it. You can
 * pass %NULL to get an empty label widget.
 *
 * Return value: the new #GtkLabel
 **/
GtkWidget*
gtk_label_new (const gchar *str)
{
  GtkLabel *label;
  
  label = g_object_new (GTK_TYPE_LABEL, NULL);

  if (str && *str)
    gtk_label_set_text (label, str);
  
  return GTK_WIDGET (label);
}

/**
 * gtk_label_new_with_mnemonic:
 * @str: The text of the label, with an underscore in front of the
 *       mnemonic character
 *
 * Creates a new #GtkLabel, containing the text in @str.
 *
 * If characters in @str are preceded by an underscore, they are
 * underlined. If you need a literal underscore character in a label, use
 * '__' (two underscores). The first underlined character represents a 
 * keyboard accelerator called a mnemonic. The mnemonic key can be used 
 * to activate another widget, chosen automatically, or explicitly using
 * gtk_label_set_mnemonic_widget().
 * 
 * If gtk_label_set_mnemonic_widget() is not called, then the first 
 * activatable ancestor of the #GtkLabel will be chosen as the mnemonic 
 * widget. For instance, if the label is inside a button or menu item, 
 * the button or menu item will automatically become the mnemonic widget 
 * and be activated by the mnemonic.
 *
 * Return value: the new #GtkLabel
 **/
GtkWidget*
gtk_label_new_with_mnemonic (const gchar *str)
{
  GtkLabel *label;
  
  label = g_object_new (GTK_TYPE_LABEL, NULL);

  if (str && *str)
    gtk_label_set_text_with_mnemonic (label, str);
  
  return GTK_WIDGET (label);
}

static gboolean
gtk_label_mnemonic_activate (GtkWidget *widget,
			     gboolean   group_cycling)
{
  GtkWidget *parent;

  if (GTK_LABEL (widget)->mnemonic_widget)
    return gtk_widget_mnemonic_activate (GTK_LABEL (widget)->mnemonic_widget, group_cycling);

  /* Try to find the widget to activate by traversing the
   * widget's ancestry.
   */
  parent = widget->parent;

  if (GTK_IS_NOTEBOOK (parent))
    return FALSE;
  
  while (parent)
    {
      if (gtk_widget_get_can_focus (parent) ||
	  (!group_cycling && GTK_WIDGET_GET_CLASS (parent)->activate_signal) ||
          GTK_IS_NOTEBOOK (parent->parent) ||
	  GTK_IS_MENU_ITEM (parent))
	return gtk_widget_mnemonic_activate (parent, group_cycling);
      parent = parent->parent;
    }

  /* barf if there was nothing to activate */
  g_warning ("Couldn't find a target for a mnemonic activation.");
  gtk_widget_error_bell (widget);

  return FALSE;
}

static void
gtk_label_setup_mnemonic (GtkLabel *label,
			  guint     last_key)
{
  GtkWidget *widget = GTK_WIDGET (label);
  GtkWidget *toplevel;
  GtkWidget *mnemonic_menu;
  
  mnemonic_menu = g_object_get_data (G_OBJECT (label), "gtk-mnemonic-menu");
  
  if (last_key != GDK_VoidSymbol)
    {
      if (label->mnemonic_window)
	{
	  gtk_window_remove_mnemonic  (label->mnemonic_window,
				       last_key,
				       widget);
	  label->mnemonic_window = NULL;
	}
      if (mnemonic_menu)
	{
	  _gtk_menu_shell_remove_mnemonic (GTK_MENU_SHELL (mnemonic_menu),
					   last_key,
					   widget);
	  mnemonic_menu = NULL;
	}
    }
  
  if (label->mnemonic_keyval == GDK_VoidSymbol)
      goto done;

  connect_mnemonics_visible_notify (GTK_LABEL (widget));

  toplevel = gtk_widget_get_toplevel (widget);
  if (gtk_widget_is_toplevel (toplevel))
    {
      GtkWidget *menu_shell;
      
      menu_shell = gtk_widget_get_ancestor (widget,
					    GTK_TYPE_MENU_SHELL);

      if (menu_shell)
	{
	  _gtk_menu_shell_add_mnemonic (GTK_MENU_SHELL (menu_shell),
					label->mnemonic_keyval,
					widget);
	  mnemonic_menu = menu_shell;
	}
      
      if (!GTK_IS_MENU (menu_shell))
	{
	  gtk_window_add_mnemonic (GTK_WINDOW (toplevel),
				   label->mnemonic_keyval,
				   widget);
	  label->mnemonic_window = GTK_WINDOW (toplevel);
	}
    }
  
 done:
  g_object_set_data (G_OBJECT (label), I_("gtk-mnemonic-menu"), mnemonic_menu);
}

static void
gtk_label_hierarchy_changed (GtkWidget *widget,
			     GtkWidget *old_toplevel)
{
  GtkLabel *label = GTK_LABEL (widget);

  gtk_label_setup_mnemonic (label, label->mnemonic_keyval);
}

static void
label_shortcut_setting_apply (GtkLabel *label)
{
  gtk_label_recalculate (label);
  if (GTK_IS_ACCEL_LABEL (label))
    gtk_accel_label_refetch (GTK_ACCEL_LABEL (label));
}

static void
label_shortcut_setting_traverse_container (GtkWidget *widget,
                                           gpointer   data)
{
  if (GTK_IS_LABEL (widget))
    label_shortcut_setting_apply (GTK_LABEL (widget));
  else if (GTK_IS_CONTAINER (widget))
    gtk_container_forall (GTK_CONTAINER (widget),
                          label_shortcut_setting_traverse_container, data);
}

static void
label_shortcut_setting_changed (GtkSettings *settings)
{
  GList *list, *l;

  list = gtk_window_list_toplevels ();

  for (l = list; l ; l = l->next)
    {
      GtkWidget *widget = l->data;

      if (gtk_widget_get_settings (widget) == settings)
        gtk_container_forall (GTK_CONTAINER (widget),
                              label_shortcut_setting_traverse_container, NULL);
    }

  g_list_free (list);
}

static void
mnemonics_visible_apply (GtkWidget *widget,
                         gboolean   mnemonics_visible)
{
  GtkLabel *label;
  GtkLabelPrivate *priv;

  label = GTK_LABEL (widget);

  priv = GTK_LABEL_GET_PRIVATE (label);

  mnemonics_visible = mnemonics_visible != FALSE;

  if (priv->mnemonics_visible != mnemonics_visible)
    {
      priv->mnemonics_visible = mnemonics_visible;

      gtk_label_recalculate (label);
    }
}

static void
label_mnemonics_visible_traverse_container (GtkWidget *widget,
                                            gpointer   data)
{
  gboolean mnemonics_visible = GPOINTER_TO_INT (data);

  _gtk_label_mnemonics_visible_apply_recursively (widget, mnemonics_visible);
}

void
_gtk_label_mnemonics_visible_apply_recursively (GtkWidget *widget,
                                                gboolean   mnemonics_visible)
{
  if (GTK_IS_LABEL (widget))
    mnemonics_visible_apply (widget, mnemonics_visible);
  else if (GTK_IS_CONTAINER (widget))
    gtk_container_forall (GTK_CONTAINER (widget),
                          label_mnemonics_visible_traverse_container,
                          GINT_TO_POINTER (mnemonics_visible));
}

static void
label_mnemonics_visible_changed (GtkWindow  *window,
                                 GParamSpec *pspec,
                                 gpointer    data)
{
  gboolean mnemonics_visible;

  g_object_get (window, "mnemonics-visible", &mnemonics_visible, NULL);

  gtk_container_forall (GTK_CONTAINER (window),
                        label_mnemonics_visible_traverse_container,
                        GINT_TO_POINTER (mnemonics_visible));
}

static void
gtk_label_screen_changed (GtkWidget *widget,
			  GdkScreen *old_screen)
{
  GtkSettings *settings;
  gboolean shortcuts_connected;

  if (!gtk_widget_has_screen (widget))
    return;

  settings = gtk_widget_get_settings (widget);

  shortcuts_connected =
    GPOINTER_TO_INT (g_object_get_data (G_OBJECT (settings),
                                        "gtk-label-shortcuts-connected"));

  if (! shortcuts_connected)
    {
      g_signal_connect (settings, "notify::gtk-enable-mnemonics",
                        G_CALLBACK (label_shortcut_setting_changed),
                        NULL);
      g_signal_connect (settings, "notify::gtk-enable-accels",
                        G_CALLBACK (label_shortcut_setting_changed),
                        NULL);

      g_object_set_data (G_OBJECT (settings), "gtk-label-shortcuts-connected",
                         GINT_TO_POINTER (TRUE));
    }

  label_shortcut_setting_apply (GTK_LABEL (widget));
}


static void
label_mnemonic_widget_weak_notify (gpointer      data,
				   GObject      *where_the_object_was)
{
  GtkLabel *label = data;

  label->mnemonic_widget = NULL;
  g_object_notify (G_OBJECT (label), "mnemonic-widget");
}

/**
 * gtk_label_set_mnemonic_widget:
 * @label: a #GtkLabel
 * @widget: (allow-none): the target #GtkWidget
 *
 * If the label has been set so that it has an mnemonic key (using
 * i.e. gtk_label_set_markup_with_mnemonic(),
 * gtk_label_set_text_with_mnemonic(), gtk_label_new_with_mnemonic()
 * or the "use_underline" property) the label can be associated with a
 * widget that is the target of the mnemonic. When the label is inside
 * a widget (like a #GtkButton or a #GtkNotebook tab) it is
 * automatically associated with the correct widget, but sometimes
 * (i.e. when the target is a #GtkEntry next to the label) you need to
 * set it explicitly using this function.
 *
 * The target widget will be accelerated by emitting the 
 * GtkWidget::mnemonic-activate signal on it. The default handler for 
 * this signal will activate the widget if there are no mnemonic collisions 
 * and toggle focus between the colliding widgets otherwise.
 **/
void
gtk_label_set_mnemonic_widget (GtkLabel  *label,
			       GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_LABEL (label));
  if (widget)
    g_return_if_fail (GTK_IS_WIDGET (widget));

  if (label->mnemonic_widget)
    {
      gtk_widget_remove_mnemonic_label (label->mnemonic_widget, GTK_WIDGET (label));
      g_object_weak_unref (G_OBJECT (label->mnemonic_widget),
			   label_mnemonic_widget_weak_notify,
			   label);
    }
  label->mnemonic_widget = widget;
  if (label->mnemonic_widget)
    {
      g_object_weak_ref (G_OBJECT (label->mnemonic_widget),
		         label_mnemonic_widget_weak_notify,
		         label);
      gtk_widget_add_mnemonic_label (label->mnemonic_widget, GTK_WIDGET (label));
    }
  
  g_object_notify (G_OBJECT (label), "mnemonic-widget");
}

/**
 * gtk_label_get_mnemonic_widget:
 * @label: a #GtkLabel
 *
 * Retrieves the target of the mnemonic (keyboard shortcut) of this
 * label. See gtk_label_set_mnemonic_widget().
 *
 * Return value: (transfer none): the target of the label's mnemonic,
 *     or %NULL if none has been set and the default algorithm will be used.
 **/
GtkWidget *
gtk_label_get_mnemonic_widget (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), NULL);

  return label->mnemonic_widget;
}

/**
 * gtk_label_get_mnemonic_keyval:
 * @label: a #GtkLabel
 *
 * If the label has been set so that it has an mnemonic key this function
 * returns the keyval used for the mnemonic accelerator. If there is no
 * mnemonic set up it returns #GDK_VoidSymbol.
 *
 * Returns: GDK keyval usable for accelerators, or #GDK_VoidSymbol
 **/
guint
gtk_label_get_mnemonic_keyval (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), GDK_VoidSymbol);

  return label->mnemonic_keyval;
}

static void
gtk_label_set_text_internal (GtkLabel *label,
			     gchar    *str)
{
  g_free (label->text);
  
  label->text = str;

  gtk_label_select_region_index (label, 0, 0);
}

static void
gtk_label_set_label_internal (GtkLabel *label,
			      gchar    *str)
{
  g_free (label->label);
  
  label->label = str;

  g_object_notify (G_OBJECT (label), "label");
}

static void
gtk_label_set_use_markup_internal (GtkLabel *label,
				   gboolean  val)
{
  val = val != FALSE;
  if (label->use_markup != val)
    {
      label->use_markup = val;

      g_object_notify (G_OBJECT (label), "use-markup");
    }
}

static void
gtk_label_set_use_underline_internal (GtkLabel *label,
				      gboolean val)
{
  val = val != FALSE;
  if (label->use_underline != val)
    {
      label->use_underline = val;

      g_object_notify (G_OBJECT (label), "use-underline");
    }
}

static void
gtk_label_compose_effective_attrs (GtkLabel *label)
{
  PangoAttrIterator *iter;
  PangoAttribute    *attr;
  GSList            *iter_attrs, *l;

  if (label->attrs)
    {
      if (label->effective_attrs)
	{
	  if ((iter = pango_attr_list_get_iterator (label->attrs)))
	    {
	      do
		{
		  iter_attrs = pango_attr_iterator_get_attrs (iter);
		  for (l = iter_attrs; l; l = l->next)
		    {
		      attr = l->data;
		      pango_attr_list_insert (label->effective_attrs, attr);
		    }
		  g_slist_free (iter_attrs);
	        }
	      while (pango_attr_iterator_next (iter));
	      pango_attr_iterator_destroy (iter);
	    }
	}
      else
	label->effective_attrs =
	  pango_attr_list_ref (label->attrs);
    }
}

static void
gtk_label_set_attributes_internal (GtkLabel      *label,
				   PangoAttrList *attrs)
{
  if (attrs)
    pango_attr_list_ref (attrs);
  
  if (label->attrs)
    pango_attr_list_unref (label->attrs);
  label->attrs = attrs;

  g_object_notify (G_OBJECT (label), "attributes");
}


/* Calculates text, attrs and mnemonic_keyval from
 * label, use_underline and use_markup
 */
static void
gtk_label_recalculate (GtkLabel *label)
{
  guint keyval = label->mnemonic_keyval;

  if (label->use_markup)
    gtk_label_set_markup_internal (label, label->label, label->use_underline);
  else if (label->use_underline)
    gtk_label_set_uline_text_internal (label, label->label);
  else
    {
      if (!label->pattern_set)
        {
          if (label->effective_attrs)
            pango_attr_list_unref (label->effective_attrs);
          label->effective_attrs = NULL;
        }
      gtk_label_set_text_internal (label, g_strdup (label->label));
    }

  gtk_label_compose_effective_attrs (label);

  if (!label->use_underline)
    label->mnemonic_keyval = GDK_VoidSymbol;

  if (keyval != label->mnemonic_keyval)
    {
      gtk_label_setup_mnemonic (label, keyval);
      g_object_notify (G_OBJECT (label), "mnemonic-keyval");
    }

  gtk_label_clear_layout (label);
  gtk_label_clear_select_info (label);
  gtk_widget_queue_resize (GTK_WIDGET (label));
}

/**
 * gtk_label_set_text:
 * @label: a #GtkLabel
 * @str: The text you want to set
 *
 * Sets the text within the #GtkLabel widget. It overwrites any text that
 * was there before.  
 *
 * This will also clear any previously set mnemonic accelerators.
 **/
void
gtk_label_set_text (GtkLabel    *label,
		    const gchar *str)
{
  g_return_if_fail (GTK_IS_LABEL (label));
  
  g_object_freeze_notify (G_OBJECT (label));

  gtk_label_set_label_internal (label, g_strdup (str ? str : ""));
  gtk_label_set_use_markup_internal (label, FALSE);
  gtk_label_set_use_underline_internal (label, FALSE);
  
  gtk_label_recalculate (label);

  g_object_thaw_notify (G_OBJECT (label));
}

/**
 * gtk_label_set_attributes:
 * @label: a #GtkLabel
 * @attrs: a #PangoAttrList
 * 
 * Sets a #PangoAttrList; the attributes in the list are applied to the
 * label text. 
 *
 * <note><para>The attributes set with this function will be applied
 * and merged with any other attributes previously effected by way
 * of the #GtkLabel:use-underline or #GtkLabel:use-markup properties.
 * While it is not recommended to mix markup strings with manually set
 * attributes, if you must; know that the attributes will be applied
 * to the label after the markup string is parsed.</para></note>
 **/
void
gtk_label_set_attributes (GtkLabel         *label,
                          PangoAttrList    *attrs)
{
  g_return_if_fail (GTK_IS_LABEL (label));

  gtk_label_set_attributes_internal (label, attrs);

  gtk_label_recalculate (label);

  gtk_label_clear_layout (label);
  gtk_widget_queue_resize (GTK_WIDGET (label));
}

/**
 * gtk_label_get_attributes:
 * @label: a #GtkLabel
 *
 * Gets the attribute list that was set on the label using
 * gtk_label_set_attributes(), if any. This function does
 * not reflect attributes that come from the labels markup
 * (see gtk_label_set_markup()). If you want to get the
 * effective attributes for the label, use
 * pango_layout_get_attribute (gtk_label_get_layout (label)).
 *
 * Return value: (transfer none): the attribute list, or %NULL
 *     if none was set.
 **/
PangoAttrList *
gtk_label_get_attributes (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), NULL);

  return label->attrs;
}

/**
 * gtk_label_set_label:
 * @label: a #GtkLabel
 * @str: the new text to set for the label
 *
 * Sets the text of the label. The label is interpreted as
 * including embedded underlines and/or Pango markup depending
 * on the values of the #GtkLabel:use-underline" and
 * #GtkLabel:use-markup properties.
 **/
void
gtk_label_set_label (GtkLabel    *label,
		     const gchar *str)
{
  g_return_if_fail (GTK_IS_LABEL (label));

  g_object_freeze_notify (G_OBJECT (label));

  gtk_label_set_label_internal (label, g_strdup (str ? str : ""));
  gtk_label_recalculate (label);

  g_object_thaw_notify (G_OBJECT (label));
}

/**
 * gtk_label_get_label:
 * @label: a #GtkLabel
 *
 * Fetches the text from a label widget including any embedded
 * underlines indicating mnemonics and Pango markup. (See
 * gtk_label_get_text()).
 *
 * Return value: the text of the label widget. This string is
 *   owned by the widget and must not be modified or freed.
 **/
const gchar *
gtk_label_get_label (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), NULL);

  return label->label;
}

typedef struct
{
  GtkLabel *label;
  GList *links;
  GString *new_str;
  GdkColor *link_color;
  GdkColor *visited_link_color;
} UriParserData;

static void
start_element_handler (GMarkupParseContext  *context,
                       const gchar          *element_name,
                       const gchar         **attribute_names,
                       const gchar         **attribute_values,
                       gpointer              user_data,
                       GError              **error)
{
  UriParserData *pdata = user_data;

  if (strcmp (element_name, "a") == 0)
    {
      GtkLabelLink *link;
      const gchar *uri = NULL;
      const gchar *title = NULL;
      gboolean visited = FALSE;
      gint line_number;
      gint char_number;
      gint i;
      GdkColor *color = NULL;

      g_markup_parse_context_get_position (context, &line_number, &char_number);

      for (i = 0; attribute_names[i] != NULL; i++)
        {
          const gchar *attr = attribute_names[i];

          if (strcmp (attr, "href") == 0)
            uri = attribute_values[i];
          else if (strcmp (attr, "title") == 0)
            title = attribute_values[i];
          else
            {
              g_set_error (error,
                           G_MARKUP_ERROR,
                           G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
                           "Attribute '%s' is not allowed on the <a> tag "
                           "on line %d char %d",
                            attr, line_number, char_number);
              return;
            }
        }

      if (uri == NULL)
        {
          g_set_error (error,
                       G_MARKUP_ERROR,
                       G_MARKUP_ERROR_INVALID_CONTENT,
                       "Attribute 'href' was missing on the <a> tag "
                       "on line %d char %d",
                       line_number, char_number);
          return;
        }

      visited = FALSE;
      if (pdata->label->track_links && pdata->label->select_info)
        {
          GList *l;
          for (l = pdata->label->select_info->links; l; l = l->next)
            {
              link = l->data;
              if (strcmp (uri, link->uri) == 0)
                {
                  visited = link->visited;
                  break;
                }
            }
        }

      if (visited)
        color = pdata->visited_link_color;
      else
        color = pdata->link_color;

      g_string_append_printf (pdata->new_str,
                              "<span color=\"#%04x%04x%04x\" underline=\"single\">",
                              color->red,
                              color->green,
                              color->blue);

      link = g_new0 (GtkLabelLink, 1);
      link->uri = g_strdup (uri);
      link->title = g_strdup (title);
      link->visited = visited;
      pdata->links = g_list_append (pdata->links, link);
    }
  else
    {
      gint i;

      g_string_append_c (pdata->new_str, '<');
      g_string_append (pdata->new_str, element_name);

      for (i = 0; attribute_names[i] != NULL; i++)
        {
          const gchar *attr  = attribute_names[i];
          const gchar *value = attribute_values[i];
          gchar *newvalue;

          newvalue = g_markup_escape_text (value, -1);

          g_string_append_c (pdata->new_str, ' ');
          g_string_append (pdata->new_str, attr);
          g_string_append (pdata->new_str, "=\"");
          g_string_append (pdata->new_str, newvalue);
          g_string_append_c (pdata->new_str, '\"');

          g_free (newvalue);
        }
      g_string_append_c (pdata->new_str, '>');
    }
}

static void
end_element_handler (GMarkupParseContext  *context,
                     const gchar          *element_name,
                     gpointer              user_data,
                     GError              **error)
{
  UriParserData *pdata = user_data;

  if (!strcmp (element_name, "a"))
    g_string_append (pdata->new_str, "</span>");
  else
    {
      g_string_append (pdata->new_str, "</");
      g_string_append (pdata->new_str, element_name);
      g_string_append_c (pdata->new_str, '>');
    }
}

static void
text_handler (GMarkupParseContext  *context,
              const gchar          *text,
              gsize                 text_len,
              gpointer              user_data,
              GError              **error)
{
  UriParserData *pdata = user_data;
  gchar *newtext;

  newtext = g_markup_escape_text (text, text_len);
  g_string_append (pdata->new_str, newtext);
  g_free (newtext);
}

static const GMarkupParser markup_parser =
{
  start_element_handler,
  end_element_handler,
  text_handler,
  NULL,
  NULL
};

static gboolean
xml_isspace (gchar c)
{
  return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
}

static void
link_free (GtkLabelLink *link)
{
  g_free (link->uri);
  g_free (link->title);
  g_free (link);
}

static void
gtk_label_get_link_colors (GtkWidget  *widget,
                           GdkColor  **link_color,
                           GdkColor  **visited_link_color)
{
  gtk_widget_ensure_style (widget);
  gtk_widget_style_get (widget,
                        "link-color", link_color,
                        "visited-link-color", visited_link_color,
                        NULL);
  if (!*link_color)
    *link_color = gdk_color_copy (&default_link_color);
  if (!*visited_link_color)
    *visited_link_color = gdk_color_copy (&default_visited_link_color);
}

static gboolean
parse_uri_markup (GtkLabel     *label,
                  const gchar  *str,
                  gchar       **new_str,
                  GList       **links,
                  GError      **error)
{
  GMarkupParseContext *context = NULL;
  const gchar *p, *end;
  gboolean needs_root = TRUE;
  gsize length;
  UriParserData pdata;

  length = strlen (str);
  p = str;
  end = str + length;

  pdata.label = label;
  pdata.links = NULL;
  pdata.new_str = g_string_sized_new (length);

  gtk_label_get_link_colors (GTK_WIDGET (label), &pdata.link_color, &pdata.visited_link_color);

  while (p != end && xml_isspace (*p))
    p++;

  if (end - p >= 8 && strncmp (p, "<markup>", 8) == 0)
    needs_root = FALSE;

  context = g_markup_parse_context_new (&markup_parser, 0, &pdata, NULL);

  if (needs_root)
    {
      if (!g_markup_parse_context_parse (context, "<markup>", -1, error))
        goto failed;
    }

  if (!g_markup_parse_context_parse (context, str, length, error))
    goto failed;

  if (needs_root)
    {
      if (!g_markup_parse_context_parse (context, "</markup>", -1, error))
        goto failed;
    }

  if (!g_markup_parse_context_end_parse (context, error))
    goto failed;

  g_markup_parse_context_free (context);

  *new_str = g_string_free (pdata.new_str, FALSE);
  *links = pdata.links;

  gdk_color_free (pdata.link_color);
  gdk_color_free (pdata.visited_link_color);

  return TRUE;

failed:
  g_markup_parse_context_free (context);
  g_string_free (pdata.new_str, TRUE);
  g_list_foreach (pdata.links, (GFunc)link_free, NULL);
  g_list_free (pdata.links);
  gdk_color_free (pdata.link_color);
  gdk_color_free (pdata.visited_link_color);

  return FALSE;
}

static void
gtk_label_ensure_has_tooltip (GtkLabel *label)
{
  GList *l;
  gboolean has_tooltip = FALSE;

  for (l = label->select_info->links; l; l = l->next)
    {
      GtkLabelLink *link = l->data;
      if (link->title)
        {
          has_tooltip = TRUE;
          break;
        }
    }

  gtk_widget_set_has_tooltip (GTK_WIDGET (label), has_tooltip);
}

static void
gtk_label_set_markup_internal (GtkLabel    *label,
                               const gchar *str,
                               gboolean     with_uline)
{
  GtkLabelPrivate *priv = GTK_LABEL_GET_PRIVATE (label);
  gchar *text = NULL;
  GError *error = NULL;
  PangoAttrList *attrs = NULL;
  gunichar accel_char = 0;
  gchar *new_str;
  GList *links = NULL;

  if (!parse_uri_markup (label, str, &new_str, &links, &error))
    {
      g_warning ("Failed to set text from markup due to error parsing markup: %s",
                 error->message);
      g_error_free (error);
      return;
    }

  gtk_label_clear_links (label);
  if (links)
    {
      gtk_label_ensure_select_info (label);
      label->select_info->links = links;
      gtk_label_ensure_has_tooltip (label);
    }

  if (with_uline)
    {
      gboolean enable_mnemonics;
      gboolean auto_mnemonics;

      g_object_get (gtk_widget_get_settings (GTK_WIDGET (label)),
                    "gtk-enable-mnemonics", &enable_mnemonics,
                    "gtk-auto-mnemonics", &auto_mnemonics,
                    NULL);

      if (!(enable_mnemonics && priv->mnemonics_visible &&
            (!auto_mnemonics ||
             (gtk_widget_is_sensitive (GTK_WIDGET (label)) &&
              (!label->mnemonic_widget ||
               gtk_widget_is_sensitive (label->mnemonic_widget))))))
        {
          gchar *tmp;
          gchar *pattern;
          guint key;

          if (separate_uline_pattern (new_str, &key, &tmp, &pattern))
            {
              g_free (new_str);
              new_str = tmp;
              g_free (pattern);
            }
        }
    }

  if (!pango_parse_markup (new_str,
                           -1,
                           with_uline ? '_' : 0,
                           &attrs,
                           &text,
                           with_uline ? &accel_char : NULL,
                           &error))
    {
      g_warning ("Failed to set text from markup due to error parsing markup: %s",
                 error->message);
      g_free (new_str);
      g_error_free (error);
      return;
    }

  g_free (new_str);

  if (text)
    gtk_label_set_text_internal (label, text);

  if (attrs)
    {
      if (label->effective_attrs)
	pango_attr_list_unref (label->effective_attrs);
      label->effective_attrs = attrs;
    }

  if (accel_char != 0)
    label->mnemonic_keyval = gdk_keyval_to_lower (gdk_unicode_to_keyval (accel_char));
  else
    label->mnemonic_keyval = GDK_VoidSymbol;
}

/**
 * gtk_label_set_markup:
 * @label: a #GtkLabel
 * @str: a markup string (see <link linkend="PangoMarkupFormat">Pango markup format</link>)
 * 
 * Parses @str which is marked up with the <link
 * linkend="PangoMarkupFormat">Pango text markup language</link>, setting the
 * label's text and attribute list based on the parse results. If the @str is
 * external data, you may need to escape it with g_markup_escape_text() or
 * g_markup_printf_escaped()<!-- -->:
 * |[
 * char *markup;
 *   
 * markup = g_markup_printf_escaped ("&lt;span style=\"italic\"&gt;&percnt;s&lt;/span&gt;", str);
 * gtk_label_set_markup (GTK_LABEL (label), markup);
 * g_free (markup);
 * ]|
 **/
void
gtk_label_set_markup (GtkLabel    *label,
                      const gchar *str)
{  
  g_return_if_fail (GTK_IS_LABEL (label));

  g_object_freeze_notify (G_OBJECT (label));

  gtk_label_set_label_internal (label, g_strdup (str ? str : ""));
  gtk_label_set_use_markup_internal (label, TRUE);
  gtk_label_set_use_underline_internal (label, FALSE);
  
  gtk_label_recalculate (label);

  g_object_thaw_notify (G_OBJECT (label));
}

/**
 * gtk_label_set_markup_with_mnemonic:
 * @label: a #GtkLabel
 * @str: a markup string (see <link linkend="PangoMarkupFormat">Pango markup format</link>)
 * 
 * Parses @str which is marked up with the <link linkend="PangoMarkupFormat">Pango text markup language</link>,
 * setting the label's text and attribute list based on the parse results.
 * If characters in @str are preceded by an underscore, they are underlined
 * indicating that they represent a keyboard accelerator called a mnemonic.
 *
 * The mnemonic key can be used to activate another widget, chosen 
 * automatically, or explicitly using gtk_label_set_mnemonic_widget().
 **/
void
gtk_label_set_markup_with_mnemonic (GtkLabel    *label,
				    const gchar *str)
{
  g_return_if_fail (GTK_IS_LABEL (label));

  g_object_freeze_notify (G_OBJECT (label));

  gtk_label_set_label_internal (label, g_strdup (str ? str : ""));
  gtk_label_set_use_markup_internal (label, TRUE);
  gtk_label_set_use_underline_internal (label, TRUE);
  
  gtk_label_recalculate (label);

  g_object_thaw_notify (G_OBJECT (label));
}

/**
 * gtk_label_get_text:
 * @label: a #GtkLabel
 * 
 * Fetches the text from a label widget, as displayed on the
 * screen. This does not include any embedded underlines
 * indicating mnemonics or Pango markup. (See gtk_label_get_label())
 * 
 * Return value: the text in the label widget. This is the internal
 *   string used by the label, and must not be modified.
 **/
const gchar *
gtk_label_get_text (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), NULL);

  return label->text;
}

static PangoAttrList *
gtk_label_pattern_to_attrs (GtkLabel      *label,
			    const gchar   *pattern)
{
  const char *start;
  const char *p = label->text;
  const char *q = pattern;
  PangoAttrList *attrs;

  attrs = pango_attr_list_new ();

  while (1)
    {
      while (*p && *q && *q != '_')
	{
	  p = g_utf8_next_char (p);
	  q++;
	}
      start = p;
      while (*p && *q && *q == '_')
	{
	  p = g_utf8_next_char (p);
	  q++;
	}
      
      if (p > start)
	{
	  PangoAttribute *attr = pango_attr_underline_new (PANGO_UNDERLINE_LOW);
	  attr->start_index = start - label->text;
	  attr->end_index = p - label->text;
	  
	  pango_attr_list_insert (attrs, attr);
	}
      else
	break;
    }

  return attrs;
}

static void
gtk_label_set_pattern_internal (GtkLabel    *label,
				const gchar *pattern,
                                gboolean     is_mnemonic)
{
  GtkLabelPrivate *priv = GTK_LABEL_GET_PRIVATE (label);
  PangoAttrList *attrs;
  gboolean enable_mnemonics;
  gboolean auto_mnemonics;

  g_return_if_fail (GTK_IS_LABEL (label));

  if (label->pattern_set)
    return;

  if (is_mnemonic)
    {
      g_object_get (gtk_widget_get_settings (GTK_WIDGET (label)),
  		    "gtk-enable-mnemonics", &enable_mnemonics,
	 	    "gtk-auto-mnemonics", &auto_mnemonics,
		    NULL);

      if (enable_mnemonics && priv->mnemonics_visible && pattern &&
          (!auto_mnemonics ||
           (gtk_widget_is_sensitive (GTK_WIDGET (label)) &&
            (!label->mnemonic_widget ||
             gtk_widget_is_sensitive (label->mnemonic_widget)))))
        attrs = gtk_label_pattern_to_attrs (label, pattern);
      else
        attrs = NULL;
    }
  else
    attrs = gtk_label_pattern_to_attrs (label, pattern);

  if (label->effective_attrs)
    pango_attr_list_unref (label->effective_attrs);
  label->effective_attrs = attrs;
}

void
gtk_label_set_pattern (GtkLabel	   *label,
		       const gchar *pattern)
{
  g_return_if_fail (GTK_IS_LABEL (label));
  
  label->pattern_set = FALSE;

  if (pattern)
    {
      gtk_label_set_pattern_internal (label, pattern, FALSE);
      label->pattern_set = TRUE;
    }
  else
    gtk_label_recalculate (label);

  gtk_label_clear_layout (label);
  gtk_widget_queue_resize (GTK_WIDGET (label));
}


/**
 * gtk_label_set_justify:
 * @label: a #GtkLabel
 * @jtype: a #GtkJustification
 *
 * Sets the alignment of the lines in the text of the label relative to
 * each other. %GTK_JUSTIFY_LEFT is the default value when the
 * widget is first created with gtk_label_new(). If you instead want
 * to set the alignment of the label as a whole, use
 * gtk_misc_set_alignment() instead. gtk_label_set_justify() has no
 * effect on labels containing only a single line.
 **/
void
gtk_label_set_justify (GtkLabel        *label,
		       GtkJustification jtype)
{
  g_return_if_fail (GTK_IS_LABEL (label));
  g_return_if_fail (jtype >= GTK_JUSTIFY_LEFT && jtype <= GTK_JUSTIFY_FILL);
  
  if ((GtkJustification) label->jtype != jtype)
    {
      label->jtype = jtype;

      /* No real need to be this drastic, but easier than duplicating the code */
      gtk_label_clear_layout (label);
      
      g_object_notify (G_OBJECT (label), "justify");
      gtk_widget_queue_resize (GTK_WIDGET (label));
    }
}

/**
 * gtk_label_get_justify:
 * @label: a #GtkLabel
 *
 * Returns the justification of the label. See gtk_label_set_justify().
 *
 * Return value: #GtkJustification
 **/
GtkJustification
gtk_label_get_justify (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), 0);

  return label->jtype;
}

/**
 * gtk_label_set_ellipsize:
 * @label: a #GtkLabel
 * @mode: a #PangoEllipsizeMode
 *
 * Sets the mode used to ellipsize (add an ellipsis: "...") to the text 
 * if there is not enough space to render the entire string.
 *
 * Since: 2.6
 **/
void
gtk_label_set_ellipsize (GtkLabel          *label,
			 PangoEllipsizeMode mode)
{
  g_return_if_fail (GTK_IS_LABEL (label));
  g_return_if_fail (mode >= PANGO_ELLIPSIZE_NONE && mode <= PANGO_ELLIPSIZE_END);
  
  if ((PangoEllipsizeMode) label->ellipsize != mode)
    {
      label->ellipsize = mode;

      /* No real need to be this drastic, but easier than duplicating the code */
      gtk_label_clear_layout (label);
      
      g_object_notify (G_OBJECT (label), "ellipsize");
      gtk_widget_queue_resize (GTK_WIDGET (label));
    }
}

/**
 * gtk_label_get_ellipsize:
 * @label: a #GtkLabel
 *
 * Returns the ellipsizing position of the label. See gtk_label_set_ellipsize().
 *
 * Return value: #PangoEllipsizeMode
 *
 * Since: 2.6
 **/
PangoEllipsizeMode
gtk_label_get_ellipsize (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), PANGO_ELLIPSIZE_NONE);

  return label->ellipsize;
}

/**
 * gtk_label_set_width_chars:
 * @label: a #GtkLabel
 * @n_chars: the new desired width, in characters.
 * 
 * Sets the desired width in characters of @label to @n_chars.
 * 
 * Since: 2.6
 **/
void
gtk_label_set_width_chars (GtkLabel *label,
			   gint      n_chars)
{
  GtkLabelPrivate *priv;

  g_return_if_fail (GTK_IS_LABEL (label));

  priv = GTK_LABEL_GET_PRIVATE (label);

  if (priv->width_chars != n_chars)
    {
      priv->width_chars = n_chars;
      g_object_notify (G_OBJECT (label), "width-chars");
      gtk_label_invalidate_wrap_width (label);
      gtk_widget_queue_resize (GTK_WIDGET (label));
    }
}

/**
 * gtk_label_get_width_chars:
 * @label: a #GtkLabel
 * 
 * Retrieves the desired width of @label, in characters. See
 * gtk_label_set_width_chars().
 * 
 * Return value: the width of the label in characters.
 * 
 * Since: 2.6
 **/
gint
gtk_label_get_width_chars (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), -1);

  return GTK_LABEL_GET_PRIVATE (label)->width_chars;
}

/**
 * gtk_label_set_max_width_chars:
 * @label: a #GtkLabel
 * @n_chars: the new desired maximum width, in characters.
 * 
 * Sets the desired maximum width in characters of @label to @n_chars.
 * 
 * Since: 2.6
 **/
void
gtk_label_set_max_width_chars (GtkLabel *label,
			       gint      n_chars)
{
  GtkLabelPrivate *priv;

  g_return_if_fail (GTK_IS_LABEL (label));

  priv = GTK_LABEL_GET_PRIVATE (label);

  if (priv->max_width_chars != n_chars)
    {
      priv->max_width_chars = n_chars;

      g_object_notify (G_OBJECT (label), "max-width-chars");
      gtk_label_invalidate_wrap_width (label);
      gtk_widget_queue_resize (GTK_WIDGET (label));
    }
}

/**
 * gtk_label_get_max_width_chars:
 * @label: a #GtkLabel
 * 
 * Retrieves the desired maximum width of @label, in characters. See
 * gtk_label_set_width_chars().
 * 
 * Return value: the maximum width of the label in characters.
 * 
 * Since: 2.6
 **/
gint
gtk_label_get_max_width_chars (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), -1);

  return GTK_LABEL_GET_PRIVATE (label)->max_width_chars;
}

/**
 * gtk_label_set_line_wrap:
 * @label: a #GtkLabel
 * @wrap: the setting
 *
 * Toggles line wrapping within the #GtkLabel widget. %TRUE makes it break
 * lines if text exceeds the widget's size. %FALSE lets the text get cut off
 * by the edge of the widget if it exceeds the widget size.
 *
 * Note that setting line wrapping to %TRUE does not make the label
 * wrap at its parent container's width, because GTK+ widgets
 * conceptually can't make their requisition depend on the parent
 * container's size. For a label that wraps at a specific position,
 * set the label's width using gtk_widget_set_size_request().
 **/
void
gtk_label_set_line_wrap (GtkLabel *label,
			 gboolean  wrap)
{
  g_return_if_fail (GTK_IS_LABEL (label));
  
  wrap = wrap != FALSE;
  
  if (label->wrap != wrap)
    {
      label->wrap = wrap;

      gtk_label_clear_layout (label);
      gtk_widget_queue_resize (GTK_WIDGET (label));
      g_object_notify (G_OBJECT (label), "wrap");
    }
}

/**
 * gtk_label_get_line_wrap:
 * @label: a #GtkLabel
 *
 * Returns whether lines in the label are automatically wrapped. 
 * See gtk_label_set_line_wrap().
 *
 * Return value: %TRUE if the lines of the label are automatically wrapped.
 */
gboolean
gtk_label_get_line_wrap (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), FALSE);

  return label->wrap;
}

/**
 * gtk_label_set_line_wrap_mode:
 * @label: a #GtkLabel
 * @wrap_mode: the line wrapping mode
 *
 * If line wrapping is on (see gtk_label_set_line_wrap()) this controls how
 * the line wrapping is done. The default is %PANGO_WRAP_WORD which means
 * wrap on word boundaries.
 *
 * Since: 2.10
 **/
void
gtk_label_set_line_wrap_mode (GtkLabel *label,
			      PangoWrapMode wrap_mode)
{
  g_return_if_fail (GTK_IS_LABEL (label));
  
  if (label->wrap_mode != wrap_mode)
    {
      label->wrap_mode = wrap_mode;
      g_object_notify (G_OBJECT (label), "wrap-mode");
      
      gtk_widget_queue_resize (GTK_WIDGET (label));
    }
}

/**
 * gtk_label_get_line_wrap_mode:
 * @label: a #GtkLabel
 *
 * Returns line wrap mode used by the label. See gtk_label_set_line_wrap_mode().
 *
 * Return value: %TRUE if the lines of the label are automatically wrapped.
 *
 * Since: 2.10
 */
PangoWrapMode
gtk_label_get_line_wrap_mode (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), FALSE);

  return label->wrap_mode;
}


void
gtk_label_get (GtkLabel *label,
	       gchar   **str)
{
  g_return_if_fail (GTK_IS_LABEL (label));
  g_return_if_fail (str != NULL);
  
  *str = label->text;
}

static void
gtk_label_destroy (GtkObject *object)
{
  GtkLabel *label = GTK_LABEL (object);

  gtk_label_set_mnemonic_widget (label, NULL);

  GTK_OBJECT_CLASS (gtk_label_parent_class)->destroy (object);
}

static void
gtk_label_finalize (GObject *object)
{
  GtkLabel *label = GTK_LABEL (object);

  g_free (label->label);
  g_free (label->text);

  if (label->layout)
    g_object_unref (label->layout);

  if (label->attrs)
    pango_attr_list_unref (label->attrs);

  if (label->effective_attrs)
    pango_attr_list_unref (label->effective_attrs);

  gtk_label_clear_links (label);
  g_free (label->select_info);

  G_OBJECT_CLASS (gtk_label_parent_class)->finalize (object);
}

static void
gtk_label_clear_layout (GtkLabel *label)
{
  if (label->layout)
    {
      g_object_unref (label->layout);
      label->layout = NULL;

      //gtk_label_clear_links (label);
    }
}

static gint
get_label_char_width (GtkLabel *label)
{
  GtkLabelPrivate *priv;
  PangoContext *context;
  PangoFontMetrics *metrics;
  gint char_width, digit_width, char_pixels, w;
  
  priv = GTK_LABEL_GET_PRIVATE (label);
  
  context = pango_layout_get_context (label->layout);
  metrics = pango_context_get_metrics (context, GTK_WIDGET (label)->style->font_desc, 
				       pango_context_get_language (context));
  
  char_width = pango_font_metrics_get_approximate_char_width (metrics);
  digit_width = pango_font_metrics_get_approximate_digit_width (metrics);
  char_pixels = MAX (char_width, digit_width);
  pango_font_metrics_unref (metrics);
  
  if (priv->width_chars < 0)
    {
      PangoRectangle rect;
      
      pango_layout_set_width (label->layout, -1);
      pango_layout_get_extents (label->layout, NULL, &rect);
      
      w = char_pixels * MAX (priv->max_width_chars, 3);
      w = MIN (rect.width, w);
    }
  else
    {
      /* enforce minimum width for ellipsized labels at ~3 chars */
      w = char_pixels * MAX (priv->width_chars, 3);
    }
  
  return w;
}

static void
gtk_label_invalidate_wrap_width (GtkLabel *label)
{
  GtkLabelPrivate *priv;

  priv = GTK_LABEL_GET_PRIVATE (label);

  priv->wrap_width = -1;
}

static gint
get_label_wrap_width (GtkLabel *label)
{
  GtkLabelPrivate *priv;

  priv = GTK_LABEL_GET_PRIVATE (label);
  
  if (priv->wrap_width < 0)
    {
      if (priv->width_chars > 0 || priv->max_width_chars > 0)
	priv->wrap_width = get_label_char_width (label);
      else
	{
	  PangoLayout *layout;
  
	  layout = gtk_widget_create_pango_layout (GTK_WIDGET (label), 
						   "This long string gives a good enough length for any line to have.");
	  pango_layout_get_size (layout, &priv->wrap_width, NULL);
	  g_object_unref (layout);
	}
    }

  return priv->wrap_width;
}

static void
gtk_label_ensure_layout (GtkLabel *label)
{
  GtkWidget *widget;
  PangoRectangle logical_rect;
  gboolean rtl;

  widget = GTK_WIDGET (label);

  rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;

  if (!label->layout)
    {
      PangoAlignment align = PANGO_ALIGN_LEFT; /* Quiet gcc */
      gdouble angle = gtk_label_get_angle (label);

      if (angle != 0.0 && !label->wrap && !label->ellipsize && !label->select_info)
	{
	  /* We rotate the standard singleton PangoContext for the widget,
	   * depending on the fact that it's meant pretty much exclusively
	   * for our use.
	   */
	  PangoMatrix matrix = PANGO_MATRIX_INIT;
	  
	  pango_matrix_rotate (&matrix, angle);

	  pango_context_set_matrix (gtk_widget_get_pango_context (widget), &matrix);
	  
	  label->have_transform = TRUE;
	}
      else 
	{
	  if (label->have_transform)
	    pango_context_set_matrix (gtk_widget_get_pango_context (widget), NULL);

	  label->have_transform = FALSE;
	}

      label->layout = gtk_widget_create_pango_layout (widget, label->text);

      if (label->effective_attrs)
	pango_layout_set_attributes (label->layout, label->effective_attrs);

      gtk_label_rescan_links (label);

      switch (label->jtype)
	{
	case GTK_JUSTIFY_LEFT:
	  align = rtl ? PANGO_ALIGN_RIGHT : PANGO_ALIGN_LEFT;
	  break;
	case GTK_JUSTIFY_RIGHT:
	  align = rtl ? PANGO_ALIGN_LEFT : PANGO_ALIGN_RIGHT;
	  break;
	case GTK_JUSTIFY_CENTER:
	  align = PANGO_ALIGN_CENTER;
	  break;
	case GTK_JUSTIFY_FILL:
	  align = rtl ? PANGO_ALIGN_RIGHT : PANGO_ALIGN_LEFT;
	  pango_layout_set_justify (label->layout, TRUE);
	  break;
	default:
	  g_assert_not_reached();
	}

      pango_layout_set_alignment (label->layout, align);
      pango_layout_set_ellipsize (label->layout, label->ellipsize);
      pango_layout_set_single_paragraph_mode (label->layout, label->single_line_mode);

      if (label->ellipsize)
	pango_layout_set_width (label->layout, 
				widget->allocation.width * PANGO_SCALE);
      else if (label->wrap)
	{
	  GtkWidgetAuxInfo *aux_info;
	  gint longest_paragraph;
	  gint width, height;

	  pango_layout_set_wrap (label->layout, label->wrap_mode);
	  
	  aux_info = _gtk_widget_get_aux_info (widget, FALSE);
	  if (aux_info && aux_info->width > 0)
	    pango_layout_set_width (label->layout, aux_info->width * PANGO_SCALE);
	  else
	    {
	      GdkScreen *screen = gtk_widget_get_screen (GTK_WIDGET (label));
	      gint wrap_width;
	      
	      pango_layout_set_width (label->layout, -1);
	      pango_layout_get_extents (label->layout, NULL, &logical_rect);

	      width = logical_rect.width;
	      
	      /* Try to guess a reasonable maximum width */
	      longest_paragraph = width;

	      wrap_width = get_label_wrap_width (label);
	      width = MIN (width, wrap_width);
	      width = MIN (width,
			   PANGO_SCALE * (gdk_screen_get_width (screen) + 1) / 2);
	      
	      pango_layout_set_width (label->layout, width);
	      pango_layout_get_extents (label->layout, NULL, &logical_rect);
	      width = logical_rect.width;
	      height = logical_rect.height;
	      
	      /* Unfortunately, the above may leave us with a very unbalanced looking paragraph,
	       * so we try short search for a narrower width that leaves us with the same height
	       */
	      if (longest_paragraph > 0)
		{
		  gint nlines, perfect_width;
		  
		  nlines = pango_layout_get_line_count (label->layout);
		  perfect_width = (longest_paragraph + nlines - 1) / nlines;
		  
		  if (perfect_width < width)
		    {
		      pango_layout_set_width (label->layout, perfect_width);
		      pango_layout_get_extents (label->layout, NULL, &logical_rect);
		      
		      if (logical_rect.height <= height)
			width = logical_rect.width;
		      else
			{
			  gint mid_width = (perfect_width + width) / 2;
			  
			  if (mid_width > perfect_width)
			    {
			      pango_layout_set_width (label->layout, mid_width);
			      pango_layout_get_extents (label->layout, NULL, &logical_rect);
			      
			      if (logical_rect.height <= height)
				width = logical_rect.width;
			    }
			}
		    }
		}
	      pango_layout_set_width (label->layout, width);
	    }
	}
      else /* !label->wrap */
	pango_layout_set_width (label->layout, -1);
    }
}

static void
gtk_label_size_request (GtkWidget      *widget,
			GtkRequisition *requisition)
{
  GtkLabel *label = GTK_LABEL (widget);
  GtkLabelPrivate *priv;
  gint width, height;
  PangoRectangle logical_rect;
  GtkWidgetAuxInfo *aux_info;

  priv = GTK_LABEL_GET_PRIVATE (widget);

  /*  
   * If word wrapping is on, then the height requisition can depend
   * on:
   *
   *   - Any width set on the widget via gtk_widget_set_size_request().
   *   - The padding of the widget (xpad, set by gtk_misc_set_padding)
   *
   * Instead of trying to detect changes to these quantities, if we
   * are wrapping, we just rewrap for each size request. Since
   * size requisitions are cached by the GTK+ core, this is not
   * expensive.
   */

  if (label->wrap)
    gtk_label_clear_layout (label);

  gtk_label_ensure_layout (label);

  width = label->misc.xpad * 2;
  height = label->misc.ypad * 2;

  aux_info = _gtk_widget_get_aux_info (widget, FALSE);

  if (label->have_transform)
    {
      PangoRectangle rect;
      PangoContext *context = pango_layout_get_context (label->layout);
      const PangoMatrix *matrix = pango_context_get_matrix (context);

      pango_layout_get_extents (label->layout, NULL, &rect);
      pango_matrix_transform_rectangle (matrix, &rect);
      pango_extents_to_pixels (&rect, NULL);
      
      requisition->width = width + rect.width;
      requisition->height = height + rect.height;

      return;
    }
  else
    pango_layout_get_extents (label->layout, NULL, &logical_rect);

  if ((label->wrap || label->ellipsize || 
       priv->width_chars > 0 || priv->max_width_chars > 0) && 
      aux_info && aux_info->width > 0)
    width += aux_info->width;
  else if (label->ellipsize || priv->width_chars > 0 || priv->max_width_chars > 0)
    {
      width += PANGO_PIXELS (get_label_char_width (label));
    }
  else
    width += PANGO_PIXELS (logical_rect.width);

  if (label->single_line_mode)
    {
      PangoContext *context;
      PangoFontMetrics *metrics;
      gint ascent, descent;

      context = pango_layout_get_context (label->layout);
      metrics = pango_context_get_metrics (context, widget->style->font_desc,
                                           pango_context_get_language (context));

      ascent = pango_font_metrics_get_ascent (metrics);
      descent = pango_font_metrics_get_descent (metrics);
      pango_font_metrics_unref (metrics);
    
      height += PANGO_PIXELS (ascent + descent);
    }
  else
    height += PANGO_PIXELS (logical_rect.height);

  requisition->width = width;
  requisition->height = height;
}

static void
gtk_label_size_allocate (GtkWidget     *widget,
                         GtkAllocation *allocation)
{
  GtkLabel *label;

  label = GTK_LABEL (widget);

  GTK_WIDGET_CLASS (gtk_label_parent_class)->size_allocate (widget, allocation);

  if (label->ellipsize)
    {
      if (label->layout)
	{
	  gint width;
	  PangoRectangle logical;

	  width = (allocation->width - label->misc.xpad * 2) * PANGO_SCALE;

	  pango_layout_set_width (label->layout, -1);
	  pango_layout_get_extents (label->layout, NULL, &logical);

	  if (logical.width > width)
	    pango_layout_set_width (label->layout, width);
	}
    }

  if (label->select_info && label->select_info->window)
    {
      gdk_window_move_resize (label->select_info->window,
                              allocation->x,
                              allocation->y,
                              allocation->width,
                              allocation->height);
    }
}

static void
gtk_label_update_cursor (GtkLabel *label)
{
  GtkWidget *widget;

  if (!label->select_info)
    return;

  widget = GTK_WIDGET (label);

  if (gtk_widget_get_realized (widget))
    {
      GdkDisplay *display;
      GdkCursor *cursor;

      if (gtk_widget_is_sensitive (widget))
        {
          display = gtk_widget_get_display (widget);

          if (label->select_info->active_link)
            cursor = gdk_cursor_new_for_display (display, GDK_HAND2);
          else if (label->select_info->selectable)
            cursor = gdk_cursor_new_for_display (display, GDK_XTERM);
          else
            cursor = NULL;
        }
      else
        cursor = NULL;

      gdk_window_set_cursor (label->select_info->window, cursor);

      if (cursor)
        gdk_cursor_unref (cursor);
    }
}

static void
gtk_label_state_changed (GtkWidget   *widget,
                         GtkStateType prev_state)
{
  GtkLabel *label = GTK_LABEL (widget);

  if (label->select_info)
    {
      gtk_label_select_region (label, 0, 0);
      gtk_label_update_cursor (label);
    }

  if (GTK_WIDGET_CLASS (gtk_label_parent_class)->state_changed)
    GTK_WIDGET_CLASS (gtk_label_parent_class)->state_changed (widget, prev_state);
}

static void
gtk_label_style_set (GtkWidget *widget,
		     GtkStyle  *previous_style)
{
  GtkLabel *label = GTK_LABEL (widget);

  /* We have to clear the layout, fonts etc. may have changed */
  gtk_label_clear_layout (label);
  gtk_label_invalidate_wrap_width (label);
}

static void 
gtk_label_direction_changed (GtkWidget        *widget,
			     GtkTextDirection previous_dir)
{
  GtkLabel *label = GTK_LABEL (widget);

  if (label->layout)
    pango_layout_context_changed (label->layout);

  GTK_WIDGET_CLASS (gtk_label_parent_class)->direction_changed (widget, previous_dir);
}

static void
get_layout_location (GtkLabel  *label,
                     gint      *xp,
                     gint      *yp)
{
  GtkMisc *misc;
  GtkWidget *widget; 
  GtkLabelPrivate *priv;
  gfloat xalign;
  gint req_width, x, y;
  PangoRectangle logical;
  
  misc = GTK_MISC (label);
  widget = GTK_WIDGET (label);
  priv = GTK_LABEL_GET_PRIVATE (label);

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
    xalign = misc->xalign;
  else
    xalign = 1.0 - misc->xalign;

  pango_layout_get_pixel_extents (label->layout, NULL, &logical);

  if (label->ellipsize || priv->width_chars > 0)
    {
      int width;

      width = pango_layout_get_width (label->layout);

      req_width = logical.width;
      if (width != -1)
	req_width = MIN(PANGO_PIXELS (width), req_width);
      req_width += 2 * misc->xpad;
    }
  else
    req_width = widget->requisition.width;

  x = floor (widget->allocation.x + (gint)misc->xpad +
	      xalign * (widget->allocation.width - req_width));

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
    x = MAX (x, widget->allocation.x + misc->xpad);
  else
    x = MIN (x, widget->allocation.x + widget->allocation.width - misc->xpad);
  x -= logical.x;

  /* bgo#315462 - For single-line labels, *do* align the requisition with
   * respect to the allocation, even if we are under-allocated.  For multi-line
   * labels, always show the top of the text when they are under-allocated.  The
   * rationale is this:
   *
   * - Single-line labels appear in GtkButtons, and it is very easy to get them
   *   to be smaller than their requisition.  The button may clip the label, but
   *   the label will still be able to show most of itself and the focus
   *   rectangle.  Also, it is fairly easy to read a single line of clipped text.
   *
   * - Multi-line labels should not be clipped to showing "something in the
   *   middle".  You want to read the first line, at least, to get some context.
   */
  if (pango_layout_get_line_count (label->layout) == 1)
    y = floor (widget->allocation.y + (gint)misc->ypad 
	       + (widget->allocation.height - widget->requisition.height) * misc->yalign);
  else
    y = floor (widget->allocation.y + (gint)misc->ypad 
	       + MAX (((widget->allocation.height - widget->requisition.height) * misc->yalign),
		      0));

  if (xp)
    *xp = x;

  if (yp)
    *yp = y;
}

static void
draw_insertion_cursor (GtkLabel      *label,
		       GdkRectangle  *cursor_location,
		       gboolean       is_primary,
		       PangoDirection direction,
		       gboolean       draw_arrow)
{
  GtkWidget *widget = GTK_WIDGET (label);
  GtkTextDirection text_dir;

  if (direction == PANGO_DIRECTION_LTR)
    text_dir = GTK_TEXT_DIR_LTR;
  else
    text_dir = GTK_TEXT_DIR_RTL;

  gtk_draw_insertion_cursor (widget, widget->window, &(widget->allocation),
			     cursor_location,
			     is_primary, text_dir, draw_arrow);
}

static PangoDirection
get_cursor_direction (GtkLabel *label)
{
  GSList *l;

  g_assert (label->select_info);

  gtk_label_ensure_layout (label);

  for (l = pango_layout_get_lines_readonly (label->layout); l; l = l->next)
    {
      PangoLayoutLine *line = l->data;

      /* If label->select_info->selection_end is at the very end of
       * the line, we don't know if the cursor is on this line or
       * the next without looking ahead at the next line. (End
       * of paragraph is different from line break.) But it's
       * definitely in this paragraph, which is good enough
       * to figure out the resolved direction.
       */
       if (line->start_index + line->length >= label->select_info->selection_end)
	return line->resolved_dir;
    }

  return PANGO_DIRECTION_LTR;
}

static void
gtk_label_draw_cursor (GtkLabel  *label, gint xoffset, gint yoffset)
{
  GtkWidget *widget;

  if (label->select_info == NULL)
    return;

  widget = GTK_WIDGET (label);
  
  if (gtk_widget_is_drawable (widget))
    {
      PangoDirection keymap_direction;
      PangoDirection cursor_direction;
      PangoRectangle strong_pos, weak_pos;
      gboolean split_cursor;
      PangoRectangle *cursor1 = NULL;
      PangoRectangle *cursor2 = NULL;
      GdkRectangle cursor_location;
      PangoDirection dir1 = PANGO_DIRECTION_NEUTRAL;
      PangoDirection dir2 = PANGO_DIRECTION_NEUTRAL;

      keymap_direction = gdk_keymap_get_direction (gdk_keymap_get_for_display (gtk_widget_get_display (widget)));
      cursor_direction = get_cursor_direction (label);

      gtk_label_ensure_layout (label);
      
      pango_layout_get_cursor_pos (label->layout, label->select_info->selection_end,
				   &strong_pos, &weak_pos);

      g_object_get (gtk_widget_get_settings (widget),
		    "gtk-split-cursor", &split_cursor,
		    NULL);

      dir1 = cursor_direction;
      
      if (split_cursor)
	{
	  cursor1 = &strong_pos;

	  if (strong_pos.x != weak_pos.x ||
	      strong_pos.y != weak_pos.y)
	    {
	      dir2 = (cursor_direction == PANGO_DIRECTION_LTR) ? PANGO_DIRECTION_RTL : PANGO_DIRECTION_LTR;
	      cursor2 = &weak_pos;
	    }
	}
      else
	{
	  if (keymap_direction == cursor_direction)
	    cursor1 = &strong_pos;
	  else
	    cursor1 = &weak_pos;
	}
      
      cursor_location.x = xoffset + PANGO_PIXELS (cursor1->x);
      cursor_location.y = yoffset + PANGO_PIXELS (cursor1->y);
      cursor_location.width = 0;
      cursor_location.height = PANGO_PIXELS (cursor1->height);

      draw_insertion_cursor (label,
			     &cursor_location, TRUE, dir1,
			     dir2 != PANGO_DIRECTION_NEUTRAL);
      
      if (dir2 != PANGO_DIRECTION_NEUTRAL)
	{
	  cursor_location.x = xoffset + PANGO_PIXELS (cursor2->x);
	  cursor_location.y = yoffset + PANGO_PIXELS (cursor2->y);
	  cursor_location.width = 0;
	  cursor_location.height = PANGO_PIXELS (cursor2->height);

	  draw_insertion_cursor (label,
				 &cursor_location, FALSE, dir2,
				 TRUE);
	}
    }
}

static GtkLabelLink *
gtk_label_get_focus_link (GtkLabel *label)
{
  GtkLabelSelectionInfo *info = label->select_info;
  GList *l;

  if (!info)
    return NULL;

  if (info->selection_anchor != info->selection_end)
    return NULL;

  for (l = info->links; l; l = l->next)
    {
      GtkLabelLink *link = l->data;
      if (link->start <= info->selection_anchor &&
          info->selection_anchor <= link->end)
        return link;
    }

  return NULL;
}

static gint
gtk_label_expose (GtkWidget      *widget,
		  GdkEventExpose *event)
{
  GtkLabel *label = GTK_LABEL (widget);
  GtkLabelSelectionInfo *info = label->select_info;
  gint x, y;

  gtk_label_ensure_layout (label);
  
  if (gtk_widget_get_visible (widget) && gtk_widget_get_mapped (widget) &&
      label->text && (*label->text != '\0'))
    {
      get_layout_location (label, &x, &y);

      gtk_paint_layout (widget->style,
                        widget->window,
                        gtk_widget_get_state (widget),
			FALSE,
                        &event->area,
                        widget,
                        "label",
                        x, y,
                        label->layout);

      if (info &&
          (info->selection_anchor != info->selection_end))
        {
          gint range[2];
          GdkRegion *clip;
	  GtkStateType state;
          cairo_t *cr;

          range[0] = info->selection_anchor;
          range[1] = info->selection_end;

          if (range[0] > range[1])
            {
              gint tmp = range[0];
              range[0] = range[1];
              range[1] = tmp;
            }

          clip = gdk_pango_layout_get_clip_region (label->layout,
                                                   x, y,
                                                   range,
                                                   1);
	  gdk_region_intersect (clip, event->region);

         /* FIXME should use gtk_paint, but it can't use a clip
           * region
           */

          cr = gdk_cairo_create (event->window);

          gdk_cairo_region (cr, clip);
          cairo_clip (cr);

	  state = GTK_STATE_SELECTED;
	  if (!gtk_widget_has_focus (widget))
	    state = GTK_STATE_ACTIVE;

          gdk_cairo_set_source_color (cr, &widget->style->base[state]);
          cairo_paint (cr);

          gdk_cairo_set_source_color (cr, &widget->style->text[state]);
          cairo_move_to (cr, x, y);
          _gtk_pango_fill_layout (cr, label->layout);

          cairo_destroy (cr);
          gdk_region_destroy (clip);
        }
      else if (info)
        {
          GtkLabelLink *focus_link;
          GtkLabelLink *active_link;
          gint range[2];
          GdkRegion *clip;
          GdkRectangle rect;
          GdkColor *text_color;
          GdkColor *base_color;
          GdkColor *link_color;
          GdkColor *visited_link_color;

          if (info->selectable && gtk_widget_has_focus (widget))
	    gtk_label_draw_cursor (label, x, y);

          focus_link = gtk_label_get_focus_link (label);
          active_link = info->active_link;


          if (active_link)
            {
              cairo_t *cr;

              range[0] = active_link->start;
              range[1] = active_link->end;

              cr = gdk_cairo_create (event->window);

              gdk_cairo_region (cr, event->region);
              cairo_clip (cr);

              clip = gdk_pango_layout_get_clip_region (label->layout,
                                                       x, y,
                                                       range,
                                                       1);
              gdk_cairo_region (cr, clip);
              cairo_clip (cr);
              gdk_region_destroy (clip);

              gtk_label_get_link_colors (widget, &link_color, &visited_link_color);
              if (active_link->visited)
                text_color = visited_link_color;
              else
                text_color = link_color;
              if (info->link_clicked)
                base_color = &widget->style->base[GTK_STATE_ACTIVE];
              else
                base_color = &widget->style->base[GTK_STATE_PRELIGHT];

              gdk_cairo_set_source_color (cr, base_color);
              cairo_paint (cr);

              gdk_cairo_set_source_color (cr, text_color);
              cairo_move_to (cr, x, y);
              _gtk_pango_fill_layout (cr, label->layout);

              gdk_color_free (link_color);
              gdk_color_free (visited_link_color);

              cairo_destroy (cr);
            }

          if (focus_link && gtk_widget_has_focus (widget))
            {
              range[0] = focus_link->start;
              range[1] = focus_link->end;

              clip = gdk_pango_layout_get_clip_region (label->layout,
                                                       x, y,
                                                       range,
                                                       1);
              gdk_region_get_clipbox (clip, &rect);

              gtk_paint_focus (widget->style, widget->window, gtk_widget_get_state (widget),
                               &event->area, widget, "label",
                               rect.x, rect.y, rect.width, rect.height);

              gdk_region_destroy (clip);
            }
        }
    }

  return FALSE;
}

static gboolean
separate_uline_pattern (const gchar  *str,
                        guint        *accel_key,
                        gchar       **new_str,
                        gchar       **pattern)
{
  gboolean underscore;
  const gchar *src;
  gchar *dest;
  gchar *pattern_dest;

  *accel_key = GDK_VoidSymbol;
  *new_str = g_new (gchar, strlen (str) + 1);
  *pattern = g_new (gchar, g_utf8_strlen (str, -1) + 1);

  underscore = FALSE;

  src = str;
  dest = *new_str;
  pattern_dest = *pattern;

  while (*src)
    {
      gunichar c;
      const gchar *next_src;

      c = g_utf8_get_char (src);
      if (c == (gunichar)-1)
	{
	  g_warning ("Invalid input string");
	  g_free (*new_str);
	  g_free (*pattern);

	  return FALSE;
	}
      next_src = g_utf8_next_char (src);

      if (underscore)
	{
	  if (c == '_')
	    *pattern_dest++ = ' ';
	  else
	    {
	      *pattern_dest++ = '_';
	      if (*accel_key == GDK_VoidSymbol)
		*accel_key = gdk_keyval_to_lower (gdk_unicode_to_keyval (c));
	    }

	  while (src < next_src)
	    *dest++ = *src++;

	  underscore = FALSE;
	}
      else
	{
	  if (c == '_')
	    {
	      underscore = TRUE;
	      src = next_src;
	    }
	  else
	    {
	      while (src < next_src)
		*dest++ = *src++;

	      *pattern_dest++ = ' ';
	    }
	}
    }

  *dest = 0;
  *pattern_dest = 0;

  return TRUE;
}

static void
gtk_label_set_uline_text_internal (GtkLabel    *label,
				   const gchar *str)
{
  guint accel_key = GDK_VoidSymbol;
  gchar *new_str;
  gchar *pattern;

  g_return_if_fail (GTK_IS_LABEL (label));
  g_return_if_fail (str != NULL);

  /* Split text into the base text and a separate pattern
   * of underscores.
   */
  if (!separate_uline_pattern (str, &accel_key, &new_str, &pattern))
    return;

  gtk_label_set_text_internal (label, new_str);
  gtk_label_set_pattern_internal (label, pattern, TRUE);
  label->mnemonic_keyval = accel_key;

  g_free (pattern);
}

guint
gtk_label_parse_uline (GtkLabel    *label,
		       const gchar *str)
{
  guint keyval;
  
  g_return_val_if_fail (GTK_IS_LABEL (label), GDK_VoidSymbol);
  g_return_val_if_fail (str != NULL, GDK_VoidSymbol);

  g_object_freeze_notify (G_OBJECT (label));
  
  gtk_label_set_label_internal (label, g_strdup (str ? str : ""));
  gtk_label_set_use_markup_internal (label, FALSE);
  gtk_label_set_use_underline_internal (label, TRUE);
  
  gtk_label_recalculate (label);

  keyval = label->mnemonic_keyval;
  if (keyval != GDK_VoidSymbol)
    {
      label->mnemonic_keyval = GDK_VoidSymbol;
      gtk_label_setup_mnemonic (label, keyval);
      g_object_notify (G_OBJECT (label), "mnemonic-keyval");
    }
  
  g_object_thaw_notify (G_OBJECT (label));

  return keyval;
}

/**
 * gtk_label_set_text_with_mnemonic:
 * @label: a #GtkLabel
 * @str: a string
 * 
 * Sets the label's text from the string @str.
 * If characters in @str are preceded by an underscore, they are underlined
 * indicating that they represent a keyboard accelerator called a mnemonic.
 * The mnemonic key can be used to activate another widget, chosen 
 * automatically, or explicitly using gtk_label_set_mnemonic_widget().
 **/
void
gtk_label_set_text_with_mnemonic (GtkLabel    *label,
				  const gchar *str)
{
  g_return_if_fail (GTK_IS_LABEL (label));
  g_return_if_fail (str != NULL);

  g_object_freeze_notify (G_OBJECT (label));

  gtk_label_set_label_internal (label, g_strdup (str ? str : ""));
  gtk_label_set_use_markup_internal (label, FALSE);
  gtk_label_set_use_underline_internal (label, TRUE);
  
  gtk_label_recalculate (label);

  g_object_thaw_notify (G_OBJECT (label));
}

static void
gtk_label_realize (GtkWidget *widget)
{
  GtkLabel *label;

  label = GTK_LABEL (widget);

  GTK_WIDGET_CLASS (gtk_label_parent_class)->realize (widget);

  if (label->select_info)
    gtk_label_create_window (label);
}

static void
gtk_label_unrealize (GtkWidget *widget)
{
  GtkLabel *label;

  label = GTK_LABEL (widget);

  if (label->select_info)
    gtk_label_destroy_window (label);

  GTK_WIDGET_CLASS (gtk_label_parent_class)->unrealize (widget);
}

static void
gtk_label_map (GtkWidget *widget)
{
  GtkLabel *label;

  label = GTK_LABEL (widget);

  GTK_WIDGET_CLASS (gtk_label_parent_class)->map (widget);

  if (label->select_info)
    gdk_window_show (label->select_info->window);
}

static void
gtk_label_unmap (GtkWidget *widget)
{
  GtkLabel *label;

  label = GTK_LABEL (widget);

  if (label->select_info)
    gdk_window_hide (label->select_info->window);

  GTK_WIDGET_CLASS (gtk_label_parent_class)->unmap (widget);
}

static void
window_to_layout_coords (GtkLabel *label,
                         gint     *x,
                         gint     *y)
{
  gint lx, ly;
  GtkWidget *widget;

  widget = GTK_WIDGET (label);
  
  /* get layout location in widget->window coords */
  get_layout_location (label, &lx, &ly);
  
  if (x)
    {
      *x += widget->allocation.x; /* go to widget->window */
      *x -= lx;                   /* go to layout */
    }

  if (y)
    {
      *y += widget->allocation.y; /* go to widget->window */
      *y -= ly;                   /* go to layout */
    }
}

#if 0
static void
layout_to_window_coords (GtkLabel *label,
                         gint     *x,
                         gint     *y)
{
  gint lx, ly;
  GtkWidget *widget;

  widget = GTK_WIDGET (label);
  
  /* get layout location in widget->window coords */
  get_layout_location (label, &lx, &ly);
  
  if (x)
    {
      *x += lx;                   /* go to widget->window */
      *x -= widget->allocation.x; /* go to selection window */
    }

  if (y)
    {
      *y += ly;                   /* go to widget->window */
      *y -= widget->allocation.y; /* go to selection window */
    }
}
#endif

static gboolean
get_layout_index (GtkLabel *label,
                  gint      x,
                  gint      y,
                  gint     *index)
{
  gint trailing = 0;
  const gchar *cluster;
  const gchar *cluster_end;
  gboolean inside;

  *index = 0;

  gtk_label_ensure_layout (label);

  window_to_layout_coords (label, &x, &y);

  x *= PANGO_SCALE;
  y *= PANGO_SCALE;

  inside = pango_layout_xy_to_index (label->layout,
                                     x, y,
                                     index, &trailing);

  cluster = label->text + *index;
  cluster_end = cluster;
  while (trailing)
    {
      cluster_end = g_utf8_next_char (cluster_end);
      --trailing;
    }

  *index += (cluster_end - cluster);

  return inside;
}

static void
gtk_label_select_word (GtkLabel *label)
{
  gint min, max;
  
  gint start_index = gtk_label_move_backward_word (label, label->select_info->selection_end);
  gint end_index = gtk_label_move_forward_word (label, label->select_info->selection_end);

  min = MIN (label->select_info->selection_anchor,
	     label->select_info->selection_end);
  max = MAX (label->select_info->selection_anchor,
	     label->select_info->selection_end);

  min = MIN (min, start_index);
  max = MAX (max, end_index);

  gtk_label_select_region_index (label, min, max);
}

static void
gtk_label_grab_focus (GtkWidget *widget)
{
  GtkLabel *label;
  gboolean select_on_focus;
  GtkLabelLink *link;

  label = GTK_LABEL (widget);

  if (label->select_info == NULL)
    return;

  GTK_WIDGET_CLASS (gtk_label_parent_class)->grab_focus (widget);

  if (label->select_info->selectable)
    {
      g_object_get (gtk_widget_get_settings (widget),
                    "gtk-label-select-on-focus",
                    &select_on_focus,
                    NULL);

      if (select_on_focus && !label->in_click)
        gtk_label_select_region (label, 0, -1);
    }
  else
    {
      if (label->select_info->links && !label->in_click)
        {
          link = label->select_info->links->data;
          label->select_info->selection_anchor = link->start;
          label->select_info->selection_end = link->start;
        }
    }
}

static gboolean
gtk_label_focus (GtkWidget        *widget,
                 GtkDirectionType  direction)
{
  GtkLabel *label = GTK_LABEL (widget);
  GtkLabelSelectionInfo *info = label->select_info;
  GtkLabelLink *focus_link;
  GList *l;

  if (!gtk_widget_is_focus (widget))
    {
      gtk_widget_grab_focus (widget);
      if (info)
        {
          focus_link = gtk_label_get_focus_link (label);
          if (focus_link && direction == GTK_DIR_TAB_BACKWARD)
            {
              l = g_list_last (info->links);
              focus_link = l->data;
              info->selection_anchor = focus_link->start;
              info->selection_end = focus_link->start;
            }
        }

      return TRUE;
    }

  if (!info)
    return FALSE;

  if (info->selectable)
    {
      gint index;

      if (info->selection_anchor != info->selection_end)
        goto out;

      index = info->selection_anchor;

      if (direction == GTK_DIR_TAB_FORWARD)
        for (l = info->links; l; l = l->next)
          {
            GtkLabelLink *link = l->data;

            if (link->start > index)
              {
                gtk_label_select_region_index (label, link->start, link->start);
                return TRUE;
              }
          }
      else if (direction == GTK_DIR_TAB_BACKWARD)
        for (l = g_list_last (info->links); l; l = l->prev)
          {
            GtkLabelLink *link = l->data;

            if (link->end < index)
              {
                gtk_label_select_region_index (label, link->start, link->start);
                return TRUE;
              }
          }

      goto out;
    }
  else
    {
      focus_link = gtk_label_get_focus_link (label);
      switch (direction)
        {
        case GTK_DIR_TAB_FORWARD:
          if (focus_link)
            {
              l = g_list_find (info->links, focus_link);
              l = l->next;
            }
          else
            l = info->links;
          break;

        case GTK_DIR_TAB_BACKWARD:
          if (focus_link)
            {
              l = g_list_find (info->links, focus_link);
              l = l->prev;
            }
          else
            l = g_list_last (info->links);
          break;

        default:
          goto out;
        }

      if (l)
        {
          focus_link = l->data;
          info->selection_anchor = focus_link->start;
          info->selection_end = focus_link->start;
          gtk_widget_queue_draw (widget);

          return TRUE;
        }
    }

out:

  return FALSE;
}

static gboolean
gtk_label_button_press (GtkWidget      *widget,
                        GdkEventButton *event)
{
  GtkLabel *label = GTK_LABEL (widget);
  GtkLabelSelectionInfo *info = label->select_info;
  gint index = 0;
  gint min, max;

  if (info == NULL)
    return FALSE;

  if (info->active_link)
    {
      if (_gtk_button_event_triggers_context_menu (event))
        {
          info->link_clicked = 1;
          gtk_label_do_popup (label, event);
          return TRUE;
        }
      else if (event->button == 1)
        {
          info->link_clicked = 1;
          gtk_widget_queue_draw (widget);
        }
    }

  if (!info->selectable)
    return FALSE;

  info->in_drag = FALSE;
  info->select_words = FALSE;

  if (_gtk_button_event_triggers_context_menu (event))
    {
      gtk_label_do_popup (label, event);

      return TRUE;
    }
  else if (event->button == 1)
    {
      if (!gtk_widget_has_focus (widget))
	{
	  label->in_click = TRUE;
	  gtk_widget_grab_focus (widget);
	  label->in_click = FALSE;
	}

      if (event->type == GDK_3BUTTON_PRESS)
	{
	  gtk_label_select_region_index (label, 0, strlen (label->text));
	  return TRUE;
	}

      if (event->type == GDK_2BUTTON_PRESS)
	{
          info->select_words = TRUE;
	  gtk_label_select_word (label);
	  return TRUE;
	}

      get_layout_index (label, event->x, event->y, &index);

      min = MIN (info->selection_anchor, info->selection_end);
      max = MAX (info->selection_anchor, info->selection_end);

      if ((info->selection_anchor != info->selection_end) &&
	  (event->state & GDK_SHIFT_MASK))
	{
	  /* extend (same as motion) */
	  min = MIN (min, index);
	  max = MAX (max, index);

	  /* ensure the anchor is opposite index */
	  if (index == min)
	    {
	      gint tmp = min;
	      min = max;
	      max = tmp;
	    }

	  gtk_label_select_region_index (label, min, max);
	}
      else
	{
	  if (event->type == GDK_3BUTTON_PRESS)
	    gtk_label_select_region_index (label, 0, strlen (label->text));
	  else if (event->type == GDK_2BUTTON_PRESS)
	    gtk_label_select_word (label);
	  else if (min < max && min <= index && index <= max)
	    {
	      info->in_drag = TRUE;
	      info->drag_start_x = event->x;
	      info->drag_start_y = event->y;
	    }
	  else
	    /* start a replacement */
	    gtk_label_select_region_index (label, index, index);
	}

      return TRUE;
    }

  return FALSE;
}

static gboolean
gtk_label_button_release (GtkWidget      *widget,
                          GdkEventButton *event)

{
  GtkLabel *label = GTK_LABEL (widget);
  GtkLabelSelectionInfo *info = label->select_info;
  gint index;

  if (info == NULL)
    return FALSE;

  if (info->in_drag)
    {
      info->in_drag = 0;

      get_layout_index (label, event->x, event->y, &index);
      gtk_label_select_region_index (label, index, index);

      return FALSE;
    }

  if (event->button != 1)
    return FALSE;

  if (info->active_link &&
      info->selection_anchor == info->selection_end &&
      info->link_clicked)
    {
      emit_activate_link (label, info->active_link);
      info->link_clicked = 0;

      return TRUE;
    }

  /* The goal here is to return TRUE iff we ate the
   * button press to start selecting.
   */
  return TRUE;
}

static void
connect_mnemonics_visible_notify (GtkLabel *label)
{
  GtkLabelPrivate *priv = GTK_LABEL_GET_PRIVATE (label);
  GtkWidget *toplevel;
  gboolean connected;

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (label));

  if (!GTK_IS_WINDOW (toplevel))
    return;

  /* always set up this widgets initial value */
  priv->mnemonics_visible =
    gtk_window_get_mnemonics_visible (GTK_WINDOW (toplevel));

  connected =
    GPOINTER_TO_INT (g_object_get_data (G_OBJECT (toplevel),
                                        "gtk-label-mnemonics-visible-connected"));

  if (!connected)
    {
      g_signal_connect (toplevel,
                        "notify::mnemonics-visible",
                        G_CALLBACK (label_mnemonics_visible_changed),
                        label);
      g_object_set_data (G_OBJECT (toplevel),
                         "gtk-label-mnemonics-visible-connected",
                         GINT_TO_POINTER (1));
    }
}

static void
drag_begin_cb (GtkWidget      *widget,
               GdkDragContext *context,
               gpointer        data)
{
  GtkLabel *label;
  GdkPixmap *pixmap = NULL;

  g_signal_handlers_disconnect_by_func (widget, drag_begin_cb, NULL);

  label = GTK_LABEL (widget);

  if ((label->select_info->selection_anchor !=
       label->select_info->selection_end) &&
      label->text)
    {
      gint start, end;
      gint len;

      start = MIN (label->select_info->selection_anchor,
                   label->select_info->selection_end);
      end = MAX (label->select_info->selection_anchor,
                 label->select_info->selection_end);
      
      len = strlen (label->text);
      
      if (end > len)
        end = len;
      
      if (start > len)
        start = len;
      
      pixmap = _gtk_text_util_create_drag_icon (widget, 
						label->text + start,
						end - start);
    }

  if (pixmap)
    gtk_drag_set_icon_pixmap (context,
                              gdk_drawable_get_colormap (pixmap),
                              pixmap,
                              NULL,
                              -2, -2);
  else
    gtk_drag_set_icon_default (context);
  
  if (pixmap)
    g_object_unref (pixmap);
}

static gboolean
gtk_label_motion (GtkWidget      *widget,
                  GdkEventMotion *event)
{
  GtkLabel *label = GTK_LABEL (widget);
  GtkLabelSelectionInfo *info = label->select_info;
  gint index;
  gint x, y;

  if (info == NULL)
    return FALSE;

  if (info->links && !info->in_drag)
    {
      GList *l;
      GtkLabelLink *link;
      gboolean found = FALSE;

      if (info->selection_anchor == info->selection_end)
        {
          gdk_window_get_pointer (event->window, &x, &y, NULL);
          if (get_layout_index (label, x, y, &index))
            {
              for (l = info->links; l != NULL; l = l->next)
                {
                  link = l->data;
                  if (index >= link->start && index <= link->end)
                    {
                      found = TRUE;
                      break;
                    }
                }
            }
        }

      if (found)
        {
          if (info->active_link != link)
            {
              info->link_clicked = 0;
              info->active_link = link;
              gtk_label_update_cursor (label);
              gtk_widget_queue_draw (widget);
            }
        }
      else
        {
          if (info->active_link != NULL)
            {
              info->link_clicked = 0;
              info->active_link = NULL;
              gtk_label_update_cursor (label);
              gtk_widget_queue_draw (widget);
            }
        }
    }

  if (!info->selectable)
    return FALSE;

  if ((event->state & GDK_BUTTON1_MASK) == 0)
    return FALSE;

  gdk_window_get_pointer (info->window, &x, &y, NULL);
 
  if (info->in_drag)
    {
      if (gtk_drag_check_threshold (widget,
				    info->drag_start_x,
				    info->drag_start_y,
				    event->x, event->y))
	{
	  GtkTargetList *target_list = gtk_target_list_new (NULL, 0);

	  gtk_target_list_add_text_targets (target_list, 0);

          g_signal_connect (widget, "drag-begin",
                            G_CALLBACK (drag_begin_cb), NULL);
	  gtk_drag_begin (widget, target_list,
			  GDK_ACTION_COPY,
			  1, (GdkEvent *)event);

	  info->in_drag = FALSE;

	  gtk_target_list_unref (target_list);
	}
    }
  else
    {
      get_layout_index (label, x, y, &index);

      if (info->select_words)
        {
          gint min, max;
          gint old_min, old_max;
          gint anchor, end;

          min = gtk_label_move_backward_word (label, index);
          max = gtk_label_move_forward_word (label, index);

          anchor = info->selection_anchor;
          end = info->selection_end;

          old_min = MIN (anchor, end);
          old_max = MAX (anchor, end);

          if (min < old_min)
            {
              anchor = min;
              end = old_max;
            }
          else if (old_max < max)
            {
              anchor = max;
              end = old_min;
            }
          else if (anchor == old_min)
            {
              if (anchor != min)
                anchor = max;
            }
          else
            {
              if (anchor != max)
                anchor = min;
            }

          gtk_label_select_region_index (label, anchor, end);
        }
      else
        gtk_label_select_region_index (label, info->selection_anchor, index);
    }

  return TRUE;
}

static gboolean
gtk_label_leave_notify (GtkWidget        *widget,
                        GdkEventCrossing *event)
{
  GtkLabel *label = GTK_LABEL (widget);

  if (label->select_info)
    {
      label->select_info->active_link = NULL;
      gtk_label_update_cursor (label);
      gtk_widget_queue_draw (widget);
    }

  if (GTK_WIDGET_CLASS (gtk_label_parent_class)->leave_notify_event)
    return GTK_WIDGET_CLASS (gtk_label_parent_class)->leave_notify_event (widget, event);

 return FALSE;
}

static void
gtk_label_create_window (GtkLabel *label)
{
  GtkWidget *widget;
  GdkWindowAttr attributes;
  gint attributes_mask;
  
  g_assert (label->select_info);
  widget = GTK_WIDGET (label);
  g_assert (gtk_widget_get_realized (widget));
  
  if (label->select_info->window)
    return;

  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.override_redirect = TRUE;
  attributes.event_mask = gtk_widget_get_events (widget) |
    GDK_BUTTON_PRESS_MASK        |
    GDK_BUTTON_RELEASE_MASK      |
    GDK_LEAVE_NOTIFY_MASK        |
    GDK_BUTTON_MOTION_MASK       |
    GDK_POINTER_MOTION_MASK      |
    GDK_POINTER_MOTION_HINT_MASK;
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_NOREDIR;
  if (gtk_widget_is_sensitive (widget))
    {
      attributes.cursor = gdk_cursor_new_for_display (gtk_widget_get_display (widget),
						      GDK_XTERM);
      attributes_mask |= GDK_WA_CURSOR;
    }


  label->select_info->window = gdk_window_new (widget->window,
                                               &attributes, attributes_mask);
  gdk_window_set_user_data (label->select_info->window, widget);

  if (attributes_mask & GDK_WA_CURSOR)
    gdk_cursor_unref (attributes.cursor);
}

static void
gtk_label_destroy_window (GtkLabel *label)
{
  g_assert (label->select_info);

  if (label->select_info->window == NULL)
    return;

  gdk_window_set_user_data (label->select_info->window, NULL);
  gdk_window_destroy (label->select_info->window);
  label->select_info->window = NULL;
}

static void
gtk_label_ensure_select_info (GtkLabel *label)
{
  if (label->select_info == NULL)
    {
      label->select_info = g_new0 (GtkLabelSelectionInfo, 1);

      gtk_widget_set_can_focus (GTK_WIDGET (label), TRUE);

      if (gtk_widget_get_realized (GTK_WIDGET (label)))
	gtk_label_create_window (label);

      if (gtk_widget_get_mapped (GTK_WIDGET (label)))
        gdk_window_show (label->select_info->window);
    }
}

static void
gtk_label_clear_select_info (GtkLabel *label)
{
  if (label->select_info == NULL)
    return;

  if (!label->select_info->selectable && !label->select_info->links)
    {
      gtk_label_destroy_window (label);

      g_free (label->select_info);
      label->select_info = NULL;

      gtk_widget_set_can_focus (GTK_WIDGET (label), FALSE);
    }
}

/**
 * gtk_label_set_selectable:
 * @label: a #GtkLabel
 * @setting: %TRUE to allow selecting text in the label
 *
 * Selectable labels allow the user to select text from the label, for
 * copy-and-paste.
 **/
void
gtk_label_set_selectable (GtkLabel *label,
                          gboolean  setting)
{
  gboolean old_setting;

  g_return_if_fail (GTK_IS_LABEL (label));

  setting = setting != FALSE;
  old_setting = label->select_info && label->select_info->selectable;

  if (setting)
    {
      gtk_label_ensure_select_info (label);
      label->select_info->selectable = TRUE;
      gtk_label_update_cursor (label);
    }
  else
    {
      if (old_setting)
        {
          /* unselect, to give up the selection */
          gtk_label_select_region (label, 0, 0);

          label->select_info->selectable = FALSE;
          gtk_label_clear_select_info (label);
          gtk_label_update_cursor (label);
        }
    }
  if (setting != old_setting)
    {
      g_object_freeze_notify (G_OBJECT (label));
      g_object_notify (G_OBJECT (label), "selectable");
      g_object_notify (G_OBJECT (label), "cursor-position");
      g_object_notify (G_OBJECT (label), "selection-bound");
      g_object_thaw_notify (G_OBJECT (label));
      gtk_widget_queue_draw (GTK_WIDGET (label));
    }
}

/**
 * gtk_label_get_selectable:
 * @label: a #GtkLabel
 * 
 * Gets the value set by gtk_label_set_selectable().
 * 
 * Return value: %TRUE if the user can copy text from the label
 **/
gboolean
gtk_label_get_selectable (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), FALSE);

  return label->select_info && label->select_info->selectable;
}

static void
free_angle (gpointer angle)
{
  g_slice_free (gdouble, angle);
}

/**
 * gtk_label_set_angle:
 * @label: a #GtkLabel
 * @angle: the angle that the baseline of the label makes with
 *   the horizontal, in degrees, measured counterclockwise
 * 
 * Sets the angle of rotation for the label. An angle of 90 reads from
 * from bottom to top, an angle of 270, from top to bottom. The angle
 * setting for the label is ignored if the label is selectable,
 * wrapped, or ellipsized.
 *
 * Since: 2.6
 **/
void
gtk_label_set_angle (GtkLabel *label,
		     gdouble   angle)
{
  gdouble *label_angle;

  g_return_if_fail (GTK_IS_LABEL (label));

  label_angle = (gdouble *)g_object_get_qdata (G_OBJECT (label), quark_angle);

  if (!label_angle)
    {
      label_angle = g_slice_new (gdouble);
      *label_angle = 0.0;
      g_object_set_qdata_full (G_OBJECT (label), quark_angle, 
			       label_angle, free_angle);
    }
  
  /* Canonicalize to [0,360]. We don't canonicalize 360 to 0, because
   * double property ranges are inclusive, and changing 360 to 0 would
   * make a property editor behave strangely.
   */
  if (angle < 0 || angle > 360.0)
    angle = angle - 360. * floor (angle / 360.);

  if (*label_angle != angle)
    {
      *label_angle = angle;
      
      gtk_label_clear_layout (label);
      gtk_widget_queue_resize (GTK_WIDGET (label));

      g_object_notify (G_OBJECT (label), "angle");
    }
}

/**
 * gtk_label_get_angle:
 * @label: a #GtkLabel
 * 
 * Gets the angle of rotation for the label. See
 * gtk_label_set_angle().
 * 
 * Return value: the angle of rotation for the label
 *
 * Since: 2.6
 **/
gdouble
gtk_label_get_angle  (GtkLabel *label)
{
  gdouble *angle;

  g_return_val_if_fail (GTK_IS_LABEL (label), 0.0);
  
  angle = (gdouble *)g_object_get_qdata (G_OBJECT (label), quark_angle);

  if (angle)
    return *angle;
  else
    return 0.0;
}

static void
gtk_label_set_selection_text (GtkLabel         *label,
			      GtkSelectionData *selection_data)
{
  if ((label->select_info->selection_anchor !=
       label->select_info->selection_end) &&
      label->text)
    {
      gint start, end;
      gint len;
      
      start = MIN (label->select_info->selection_anchor,
                   label->select_info->selection_end);
      end = MAX (label->select_info->selection_anchor,
                 label->select_info->selection_end);
      
      len = strlen (label->text);
      
      if (end > len)
        end = len;
      
      if (start > len)
        start = len;
      
      gtk_selection_data_set_text (selection_data,
				   label->text + start,
				   end - start);
    }
}

static void
gtk_label_drag_data_get (GtkWidget        *widget,
			 GdkDragContext   *context,
			 GtkSelectionData *selection_data,
			 guint             info,
			 guint             time)
{
  gtk_label_set_selection_text (GTK_LABEL (widget), selection_data);
}

static void
get_text_callback (GtkClipboard     *clipboard,
                   GtkSelectionData *selection_data,
                   guint             info,
                   gpointer          user_data_or_owner)
{
  gtk_label_set_selection_text (GTK_LABEL (user_data_or_owner), selection_data);
}

static void
clear_text_callback (GtkClipboard     *clipboard,
                     gpointer          user_data_or_owner)
{
  GtkLabel *label;

  label = GTK_LABEL (user_data_or_owner);

  if (label->select_info)
    {
      label->select_info->selection_anchor = label->select_info->selection_end;
      
      gtk_widget_queue_draw (GTK_WIDGET (label));
    }
}

static void
gtk_label_select_region_index (GtkLabel *label,
                               gint      anchor_index,
                               gint      end_index)
{
  g_return_if_fail (GTK_IS_LABEL (label));
  
  if (label->select_info && label->select_info->selectable)
    {
      GtkClipboard *clipboard;

      if (label->select_info->selection_anchor == anchor_index &&
	  label->select_info->selection_end == end_index)
	return;

      label->select_info->selection_anchor = anchor_index;
      label->select_info->selection_end = end_index;

      clipboard = gtk_widget_get_clipboard (GTK_WIDGET (label),
					    GDK_SELECTION_PRIMARY);

      if (anchor_index != end_index)
        {
          GtkTargetList *list;
          GtkTargetEntry *targets;
          gint n_targets;

          list = gtk_target_list_new (NULL, 0);
          gtk_target_list_add_text_targets (list, 0);
          targets = gtk_target_table_new_from_list (list, &n_targets);

          gtk_clipboard_set_with_owner (clipboard,
                                        targets, n_targets,
                                        get_text_callback,
                                        clear_text_callback,
                                        G_OBJECT (label));

          gtk_target_table_free (targets, n_targets);
          gtk_target_list_unref (list);
        }
      else
        {
          if (gtk_clipboard_get_owner (clipboard) == G_OBJECT (label))
            gtk_clipboard_clear (clipboard);
        }

      gtk_widget_queue_draw (GTK_WIDGET (label));

      g_object_freeze_notify (G_OBJECT (label));
      g_object_notify (G_OBJECT (label), "cursor-position");
      g_object_notify (G_OBJECT (label), "selection-bound");
      g_object_thaw_notify (G_OBJECT (label));
    }
}

/**
 * gtk_label_select_region:
 * @label: a #GtkLabel
 * @start_offset: start offset (in characters not bytes)
 * @end_offset: end offset (in characters not bytes)
 *
 * Selects a range of characters in the label, if the label is selectable.
 * See gtk_label_set_selectable(). If the label is not selectable,
 * this function has no effect. If @start_offset or
 * @end_offset are -1, then the end of the label will be substituted.
 **/
void
gtk_label_select_region  (GtkLabel *label,
                          gint      start_offset,
                          gint      end_offset)
{
  g_return_if_fail (GTK_IS_LABEL (label));
  
  if (label->text && label->select_info)
    {
      if (start_offset < 0)
        start_offset = g_utf8_strlen (label->text, -1);
      
      if (end_offset < 0)
        end_offset = g_utf8_strlen (label->text, -1);
      
      gtk_label_select_region_index (label,
                                     g_utf8_offset_to_pointer (label->text, start_offset) - label->text,
                                     g_utf8_offset_to_pointer (label->text, end_offset) - label->text);
    }
}

/**
 * gtk_label_get_selection_bounds:
 * @label: a #GtkLabel
 * @start: (out): return location for start of selection, as a character offset
 * @end: (out): return location for end of selection, as a character offset
 * 
 * Gets the selected range of characters in the label, returning %TRUE
 * if there's a selection.
 * 
 * Return value: %TRUE if selection is non-empty
 **/
gboolean
gtk_label_get_selection_bounds (GtkLabel  *label,
                                gint      *start,
                                gint      *end)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), FALSE);

  if (label->select_info == NULL)
    {
      /* not a selectable label */
      if (start)
        *start = 0;
      if (end)
        *end = 0;

      return FALSE;
    }
  else
    {
      gint start_index, end_index;
      gint start_offset, end_offset;
      gint len;
      
      start_index = MIN (label->select_info->selection_anchor,
                   label->select_info->selection_end);
      end_index = MAX (label->select_info->selection_anchor,
                 label->select_info->selection_end);

      len = strlen (label->text);

      if (end_index > len)
        end_index = len;

      if (start_index > len)
        start_index = len;
      
      start_offset = g_utf8_strlen (label->text, start_index);
      end_offset = g_utf8_strlen (label->text, end_index);

      if (start_offset > end_offset)
        {
          gint tmp = start_offset;
          start_offset = end_offset;
          end_offset = tmp;
        }
      
      if (start)
        *start = start_offset;

      if (end)
        *end = end_offset;

      return start_offset != end_offset;
    }
}


/**
 * gtk_label_get_layout:
 * @label: a #GtkLabel
 * 
 * Gets the #PangoLayout used to display the label.
 * The layout is useful to e.g. convert text positions to
 * pixel positions, in combination with gtk_label_get_layout_offsets().
 * The returned layout is owned by the label so need not be
 * freed by the caller.
 *
 * Return value: (transfer none): the #PangoLayout for this label
 **/
PangoLayout*
gtk_label_get_layout (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), NULL);

  gtk_label_ensure_layout (label);

  return label->layout;
}

/**
 * gtk_label_get_layout_offsets:
 * @label: a #GtkLabel
 * @x: (out) (allow-none): location to store X offset of layout, or %NULL
 * @y: (out) (allow-none): location to store Y offset of layout, or %NULL
 *
 * Obtains the coordinates where the label will draw the #PangoLayout
 * representing the text in the label; useful to convert mouse events
 * into coordinates inside the #PangoLayout, e.g. to take some action
 * if some part of the label is clicked. Of course you will need to
 * create a #GtkEventBox to receive the events, and pack the label
 * inside it, since labels are a #GTK_NO_WINDOW widget. Remember
 * when using the #PangoLayout functions you need to convert to
 * and from pixels using PANGO_PIXELS() or #PANGO_SCALE.
 **/
void
gtk_label_get_layout_offsets (GtkLabel *label,
                              gint     *x,
                              gint     *y)
{
  g_return_if_fail (GTK_IS_LABEL (label));

  gtk_label_ensure_layout (label);

  get_layout_location (label, x, y);
}

/**
 * gtk_label_set_use_markup:
 * @label: a #GtkLabel
 * @setting: %TRUE if the label's text should be parsed for markup.
 *
 * Sets whether the text of the label contains markup in <link
 * linkend="PangoMarkupFormat">Pango's text markup
 * language</link>. See gtk_label_set_markup().
 **/
void
gtk_label_set_use_markup (GtkLabel *label,
			  gboolean  setting)
{
  g_return_if_fail (GTK_IS_LABEL (label));

  gtk_label_set_use_markup_internal (label, setting);
  gtk_label_recalculate (label);
}

/**
 * gtk_label_get_use_markup:
 * @label: a #GtkLabel
 *
 * Returns whether the label's text is interpreted as marked up with
 * the <link linkend="PangoMarkupFormat">Pango text markup
 * language</link>. See gtk_label_set_use_markup ().
 *
 * Return value: %TRUE if the label's text will be parsed for markup.
 **/
gboolean
gtk_label_get_use_markup (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), FALSE);
  
  return label->use_markup;
}

/**
 * gtk_label_set_use_underline:
 * @label: a #GtkLabel
 * @setting: %TRUE if underlines in the text indicate mnemonics
 *
 * If true, an underline in the text indicates the next character should be
 * used for the mnemonic accelerator key.
 */
void
gtk_label_set_use_underline (GtkLabel *label,
			     gboolean  setting)
{
  g_return_if_fail (GTK_IS_LABEL (label));

  gtk_label_set_use_underline_internal (label, setting);
  gtk_label_recalculate (label);
}

/**
 * gtk_label_get_use_underline:
 * @label: a #GtkLabel
 *
 * Returns whether an embedded underline in the label indicates a
 * mnemonic. See gtk_label_set_use_underline().
 *
 * Return value: %TRUE whether an embedded underline in the label indicates
 *               the mnemonic accelerator keys.
 **/
gboolean
gtk_label_get_use_underline (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), FALSE);
  
  return label->use_underline;
}

/**
 * gtk_label_set_single_line_mode:
 * @label: a #GtkLabel
 * @single_line_mode: %TRUE if the label should be in single line mode
 *
 * Sets whether the label is in single line mode.
 *
 * Since: 2.6
 */
void
gtk_label_set_single_line_mode (GtkLabel *label,
                                gboolean single_line_mode)
{
  g_return_if_fail (GTK_IS_LABEL (label));

  single_line_mode = single_line_mode != FALSE;

  if (label->single_line_mode != single_line_mode)
    {
      label->single_line_mode = single_line_mode;

      gtk_label_clear_layout (label);
      gtk_widget_queue_resize (GTK_WIDGET (label));

      g_object_notify (G_OBJECT (label), "single-line-mode");
    }
}

/**
 * gtk_label_get_single_line_mode:
 * @label: a #GtkLabel
 *
 * Returns whether the label is in single line mode.
 *
 * Return value: %TRUE when the label is in single line mode.
 *
 * Since: 2.6
 **/
gboolean
gtk_label_get_single_line_mode  (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), FALSE);

  return label->single_line_mode;
}

/* Compute the X position for an offset that corresponds to the "more important
 * cursor position for that offset. We use this when trying to guess to which
 * end of the selection we should go to when the user hits the left or
 * right arrow key.
 */
static void
get_better_cursor (GtkLabel *label,
		   gint      index,
		   gint      *x,
		   gint      *y)
{
  GdkKeymap *keymap = gdk_keymap_get_for_display (gtk_widget_get_display (GTK_WIDGET (label)));
  PangoDirection keymap_direction = gdk_keymap_get_direction (keymap);
  PangoDirection cursor_direction = get_cursor_direction (label);
  gboolean split_cursor;
  PangoRectangle strong_pos, weak_pos;
  
  g_object_get (gtk_widget_get_settings (GTK_WIDGET (label)),
		"gtk-split-cursor", &split_cursor,
		NULL);

  gtk_label_ensure_layout (label);
  
  pango_layout_get_cursor_pos (label->layout, index,
			       &strong_pos, &weak_pos);

  if (split_cursor)
    {
      *x = strong_pos.x / PANGO_SCALE;
      *y = strong_pos.y / PANGO_SCALE;
    }
  else
    {
      if (keymap_direction == cursor_direction)
	{
	  *x = strong_pos.x / PANGO_SCALE;
	  *y = strong_pos.y / PANGO_SCALE;
	}
      else
	{
	  *x = weak_pos.x / PANGO_SCALE;
	  *y = weak_pos.y / PANGO_SCALE;
	}
    }
}


static gint
gtk_label_move_logically (GtkLabel *label,
			  gint      start,
			  gint      count)
{
  gint offset = g_utf8_pointer_to_offset (label->text,
					  label->text + start);

  if (label->text)
    {
      PangoLogAttr *log_attrs;
      gint n_attrs;
      gint length;

      gtk_label_ensure_layout (label);
      
      length = g_utf8_strlen (label->text, -1);

      pango_layout_get_log_attrs (label->layout, &log_attrs, &n_attrs);

      while (count > 0 && offset < length)
	{
	  do
	    offset++;
	  while (offset < length && !log_attrs[offset].is_cursor_position);
	  
	  count--;
	}
      while (count < 0 && offset > 0)
	{
	  do
	    offset--;
	  while (offset > 0 && !log_attrs[offset].is_cursor_position);
	  
	  count++;
	}
      
      g_free (log_attrs);
    }

  return g_utf8_offset_to_pointer (label->text, offset) - label->text;
}

static gint
gtk_label_move_visually (GtkLabel *label,
			 gint      start,
			 gint      count)
{
  gint index;

  index = start;
  
  while (count != 0)
    {
      int new_index, new_trailing;
      gboolean split_cursor;
      gboolean strong;

      gtk_label_ensure_layout (label);

      g_object_get (gtk_widget_get_settings (GTK_WIDGET (label)),
		    "gtk-split-cursor", &split_cursor,
		    NULL);

      if (split_cursor)
	strong = TRUE;
      else
	{
	  GdkKeymap *keymap = gdk_keymap_get_for_display (gtk_widget_get_display (GTK_WIDGET (label)));
	  PangoDirection keymap_direction = gdk_keymap_get_direction (keymap);

	  strong = keymap_direction == get_cursor_direction (label);
	}
      
      if (count > 0)
	{
	  pango_layout_move_cursor_visually (label->layout, strong, index, 0, 1, &new_index, &new_trailing);
	  count--;
	}
      else
	{
	  pango_layout_move_cursor_visually (label->layout, strong, index, 0, -1, &new_index, &new_trailing);
	  count++;
	}

      if (new_index < 0 || new_index == G_MAXINT)
	break;

      index = new_index;
      
      while (new_trailing--)
	index = g_utf8_next_char (label->text + new_index) - label->text;
    }
  
  return index;
}

static gint
gtk_label_move_forward_word (GtkLabel *label,
			     gint      start)
{
  gint new_pos = g_utf8_pointer_to_offset (label->text,
					   label->text + start);
  gint length;

  length = g_utf8_strlen (label->text, -1);
  if (new_pos < length)
    {
      PangoLogAttr *log_attrs;
      gint n_attrs;

      gtk_label_ensure_layout (label);
      
      pango_layout_get_log_attrs (label->layout, &log_attrs, &n_attrs);
      
      /* Find the next word end */
      new_pos++;
      while (new_pos < n_attrs && !log_attrs[new_pos].is_word_end)
	new_pos++;

      g_free (log_attrs);
    }

  return g_utf8_offset_to_pointer (label->text, new_pos) - label->text;
}


static gint
gtk_label_move_backward_word (GtkLabel *label,
			      gint      start)
{
  gint new_pos = g_utf8_pointer_to_offset (label->text,
					   label->text + start);

  if (new_pos > 0)
    {
      PangoLogAttr *log_attrs;
      gint n_attrs;

      gtk_label_ensure_layout (label);
      
      pango_layout_get_log_attrs (label->layout, &log_attrs, &n_attrs);
      
      new_pos -= 1;

      /* Find the previous word beginning */
      while (new_pos > 0 && !log_attrs[new_pos].is_word_start)
	new_pos--;

      g_free (log_attrs);
    }

  return g_utf8_offset_to_pointer (label->text, new_pos) - label->text;
}

static void
gtk_label_move_cursor (GtkLabel       *label,
		       GtkMovementStep step,
		       gint            count,
		       gboolean        extend_selection)
{
  gint old_pos;
  gint new_pos;
  
  if (label->select_info == NULL)
    return;

  old_pos = new_pos = label->select_info->selection_end;

  if (label->select_info->selection_end != label->select_info->selection_anchor &&
      !extend_selection)
    {
      /* If we have a current selection and aren't extending it, move to the
       * start/or end of the selection as appropriate
       */
      switch (step)
	{
	case GTK_MOVEMENT_VISUAL_POSITIONS:
	  {
	    gint end_x, end_y;
	    gint anchor_x, anchor_y;
	    gboolean end_is_left;
	    
	    get_better_cursor (label, label->select_info->selection_end, &end_x, &end_y);
	    get_better_cursor (label, label->select_info->selection_anchor, &anchor_x, &anchor_y);

	    end_is_left = (end_y < anchor_y) || (end_y == anchor_y && end_x < anchor_x);
	    
	    if (count < 0)
	      new_pos = end_is_left ? label->select_info->selection_end : label->select_info->selection_anchor;
	    else
	      new_pos = !end_is_left ? label->select_info->selection_end : label->select_info->selection_anchor;
	    break;
	  }
	case GTK_MOVEMENT_LOGICAL_POSITIONS:
	case GTK_MOVEMENT_WORDS:
	  if (count < 0)
	    new_pos = MIN (label->select_info->selection_end, label->select_info->selection_anchor);
	  else
	    new_pos = MAX (label->select_info->selection_end, label->select_info->selection_anchor);
	  break;
	case GTK_MOVEMENT_DISPLAY_LINE_ENDS:
	case GTK_MOVEMENT_PARAGRAPH_ENDS:
	case GTK_MOVEMENT_BUFFER_ENDS:
	  /* FIXME: Can do better here */
	  new_pos = count < 0 ? 0 : strlen (label->text);
	  break;
	case GTK_MOVEMENT_DISPLAY_LINES:
	case GTK_MOVEMENT_PARAGRAPHS:
	case GTK_MOVEMENT_PAGES:
	case GTK_MOVEMENT_HORIZONTAL_PAGES:
	  break;
	}
    }
  else
    {
      switch (step)
	{
	case GTK_MOVEMENT_LOGICAL_POSITIONS:
	  new_pos = gtk_label_move_logically (label, new_pos, count);
	  break;
	case GTK_MOVEMENT_VISUAL_POSITIONS:
	  new_pos = gtk_label_move_visually (label, new_pos, count);
          if (new_pos == old_pos)
            {
              if (!extend_selection)
                {
                  if (!gtk_widget_keynav_failed (GTK_WIDGET (label),
                                                 count > 0 ?
                                                 GTK_DIR_RIGHT : GTK_DIR_LEFT))
                    {
                      GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (label));

                      if (toplevel)
                        gtk_widget_child_focus (toplevel,
                                                count > 0 ?
                                                GTK_DIR_RIGHT : GTK_DIR_LEFT);
                    }
                }
              else
                {
                  gtk_widget_error_bell (GTK_WIDGET (label));
                }
            }
	  break;
	case GTK_MOVEMENT_WORDS:
	  while (count > 0)
	    {
	      new_pos = gtk_label_move_forward_word (label, new_pos);
	      count--;
	    }
	  while (count < 0)
	    {
	      new_pos = gtk_label_move_backward_word (label, new_pos);
	      count++;
	    }
          if (new_pos == old_pos)
            gtk_widget_error_bell (GTK_WIDGET (label));
	  break;
	case GTK_MOVEMENT_DISPLAY_LINE_ENDS:
	case GTK_MOVEMENT_PARAGRAPH_ENDS:
	case GTK_MOVEMENT_BUFFER_ENDS:
	  /* FIXME: Can do better here */
	  new_pos = count < 0 ? 0 : strlen (label->text);
          if (new_pos == old_pos)
            gtk_widget_error_bell (GTK_WIDGET (label));
	  break;
	case GTK_MOVEMENT_DISPLAY_LINES:
	case GTK_MOVEMENT_PARAGRAPHS:
	case GTK_MOVEMENT_PAGES:
	case GTK_MOVEMENT_HORIZONTAL_PAGES:
	  break;
	}
    }

  if (extend_selection)
    gtk_label_select_region_index (label,
				   label->select_info->selection_anchor,
				   new_pos);
  else
    gtk_label_select_region_index (label, new_pos, new_pos);
}

static void
gtk_label_copy_clipboard (GtkLabel *label)
{
  if (label->text && label->select_info)
    {
      gint start, end;
      gint len;
      GtkClipboard *clipboard;

      start = MIN (label->select_info->selection_anchor,
                   label->select_info->selection_end);
      end = MAX (label->select_info->selection_anchor,
                 label->select_info->selection_end);

      len = strlen (label->text);

      if (end > len)
        end = len;

      if (start > len)
        start = len;

      clipboard = gtk_widget_get_clipboard (GTK_WIDGET (label), GDK_SELECTION_CLIPBOARD);

      if (start != end)
	gtk_clipboard_set_text (clipboard, label->text + start, end - start);
      else
        {
          GtkLabelLink *link;

          link = gtk_label_get_focus_link (label);
          if (link)
            gtk_clipboard_set_text (clipboard, link->uri, -1);
        }
    }
}

static void
gtk_label_select_all (GtkLabel *label)
{
  gtk_label_select_region_index (label, 0, strlen (label->text));
}

/* Quick hack of a popup menu
 */
static void
activate_cb (GtkWidget *menuitem,
	     GtkLabel  *label)
{
  const gchar *signal = g_object_get_data (G_OBJECT (menuitem), "gtk-signal");
  g_signal_emit_by_name (label, signal);
}

static void
append_action_signal (GtkLabel     *label,
		      GtkWidget    *menu,
		      const gchar  *stock_id,
		      const gchar  *signal,
                      gboolean      sensitive)
{
  GtkWidget *menuitem = gtk_image_menu_item_new_from_stock (stock_id, NULL);

  g_object_set_data (G_OBJECT (menuitem), I_("gtk-signal"), (char *)signal);
  g_signal_connect (menuitem, "activate",
		    G_CALLBACK (activate_cb), label);

  gtk_widget_set_sensitive (menuitem, sensitive);
  
  gtk_widget_show (menuitem);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
}

static void
popup_menu_detach (GtkWidget *attach_widget,
		   GtkMenu   *menu)
{
  GtkLabel *label = GTK_LABEL (attach_widget);

  if (label->select_info)
    label->select_info->popup_menu = NULL;
}

static void
popup_position_func (GtkMenu   *menu,
                     gint      *x,
                     gint      *y,
                     gboolean  *push_in,
                     gpointer	user_data)
{
  GtkLabel *label;
  GtkWidget *widget;
  GtkRequisition req;
  GdkScreen *screen;

  label = GTK_LABEL (user_data);
  widget = GTK_WIDGET (label);

  g_return_if_fail (gtk_widget_get_realized (widget));

  screen = gtk_widget_get_screen (widget);
  gdk_window_get_origin (widget->window, x, y);

  *x += widget->allocation.x;
  *y += widget->allocation.y;

  gtk_widget_size_request (GTK_WIDGET (menu), &req);

  *x += widget->allocation.width / 2;
  *y += widget->allocation.height;

  *x = CLAMP (*x, 0, MAX (0, gdk_screen_get_width (screen) - req.width));
  *y = CLAMP (*y, 0, MAX (0, gdk_screen_get_height (screen) - req.height));
}

static void
open_link_activate_cb (GtkMenuItem *menu_item,
                       GtkLabel    *label)
{
  GtkLabelLink *link;

  link = gtk_label_get_current_link (label);

  if (link)
    emit_activate_link (label, link);
}

static void
copy_link_activate_cb (GtkMenuItem *menu_item,
                       GtkLabel    *label)
{
  GtkClipboard *clipboard;
  const gchar *uri;

  uri = gtk_label_get_current_uri (label);
  if (uri)
    {
      clipboard = gtk_widget_get_clipboard (GTK_WIDGET (label), GDK_SELECTION_CLIPBOARD);
      gtk_clipboard_set_text (clipboard, uri, -1);
    }
}

static gboolean
gtk_label_popup_menu (GtkWidget *widget)
{
  gtk_label_do_popup (GTK_LABEL (widget), NULL);

  return TRUE;
}

static void
gtk_label_do_popup (GtkLabel       *label,
                    GdkEventButton *event)
{
  GtkWidget *menuitem;
  GtkWidget *menu;
  GtkWidget *image;
  gboolean have_selection;
  GtkLabelLink *link;

  if (!label->select_info)
    return;

  if (label->select_info->popup_menu)
    gtk_widget_destroy (label->select_info->popup_menu);

  label->select_info->popup_menu = menu = gtk_menu_new ();

  gtk_menu_attach_to_widget (GTK_MENU (menu), GTK_WIDGET (label), popup_menu_detach);

  have_selection =
    label->select_info->selection_anchor != label->select_info->selection_end;

  if (event)
    {
      if (label->select_info->link_clicked)
        link = label->select_info->active_link;
      else
        link = NULL;
    }
  else
    link = gtk_label_get_focus_link (label);

  if (!have_selection && link)
    {
      /* Open Link */
      menuitem = gtk_image_menu_item_new_with_mnemonic (_("_Open Link"));
      gtk_widget_show (menuitem);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

      g_signal_connect (G_OBJECT (menuitem), "activate",
                        G_CALLBACK (open_link_activate_cb), label);

      image = gtk_image_new_from_stock (GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_MENU);
      gtk_widget_show (image);
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);

      /* Copy Link Address */
      menuitem = gtk_image_menu_item_new_with_mnemonic (_("Copy _Link Address"));
      gtk_widget_show (menuitem);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

      g_signal_connect (G_OBJECT (menuitem), "activate",
                        G_CALLBACK (copy_link_activate_cb), label);

      image = gtk_image_new_from_stock (GTK_STOCK_COPY, GTK_ICON_SIZE_MENU);
      gtk_widget_show (image);
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);
    }
  else
    {
      append_action_signal (label, menu, GTK_STOCK_CUT, "cut-clipboard", FALSE);
      append_action_signal (label, menu, GTK_STOCK_COPY, "copy-clipboard", have_selection);
      append_action_signal (label, menu, GTK_STOCK_PASTE, "paste-clipboard", FALSE);
  
      menuitem = gtk_image_menu_item_new_from_stock (GTK_STOCK_DELETE, NULL);
      gtk_widget_set_sensitive (menuitem, FALSE);
      gtk_widget_show (menuitem);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

      menuitem = gtk_separator_menu_item_new ();
      gtk_widget_show (menuitem);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

      menuitem = gtk_image_menu_item_new_from_stock (GTK_STOCK_SELECT_ALL, NULL);
      g_signal_connect_swapped (menuitem, "activate",
			        G_CALLBACK (gtk_label_select_all), label);
      gtk_widget_show (menuitem);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    }

  g_signal_emit (label, signals[POPULATE_POPUP], 0, menu);

  if (event)
    gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
                    NULL, NULL,
                    event->button, event->time);
  else
    {
      gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
                      popup_position_func, label,
                      0, gtk_get_current_event_time ());
      gtk_menu_shell_select_first (GTK_MENU_SHELL (menu), FALSE);
    }
}

static void
gtk_label_clear_links (GtkLabel *label)
{
  if (!label->select_info)
    return;

  g_list_foreach (label->select_info->links, (GFunc)link_free, NULL);
  g_list_free (label->select_info->links);
  label->select_info->links = NULL;
  label->select_info->active_link = NULL;
}

static void
gtk_label_rescan_links (GtkLabel *label)
{
  PangoLayout *layout = label->layout;
  PangoAttrList *attlist;
  PangoAttrIterator *iter;
  GList *links;

  if (!label->select_info || !label->select_info->links)
    return;

  attlist = pango_layout_get_attributes (layout);

  if (attlist == NULL)
    return;

  iter = pango_attr_list_get_iterator (attlist);

  links = label->select_info->links;

  do
    {
      PangoAttribute *underline;
      PangoAttribute *color;

      underline = pango_attr_iterator_get (iter, PANGO_ATTR_UNDERLINE);
      color = pango_attr_iterator_get (iter, PANGO_ATTR_FOREGROUND);

      if (underline != NULL && color != NULL)
        {
          gint start, end;
          PangoRectangle start_pos;
          PangoRectangle end_pos;
          GtkLabelLink *link;

          pango_attr_iterator_range (iter, &start, &end);
          pango_layout_index_to_pos (layout, start, &start_pos);
          pango_layout_index_to_pos (layout, end, &end_pos);

          if (links == NULL)
            {
              g_warning ("Ran out of links");
              break;
            }
          link = links->data;
          links = links->next;
          link->start = start;
          link->end = end;
        }
      } while (pango_attr_iterator_next (iter));

    pango_attr_iterator_destroy (iter);
}

static gboolean
gtk_label_activate_link (GtkLabel    *label,
                         const gchar *uri)
{
  GtkWidget *widget = GTK_WIDGET (label);
  GError *error = NULL;

  if (!gtk_show_uri (gtk_widget_get_screen (widget),
                     uri, gtk_get_current_event_time (), &error))
    {
      g_warning ("Unable to show '%s': %s", uri, error->message);
      g_error_free (error);
    }

  return TRUE;
}

static void
emit_activate_link (GtkLabel     *label,
                    GtkLabelLink *link)
{
  gboolean handled;

  g_signal_emit (label, signals[ACTIVATE_LINK], 0, link->uri, &handled);
  if (handled && label->track_links && !link->visited)
    {
      link->visited = TRUE;
      /* FIXME: shouldn't have to redo everything here */
      gtk_label_recalculate (label);
    }
}

static void
gtk_label_activate_current_link (GtkLabel *label)
{
  GtkLabelLink *link;
  GtkWidget *widget = GTK_WIDGET (label);

  link = gtk_label_get_focus_link (label);

  if (link)
    {
      emit_activate_link (label, link);
    }
  else
    {
      GtkWidget *toplevel;
      GtkWindow *window;

      toplevel = gtk_widget_get_toplevel (widget);
      if (GTK_IS_WINDOW (toplevel))
        {
          window = GTK_WINDOW (toplevel);

          if (window &&
              window->default_widget != widget &&
              !(widget == window->focus_widget &&
                (!window->default_widget || !gtk_widget_is_sensitive (window->default_widget))))
            gtk_window_activate_default (window);
        }
    }
}

static GtkLabelLink *
gtk_label_get_current_link (GtkLabel *label)
{
  GtkLabelLink *link;

  if (!label->select_info)
    return NULL;

  if (label->select_info->link_clicked)
    link = label->select_info->active_link;
  else
    link = gtk_label_get_focus_link (label);

  return link;
}

/**
 * gtk_label_get_current_uri:
 * @label: a #GtkLabel
 *
 * Returns the URI for the currently active link in the label.
 * The active link is the one under the mouse pointer or, in a
 * selectable label, the link in which the text cursor is currently
 * positioned.
 *
 * This function is intended for use in a #GtkLabel::activate-link handler
 * or for use in a #GtkWidget::query-tooltip handler.
 *
 * Returns: the currently active URI. The string is owned by GTK+ and must
 *   not be freed or modified.
 *
 * Since: 2.18
 */
const gchar *
gtk_label_get_current_uri (GtkLabel *label)
{
  GtkLabelLink *link;
  g_return_val_if_fail (GTK_IS_LABEL (label), NULL);

  link = gtk_label_get_current_link (label);

  if (link)
    return link->uri;

  return NULL;
}

/**
 * gtk_label_set_track_visited_links:
 * @label: a #GtkLabel
 * @track_links: %TRUE to track visited links
 *
 * Sets whether the label should keep track of clicked
 * links (and use a different color for them).
 *
 * Since: 2.18
 */
void
gtk_label_set_track_visited_links (GtkLabel *label,
                                   gboolean  track_links)
{
  g_return_if_fail (GTK_IS_LABEL (label));

  track_links = track_links != FALSE;

  if (label->track_links != track_links)
    {
      label->track_links = track_links;

      /* FIXME: shouldn't have to redo everything here */
      gtk_label_recalculate (label);

      g_object_notify (G_OBJECT (label), "track-visited-links");
    }
}

/**
 * gtk_label_get_track_visited_links:
 * @label: a #GtkLabel
 *
 * Returns whether the label is currently keeping track
 * of clicked links.
 *
 * Returns: %TRUE if clicked links are remembered
 *
 * Since: 2.18
 */
gboolean
gtk_label_get_track_visited_links (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), FALSE);

  return label->track_links;
}

static gboolean
gtk_label_query_tooltip (GtkWidget  *widget,
                         gint        x,
                         gint        y,
                         gboolean    keyboard_tip,
                         GtkTooltip *tooltip)
{
  GtkLabel *label = GTK_LABEL (widget);
  GtkLabelSelectionInfo *info = label->select_info;
  gint index = -1;
  GList *l;

  if (info && info->links)
    {
      if (keyboard_tip)
        {
          if (info->selection_anchor == info->selection_end)
            index = info->selection_anchor;
        }
      else
        {
          if (!get_layout_index (label, x, y, &index))
            index = -1;
        }

      if (index != -1)
        {
          for (l = info->links; l != NULL; l = l->next)
            {
              GtkLabelLink *link = l->data;
              if (index >= link->start && index <= link->end)
                {
                  if (link->title)
                    {
                      gtk_tooltip_set_markup (tooltip, link->title);
                      return TRUE;
                    }
                  break;
                }
            }
        }
    }

  return GTK_WIDGET_CLASS (gtk_label_parent_class)->query_tooltip (widget,
                                                                   x, y,
                                                                   keyboard_tip,
                                                                   tooltip);
}


#define __GTK_LABEL_C__
#include "gtkaliasdef.c"
