/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 2004-2006 Christian Hammond
 * Copyright (C) 2008 Cody Russell
 * Copyright (C) 2008 Red Hat, Inc.
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
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include <math.h>
#include <string.h>

#include "gdk/gdkkeysyms.h"
#include "gtkalignment.h"
#include "gtkbindings.h"
#include "gtkcelleditable.h"
#include "gtkclipboard.h"
#include "gtkdnd.h"
#include "gtkentry.h"
#include "gtkentrybuffer.h"
#include "gtkimagemenuitem.h"
#include "gtkimcontextsimple.h"
#include "gtkimmulticontext.h"
#include "gtkintl.h"
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkmenu.h"
#include "gtkmenuitem.h"
#include "gtkseparatormenuitem.h"
#include "gtkselection.h"
#include "gtksettings.h"
#include "gtkspinbutton.h"
#include "gtkstock.h"
#include "gtktextutil.h"
#include "gtkwindow.h"
#include "gtktreeview.h"
#include "gtktreeselection.h"
#include "gtkprivate.h"
#include "gtkentryprivate.h"
#include "gtkcelllayout.h"
#include "gtktooltip.h"
#include "gtkiconfactory.h"
#include "gtkicontheme.h"
#include "gtkalias.h"

#define GTK_ENTRY_COMPLETION_KEY "gtk-entry-completion-key"

#define MIN_ENTRY_WIDTH  150
#define DRAW_TIMEOUT     20
#define COMPLETION_TIMEOUT 300
#define PASSWORD_HINT_MAX 8

#define MAX_ICONS 2

#define IS_VALID_ICON_POSITION(pos)               \
  ((pos) == GTK_ENTRY_ICON_PRIMARY ||                   \
   (pos) == GTK_ENTRY_ICON_SECONDARY)

static const GtkBorder default_inner_border = { 2, 2, 2, 2 };
static GQuark          quark_inner_border   = 0;
static GQuark          quark_password_hint  = 0;
static GQuark          quark_cursor_hadjustment = 0;
static GQuark          quark_capslock_feedback = 0;

typedef struct _GtkEntryPrivate GtkEntryPrivate;

#define GTK_ENTRY_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_ENTRY, GtkEntryPrivate))

typedef struct
{
  GdkWindow *window;
  gchar *tooltip;
  guint insensitive    : 1;
  guint nonactivatable : 1;
  guint prelight       : 1;
  guint in_drag        : 1;
  guint pressed        : 1;

  GtkImageType  storage_type;
  GdkPixbuf    *pixbuf;
  gchar        *stock_id;
  gchar        *icon_name;
  GIcon        *gicon;

  GtkTargetList *target_list;
  GdkDragAction actions;
} EntryIconInfo;

struct _GtkEntryPrivate 
{
  GtkEntryBuffer* buffer;

  gfloat xalign;
  gint insert_pos;
  guint blink_time;  /* time in msec the cursor has blinked since last user event */
  guint interior_focus          : 1;
  guint real_changed            : 1;
  guint invisible_char_set      : 1;
  guint caps_lock_warning       : 1;
  guint caps_lock_warning_shown : 1;
  guint change_count            : 8;
  guint progress_pulse_mode     : 1;
  guint progress_pulse_way_back : 1;

  gint focus_width;
  GtkShadowType shadow_type;

  gdouble progress_fraction;
  gdouble progress_pulse_fraction;
  gdouble progress_pulse_current;

  EntryIconInfo *icons[MAX_ICONS];
  gint icon_margin;
  gint start_x;
  gint start_y;

  gchar *im_module;
};

typedef struct _GtkEntryPasswordHint GtkEntryPasswordHint;

struct _GtkEntryPasswordHint
{
  gint position;      /* Position (in text) of the last password hint */
  guint source_id;    /* Timeout source id */
};

typedef struct _GtkEntryCapslockFeedback GtkEntryCapslockFeedback;

struct _GtkEntryCapslockFeedback
{
  GtkWidget *entry;
  GtkWidget *window;
  GtkWidget *label;
};

enum {
  ACTIVATE,
  POPULATE_POPUP,
  MOVE_CURSOR,
  INSERT_AT_CURSOR,
  DELETE_FROM_CURSOR,
  BACKSPACE,
  CUT_CLIPBOARD,
  COPY_CLIPBOARD,
  PASTE_CLIPBOARD,
  TOGGLE_OVERWRITE,
  ICON_PRESS,
  ICON_RELEASE,
  PREEDIT_CHANGED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_BUFFER,
  PROP_CURSOR_POSITION,
  PROP_SELECTION_BOUND,
  PROP_EDITABLE,
  PROP_MAX_LENGTH,
  PROP_VISIBILITY,
  PROP_HAS_FRAME,
  PROP_INNER_BORDER,
  PROP_INVISIBLE_CHAR,
  PROP_ACTIVATES_DEFAULT,
  PROP_WIDTH_CHARS,
  PROP_SCROLL_OFFSET,
  PROP_TEXT,
  PROP_XALIGN,
  PROP_TRUNCATE_MULTILINE,
  PROP_SHADOW_TYPE,
  PROP_OVERWRITE_MODE,
  PROP_TEXT_LENGTH,
  PROP_INVISIBLE_CHAR_SET,
  PROP_CAPS_LOCK_WARNING,
  PROP_PROGRESS_FRACTION,
  PROP_PROGRESS_PULSE_STEP,
  PROP_PIXBUF_PRIMARY,
  PROP_PIXBUF_SECONDARY,
  PROP_STOCK_PRIMARY,
  PROP_STOCK_SECONDARY,
  PROP_ICON_NAME_PRIMARY,
  PROP_ICON_NAME_SECONDARY,
  PROP_GICON_PRIMARY,
  PROP_GICON_SECONDARY,
  PROP_STORAGE_TYPE_PRIMARY,
  PROP_STORAGE_TYPE_SECONDARY,
  PROP_ACTIVATABLE_PRIMARY,
  PROP_ACTIVATABLE_SECONDARY,
  PROP_SENSITIVE_PRIMARY,
  PROP_SENSITIVE_SECONDARY,
  PROP_TOOLTIP_TEXT_PRIMARY,
  PROP_TOOLTIP_TEXT_SECONDARY,
  PROP_TOOLTIP_MARKUP_PRIMARY,
  PROP_TOOLTIP_MARKUP_SECONDARY,
  PROP_IM_MODULE,
  PROP_EDITING_CANCELED
};

static guint signals[LAST_SIGNAL] = { 0 };

typedef enum {
  CURSOR_STANDARD,
  CURSOR_DND
} CursorType;

typedef enum
{
  DISPLAY_NORMAL,       /* The entry text is being shown */
  DISPLAY_INVISIBLE,    /* In invisible mode, text replaced by (eg) bullets */
  DISPLAY_BLANK         /* In invisible mode, nothing shown at all */
} DisplayMode;

/* GObject, GtkObject methods
 */
static void   gtk_entry_editable_init        (GtkEditableClass     *iface);
static void   gtk_entry_cell_editable_init   (GtkCellEditableIface *iface);
static void   gtk_entry_set_property         (GObject          *object,
                                              guint             prop_id,
                                              const GValue     *value,
                                              GParamSpec       *pspec);
static void   gtk_entry_get_property         (GObject          *object,
                                              guint             prop_id,
                                              GValue           *value,
                                              GParamSpec       *pspec);
static void   gtk_entry_finalize             (GObject          *object);
static void   gtk_entry_destroy              (GtkObject        *object);
static void   gtk_entry_dispose              (GObject          *object);

/* GtkWidget methods
 */
static void   gtk_entry_realize              (GtkWidget        *widget);
static void   gtk_entry_unrealize            (GtkWidget        *widget);
static void   gtk_entry_map                  (GtkWidget        *widget);
static void   gtk_entry_unmap                (GtkWidget        *widget);
static void   gtk_entry_size_request         (GtkWidget        *widget,
					      GtkRequisition   *requisition);
static void   gtk_entry_size_allocate        (GtkWidget        *widget,
					      GtkAllocation    *allocation);
static void   gtk_entry_draw_frame           (GtkWidget        *widget,
                                              GdkEventExpose   *event);
static void   gtk_entry_draw_progress        (GtkWidget        *widget,
                                              GdkEventExpose   *event);
static gint   gtk_entry_expose               (GtkWidget        *widget,
					      GdkEventExpose   *event);
static gint   gtk_entry_button_press         (GtkWidget        *widget,
					      GdkEventButton   *event);
static gint   gtk_entry_button_release       (GtkWidget        *widget,
					      GdkEventButton   *event);
static gint   gtk_entry_enter_notify         (GtkWidget        *widget,
                                              GdkEventCrossing *event);
static gint   gtk_entry_leave_notify         (GtkWidget        *widget,
                                              GdkEventCrossing *event);
static gint   gtk_entry_motion_notify        (GtkWidget        *widget,
					      GdkEventMotion   *event);
static gint   gtk_entry_key_press            (GtkWidget        *widget,
					      GdkEventKey      *event);
static gint   gtk_entry_key_release          (GtkWidget        *widget,
					      GdkEventKey      *event);
static gint   gtk_entry_focus_in             (GtkWidget        *widget,
					      GdkEventFocus    *event);
static gint   gtk_entry_focus_out            (GtkWidget        *widget,
					      GdkEventFocus    *event);
static void   gtk_entry_grab_focus           (GtkWidget        *widget);
static void   gtk_entry_style_set            (GtkWidget        *widget,
					      GtkStyle         *previous_style);
static gboolean gtk_entry_query_tooltip      (GtkWidget        *widget,
                                              gint              x,
                                              gint              y,
                                              gboolean          keyboard_tip,
                                              GtkTooltip       *tooltip);
static void   gtk_entry_direction_changed    (GtkWidget        *widget,
					      GtkTextDirection  previous_dir);
static void   gtk_entry_state_changed        (GtkWidget        *widget,
					      GtkStateType      previous_state);
static void   gtk_entry_screen_changed       (GtkWidget        *widget,
					      GdkScreen        *old_screen);

static gboolean gtk_entry_drag_drop          (GtkWidget        *widget,
                                              GdkDragContext   *context,
                                              gint              x,
                                              gint              y,
                                              guint             time);
static gboolean gtk_entry_drag_motion        (GtkWidget        *widget,
					      GdkDragContext   *context,
					      gint              x,
					      gint              y,
					      guint             time);
static void     gtk_entry_drag_leave         (GtkWidget        *widget,
					      GdkDragContext   *context,
					      guint             time);
static void     gtk_entry_drag_data_received (GtkWidget        *widget,
					      GdkDragContext   *context,
					      gint              x,
					      gint              y,
					      GtkSelectionData *selection_data,
					      guint             info,
					      guint             time);
static void     gtk_entry_drag_data_get      (GtkWidget        *widget,
					      GdkDragContext   *context,
					      GtkSelectionData *selection_data,
					      guint             info,
					      guint             time);
static void     gtk_entry_drag_data_delete   (GtkWidget        *widget,
					      GdkDragContext   *context);
static void     gtk_entry_drag_begin         (GtkWidget        *widget,
                                              GdkDragContext   *context);
static void     gtk_entry_drag_end           (GtkWidget        *widget,
                                              GdkDragContext   *context);


/* GtkEditable method implementations
 */
static void     gtk_entry_insert_text          (GtkEditable *editable,
						const gchar *new_text,
						gint         new_text_length,
						gint        *position);
static void     gtk_entry_delete_text          (GtkEditable *editable,
						gint         start_pos,
						gint         end_pos);
static gchar *  gtk_entry_get_chars            (GtkEditable *editable,
						gint         start_pos,
						gint         end_pos);
static void     gtk_entry_real_set_position    (GtkEditable *editable,
						gint         position);
static gint     gtk_entry_get_position         (GtkEditable *editable);
static void     gtk_entry_set_selection_bounds (GtkEditable *editable,
						gint         start,
						gint         end);
static gboolean gtk_entry_get_selection_bounds (GtkEditable *editable,
						gint        *start,
						gint        *end);

/* GtkCellEditable method implementations
 */
static void gtk_entry_start_editing (GtkCellEditable *cell_editable,
				     GdkEvent        *event);

/* Default signal handlers
 */
static void gtk_entry_real_insert_text   (GtkEditable     *editable,
					  const gchar     *new_text,
					  gint             new_text_length,
					  gint            *position);
static void gtk_entry_real_delete_text   (GtkEditable     *editable,
					  gint             start_pos,
					  gint             end_pos);
static void gtk_entry_move_cursor        (GtkEntry        *entry,
					  GtkMovementStep  step,
					  gint             count,
					  gboolean         extend_selection);
static void gtk_entry_insert_at_cursor   (GtkEntry        *entry,
					  const gchar     *str);
static void gtk_entry_delete_from_cursor (GtkEntry        *entry,
					  GtkDeleteType    type,
					  gint             count);
static void gtk_entry_backspace          (GtkEntry        *entry);
static void gtk_entry_cut_clipboard      (GtkEntry        *entry);
static void gtk_entry_copy_clipboard     (GtkEntry        *entry);
static void gtk_entry_paste_clipboard    (GtkEntry        *entry);
static void gtk_entry_toggle_overwrite   (GtkEntry        *entry);
static void gtk_entry_select_all         (GtkEntry        *entry);
static void gtk_entry_real_activate      (GtkEntry        *entry);
static gboolean gtk_entry_popup_menu     (GtkWidget       *widget);

static void keymap_direction_changed     (GdkKeymap       *keymap,
					  GtkEntry        *entry);
static void keymap_state_changed         (GdkKeymap       *keymap,
					  GtkEntry        *entry);
static void remove_capslock_feedback     (GtkEntry        *entry);

/* IM Context Callbacks
 */
static void     gtk_entry_commit_cb               (GtkIMContext *context,
						   const gchar  *str,
						   GtkEntry     *entry);
static void     gtk_entry_preedit_changed_cb      (GtkIMContext *context,
						   GtkEntry     *entry);
static gboolean gtk_entry_retrieve_surrounding_cb (GtkIMContext *context,
						   GtkEntry     *entry);
static gboolean gtk_entry_delete_surrounding_cb   (GtkIMContext *context,
						   gint          offset,
						   gint          n_chars,
						   GtkEntry     *entry);

/* Internal routines
 */
static void         gtk_entry_enter_text               (GtkEntry       *entry,
                                                        const gchar    *str);
static void         gtk_entry_set_positions            (GtkEntry       *entry,
							gint            current_pos,
							gint            selection_bound);
static void         gtk_entry_draw_text                (GtkEntry       *entry);
static void         gtk_entry_draw_cursor              (GtkEntry       *entry,
							CursorType      type);
static PangoLayout *gtk_entry_ensure_layout            (GtkEntry       *entry,
                                                        gboolean        include_preedit);
static void         gtk_entry_reset_layout             (GtkEntry       *entry);
static void         gtk_entry_queue_draw               (GtkEntry       *entry);
static void         gtk_entry_recompute                (GtkEntry       *entry);
static gint         gtk_entry_find_position            (GtkEntry       *entry,
							gint            x);
static void         gtk_entry_get_cursor_locations     (GtkEntry       *entry,
							CursorType      type,
							gint           *strong_x,
							gint           *weak_x);
static void         gtk_entry_adjust_scroll            (GtkEntry       *entry);
static gint         gtk_entry_move_visually            (GtkEntry       *editable,
							gint            start,
							gint            count);
static gint         gtk_entry_move_logically           (GtkEntry       *entry,
							gint            start,
							gint            count);
static gint         gtk_entry_move_forward_word        (GtkEntry       *entry,
							gint            start,
                                                        gboolean        allow_whitespace);
static gint         gtk_entry_move_backward_word       (GtkEntry       *entry,
							gint            start,
                                                        gboolean        allow_whitespace);
static void         gtk_entry_delete_whitespace        (GtkEntry       *entry);
static void         gtk_entry_select_word              (GtkEntry       *entry);
static void         gtk_entry_select_line              (GtkEntry       *entry);
static void         gtk_entry_paste                    (GtkEntry       *entry,
							GdkAtom         selection);
static void         gtk_entry_update_primary_selection (GtkEntry       *entry);
static void         gtk_entry_do_popup                 (GtkEntry       *entry,
							GdkEventButton *event);
static gboolean     gtk_entry_mnemonic_activate        (GtkWidget      *widget,
							gboolean        group_cycling);
static void         gtk_entry_check_cursor_blink       (GtkEntry       *entry);
static void         gtk_entry_pend_cursor_blink        (GtkEntry       *entry);
static void         gtk_entry_reset_blink_time         (GtkEntry       *entry);
static void         gtk_entry_get_text_area_size       (GtkEntry       *entry,
							gint           *x,
							gint           *y,
							gint           *width,
							gint           *height);
static void         get_text_area_size                 (GtkEntry       *entry,
							gint           *x,
							gint           *y,
							gint           *width,
							gint           *height);
static void         get_widget_window_size             (GtkEntry       *entry,
							gint           *x,
							gint           *y,
							gint           *width,
							gint           *height);
static void         gtk_entry_move_adjustments         (GtkEntry             *entry);
static void         gtk_entry_ensure_pixbuf            (GtkEntry             *entry,
                                                        GtkEntryIconPosition  icon_pos);


/* Completion */
static gint         gtk_entry_completion_timeout       (gpointer            data);
static gboolean     gtk_entry_completion_key_press     (GtkWidget          *widget,
							GdkEventKey        *event,
							gpointer            user_data);
static void         gtk_entry_completion_changed       (GtkWidget          *entry,
							gpointer            user_data);
static gboolean     check_completion_callback          (GtkEntryCompletion *completion);
static void         clear_completion_callback          (GtkEntry           *entry,
							GParamSpec         *pspec);
static gboolean     accept_completion_callback         (GtkEntry           *entry);
static void         completion_insert_text_callback    (GtkEntry           *entry,
							const gchar        *text,
							gint                length,
							gint                position,
							GtkEntryCompletion *completion);
static void         disconnect_completion_signals      (GtkEntry           *entry,
							GtkEntryCompletion *completion);
static void         connect_completion_signals         (GtkEntry           *entry,
							GtkEntryCompletion *completion);

static void         begin_change                       (GtkEntry *entry);
static void         end_change                         (GtkEntry *entry);
static void         emit_changed                       (GtkEntry *entry);


static void         buffer_inserted_text               (GtkEntryBuffer *buffer, 
                                                        guint           position,
                                                        const gchar    *chars,
                                                        guint           n_chars,
                                                        GtkEntry       *entry);
static void         buffer_deleted_text                (GtkEntryBuffer *buffer, 
                                                        guint           position,
                                                        guint           n_chars,
                                                        GtkEntry       *entry);
static void         buffer_notify_text                 (GtkEntryBuffer *buffer, 
                                                        GParamSpec     *spec,
                                                        GtkEntry       *entry);
static void         buffer_notify_length               (GtkEntryBuffer *buffer, 
                                                        GParamSpec     *spec,
                                                        GtkEntry       *entry);
static void         buffer_notify_max_length           (GtkEntryBuffer *buffer, 
                                                        GParamSpec     *spec,
                                                        GtkEntry       *entry);
static void         buffer_connect_signals             (GtkEntry       *entry);
static void         buffer_disconnect_signals          (GtkEntry       *entry);
static GtkEntryBuffer *get_buffer                      (GtkEntry       *entry);


G_DEFINE_TYPE_WITH_CODE (GtkEntry, gtk_entry, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_EDITABLE,
                                                gtk_entry_editable_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_EDITABLE,
                                                gtk_entry_cell_editable_init))

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
gtk_entry_class_init (GtkEntryClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class;
  GtkObjectClass *gtk_object_class;
  GtkBindingSet *binding_set;

  widget_class = (GtkWidgetClass*) class;
  gtk_object_class = (GtkObjectClass *)class;

  gobject_class->dispose = gtk_entry_dispose;
  gobject_class->finalize = gtk_entry_finalize;
  gobject_class->set_property = gtk_entry_set_property;
  gobject_class->get_property = gtk_entry_get_property;

  widget_class->map = gtk_entry_map;
  widget_class->unmap = gtk_entry_unmap;
  widget_class->realize = gtk_entry_realize;
  widget_class->unrealize = gtk_entry_unrealize;
  widget_class->size_request = gtk_entry_size_request;
  widget_class->size_allocate = gtk_entry_size_allocate;
  widget_class->expose_event = gtk_entry_expose;
  widget_class->enter_notify_event = gtk_entry_enter_notify;
  widget_class->leave_notify_event = gtk_entry_leave_notify;
  widget_class->button_press_event = gtk_entry_button_press;
  widget_class->button_release_event = gtk_entry_button_release;
  widget_class->motion_notify_event = gtk_entry_motion_notify;
  widget_class->key_press_event = gtk_entry_key_press;
  widget_class->key_release_event = gtk_entry_key_release;
  widget_class->focus_in_event = gtk_entry_focus_in;
  widget_class->focus_out_event = gtk_entry_focus_out;
  widget_class->grab_focus = gtk_entry_grab_focus;
  widget_class->style_set = gtk_entry_style_set;
  widget_class->query_tooltip = gtk_entry_query_tooltip;
  widget_class->drag_begin = gtk_entry_drag_begin;
  widget_class->drag_end = gtk_entry_drag_end;
  widget_class->direction_changed = gtk_entry_direction_changed;
  widget_class->state_changed = gtk_entry_state_changed;
  widget_class->screen_changed = gtk_entry_screen_changed;
  widget_class->mnemonic_activate = gtk_entry_mnemonic_activate;

  widget_class->drag_drop = gtk_entry_drag_drop;
  widget_class->drag_motion = gtk_entry_drag_motion;
  widget_class->drag_leave = gtk_entry_drag_leave;
  widget_class->drag_data_received = gtk_entry_drag_data_received;
  widget_class->drag_data_get = gtk_entry_drag_data_get;
  widget_class->drag_data_delete = gtk_entry_drag_data_delete;

  widget_class->popup_menu = gtk_entry_popup_menu;

  gtk_object_class->destroy = gtk_entry_destroy;

  class->move_cursor = gtk_entry_move_cursor;
  class->insert_at_cursor = gtk_entry_insert_at_cursor;
  class->delete_from_cursor = gtk_entry_delete_from_cursor;
  class->backspace = gtk_entry_backspace;
  class->cut_clipboard = gtk_entry_cut_clipboard;
  class->copy_clipboard = gtk_entry_copy_clipboard;
  class->paste_clipboard = gtk_entry_paste_clipboard;
  class->toggle_overwrite = gtk_entry_toggle_overwrite;
  class->activate = gtk_entry_real_activate;
  class->get_text_area_size = gtk_entry_get_text_area_size;
  
  quark_inner_border = g_quark_from_static_string ("gtk-entry-inner-border");
  quark_password_hint = g_quark_from_static_string ("gtk-entry-password-hint");
  quark_cursor_hadjustment = g_quark_from_static_string ("gtk-hadjustment");
  quark_capslock_feedback = g_quark_from_static_string ("gtk-entry-capslock-feedback");

  g_object_class_override_property (gobject_class,
                                    PROP_EDITING_CANCELED,
                                    "editing-canceled");

  g_object_class_install_property (gobject_class,
                                   PROP_BUFFER,
                                   g_param_spec_object ("buffer",
                                                        P_("Text Buffer"),
                                                        P_("Text buffer object which actually stores entry text"),
                                                        GTK_TYPE_ENTRY_BUFFER,
                                                        GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (gobject_class,
                                   PROP_CURSOR_POSITION,
                                   g_param_spec_int ("cursor-position",
                                                     P_("Cursor Position"),
                                                     P_("The current position of the insertion cursor in chars"),
                                                     0,
                                                     GTK_ENTRY_BUFFER_MAX_SIZE,
                                                     0,
                                                     GTK_PARAM_READABLE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_SELECTION_BOUND,
                                   g_param_spec_int ("selection-bound",
                                                     P_("Selection Bound"),
                                                     P_("The position of the opposite end of the selection from the cursor in chars"),
                                                     0,
                                                     GTK_ENTRY_BUFFER_MAX_SIZE,
                                                     0,
                                                     GTK_PARAM_READABLE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_EDITABLE,
                                   g_param_spec_boolean ("editable",
							 P_("Editable"),
							 P_("Whether the entry contents can be edited"),
                                                         TRUE,
							 GTK_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_MAX_LENGTH,
                                   g_param_spec_int ("max-length",
                                                     P_("Maximum length"),
                                                     P_("Maximum number of characters for this entry. Zero if no maximum"),
                                                     0,
                                                     GTK_ENTRY_BUFFER_MAX_SIZE,
                                                     0,
                                                     GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_VISIBILITY,
                                   g_param_spec_boolean ("visibility",
							 P_("Visibility"),
							 P_("FALSE displays the \"invisible char\" instead of the actual text (password mode)"),
                                                         TRUE,
							 GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_HAS_FRAME,
                                   g_param_spec_boolean ("has-frame",
							 P_("Has Frame"),
							 P_("FALSE removes outside bevel from entry"),
                                                         TRUE,
							 GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_INNER_BORDER,
                                   g_param_spec_boxed ("inner-border",
                                                       P_("Inner Border"),
                                                       P_("Border between text and frame. Overrides the inner-border style property"),
                                                       GTK_TYPE_BORDER,
                                                       GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_INVISIBLE_CHAR,
                                   g_param_spec_unichar ("invisible-char",
							 P_("Invisible character"),
							 P_("The character to use when masking entry contents (in \"password mode\")"),
							 '*',
							 GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_ACTIVATES_DEFAULT,
                                   g_param_spec_boolean ("activates-default",
							 P_("Activates default"),
							 P_("Whether to activate the default widget (such as the default button in a dialog) when Enter is pressed"),
                                                         FALSE,
							 GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_WIDTH_CHARS,
                                   g_param_spec_int ("width-chars",
                                                     P_("Width in chars"),
                                                     P_("Number of characters to leave space for in the entry"),
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_SCROLL_OFFSET,
                                   g_param_spec_int ("scroll-offset",
                                                     P_("Scroll offset"),
                                                     P_("Number of pixels of the entry scrolled off the screen to the left"),
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     GTK_PARAM_READABLE));

  g_object_class_install_property (gobject_class,
                                   PROP_TEXT,
                                   g_param_spec_string ("text",
							P_("Text"),
							P_("The contents of the entry"),
							"",
							GTK_PARAM_READWRITE));

  /**
   * GtkEntry:xalign:
   *
   * The horizontal alignment, from 0 (left) to 1 (right). 
   * Reversed for RTL layouts.
   * 
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_XALIGN,
                                   g_param_spec_float ("xalign",
						       P_("X align"),
						       P_("The horizontal alignment, from 0 (left) to 1 (right). Reversed for RTL layouts."),
						       0.0,
						       1.0,
						       0.0,
						       GTK_PARAM_READWRITE));

  /**
   * GtkEntry:truncate-multiline:
   *
   * When %TRUE, pasted multi-line text is truncated to the first line.
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
                                   PROP_TRUNCATE_MULTILINE,
                                   g_param_spec_boolean ("truncate-multiline",
                                                         P_("Truncate multiline"),
                                                         P_("Whether to truncate multiline pastes to one line."),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkEntry:shadow-type:
   *
   * Which kind of shadow to draw around the entry when 
   * #GtkEntry:has-frame is set to %TRUE.
   *
   * Since: 2.12
   */
  g_object_class_install_property (gobject_class,
                                   PROP_SHADOW_TYPE,
                                   g_param_spec_enum ("shadow-type",
                                                      P_("Shadow type"),
                                                      P_("Which kind of shadow to draw around the entry when has-frame is set"),
                                                      GTK_TYPE_SHADOW_TYPE,
                                                      GTK_SHADOW_IN,
                                                      GTK_PARAM_READWRITE));

  /**
   * GtkEntry:overwrite-mode:
   *
   * If text is overwritten when typing in the #GtkEntry.
   *
   * Since: 2.14
   */
  g_object_class_install_property (gobject_class,
                                   PROP_OVERWRITE_MODE,
                                   g_param_spec_boolean ("overwrite-mode",
                                                         P_("Overwrite mode"),
                                                         P_("Whether new text overwrites existing text"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkEntry:text-length:
   *
   * The length of the text in the #GtkEntry.
   *
   * Since: 2.14
   */
  g_object_class_install_property (gobject_class,
                                   PROP_TEXT_LENGTH,
                                   g_param_spec_uint ("text-length",
                                                      P_("Text length"),
                                                      P_("Length of the text currently in the entry"),
                                                      0, 
                                                      G_MAXUINT16,
                                                      0,
                                                      GTK_PARAM_READABLE));
  /**
   * GtkEntry:invisible-char-set:
   *
   * Whether the invisible char has been set for the #GtkEntry.
   *
   * Since: 2.16
   */
  g_object_class_install_property (gobject_class,
                                   PROP_INVISIBLE_CHAR_SET,
                                   g_param_spec_boolean ("invisible-char-set",
                                                         P_("Invisible char set"),
                                                         P_("Whether the invisible char has been set"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkEntry:caps-lock-warning
   *
   * Whether password entries will show a warning when Caps Lock is on.
   *
   * Note that the warning is shown using a secondary icon, and thus
   * does not work if you are using the secondary icon position for some 
   * other purpose.
   *
   * Since: 2.16
   */
  g_object_class_install_property (gobject_class,
                                   PROP_CAPS_LOCK_WARNING,
                                   g_param_spec_boolean ("caps-lock-warning",
                                                         P_("Caps Lock warning"),
                                                         P_("Whether password entries will show a warning when Caps Lock is on"),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkEntry:progress-fraction:
   *
   * The current fraction of the task that's been completed.
   *
   * Since: 2.16
   */
  g_object_class_install_property (gobject_class,
                                   PROP_PROGRESS_FRACTION,
                                   g_param_spec_double ("progress-fraction",
                                                        P_("Progress Fraction"),
                                                        P_("The current fraction of the task that's been completed"),
                                                        0.0,
                                                        1.0,
                                                        0.0,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkEntry:progress-pulse-step:
   *
   * The fraction of total entry width to move the progress
   * bouncing block for each call to gtk_entry_progress_pulse().
   *
   * Since: 2.16
   */
  g_object_class_install_property (gobject_class,
                                   PROP_PROGRESS_PULSE_STEP,
                                   g_param_spec_double ("progress-pulse-step",
                                                        P_("Progress Pulse Step"),
                                                        P_("The fraction of total entry width to move the progress bouncing block for each call to gtk_entry_progress_pulse()"),
                                                        0.0,
                                                        1.0,
                                                        0.1,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkEntry:primary-icon-pixbuf:
   *
   * A pixbuf to use as the primary icon for the entry.
   *
   * Since: 2.16
   */
  g_object_class_install_property (gobject_class,
                                   PROP_PIXBUF_PRIMARY,
                                   g_param_spec_object ("primary-icon-pixbuf",
                                                        P_("Primary pixbuf"),
                                                        P_("Primary pixbuf for the entry"),
                                                        GDK_TYPE_PIXBUF,
                                                        GTK_PARAM_READWRITE));
  
  /**
   * GtkEntry:secondary-icon-pixbuf:
   *
   * An pixbuf to use as the secondary icon for the entry.
   *
   * Since: 2.16
   */
  g_object_class_install_property (gobject_class,
                                   PROP_PIXBUF_SECONDARY,
                                   g_param_spec_object ("secondary-icon-pixbuf",
                                                        P_("Secondary pixbuf"),
                                                        P_("Secondary pixbuf for the entry"),
                                                        GDK_TYPE_PIXBUF,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkEntry:primary-icon-stock:
   *
   * The stock id to use for the primary icon for the entry.
   *
   * Since: 2.16
   */
  g_object_class_install_property (gobject_class,
                                   PROP_STOCK_PRIMARY,
                                   g_param_spec_string ("primary-icon-stock",
                                                        P_("Primary stock ID"),
                                                        P_("Stock ID for primary icon"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkEntry:secondary-icon-stock:
   *
   * The stock id to use for the secondary icon for the entry.
   *
   * Since: 2.16
   */
  g_object_class_install_property (gobject_class,
                                   PROP_STOCK_SECONDARY,
                                   g_param_spec_string ("secondary-icon-stock",
                                                        P_("Secondary stock ID"),
                                                        P_("Stock ID for secondary icon"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));
  
  /**
   * GtkEntry:primary-icon-name:
   *
   * The icon name to use for the primary icon for the entry.
   *
   * Since: 2.16
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ICON_NAME_PRIMARY,
                                   g_param_spec_string ("primary-icon-name",
                                                        P_("Primary icon name"),
                                                        P_("Icon name for primary icon"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));
  
  /**
   * GtkEntry:secondary-icon-name:
   *
   * The icon name to use for the secondary icon for the entry.
   *
   * Since: 2.16
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ICON_NAME_SECONDARY,
                                   g_param_spec_string ("secondary-icon-name",
                                                        P_("Secondary icon name"),
                                                        P_("Icon name for secondary icon"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));
  
  /**
   * GtkEntry:primary-icon-gicon:
   *
   * The #GIcon to use for the primary icon for the entry.
   *
   * Since: 2.16
   */
  g_object_class_install_property (gobject_class,
                                   PROP_GICON_PRIMARY,
                                   g_param_spec_object ("primary-icon-gicon",
                                                        P_("Primary GIcon"),
                                                        P_("GIcon for primary icon"),
                                                        G_TYPE_ICON,
                                                        GTK_PARAM_READWRITE));
  
  /**
   * GtkEntry:secondary-icon-gicon:
   *
   * The #GIcon to use for the secondary icon for the entry.
   *
   * Since: 2.16
   */
  g_object_class_install_property (gobject_class,
                                   PROP_GICON_SECONDARY,
                                   g_param_spec_object ("secondary-icon-gicon",
                                                        P_("Secondary GIcon"),
                                                        P_("GIcon for secondary icon"),
                                                        G_TYPE_ICON,
                                                        GTK_PARAM_READWRITE));
  
  /**
   * GtkEntry:primary-icon-storage-type:
   *
   * The representation which is used for the primary icon of the entry.
   *
   * Since: 2.16
   */
  g_object_class_install_property (gobject_class,
                                   PROP_STORAGE_TYPE_PRIMARY,
                                   g_param_spec_enum ("primary-icon-storage-type",
                                                      P_("Primary storage type"),
                                                      P_("The representation being used for primary icon"),
                                                      GTK_TYPE_IMAGE_TYPE,
                                                      GTK_IMAGE_EMPTY,
                                                      GTK_PARAM_READABLE));
  
  /**
   * GtkEntry:secondary-icon-storage-type:
   *
   * The representation which is used for the secondary icon of the entry.
   *
   * Since: 2.16
   */
  g_object_class_install_property (gobject_class,
                                   PROP_STORAGE_TYPE_SECONDARY,
                                   g_param_spec_enum ("secondary-icon-storage-type",
                                                      P_("Secondary storage type"),
                                                      P_("The representation being used for secondary icon"),
                                                      GTK_TYPE_IMAGE_TYPE,
                                                      GTK_IMAGE_EMPTY,
                                                      GTK_PARAM_READABLE));
  
  /**
   * GtkEntry:primary-icon-activatable:
   *
   * Whether the primary icon is activatable.
   *
   * GTK+ emits the #GtkEntry::icon-press and #GtkEntry::icon-release 
   * signals only on sensitive, activatable icons. 
   *
   * Sensitive, but non-activatable icons can be used for purely 
   * informational purposes.
   *
   * Since: 2.16
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ACTIVATABLE_PRIMARY,
                                   g_param_spec_boolean ("primary-icon-activatable",
                                                         P_("Primary icon activatable"),
                                                         P_("Whether the primary icon is activatable"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));
  
  /**
   * GtkEntry:secondary-icon-activatable:
   *
   * Whether the secondary icon is activatable.
   *
   * GTK+ emits the #GtkEntry::icon-press and #GtkEntry::icon-release 
   * signals only on sensitive, activatable icons.
   *
   * Sensitive, but non-activatable icons can be used for purely 
   * informational purposes.
   *
   * Since: 2.16
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ACTIVATABLE_SECONDARY,
                                   g_param_spec_boolean ("secondary-icon-activatable",
                                                         P_("Secondary icon activatable"),
                                                         P_("Whether the secondary icon is activatable"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));
  
  
  /**
   * GtkEntry:primary-icon-sensitive:
   *
   * Whether the primary icon is sensitive.
   *
   * An insensitive icon appears grayed out. GTK+ does not emit the 
   * #GtkEntry::icon-press and #GtkEntry::icon-release signals and 
   * does not allow DND from insensitive icons.
   *
   * An icon should be set insensitive if the action that would trigger
   * when clicked is currently not available.
   * 
   * Since: 2.16
   */
  g_object_class_install_property (gobject_class,
                                   PROP_SENSITIVE_PRIMARY,
                                   g_param_spec_boolean ("primary-icon-sensitive",
                                                         P_("Primary icon sensitive"),
                                                         P_("Whether the primary icon is sensitive"),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE));
  
  /**
   * GtkEntry:secondary-icon-sensitive:
   *
   * Whether the secondary icon is sensitive.
   *
   * An insensitive icon appears grayed out. GTK+ does not emit the 
   * #GtkEntry::icon-press and #GtkEntry::icon-release signals and 
   * does not allow DND from insensitive icons.
   *
   * An icon should be set insensitive if the action that would trigger
   * when clicked is currently not available.
   *
   * Since: 2.16
   */
  g_object_class_install_property (gobject_class,
                                   PROP_SENSITIVE_SECONDARY,
                                   g_param_spec_boolean ("secondary-icon-sensitive",
                                                         P_("Secondary icon sensitive"),
                                                         P_("Whether the secondary icon is sensitive"),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE));
  
  /**
   * GtkEntry:primary-icon-tooltip-text:
   * 
   * The contents of the tooltip on the primary icon.
   *
   * Also see gtk_entry_set_icon_tooltip_text().
   *
   * Since: 2.16
   */
  g_object_class_install_property (gobject_class,
                                   PROP_TOOLTIP_TEXT_PRIMARY,
                                   g_param_spec_string ("primary-icon-tooltip-text",
                                                        P_("Primary icon tooltip text"),
                                                        P_("The contents of the tooltip on the primary icon"),                              
                                                        NULL,
                                                        GTK_PARAM_READWRITE));
  
  /**
   * GtkEntry:secondary-icon-tooltip-text:
   * 
   * The contents of the tooltip on the secondary icon.
   *
   * Also see gtk_entry_set_icon_tooltip_text().
   *
   * Since: 2.16
   */
  g_object_class_install_property (gobject_class,
                                   PROP_TOOLTIP_TEXT_SECONDARY,
                                   g_param_spec_string ("secondary-icon-tooltip-text",
                                                        P_("Secondary icon tooltip text"),
                                                        P_("The contents of the tooltip on the secondary icon"),                              
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkEntry:primary-icon-tooltip-markup:
   * 
   * The contents of the tooltip on the primary icon, which is marked up
   * with the <link linkend="PangoMarkupFormat">Pango text markup 
   * language</link>.
   *
   * Also see gtk_entry_set_icon_tooltip_markup().
   *
   * Since: 2.16
   */
  g_object_class_install_property (gobject_class,
                                   PROP_TOOLTIP_MARKUP_PRIMARY,
                                   g_param_spec_string ("primary-icon-tooltip-markup",
                                                        P_("Primary icon tooltip markup"),
                                                        P_("The contents of the tooltip on the primary icon"),                              
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkEntry:secondary-icon-tooltip-markup:
   * 
   * The contents of the tooltip on the secondary icon, which is marked up
   * with the <link linkend="PangoMarkupFormat">Pango text markup 
   * language</link>.
   *
   * Also see gtk_entry_set_icon_tooltip_markup().
   *
   * Since: 2.16
   */
  g_object_class_install_property (gobject_class,
                                   PROP_TOOLTIP_MARKUP_SECONDARY,
                                   g_param_spec_string ("secondary-icon-tooltip-markup",
                                                        P_("Secondary icon tooltip markup"),
                                                        P_("The contents of the tooltip on the secondary icon"),                              
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkEntry:im-module:
   *
   * Which IM (input method) module should be used for this entry. 
   * See #GtkIMContext.
   * 
   * Setting this to a non-%NULL value overrides the
   * system-wide IM module setting. See the GtkSettings 
   * #GtkSettings:gtk-im-module property.
   *
   * Since: 2.16
   */  
  g_object_class_install_property (gobject_class,
                                   PROP_IM_MODULE,
                                   g_param_spec_string ("im-module",
                                                        P_("IM module"),
                                                        P_("Which IM module should be used"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkEntry:icon-prelight:
   *
   * The prelight style property determines whether activatable
   * icons prelight on mouseover.
   *
   * Since: 2.16
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_boolean ("icon-prelight",
                                                                 P_("Icon Prelight"),
                                                                 P_("Whether activatable icons should prelight when hovered"),
                                                                 TRUE,
                                                                 GTK_PARAM_READABLE));

  /**
   * GtkEntry:progress-border:
   *
   * The border around the progress bar in the entry.
   *
   * Since: 2.16
   */
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_boxed ("progress-border",
                                                               P_("Progress Border"),
                                                               P_("Border around the progress bar"),
                                                               GTK_TYPE_BORDER,
                                                               GTK_PARAM_READABLE));
  
  /**
   * GtkEntry:invisible-char:
   *
   * The invisible character is used when masking entry contents (in
   * \"password mode\")"). When it is not explicitly set with the
   * #GtkEntry::invisible-char property, GTK+ determines the character
   * to use from a list of possible candidates, depending on availability
   * in the current font.
   *
   * This style property allows the theme to prepend a character
   * to the list of candidates.
   *
   * Since: 2.18
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_unichar ("invisible-char",
					    		         P_("Invisible character"),
							         P_("The character to use when masking entry contents (in \"password mode\")"),
							         0,
							         GTK_PARAM_READABLE));
  
  /**
   * GtkEntry::populate-popup:
   * @entry: The entry on which the signal is emitted
   * @menu: the menu that is being populated
   *
   * The ::populate-popup signal gets emitted before showing the 
   * context menu of the entry. 
   *
   * If you need to add items to the context menu, connect
   * to this signal and append your menuitems to the @menu.
   */
  signals[POPULATE_POPUP] =
    g_signal_new (I_("populate-popup"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkEntryClass, populate_popup),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_MENU);
  
 /* Action signals */
  
  /**
   * GtkEntry::activate:
   * @entry: The entry on which the signal is emitted
   *
   * The ::activate signal is emitted when the the user hits
   * the Enter key.
   *
   * While this signal is used as a <link linkend="keybinding-signals">keybinding signal</link>,
   * it is also commonly used by applications to intercept
   * activation of entries.
   *
   * The default bindings for this signal are all forms of the Enter key.
   */
  signals[ACTIVATE] =
    g_signal_new (I_("activate"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkEntryClass, activate),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  widget_class->activate_signal = signals[ACTIVATE];

  /**
   * GtkEntry::move-cursor:
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
		  G_STRUCT_OFFSET (GtkEntryClass, move_cursor),
		  NULL, NULL,
		  _gtk_marshal_VOID__ENUM_INT_BOOLEAN,
		  G_TYPE_NONE, 3,
		  GTK_TYPE_MOVEMENT_STEP,
		  G_TYPE_INT,
		  G_TYPE_BOOLEAN);

  /**
   * GtkEntry::insert-at-cursor:
   * @entry: the object which received the signal
   * @string: the string to insert
   *
   * The ::insert-at-cursor signal is a
   * <link linkend="keybinding-signals">keybinding signal</link>
   * which gets emitted when the user initiates the insertion of a
   * fixed string at the cursor.
   *
   * This signal has no default bindings.
   */
  signals[INSERT_AT_CURSOR] = 
    g_signal_new (I_("insert-at-cursor"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkEntryClass, insert_at_cursor),
		  NULL, NULL,
		  _gtk_marshal_VOID__STRING,
		  G_TYPE_NONE, 1,
		  G_TYPE_STRING);

  /**
   * GtkEntry::delete-from-cursor:
   * @entry: the object which received the signal
   * @type: the granularity of the deletion, as a #GtkDeleteType
   * @count: the number of @type units to delete
   *
   * The ::delete-from-cursor signal is a
   * <link linkend="keybinding-signals">keybinding signal</link>
   * which gets emitted when the user initiates a text deletion.
   *
   * If the @type is %GTK_DELETE_CHARS, GTK+ deletes the selection
   * if there is one, otherwise it deletes the requested number
   * of characters.
   *
   * The default bindings for this signal are
   * Delete for deleting a character and Ctrl-Delete for
   * deleting a word.
   */
  signals[DELETE_FROM_CURSOR] = 
    g_signal_new (I_("delete-from-cursor"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkEntryClass, delete_from_cursor),
		  NULL, NULL,
		  _gtk_marshal_VOID__ENUM_INT,
		  G_TYPE_NONE, 2,
		  GTK_TYPE_DELETE_TYPE,
		  G_TYPE_INT);

  /**
   * GtkEntry::backspace:
   * @entry: the object which received the signal
   *
   * The ::backspace signal is a
   * <link linkend="keybinding-signals">keybinding signal</link>
   * which gets emitted when the user asks for it.
   *
   * The default bindings for this signal are
   * Backspace and Shift-Backspace.
   */
  signals[BACKSPACE] =
    g_signal_new (I_("backspace"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkEntryClass, backspace),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkEntry::cut-clipboard:
   * @entry: the object which received the signal
   *
   * The ::cut-clipboard signal is a
   * <link linkend="keybinding-signals">keybinding signal</link>
   * which gets emitted to cut the selection to the clipboard.
   *
   * The default bindings for this signal are
   * Ctrl-x and Shift-Delete.
   */
  signals[CUT_CLIPBOARD] =
    g_signal_new (I_("cut-clipboard"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkEntryClass, cut_clipboard),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkEntry::copy-clipboard:
   * @entry: the object which received the signal
   *
   * The ::copy-clipboard signal is a
   * <link linkend="keybinding-signals">keybinding signal</link>
   * which gets emitted to copy the selection to the clipboard.
   *
   * The default bindings for this signal are
   * Ctrl-c and Ctrl-Insert.
   */
  signals[COPY_CLIPBOARD] =
    g_signal_new (I_("copy-clipboard"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkEntryClass, copy_clipboard),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkEntry::paste-clipboard:
   * @entry: the object which received the signal
   *
   * The ::paste-clipboard signal is a
   * <link linkend="keybinding-signals">keybinding signal</link>
   * which gets emitted to paste the contents of the clipboard
   * into the text view.
   *
   * The default bindings for this signal are
   * Ctrl-v and Shift-Insert.
   */
  signals[PASTE_CLIPBOARD] =
    g_signal_new (I_("paste-clipboard"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkEntryClass, paste_clipboard),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkEntry::toggle-overwrite:
   * @entry: the object which received the signal
   *
   * The ::toggle-overwrite signal is a
   * <link linkend="keybinding-signals">keybinding signal</link>
   * which gets emitted to toggle the overwrite mode of the entry.
   *
   * The default bindings for this signal is Insert.
   */
  signals[TOGGLE_OVERWRITE] =
    g_signal_new (I_("toggle-overwrite"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkEntryClass, toggle_overwrite),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkEntry::icon-press:
   * @entry: The entry on which the signal is emitted
   * @icon_pos: The position of the clicked icon
   * @event: the button press event
   *
   * The ::icon-press signal is emitted when an activatable icon
   * is clicked.
   *
   * Since: 2.16
   */
  signals[ICON_PRESS] =
    g_signal_new (I_("icon-press"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  _gtk_marshal_VOID__ENUM_BOXED,
                  G_TYPE_NONE, 2,
                  GTK_TYPE_ENTRY_ICON_POSITION,
                  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
  
  /**
   * GtkEntry::icon-release:
   * @entry: The entry on which the signal is emitted
   * @icon_pos: The position of the clicked icon
   * @event: the button release event
   *
   * The ::icon-release signal is emitted on the button release from a
   * mouse click over an activatable icon.
   *
   * Since: 2.16
   */
  signals[ICON_RELEASE] =
    g_signal_new (I_("icon-release"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  _gtk_marshal_VOID__ENUM_BOXED,
                  G_TYPE_NONE, 2,
                  GTK_TYPE_ENTRY_ICON_POSITION,
                  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GtkEntry::preedit-changed:
   * @entry: the object which received the signal
   * @preedit: the current preedit string
   *
   * If an input method is used, the typed text will not immediately
   * be committed to the buffer. So if you are interested in the text,
   * connect to this signal.
   *
   * Since: 2.20
   */
  signals[PREEDIT_CHANGED] =
    g_signal_new_class_handler (I_("preedit-changed"),
                                G_OBJECT_CLASS_TYPE (gobject_class),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                NULL,
                                NULL, NULL,
                                _gtk_marshal_VOID__STRING,
                                G_TYPE_NONE, 1,
                                G_TYPE_STRING);


  /*
   * Key bindings
   */
#ifdef __APPLE__
#define OS_CTRL (GDK_MOD2_MASK|GDK_META_MASK)
#else
#define OS_CTRL GDK_CONTROL_MASK
#endif

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
  
  add_move_binding (binding_set, GDK_Right, OS_CTRL,
		    GTK_MOVEMENT_WORDS, 1);

  add_move_binding (binding_set, GDK_Left, OS_CTRL,
		    GTK_MOVEMENT_WORDS, -1);

  add_move_binding (binding_set, GDK_KP_Right, OS_CTRL,
		    GTK_MOVEMENT_WORDS, 1);

  add_move_binding (binding_set, GDK_KP_Left, OS_CTRL,
		    GTK_MOVEMENT_WORDS, -1);
  
  add_move_binding (binding_set, GDK_Home, 0,
		    GTK_MOVEMENT_DISPLAY_LINE_ENDS, -1);

  add_move_binding (binding_set, GDK_End, 0,
		    GTK_MOVEMENT_DISPLAY_LINE_ENDS, 1);

  add_move_binding (binding_set, GDK_KP_Home, 0,
		    GTK_MOVEMENT_DISPLAY_LINE_ENDS, -1);

  add_move_binding (binding_set, GDK_KP_End, 0,
		    GTK_MOVEMENT_DISPLAY_LINE_ENDS, 1);
  
  add_move_binding (binding_set, GDK_Home, OS_CTRL,
		    GTK_MOVEMENT_BUFFER_ENDS, -1);

  add_move_binding (binding_set, GDK_End, OS_CTRL,
		    GTK_MOVEMENT_BUFFER_ENDS, 1);

  add_move_binding (binding_set, GDK_KP_Home, OS_CTRL,
		    GTK_MOVEMENT_BUFFER_ENDS, -1);

  add_move_binding (binding_set, GDK_KP_End, OS_CTRL,
		    GTK_MOVEMENT_BUFFER_ENDS, 1);

  /* Select all
   */
  gtk_binding_entry_add_signal (binding_set, GDK_a, OS_CTRL,
                                "move-cursor", 3,
                                GTK_TYPE_MOVEMENT_STEP, GTK_MOVEMENT_BUFFER_ENDS,
                                G_TYPE_INT, -1,
				G_TYPE_BOOLEAN, FALSE);
  gtk_binding_entry_add_signal (binding_set, GDK_a, OS_CTRL,
                                "move-cursor", 3,
                                GTK_TYPE_MOVEMENT_STEP, GTK_MOVEMENT_BUFFER_ENDS,
                                G_TYPE_INT, 1,
				G_TYPE_BOOLEAN, TRUE);  

  gtk_binding_entry_add_signal (binding_set, GDK_slash, OS_CTRL,
                                "move-cursor", 3,
                                GTK_TYPE_MOVEMENT_STEP, GTK_MOVEMENT_BUFFER_ENDS,
                                G_TYPE_INT, -1,
				G_TYPE_BOOLEAN, FALSE);
  gtk_binding_entry_add_signal (binding_set, GDK_slash, OS_CTRL,
                                "move-cursor", 3,
                                GTK_TYPE_MOVEMENT_STEP, GTK_MOVEMENT_BUFFER_ENDS,
                                G_TYPE_INT, 1,
				G_TYPE_BOOLEAN, TRUE);  
  /* Unselect all 
   */
  gtk_binding_entry_add_signal (binding_set, GDK_backslash, OS_CTRL,
                                "move-cursor", 3,
                                GTK_TYPE_MOVEMENT_STEP, GTK_MOVEMENT_VISUAL_POSITIONS,
                                G_TYPE_INT, 0,
				G_TYPE_BOOLEAN, FALSE);
  gtk_binding_entry_add_signal (binding_set, GDK_a, GDK_SHIFT_MASK | OS_CTRL,
                                "move-cursor", 3,
                                GTK_TYPE_MOVEMENT_STEP, GTK_MOVEMENT_VISUAL_POSITIONS,
                                G_TYPE_INT, 0,
				G_TYPE_BOOLEAN, FALSE);

  /* Activate
   */
  gtk_binding_entry_add_signal (binding_set, GDK_Return, 0,
				"activate", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_ISO_Enter, 0,
				"activate", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Enter, 0,
				"activate", 0);
  
  /* Deleting text */
  gtk_binding_entry_add_signal (binding_set, GDK_Delete, 0,
				"delete-from-cursor", 2,
				G_TYPE_ENUM, GTK_DELETE_CHARS,
				G_TYPE_INT, 1);

  gtk_binding_entry_add_signal (binding_set, GDK_KP_Delete, 0,
				"delete-from-cursor", 2,
				G_TYPE_ENUM, GTK_DELETE_CHARS,
				G_TYPE_INT, 1);
  
  gtk_binding_entry_add_signal (binding_set, GDK_BackSpace, 0,
				"backspace", 0);

  /* Make this do the same as Backspace, to help with mis-typing */
  gtk_binding_entry_add_signal (binding_set, GDK_BackSpace, GDK_SHIFT_MASK,
				"backspace", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_Delete, OS_CTRL,
				"delete-from-cursor", 2,
				G_TYPE_ENUM, GTK_DELETE_WORD_ENDS,
				G_TYPE_INT, 1);

  gtk_binding_entry_add_signal (binding_set, GDK_KP_Delete, OS_CTRL,
				"delete-from-cursor", 2,
				G_TYPE_ENUM, GTK_DELETE_WORD_ENDS,
				G_TYPE_INT, 1);
  
  gtk_binding_entry_add_signal (binding_set, GDK_BackSpace, OS_CTRL,
				"delete-from-cursor", 2,
				G_TYPE_ENUM, GTK_DELETE_WORD_ENDS,
				G_TYPE_INT, -1);

  /* Cut/copy/paste */

  gtk_binding_entry_add_signal (binding_set, GDK_x, OS_CTRL,
				"cut-clipboard", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_c, OS_CTRL,
				"copy-clipboard", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_v, OS_CTRL,
				"paste-clipboard", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_Delete, GDK_SHIFT_MASK,
				"cut-clipboard", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_Insert, OS_CTRL,
				"copy-clipboard", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_Insert, GDK_SHIFT_MASK,
				"paste-clipboard", 0);

  /* Overwrite */
  gtk_binding_entry_add_signal (binding_set, GDK_Insert, 0,
				"toggle-overwrite", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Insert, 0,
				"toggle-overwrite", 0);

  /**
   * GtkEntry:inner-border:
   *
   * Sets the text area's border between the text and the frame.
   *
   * Since: 2.10
   */
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_boxed ("inner-border",
                                                               P_("Inner Border"),
                                                               P_("Border between text and frame."),
                                                               GTK_TYPE_BORDER,
                                                               GTK_PARAM_READABLE));

   /**
    * GtkEntry:state-hint:
    *
    * Indicates whether to pass a proper widget state when
    * drawing the shadow and the widget background.
    *
    * Since: 2.16
    *
    * Deprecated: 2.22: This style property will be removed in GTK+ 3
    */
   gtk_widget_class_install_style_property (widget_class,
                                            g_param_spec_boolean ("state-hint",
                                                                  P_("State Hint"),
                                                                  P_("Whether to pass a proper state when drawing shadow or background"),
                                                                  FALSE,
                                                                  GTK_PARAM_READABLE));

  g_type_class_add_private (gobject_class, sizeof (GtkEntryPrivate));
}

static void
gtk_entry_editable_init (GtkEditableClass *iface)
{
  iface->do_insert_text = gtk_entry_insert_text;
  iface->do_delete_text = gtk_entry_delete_text;
  iface->insert_text = gtk_entry_real_insert_text;
  iface->delete_text = gtk_entry_real_delete_text;
  iface->get_chars = gtk_entry_get_chars;
  iface->set_selection_bounds = gtk_entry_set_selection_bounds;
  iface->get_selection_bounds = gtk_entry_get_selection_bounds;
  iface->set_position = gtk_entry_real_set_position;
  iface->get_position = gtk_entry_get_position;
}

static void
gtk_entry_cell_editable_init (GtkCellEditableIface *iface)
{
  iface->start_editing = gtk_entry_start_editing;
}

static void
gtk_entry_set_property (GObject         *object,
                        guint            prop_id,
                        const GValue    *value,
                        GParamSpec      *pspec)
{
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (object);
  GtkEntry *entry = GTK_ENTRY (object);
  GtkWidget *widget;

  switch (prop_id)
    {
    case PROP_BUFFER:
      gtk_entry_set_buffer (entry, g_value_get_object (value));
      break;

    case PROP_EDITABLE:
      {
        gboolean new_value = g_value_get_boolean (value);

      	if (new_value != entry->editable)
	  {
            widget = GTK_WIDGET (entry);
	    if (!new_value)
	      {
		_gtk_entry_reset_im_context (entry);
		if (gtk_widget_has_focus (widget))
		  gtk_im_context_focus_out (entry->im_context);

		entry->preedit_length = 0;
		entry->preedit_cursor = 0;
	      }

	    entry->editable = new_value;

	    if (new_value && gtk_widget_has_focus (widget))
	      gtk_im_context_focus_in (entry->im_context);
	    
	    gtk_entry_queue_draw (entry);
	  }
      }
      break;

    case PROP_MAX_LENGTH:
      gtk_entry_set_max_length (entry, g_value_get_int (value));
      break;
      
    case PROP_VISIBILITY:
      gtk_entry_set_visibility (entry, g_value_get_boolean (value));
      break;

    case PROP_HAS_FRAME:
      gtk_entry_set_has_frame (entry, g_value_get_boolean (value));
      break;

    case PROP_INNER_BORDER:
      gtk_entry_set_inner_border (entry, g_value_get_boxed (value));
      break;

    case PROP_INVISIBLE_CHAR:
      gtk_entry_set_invisible_char (entry, g_value_get_uint (value));
      break;

    case PROP_ACTIVATES_DEFAULT:
      gtk_entry_set_activates_default (entry, g_value_get_boolean (value));
      break;

    case PROP_WIDTH_CHARS:
      gtk_entry_set_width_chars (entry, g_value_get_int (value));
      break;

    case PROP_TEXT:
      gtk_entry_set_text (entry, g_value_get_string (value));
      break;

    case PROP_XALIGN:
      gtk_entry_set_alignment (entry, g_value_get_float (value));
      break;

    case PROP_TRUNCATE_MULTILINE:
      entry->truncate_multiline = g_value_get_boolean (value);
      break;

    case PROP_SHADOW_TYPE:
      priv->shadow_type = g_value_get_enum (value);
      break;

    case PROP_OVERWRITE_MODE:
      gtk_entry_set_overwrite_mode (entry, g_value_get_boolean (value));
      break;

    case PROP_INVISIBLE_CHAR_SET:
      if (g_value_get_boolean (value))
        priv->invisible_char_set = TRUE;
      else
        gtk_entry_unset_invisible_char (entry);
      break;

    case PROP_CAPS_LOCK_WARNING:
      priv->caps_lock_warning = g_value_get_boolean (value);
      break;

    case PROP_PROGRESS_FRACTION:
      gtk_entry_set_progress_fraction (entry, g_value_get_double (value));
      break;

    case PROP_PROGRESS_PULSE_STEP:
      gtk_entry_set_progress_pulse_step (entry, g_value_get_double (value));
      break;

    case PROP_PIXBUF_PRIMARY:
      gtk_entry_set_icon_from_pixbuf (entry,
                                      GTK_ENTRY_ICON_PRIMARY,
                                      g_value_get_object (value));
      break;

    case PROP_PIXBUF_SECONDARY:
      gtk_entry_set_icon_from_pixbuf (entry,
                                      GTK_ENTRY_ICON_SECONDARY,
                                      g_value_get_object (value));
      break;

    case PROP_STOCK_PRIMARY:
      gtk_entry_set_icon_from_stock (entry,
                                     GTK_ENTRY_ICON_PRIMARY,
                                     g_value_get_string (value));
      break;

    case PROP_STOCK_SECONDARY:
      gtk_entry_set_icon_from_stock (entry,
                                     GTK_ENTRY_ICON_SECONDARY,
                                     g_value_get_string (value));
      break;

    case PROP_ICON_NAME_PRIMARY:
      gtk_entry_set_icon_from_icon_name (entry,
                                         GTK_ENTRY_ICON_PRIMARY,
                                         g_value_get_string (value));
      break;

    case PROP_ICON_NAME_SECONDARY:
      gtk_entry_set_icon_from_icon_name (entry,
                                         GTK_ENTRY_ICON_SECONDARY,
                                         g_value_get_string (value));
      break;

    case PROP_GICON_PRIMARY:
      gtk_entry_set_icon_from_gicon (entry,
                                     GTK_ENTRY_ICON_PRIMARY,
                                     g_value_get_object (value));
      break;

    case PROP_GICON_SECONDARY:
      gtk_entry_set_icon_from_gicon (entry,
                                     GTK_ENTRY_ICON_SECONDARY,
                                     g_value_get_object (value));
      break;

    case PROP_ACTIVATABLE_PRIMARY:
      gtk_entry_set_icon_activatable (entry,
                                      GTK_ENTRY_ICON_PRIMARY,
                                      g_value_get_boolean (value));
      break;

    case PROP_ACTIVATABLE_SECONDARY:
      gtk_entry_set_icon_activatable (entry,
                                      GTK_ENTRY_ICON_SECONDARY,
                                      g_value_get_boolean (value));
      break;

    case PROP_SENSITIVE_PRIMARY:
      gtk_entry_set_icon_sensitive (entry,
                                    GTK_ENTRY_ICON_PRIMARY,
                                    g_value_get_boolean (value));
      break;

    case PROP_SENSITIVE_SECONDARY:
      gtk_entry_set_icon_sensitive (entry,
                                    GTK_ENTRY_ICON_SECONDARY,
                                    g_value_get_boolean (value));
      break;

    case PROP_TOOLTIP_TEXT_PRIMARY:
      gtk_entry_set_icon_tooltip_text (entry,
                                       GTK_ENTRY_ICON_PRIMARY,
                                       g_value_get_string (value));
      break;

    case PROP_TOOLTIP_TEXT_SECONDARY:
      gtk_entry_set_icon_tooltip_text (entry,
                                       GTK_ENTRY_ICON_SECONDARY,
                                       g_value_get_string (value));
      break;

    case PROP_TOOLTIP_MARKUP_PRIMARY:
      gtk_entry_set_icon_tooltip_markup (entry,
                                         GTK_ENTRY_ICON_PRIMARY,
                                         g_value_get_string (value));
      break;

    case PROP_TOOLTIP_MARKUP_SECONDARY:
      gtk_entry_set_icon_tooltip_markup (entry,
                                         GTK_ENTRY_ICON_SECONDARY,
                                         g_value_get_string (value));
      break;

    case PROP_IM_MODULE:
      g_free (priv->im_module);
      priv->im_module = g_value_dup_string (value);
      if (GTK_IS_IM_MULTICONTEXT (entry->im_context))
        gtk_im_multicontext_set_context_id (GTK_IM_MULTICONTEXT (entry->im_context), priv->im_module);
      break;

    case PROP_EDITING_CANCELED:
      entry->editing_canceled = g_value_get_boolean (value);
      break;

    case PROP_SCROLL_OFFSET:
    case PROP_CURSOR_POSITION:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_entry_get_property (GObject         *object,
                        guint            prop_id,
                        GValue          *value,
                        GParamSpec      *pspec)
{
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (object);
  GtkEntry *entry = GTK_ENTRY (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      g_value_set_object (value, gtk_entry_get_buffer (entry));
      break;

    case PROP_CURSOR_POSITION:
      g_value_set_int (value, entry->current_pos);
      break;

    case PROP_SELECTION_BOUND:
      g_value_set_int (value, entry->selection_bound);
      break;

    case PROP_EDITABLE:
      g_value_set_boolean (value, entry->editable);
      break;

    case PROP_MAX_LENGTH:
      g_value_set_int (value, gtk_entry_buffer_get_max_length (get_buffer (entry)));
      break;

    case PROP_VISIBILITY:
      g_value_set_boolean (value, entry->visible);
      break;

    case PROP_HAS_FRAME:
      g_value_set_boolean (value, entry->has_frame);
      break;

    case PROP_INNER_BORDER:
      g_value_set_boxed (value, gtk_entry_get_inner_border (entry));
      break;

    case PROP_INVISIBLE_CHAR:
      g_value_set_uint (value, entry->invisible_char);
      break;

    case PROP_ACTIVATES_DEFAULT:
      g_value_set_boolean (value, entry->activates_default);
      break;

    case PROP_WIDTH_CHARS:
      g_value_set_int (value, entry->width_chars);
      break;

    case PROP_SCROLL_OFFSET:
      g_value_set_int (value, entry->scroll_offset);
      break;

    case PROP_TEXT:
      g_value_set_string (value, gtk_entry_get_text (entry));
      break;

    case PROP_XALIGN:
      g_value_set_float (value, gtk_entry_get_alignment (entry));
      break;

    case PROP_TRUNCATE_MULTILINE:
      g_value_set_boolean (value, entry->truncate_multiline);
      break;

    case PROP_SHADOW_TYPE:
      g_value_set_enum (value, priv->shadow_type);
      break;

    case PROP_OVERWRITE_MODE:
      g_value_set_boolean (value, entry->overwrite_mode);
      break;

    case PROP_TEXT_LENGTH:
      g_value_set_uint (value, gtk_entry_buffer_get_length (get_buffer (entry)));
      break;

    case PROP_INVISIBLE_CHAR_SET:
      g_value_set_boolean (value, priv->invisible_char_set);
      break;

    case PROP_IM_MODULE:
      g_value_set_string (value, priv->im_module);
      break;

    case PROP_CAPS_LOCK_WARNING:
      g_value_set_boolean (value, priv->caps_lock_warning);
      break;

    case PROP_PROGRESS_FRACTION:
      g_value_set_double (value, priv->progress_fraction);
      break;

    case PROP_PROGRESS_PULSE_STEP:
      g_value_set_double (value, priv->progress_pulse_fraction);
      break;

    case PROP_PIXBUF_PRIMARY:
      g_value_set_object (value,
                          gtk_entry_get_icon_pixbuf (entry,
                                                     GTK_ENTRY_ICON_PRIMARY));
      break;

    case PROP_PIXBUF_SECONDARY:
      g_value_set_object (value,
                          gtk_entry_get_icon_pixbuf (entry,
                                                     GTK_ENTRY_ICON_SECONDARY));
      break;

    case PROP_STOCK_PRIMARY:
      g_value_set_string (value,
                          gtk_entry_get_icon_stock (entry,
                                                    GTK_ENTRY_ICON_PRIMARY));
      break;

    case PROP_STOCK_SECONDARY:
      g_value_set_string (value,
                          gtk_entry_get_icon_stock (entry,
                                                    GTK_ENTRY_ICON_SECONDARY));
      break;

    case PROP_ICON_NAME_PRIMARY:
      g_value_set_string (value,
                          gtk_entry_get_icon_name (entry,
                                                   GTK_ENTRY_ICON_PRIMARY));
      break;

    case PROP_ICON_NAME_SECONDARY:
      g_value_set_string (value,
                          gtk_entry_get_icon_name (entry,
                                                   GTK_ENTRY_ICON_SECONDARY));
      break;

    case PROP_GICON_PRIMARY:
      g_value_set_object (value,
                          gtk_entry_get_icon_gicon (entry,
                                                    GTK_ENTRY_ICON_PRIMARY));
      break;

    case PROP_GICON_SECONDARY:
      g_value_set_object (value,
                          gtk_entry_get_icon_gicon (entry,
                                                    GTK_ENTRY_ICON_SECONDARY));
      break;

    case PROP_STORAGE_TYPE_PRIMARY:
      g_value_set_enum (value,
                        gtk_entry_get_icon_storage_type (entry, 
                                                         GTK_ENTRY_ICON_PRIMARY));
      break;

    case PROP_STORAGE_TYPE_SECONDARY:
      g_value_set_enum (value,
                        gtk_entry_get_icon_storage_type (entry, 
                                                         GTK_ENTRY_ICON_SECONDARY));
      break;

    case PROP_ACTIVATABLE_PRIMARY:
      g_value_set_boolean (value,
                           gtk_entry_get_icon_activatable (entry, GTK_ENTRY_ICON_PRIMARY));
      break;

    case PROP_ACTIVATABLE_SECONDARY:
      g_value_set_boolean (value,
                           gtk_entry_get_icon_activatable (entry, GTK_ENTRY_ICON_SECONDARY));
      break;

    case PROP_SENSITIVE_PRIMARY:
      g_value_set_boolean (value,
                           gtk_entry_get_icon_sensitive (entry, GTK_ENTRY_ICON_PRIMARY));
      break;

    case PROP_SENSITIVE_SECONDARY:
      g_value_set_boolean (value,
                           gtk_entry_get_icon_sensitive (entry, GTK_ENTRY_ICON_SECONDARY));
      break;

    case PROP_TOOLTIP_TEXT_PRIMARY:
      g_value_take_string (value,
                           gtk_entry_get_icon_tooltip_text (entry, GTK_ENTRY_ICON_PRIMARY));
      break;

    case PROP_TOOLTIP_TEXT_SECONDARY:
      g_value_take_string (value,
                           gtk_entry_get_icon_tooltip_text (entry, GTK_ENTRY_ICON_SECONDARY));
      break;

    case PROP_TOOLTIP_MARKUP_PRIMARY:
      g_value_take_string (value,
                           gtk_entry_get_icon_tooltip_markup (entry, GTK_ENTRY_ICON_PRIMARY));
      break;

    case PROP_TOOLTIP_MARKUP_SECONDARY:
      g_value_take_string (value,
                           gtk_entry_get_icon_tooltip_markup (entry, GTK_ENTRY_ICON_SECONDARY));
      break;

    case PROP_EDITING_CANCELED:
      g_value_set_boolean (value,
                           entry->editing_canceled);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gunichar
find_invisible_char (GtkWidget *widget)
{
  PangoLayout *layout;
  PangoAttrList *attr_list;
  gint i;
  gunichar invisible_chars [] = {
    0,
    0x25cf, /* BLACK CIRCLE */
    0x2022, /* BULLET */
    0x2731, /* HEAVY ASTERISK */
    0x273a  /* SIXTEEN POINTED ASTERISK */
  };

  if (widget->style)
    gtk_widget_style_get (widget,
                          "invisible-char", &invisible_chars[0],
                          NULL);

  layout = gtk_widget_create_pango_layout (widget, NULL);

  attr_list = pango_attr_list_new ();
  pango_attr_list_insert (attr_list, pango_attr_fallback_new (FALSE));

  pango_layout_set_attributes (layout, attr_list);
  pango_attr_list_unref (attr_list);

  for (i = (invisible_chars[0] != 0 ? 0 : 1); i < G_N_ELEMENTS (invisible_chars); i++)
    {
      gchar text[7] = { 0, };
      gint len, count;

      len = g_unichar_to_utf8 (invisible_chars[i], text);
      pango_layout_set_text (layout, text, len);

      count = pango_layout_get_unknown_glyphs_count (layout);

      if (count == 0)
        {
          g_object_unref (layout);
          return invisible_chars[i];
        }
    }

  g_object_unref (layout);

  return '*';
}

static void
gtk_entry_init (GtkEntry *entry)
{
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (entry);

  gtk_widget_set_can_focus (GTK_WIDGET (entry), TRUE);

  entry->editable = TRUE;
  entry->visible = TRUE;
  entry->invisible_char = find_invisible_char (GTK_WIDGET (entry));
  entry->dnd_position = -1;
  entry->width_chars = -1;
  entry->is_cell_renderer = FALSE;
  entry->editing_canceled = FALSE;
  entry->has_frame = TRUE;
  entry->truncate_multiline = FALSE;
  priv->shadow_type = GTK_SHADOW_IN;
  priv->xalign = 0.0;
  priv->caps_lock_warning = TRUE;
  priv->caps_lock_warning_shown = FALSE;
  priv->progress_fraction = 0.0;
  priv->progress_pulse_fraction = 0.1;

  gtk_drag_dest_set (GTK_WIDGET (entry),
                     GTK_DEST_DEFAULT_HIGHLIGHT,
                     NULL, 0,
                     GDK_ACTION_COPY | GDK_ACTION_MOVE);
  gtk_drag_dest_add_text_targets (GTK_WIDGET (entry));

  /* This object is completely private. No external entity can gain a reference
   * to it; so we create it here and destroy it in finalize().
   */
  entry->im_context = gtk_im_multicontext_new ();
  
  g_signal_connect (entry->im_context, "commit",
		    G_CALLBACK (gtk_entry_commit_cb), entry);
  g_signal_connect (entry->im_context, "preedit-changed",
		    G_CALLBACK (gtk_entry_preedit_changed_cb), entry);
  g_signal_connect (entry->im_context, "retrieve-surrounding",
		    G_CALLBACK (gtk_entry_retrieve_surrounding_cb), entry);
  g_signal_connect (entry->im_context, "delete-surrounding",
		    G_CALLBACK (gtk_entry_delete_surrounding_cb), entry);

}

static gint
get_icon_width (GtkEntry             *entry,
                GtkEntryIconPosition  icon_pos)
{
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (entry);
  EntryIconInfo *icon_info = priv->icons[icon_pos];
  GdkScreen *screen;
  GtkSettings *settings;
  gint menu_icon_width;

  if (!icon_info || icon_info->pixbuf == NULL)
    return 0;

  screen = gtk_widget_get_screen (GTK_WIDGET (entry));
  settings = gtk_settings_get_for_screen (screen);

  gtk_icon_size_lookup_for_settings (settings, GTK_ICON_SIZE_MENU,
                                     &menu_icon_width, NULL);

  return MAX (gdk_pixbuf_get_width (icon_info->pixbuf), menu_icon_width);
}

static void
get_icon_allocations (GtkEntry      *entry,
                      GtkAllocation *primary,
                      GtkAllocation *secondary)

{
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (entry);
  gint x, y, width, height;

  get_text_area_size (entry, &x, &y, &width, &height);

  if (gtk_widget_has_focus (GTK_WIDGET (entry)) && !priv->interior_focus)
    y += priv->focus_width;

  primary->y = y;
  primary->height = height;
  primary->width = get_icon_width (entry, GTK_ENTRY_ICON_PRIMARY);
  if (primary->width > 0)
    primary->width += 2 * priv->icon_margin;

  secondary->y = y;
  secondary->height = height;
  secondary->width = get_icon_width (entry, GTK_ENTRY_ICON_SECONDARY);
  if (secondary->width > 0)
    secondary->width += 2 * priv->icon_margin;

  if (gtk_widget_get_direction (GTK_WIDGET (entry)) == GTK_TEXT_DIR_RTL)
    {
      primary->x = x + width - primary->width;
      secondary->x = x;
    }
  else
    {
      primary->x = x;
      secondary->x = x + width - secondary->width;
    }
}


static void
begin_change (GtkEntry *entry)
{
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (entry);

  priv->change_count++;

  g_object_freeze_notify (G_OBJECT (entry));
}

static void
end_change (GtkEntry *entry)
{
  GtkEditable *editable = GTK_EDITABLE (entry);
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (entry);
 
  g_return_if_fail (priv->change_count > 0);

  g_object_thaw_notify (G_OBJECT (entry));

  priv->change_count--;

  if (priv->change_count == 0)
    {
       if (priv->real_changed) 
         {
           g_signal_emit_by_name (editable, "changed");
           priv->real_changed = FALSE;
         }
    } 
}

static void
emit_changed (GtkEntry *entry)
{
  GtkEditable *editable = GTK_EDITABLE (entry);
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (entry);

  if (priv->change_count == 0)
    g_signal_emit_by_name (editable, "changed");
  else 
    priv->real_changed = TRUE;
}

static void
gtk_entry_destroy (GtkObject *object)
{
  GtkEntry *entry = GTK_ENTRY (object);

  entry->current_pos = entry->selection_bound = 0;
  _gtk_entry_reset_im_context (entry);
  gtk_entry_reset_layout (entry);

  if (entry->blink_timeout)
    {
      g_source_remove (entry->blink_timeout);
      entry->blink_timeout = 0;
    }

  if (entry->recompute_idle)
    {
      g_source_remove (entry->recompute_idle);
      entry->recompute_idle = 0;
    }

  GTK_OBJECT_CLASS (gtk_entry_parent_class)->destroy (object);
}

static void
gtk_entry_dispose (GObject *object)
{
  GtkEntry *entry = GTK_ENTRY (object);
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (entry);
  GdkKeymap *keymap;

  gtk_entry_set_icon_from_pixbuf (entry, GTK_ENTRY_ICON_PRIMARY, NULL);
  gtk_entry_set_icon_tooltip_markup (entry, GTK_ENTRY_ICON_PRIMARY, NULL);
  gtk_entry_set_icon_from_pixbuf (entry, GTK_ENTRY_ICON_SECONDARY, NULL);
  gtk_entry_set_icon_tooltip_markup (entry, GTK_ENTRY_ICON_SECONDARY, NULL);
  gtk_entry_set_completion (entry, NULL);

  if (priv->buffer)
    {
      buffer_disconnect_signals (entry);
      g_object_unref (priv->buffer);
      priv->buffer = NULL;
    }

  keymap = gdk_keymap_get_for_display (gtk_widget_get_display (GTK_WIDGET (object)));
  g_signal_handlers_disconnect_by_func (keymap, keymap_state_changed, entry);
  g_signal_handlers_disconnect_by_func (keymap, keymap_direction_changed, entry);

  G_OBJECT_CLASS (gtk_entry_parent_class)->dispose (object);
}

static void
gtk_entry_finalize (GObject *object)
{
  GtkEntry *entry = GTK_ENTRY (object);
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (entry);
  EntryIconInfo *icon_info = NULL;
  gint i;

  for (i = 0; i < MAX_ICONS; i++)
    {
      if ((icon_info = priv->icons[i]) != NULL)
        {
          if (icon_info->target_list != NULL)
            {
              gtk_target_list_unref (icon_info->target_list);
              icon_info->target_list = NULL;
            }

          g_slice_free (EntryIconInfo, icon_info);
          priv->icons[i] = NULL;
        }
    }

  if (entry->cached_layout)
    g_object_unref (entry->cached_layout);

  g_object_unref (entry->im_context);

  if (entry->blink_timeout)
    g_source_remove (entry->blink_timeout);

  if (entry->recompute_idle)
    g_source_remove (entry->recompute_idle);

  g_free (priv->im_module);

  G_OBJECT_CLASS (gtk_entry_parent_class)->finalize (object);
}

static DisplayMode
gtk_entry_get_display_mode (GtkEntry *entry)
{
  GtkEntryPrivate *priv;
  if (entry->visible)
    return DISPLAY_NORMAL;
  priv = GTK_ENTRY_GET_PRIVATE (entry);
  if (entry->invisible_char == 0 && priv->invisible_char_set)
    return DISPLAY_BLANK;
  return DISPLAY_INVISIBLE;
}

static gchar*
gtk_entry_get_display_text (GtkEntry *entry,
                            gint      start_pos,
                            gint      end_pos)
{
  GtkEntryPasswordHint *password_hint;
  GtkEntryPrivate *priv;
  gunichar invisible_char;
  const gchar *start;
  const gchar *end;
  const gchar *text;
  gchar char_str[7];
  gint char_len;
  GString *str;
  guint length;
  gint i;

  priv = GTK_ENTRY_GET_PRIVATE (entry);
  text = gtk_entry_buffer_get_text (get_buffer (entry));
  length = gtk_entry_buffer_get_length (get_buffer (entry));

  if (end_pos < 0)
    end_pos = length;
  if (start_pos > length)
    start_pos = length;

  if (end_pos <= start_pos)
      return g_strdup ("");
  else if (entry->visible)
    {
      start = g_utf8_offset_to_pointer (text, start_pos);
      end = g_utf8_offset_to_pointer (start, end_pos - start_pos);
      return g_strndup (start, end - start);
    }
  else
    {
      str = g_string_sized_new (length * 2);

      /* Figure out what our invisible char is and encode it */
      if (!entry->invisible_char)
          invisible_char = priv->invisible_char_set ? ' ' : '*';
      else
          invisible_char = entry->invisible_char;
      char_len = g_unichar_to_utf8 (invisible_char, char_str);

      /*
       * Add hidden characters for each character in the text
       * buffer. If there is a password hint, then keep that
       * character visible.
       */

      password_hint = g_object_get_qdata (G_OBJECT (entry), quark_password_hint);
      for (i = start_pos; i < end_pos; ++i)
        {
          if (password_hint && i == password_hint->position)
            {
              start = g_utf8_offset_to_pointer (text, i);
              g_string_append_len (str, start, g_utf8_next_char (start) - start);
            }
          else
            {
              g_string_append_len (str, char_str, char_len);
            }
        }

      return g_string_free (str, FALSE);
    }

}

static void
update_cursors (GtkWidget *widget)
{
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (widget);
  EntryIconInfo *icon_info = NULL;
  GdkDisplay *display;
  GdkCursor *cursor;
  gint i;

  for (i = 0; i < MAX_ICONS; i++)
    {
      if ((icon_info = priv->icons[i]) != NULL)
        {
          if (icon_info->pixbuf != NULL && icon_info->window != NULL)
            gdk_window_show_unraised (icon_info->window);

          /* The icon windows are not children of the visible entry window,
           * thus we can't just inherit the xterm cursor. Slight complication 
           * here is that for the entry, insensitive => arrow cursor, but for 
           * an icon in a sensitive entry, insensitive => xterm cursor.
           */
          if (gtk_widget_is_sensitive (widget) &&
              (icon_info->insensitive || 
               (icon_info->nonactivatable && icon_info->target_list == NULL)))
            {
              display = gtk_widget_get_display (widget);
              cursor = gdk_cursor_new_for_display (display, GDK_XTERM);
              gdk_window_set_cursor (icon_info->window, cursor);
              gdk_cursor_unref (cursor);
            }
          else
            {
              gdk_window_set_cursor (icon_info->window, NULL);
            }
        }
    }
}

static void
realize_icon_info (GtkWidget            *widget, 
                   GtkEntryIconPosition  icon_pos)
{
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (widget);
  EntryIconInfo *icon_info = priv->icons[icon_pos];
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (icon_info != NULL);

  attributes.x = 0;
  attributes.y = 0;
  attributes.width = 1;
  attributes.height = 1;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_EXPOSURE_MASK |
                                GDK_BUTTON_PRESS_MASK |
                                GDK_BUTTON_RELEASE_MASK |
                                GDK_BUTTON1_MOTION_MASK |
                                GDK_BUTTON3_MOTION_MASK |
                                GDK_POINTER_MOTION_HINT_MASK |
                                GDK_POINTER_MOTION_MASK |
                                GDK_ENTER_NOTIFY_MASK |
                            GDK_LEAVE_NOTIFY_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  icon_info->window = gdk_window_new (widget->window,
                                      &attributes,
                                      attributes_mask);
  gdk_window_set_user_data (icon_info->window, widget);
  gdk_window_set_background (icon_info->window,
                             &widget->style->base[gtk_widget_get_state (widget)]);

  gtk_widget_queue_resize (widget);
}

static EntryIconInfo*
construct_icon_info (GtkWidget            *widget, 
                     GtkEntryIconPosition  icon_pos)
{
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (widget);
  EntryIconInfo *icon_info;

  g_return_val_if_fail (priv->icons[icon_pos] == NULL, NULL);

  icon_info = g_slice_new0 (EntryIconInfo);
  priv->icons[icon_pos] = icon_info;

  if (gtk_widget_get_realized (widget))
    realize_icon_info (widget, icon_pos);

  return icon_info;
}

static void
gtk_entry_map (GtkWidget *widget)
{
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (widget);
  EntryIconInfo *icon_info = NULL;
  gint i;

  if (gtk_widget_get_realized (widget) && !gtk_widget_get_mapped (widget))
    {
      GTK_WIDGET_CLASS (gtk_entry_parent_class)->map (widget);

      for (i = 0; i < MAX_ICONS; i++)
        {
          if ((icon_info = priv->icons[i]) != NULL)
            {
              if (icon_info->pixbuf != NULL && icon_info->window != NULL)
                gdk_window_show (icon_info->window);
            }
        }

      update_cursors (widget);
    }
}

static void
gtk_entry_unmap (GtkWidget *widget)
{
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (widget);
  EntryIconInfo *icon_info = NULL;
  gint i;

  if (gtk_widget_get_mapped (widget))
    {
      for (i = 0; i < MAX_ICONS; i++)
        {
          if ((icon_info = priv->icons[i]) != NULL)
            {
              if (icon_info->pixbuf != NULL && icon_info->window != NULL)
                gdk_window_hide (icon_info->window);
            }
        }

      GTK_WIDGET_CLASS (gtk_entry_parent_class)->unmap (widget);
    }
}

static void
gtk_entry_realize (GtkWidget *widget)
{
  GtkEntry *entry;
  GtkEntryPrivate *priv;
  EntryIconInfo *icon_info;
  GdkWindowAttr attributes;
  gint attributes_mask;
  int i;

  gtk_widget_set_realized (widget, TRUE);
  entry = GTK_ENTRY (widget);
  priv = GTK_ENTRY_GET_PRIVATE (entry);

  attributes.window_type = GDK_WINDOW_CHILD;
  
  get_widget_window_size (entry, &attributes.x, &attributes.y, &attributes.width, &attributes.height);

  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_EXPOSURE_MASK |
			    GDK_BUTTON_PRESS_MASK |
			    GDK_BUTTON_RELEASE_MASK |
			    GDK_BUTTON1_MOTION_MASK |
			    GDK_BUTTON3_MOTION_MASK |
			    GDK_POINTER_MOTION_HINT_MASK |
			    GDK_POINTER_MOTION_MASK |
                            GDK_ENTER_NOTIFY_MASK |
			    GDK_LEAVE_NOTIFY_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, entry);

  get_text_area_size (entry, &attributes.x, &attributes.y, &attributes.width, &attributes.height);
 
  if (gtk_widget_is_sensitive (widget))
    {
      attributes.cursor = gdk_cursor_new_for_display (gtk_widget_get_display (widget), GDK_XTERM);
      attributes_mask |= GDK_WA_CURSOR;
    }

  entry->text_area = gdk_window_new (widget->window, &attributes, attributes_mask);

  gdk_window_set_user_data (entry->text_area, entry);

  if (attributes_mask & GDK_WA_CURSOR)
    gdk_cursor_unref (attributes.cursor);

  widget->style = gtk_style_attach (widget->style, widget->window);

  gdk_window_set_background (widget->window, &widget->style->base[gtk_widget_get_state (widget)]);
  gdk_window_set_background (entry->text_area, &widget->style->base[gtk_widget_get_state (widget)]);

  gdk_window_show (entry->text_area);

  gtk_im_context_set_client_window (entry->im_context, entry->text_area);

  gtk_entry_adjust_scroll (entry);
  gtk_entry_update_primary_selection (entry);


  /* If the icon positions are already setup, create their windows.
   * Otherwise if they don't exist yet, then construct_icon_info()
   * will create the windows once the widget is already realized.
   */
  for (i = 0; i < MAX_ICONS; i++)
    {
      if ((icon_info = priv->icons[i]) != NULL)
        {
          if (icon_info->window == NULL)
            realize_icon_info (widget, i);
        }
    }
}

static void
gtk_entry_unrealize (GtkWidget *widget)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (entry);
  GtkClipboard *clipboard;
  EntryIconInfo *icon_info;
  gint i;

  gtk_entry_reset_layout (entry);
  
  gtk_im_context_set_client_window (entry->im_context, NULL);

  clipboard = gtk_widget_get_clipboard (widget, GDK_SELECTION_PRIMARY);
  if (gtk_clipboard_get_owner (clipboard) == G_OBJECT (entry))
    gtk_clipboard_clear (clipboard);
  
  if (entry->text_area)
    {
      gdk_window_set_user_data (entry->text_area, NULL);
      gdk_window_destroy (entry->text_area);
      entry->text_area = NULL;
    }

  if (entry->popup_menu)
    {
      gtk_widget_destroy (entry->popup_menu);
      entry->popup_menu = NULL;
    }

  GTK_WIDGET_CLASS (gtk_entry_parent_class)->unrealize (widget);

  for (i = 0; i < MAX_ICONS; i++)
    {
      if ((icon_info = priv->icons[i]) != NULL)
        {
          if (icon_info->window != NULL)
            {
              gdk_window_destroy (icon_info->window);
              icon_info->window = NULL;
            }
        }
    }
}

void
_gtk_entry_get_borders (GtkEntry *entry,
			gint     *xborder,
			gint     *yborder)
{
  GtkWidget *widget = GTK_WIDGET (entry);
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (widget);

  if (entry->has_frame)
    {
      *xborder = widget->style->xthickness;
      *yborder = widget->style->ythickness;
    }
  else
    {
      *xborder = 0;
      *yborder = 0;
    }

  if (!priv->interior_focus)
    {
      *xborder += priv->focus_width;
      *yborder += priv->focus_width;
    }
}

static void
gtk_entry_size_request (GtkWidget      *widget,
			GtkRequisition *requisition)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (entry);
  PangoFontMetrics *metrics;
  gint xborder, yborder;
  GtkBorder inner_border;
  PangoContext *context;
  int icon_widths = 0;
  int icon_width, i;
  
  gtk_widget_ensure_style (widget);
  context = gtk_widget_get_pango_context (widget);
  metrics = pango_context_get_metrics (context,
				       widget->style->font_desc,
				       pango_context_get_language (context));

  entry->ascent = pango_font_metrics_get_ascent (metrics);
  entry->descent = pango_font_metrics_get_descent (metrics);
  
  _gtk_entry_get_borders (entry, &xborder, &yborder);
  _gtk_entry_effective_inner_border (entry, &inner_border);

  if (entry->width_chars < 0)
    requisition->width = MIN_ENTRY_WIDTH + xborder * 2 + inner_border.left + inner_border.right;
  else
    {
      gint char_width = pango_font_metrics_get_approximate_char_width (metrics);
      gint digit_width = pango_font_metrics_get_approximate_digit_width (metrics);
      gint char_pixels = (MAX (char_width, digit_width) + PANGO_SCALE - 1) / PANGO_SCALE;
      
      requisition->width = char_pixels * entry->width_chars + xborder * 2 + inner_border.left + inner_border.right;
    }
    
  requisition->height = PANGO_PIXELS (entry->ascent + entry->descent) + yborder * 2 + inner_border.top + inner_border.bottom;

  for (i = 0; i < MAX_ICONS; i++)
    {
      icon_width = get_icon_width (entry, i);
      if (icon_width > 0)
        icon_widths += icon_width + 2 * priv->icon_margin;
    }

  if (icon_widths > requisition->width)
    requisition->width += icon_widths;

  pango_font_metrics_unref (metrics);
}

static void
place_windows (GtkEntry *entry)
{
  GtkWidget *widget = GTK_WIDGET (entry);
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (entry);
  gint x, y, width, height;
  GtkAllocation primary;
  GtkAllocation secondary;
  EntryIconInfo *icon_info = NULL;

  get_text_area_size (entry, &x, &y, &width, &height);
  get_icon_allocations (entry, &primary, &secondary);

  if (gtk_widget_has_focus (widget) && !priv->interior_focus)
    y += priv->focus_width;

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    x += secondary.width;
  else
    x += primary.width;
  width -= primary.width + secondary.width;

  if ((icon_info = priv->icons[GTK_ENTRY_ICON_PRIMARY]) != NULL)
    gdk_window_move_resize (icon_info->window,
                            primary.x, primary.y,
                            primary.width, primary.height);

  if ((icon_info = priv->icons[GTK_ENTRY_ICON_SECONDARY]) != NULL)
    gdk_window_move_resize (icon_info->window,
                            secondary.x, secondary.y,
                            secondary.width, secondary.height);

  gdk_window_move_resize (entry->text_area, x, y, width, height);
}

static void
gtk_entry_get_text_area_size (GtkEntry *entry,
                              gint     *x,
			      gint     *y,
			      gint     *width,
			      gint     *height)
{
  GtkWidget *widget = GTK_WIDGET (entry);
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (widget);
  gint frame_height;
  gint xborder, yborder;
  GtkRequisition requisition;

  gtk_widget_get_child_requisition (widget, &requisition);
  _gtk_entry_get_borders (entry, &xborder, &yborder);

  if (gtk_widget_get_realized (widget))
    frame_height = gdk_window_get_height (widget->window);
  else
    frame_height = requisition.height;

  if (gtk_widget_has_focus (widget) && !priv->interior_focus)
    frame_height -= 2 * priv->focus_width;

  if (x)
    *x = xborder;

  if (y)
    *y = frame_height / 2 - (requisition.height - yborder * 2) / 2;

  if (width)
    *width = GTK_WIDGET (entry)->allocation.width - xborder * 2;

  if (height)
    *height = requisition.height - yborder * 2;
}

static void
get_text_area_size (GtkEntry *entry,
                    gint     *x,
                    gint     *y,
                    gint     *width,
                    gint     *height)
{
  GtkEntryClass *class;

  g_return_if_fail (GTK_IS_ENTRY (entry));

  class = GTK_ENTRY_GET_CLASS (entry);

  if (class->get_text_area_size)
    class->get_text_area_size (entry, x, y, width, height);
}


static void
get_widget_window_size (GtkEntry *entry,
                        gint     *x,
                        gint     *y,
                        gint     *width,
                        gint     *height)
{
  GtkRequisition requisition;
  GtkWidget *widget = GTK_WIDGET (entry);
      
  gtk_widget_get_child_requisition (widget, &requisition);

  if (x)
    *x = widget->allocation.x;

  if (y)
    {
      if (entry->is_cell_renderer)
	*y = widget->allocation.y;
      else
	*y = widget->allocation.y + (widget->allocation.height - requisition.height) / 2;
    }

  if (width)
    *width = widget->allocation.width;

  if (height)
    {
      if (entry->is_cell_renderer)
	*height = widget->allocation.height;
      else
	*height = requisition.height;
    }
}

void
_gtk_entry_effective_inner_border (GtkEntry  *entry,
                                   GtkBorder *border)
{
  GtkBorder *tmp_border;

  tmp_border = g_object_get_qdata (G_OBJECT (entry), quark_inner_border);

  if (tmp_border)
    {
      *border = *tmp_border;
      return;
    }

  gtk_widget_style_get (GTK_WIDGET (entry), "inner-border", &tmp_border, NULL);

  if (tmp_border)
    {
      *border = *tmp_border;
      gtk_border_free (tmp_border);
      return;
    }

  *border = default_inner_border;
}

static void
gtk_entry_size_allocate (GtkWidget     *widget,
			 GtkAllocation *allocation)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  
  widget->allocation = *allocation;
  
  if (gtk_widget_get_realized (widget))
    {
      /* We call gtk_widget_get_child_requisition, since we want (for
       * backwards compatibility reasons) the realization here to
       * be affected by the usize of the entry, if set
       */
      gint x, y, width, height;
      GtkEntryCompletion* completion;

      get_widget_window_size (entry, &x, &y, &width, &height);
      gdk_window_move_resize (widget->window, x, y, width, height);

      place_windows (entry);
      gtk_entry_recompute (entry);

      completion = gtk_entry_get_completion (entry);
      if (completion && gtk_widget_get_mapped (completion->priv->popup_window))
        _gtk_entry_completion_resize_popup (completion);
    }
}

/* Kudos to the gnome-panel guys. */
static void
colorshift_pixbuf (GdkPixbuf *dest,
                   GdkPixbuf *src,
                   gint       shift)
{
  gint i, j;
  gint width, height, has_alpha, src_rowstride, dest_rowstride;
  guchar *target_pixels;
  guchar *original_pixels;
  guchar *pix_src;
  guchar *pix_dest;
  int val;
  guchar r, g, b;

  has_alpha       = gdk_pixbuf_get_has_alpha (src);
  width           = gdk_pixbuf_get_width (src);
  height          = gdk_pixbuf_get_height (src);
  src_rowstride   = gdk_pixbuf_get_rowstride (src);
  dest_rowstride  = gdk_pixbuf_get_rowstride (dest);
  original_pixels = gdk_pixbuf_get_pixels (src);
  target_pixels   = gdk_pixbuf_get_pixels (dest);

  for (i = 0; i < height; i++)
    {
      pix_dest = target_pixels   + i * dest_rowstride;
      pix_src  = original_pixels + i * src_rowstride;

      for (j = 0; j < width; j++)
        {
          r = *(pix_src++);
          g = *(pix_src++);
          b = *(pix_src++);

          val = r + shift;
          *(pix_dest++) = CLAMP (val, 0, 255);

          val = g + shift;
          *(pix_dest++) = CLAMP (val, 0, 255);

          val = b + shift;
          *(pix_dest++) = CLAMP (val, 0, 255);

          if (has_alpha)
            *(pix_dest++) = *(pix_src++);
        }
    }
}

static gboolean
should_prelight (GtkEntry             *entry,
                 GtkEntryIconPosition  icon_pos)
{
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (entry);
  EntryIconInfo *icon_info = priv->icons[icon_pos];
  gboolean prelight;

  if (!icon_info) 
    return FALSE;

  if (icon_info->nonactivatable && icon_info->target_list == NULL)
    return FALSE;

  if (icon_info->pressed)
    return FALSE;

  gtk_widget_style_get (GTK_WIDGET (entry),
                        "icon-prelight", &prelight,
                        NULL);

  return prelight;
}

static void
draw_icon (GtkWidget            *widget,
           GtkEntryIconPosition  icon_pos)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (entry);
  EntryIconInfo *icon_info = priv->icons[icon_pos];
  GdkPixbuf *pixbuf;
  gint x, y, width, height;
  cairo_t *cr;

  if (!icon_info)
    return;

  gtk_entry_ensure_pixbuf (entry, icon_pos);

  if (icon_info->pixbuf == NULL)
    return;

  width = gdk_window_get_width (icon_info->window);
  height = gdk_window_get_height (icon_info->window);

  /* size_allocate hasn't been called yet. These are the default values.
   */
  if (width == 1 || height == 1)
    return;

  pixbuf = icon_info->pixbuf;
  g_object_ref (pixbuf);

  if (gdk_pixbuf_get_height (pixbuf) > height)
    {
      GdkPixbuf *temp_pixbuf;
      gint scale;

      scale = height - 2 * priv->icon_margin;
      temp_pixbuf = gdk_pixbuf_scale_simple (pixbuf, scale, scale,
                                             GDK_INTERP_BILINEAR);
      g_object_unref (pixbuf);
      pixbuf = temp_pixbuf;
    }

  x = (width  - gdk_pixbuf_get_width (pixbuf)) / 2;
  y = (height - gdk_pixbuf_get_height (pixbuf)) / 2;

  if (!gtk_widget_is_sensitive (widget) ||
      icon_info->insensitive)
    {
      GdkPixbuf *temp_pixbuf;

      temp_pixbuf = gdk_pixbuf_copy (pixbuf);
      gdk_pixbuf_saturate_and_pixelate (pixbuf,
                                        temp_pixbuf,
                                        0.8f,
                                        TRUE);
      g_object_unref (pixbuf);
      pixbuf = temp_pixbuf;
    }
  else if (icon_info->prelight)
    {
      GdkPixbuf *temp_pixbuf;

      temp_pixbuf = gdk_pixbuf_copy (pixbuf);
      colorshift_pixbuf (temp_pixbuf, pixbuf, 30);
      g_object_unref (pixbuf);
      pixbuf = temp_pixbuf;
    }

  cr = gdk_cairo_create (icon_info->window);
  gdk_cairo_set_source_pixbuf (cr, pixbuf, x, y);
  cairo_paint (cr);
  cairo_destroy (cr);

  g_object_unref (pixbuf);
}


static void
gtk_entry_draw_frame (GtkWidget      *widget,
                      GdkEventExpose *event)
{
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (widget);
  gint x = 0, y = 0, width, height;
  gboolean state_hint;
  GtkStateType state;

  width = gdk_window_get_width (widget->window);
  height = gdk_window_get_height (widget->window);

  /* Fix a problem with some themes which assume that entry->text_area's
   * width equals widget->window's width */
  if (GTK_IS_SPIN_BUTTON (widget))
    {
      gint xborder, yborder;

      gtk_entry_get_text_area_size (GTK_ENTRY (widget), &x, NULL, &width, NULL);
      _gtk_entry_get_borders (GTK_ENTRY (widget), &xborder, &yborder);

      x -= xborder;
      width += xborder * 2;
    }

  if (gtk_widget_has_focus (widget) && !priv->interior_focus)
    {
      x += priv->focus_width;
      y += priv->focus_width;
      width -= 2 * priv->focus_width;
      height -= 2 * priv->focus_width;
    }

  gtk_widget_style_get (widget, "state-hint", &state_hint, NULL);
  if (state_hint)
      state = gtk_widget_has_focus (widget) ?
        GTK_STATE_ACTIVE : gtk_widget_get_state (widget);
  else
      state = GTK_STATE_NORMAL;

  gtk_paint_shadow (widget->style, widget->window,
                    state, priv->shadow_type,
                    &event->area, widget, "entry", x, y, width, height);


  gtk_entry_draw_progress (widget, event);

  if (gtk_widget_has_focus (widget) && !priv->interior_focus)
    {
      x -= priv->focus_width;
      y -= priv->focus_width;
      width += 2 * priv->focus_width;
      height += 2 * priv->focus_width;
      
      gtk_paint_focus (widget->style, widget->window,
                       gtk_widget_get_state (widget),
		       &event->area, widget, "entry",
		       0, 0, width, height);
    }
}

static void
get_progress_area (GtkWidget *widget,
                   gint       *x,
                   gint       *y,
                   gint       *width,
                   gint       *height)
{
  GtkEntryPrivate *private = GTK_ENTRY_GET_PRIVATE (widget);
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkBorder *progress_border;

  get_text_area_size (entry, x, y, width, height);

  if (!private->interior_focus)
    {
      *x -= private->focus_width;
      *y -= private->focus_width;
      *width += 2 * private->focus_width;
      *height += 2 * private->focus_width;
    }

  gtk_widget_style_get (widget, "progress-border", &progress_border, NULL);

  if (progress_border)
    {
      *x += progress_border->left;
      *y += progress_border->top;
      *width -= progress_border->left + progress_border->right;
      *height -= progress_border->top + progress_border->bottom;

      gtk_border_free (progress_border);
    }

  if (private->progress_pulse_mode)
    {
      gdouble value = private->progress_pulse_current;

      *x += (gint) floor(value * (*width));
      *width = (gint) ceil(private->progress_pulse_fraction * (*width));
    }
  else if (private->progress_fraction > 0)
    {
      gdouble value = private->progress_fraction;

      if (gtk_widget_get_direction (GTK_WIDGET (entry)) == GTK_TEXT_DIR_RTL)
        {
          gint bar_width;

          bar_width = floor(value * (*width) + 0.5);
          *x += *width - bar_width;
          *width = bar_width;
        }
      else
        {
          *width = (gint) floor(value * (*width) + 0.5);
        }
    }
  else
    {
      *width = 0;
      *height = 0;
    }
}

static void
gtk_entry_draw_progress (GtkWidget      *widget,
                         GdkEventExpose *event)
{
  gint x, y, width, height;
  GtkStateType state;

  get_progress_area (widget, &x, &y, &width, &height);

  if ((width <= 0) || (height <= 0))
    return;

  if (event->window != widget->window)
    {
      gint pos_x, pos_y;

      gdk_window_get_position (event->window, &pos_x, &pos_y);

      x -= pos_x;
      y -= pos_y;
    }

  state = GTK_STATE_SELECTED;
  if (!gtk_widget_get_sensitive (widget))
    state = GTK_STATE_INSENSITIVE;

  gtk_paint_box (widget->style, event->window,
                 state, GTK_SHADOW_OUT,
                 &event->area, widget, "entry-progress",
                 x, y,
                 width, height);
}

static gint
gtk_entry_expose (GtkWidget      *widget,
		  GdkEventExpose *event)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  gboolean state_hint;
  GtkStateType state;
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (entry);

  gtk_widget_style_get (widget, "state-hint", &state_hint, NULL);
  if (state_hint)
    state = gtk_widget_has_focus (widget) ?
      GTK_STATE_ACTIVE : gtk_widget_get_state (widget);
  else
    state = gtk_widget_get_state(widget);

  if (widget->window == event->window)
    {
      gtk_entry_draw_frame (widget, event);
    }
  else if (entry->text_area == event->window)
    {
      gint width, height;

      width = gdk_window_get_width (entry->text_area);
      height = gdk_window_get_height (entry->text_area);

      gtk_paint_flat_box (widget->style, entry->text_area, 
			  state, GTK_SHADOW_NONE,
			  &event->area, widget, "entry_bg",
			  0, 0, width, height);

      gtk_entry_draw_progress (widget, event);

      if (entry->dnd_position != -1)
	gtk_entry_draw_cursor (GTK_ENTRY (widget), CURSOR_DND);
      
      gtk_entry_draw_text (GTK_ENTRY (widget));

      /* When no text is being displayed at all, don't show the cursor */
      if (gtk_entry_get_display_mode (entry) != DISPLAY_BLANK &&
	  gtk_widget_has_focus (widget) &&
	  entry->selection_bound == entry->current_pos && entry->cursor_visible) 
        gtk_entry_draw_cursor (GTK_ENTRY (widget), CURSOR_STANDARD);
    }
  else
    {
      int i;

      for (i = 0; i < MAX_ICONS; i++)
        {
          EntryIconInfo *icon_info = priv->icons[i];

          if (icon_info != NULL && event->window == icon_info->window)
            {
              gint width, height;

              width = gdk_window_get_width (icon_info->window);
              height = gdk_window_get_height (icon_info->window);

              gtk_paint_flat_box (widget->style, icon_info->window,
                                  gtk_widget_get_state (widget), GTK_SHADOW_NONE,
                                  NULL, widget, "entry_bg",
                                  0, 0, width, height);

              gtk_entry_draw_progress (widget, event);
              draw_icon (widget, i);

              break;
            }
        }
    }

  return FALSE;
}

static gint
gtk_entry_enter_notify (GtkWidget *widget,
                        GdkEventCrossing *event)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (entry);
  gint i;

  for (i = 0; i < MAX_ICONS; i++)
    {
      EntryIconInfo *icon_info = priv->icons[i];

      if (icon_info != NULL && event->window == icon_info->window)
        {
          if (should_prelight (entry, i))
            {
              icon_info->prelight = TRUE;
              gtk_widget_queue_draw (widget);
            }

          break;
        }
    }

    return FALSE;
}

static gint
gtk_entry_leave_notify (GtkWidget        *widget,
                        GdkEventCrossing *event)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (entry);
  gint i;

  for (i = 0; i < MAX_ICONS; i++)
    {
      EntryIconInfo *icon_info = priv->icons[i];

      if (icon_info != NULL && event->window == icon_info->window)
        {
          /* a grab means that we may never see the button release */
          if (event->mode == GDK_CROSSING_GRAB || event->mode == GDK_CROSSING_GTK_GRAB)
            icon_info->pressed = FALSE;

          if (should_prelight (entry, i))
            {
              icon_info->prelight = FALSE;
              gtk_widget_queue_draw (widget);
            }

          break;
        }
    }

  return FALSE;
}

static void
gtk_entry_get_pixel_ranges (GtkEntry  *entry,
			    gint     **ranges,
			    gint      *n_ranges)
{
  gint start_char, end_char;

  if (gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &start_char, &end_char))
    {
      PangoLayout *layout = gtk_entry_ensure_layout (entry, TRUE);
      PangoLayoutLine *line = pango_layout_get_lines_readonly (layout)->data;
      const char *text = pango_layout_get_text (layout);
      gint start_index = g_utf8_offset_to_pointer (text, start_char) - text;
      gint end_index = g_utf8_offset_to_pointer (text, end_char) - text;
      gint real_n_ranges, i;

      pango_layout_line_get_x_ranges (line, start_index, end_index, ranges, &real_n_ranges);

      if (ranges)
	{
	  gint *r = *ranges;
	  
	  for (i = 0; i < real_n_ranges; ++i)
	    {
	      r[2 * i + 1] = (r[2 * i + 1] - r[2 * i]) / PANGO_SCALE;
	      r[2 * i] = r[2 * i] / PANGO_SCALE;
	    }
	}
      
      if (n_ranges)
	*n_ranges = real_n_ranges;
    }
  else
    {
      if (n_ranges)
	*n_ranges = 0;
      if (ranges)
	*ranges = NULL;
    }
}

static gboolean
in_selection (GtkEntry *entry,
	      gint	x)
{
  gint *ranges;
  gint n_ranges, i;
  gint retval = FALSE;

  gtk_entry_get_pixel_ranges (entry, &ranges, &n_ranges);

  for (i = 0; i < n_ranges; ++i)
    {
      if (x >= ranges[2 * i] && x < ranges[2 * i] + ranges[2 * i + 1])
	{
	  retval = TRUE;
	  break;
	}
    }

  g_free (ranges);
  return retval;
}
	      
static gint
gtk_entry_button_press (GtkWidget      *widget,
			GdkEventButton *event)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEditable *editable = GTK_EDITABLE (widget);
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (entry);
  EntryIconInfo *icon_info = NULL;
  gint tmp_pos;
  gint sel_start, sel_end;
  gint i;

  for (i = 0; i < MAX_ICONS; i++)
    {
      icon_info = priv->icons[i];

      if (!icon_info || icon_info->insensitive)
        continue;

      if (event->window == icon_info->window)
        {
          if (should_prelight (entry, i))
            {
              icon_info->prelight = FALSE;
              gtk_widget_queue_draw (widget);
            }

          priv->start_x = event->x;
          priv->start_y = event->y;
          icon_info->pressed = TRUE;

          if (!icon_info->nonactivatable)
            g_signal_emit (entry, signals[ICON_PRESS], 0, i, event);

          return TRUE;
        }
    }

  if (event->window != entry->text_area ||
      (entry->button && event->button != entry->button))
    return FALSE;

  gtk_entry_reset_blink_time (entry);

  entry->button = event->button;
  
  if (!gtk_widget_has_focus (widget))
    {
      entry->in_click = TRUE;
      gtk_widget_grab_focus (widget);
      entry->in_click = FALSE;
    }
  
  tmp_pos = gtk_entry_find_position (entry, event->x + entry->scroll_offset);

  if (_gtk_button_event_triggers_context_menu (event))
    {
      gtk_entry_do_popup (entry, event);
      entry->button = 0;	/* Don't wait for release, since the menu will gtk_grab_add */

      return TRUE;
    }
  else if (event->button == 1)
    {
      gboolean have_selection = gtk_editable_get_selection_bounds (editable, &sel_start, &sel_end);
      
      entry->select_words = FALSE;
      entry->select_lines = FALSE;

      if (event->state & GTK_EXTEND_SELECTION_MOD_MASK)
	{
	  _gtk_entry_reset_im_context (entry);
	  
	  if (!have_selection) /* select from the current position to the clicked position */
	    sel_start = sel_end = entry->current_pos;
	  
	  if (tmp_pos > sel_start && tmp_pos < sel_end)
	    {
	      /* Truncate current selection, but keep it as big as possible */
	      if (tmp_pos - sel_start > sel_end - tmp_pos)
		gtk_entry_set_positions (entry, sel_start, tmp_pos);
	      else
		gtk_entry_set_positions (entry, tmp_pos, sel_end);
	    }
	  else
	    {
	      gboolean extend_to_left;
	      gint start, end;

	      /* Figure out what click selects and extend current selection */
	      switch (event->type)
		{
		case GDK_BUTTON_PRESS:
		  gtk_entry_set_positions (entry, tmp_pos, tmp_pos);
		  break;
		  
		case GDK_2BUTTON_PRESS:
		  entry->select_words = TRUE;
		  gtk_entry_select_word (entry);
		  break;
		  
		case GDK_3BUTTON_PRESS:
		  entry->select_lines = TRUE;
		  gtk_entry_select_line (entry);
		  break;

		default:
		  break;
		}

	      start = MIN (entry->current_pos, entry->selection_bound);
	      start = MIN (sel_start, start);
	      
	      end = MAX (entry->current_pos, entry->selection_bound);
	      end = MAX (sel_end, end);

	      if (tmp_pos == sel_start || tmp_pos == sel_end)
		extend_to_left = (tmp_pos == start);
	      else
		extend_to_left = (end == sel_end);
	      
	      if (extend_to_left)
		gtk_entry_set_positions (entry, start, end);
	      else
		gtk_entry_set_positions (entry, end, start);
	    }
	}
      else /* no shift key */
	switch (event->type)
	{
	case GDK_BUTTON_PRESS:
	  if (in_selection (entry, event->x + entry->scroll_offset))
	    {
	      /* Click inside the selection - we'll either start a drag, or
	       * clear the selection
	       */
	      entry->in_drag = TRUE;
	      entry->drag_start_x = event->x + entry->scroll_offset;
	      entry->drag_start_y = event->y;
	    }
	  else
            gtk_editable_set_position (editable, tmp_pos);
	  break;
 
	case GDK_2BUTTON_PRESS:
	  /* We ALWAYS receive a GDK_BUTTON_PRESS immediately before 
	   * receiving a GDK_2BUTTON_PRESS so we need to reset
 	   * entry->in_drag which may have been set above
           */
	  entry->in_drag = FALSE;
	  entry->select_words = TRUE;
	  gtk_entry_select_word (entry);
	  break;
	
	case GDK_3BUTTON_PRESS:
	  /* We ALWAYS receive a GDK_BUTTON_PRESS immediately before
	   * receiving a GDK_3BUTTON_PRESS so we need to reset
	   * entry->in_drag which may have been set above
	   */
	  entry->in_drag = FALSE;
	  entry->select_lines = TRUE;
	  gtk_entry_select_line (entry);
	  break;

	default:
	  break;
	}

      return TRUE;
    }
  else if (event->button == 2 && event->type == GDK_BUTTON_PRESS)
    {
      if (entry->editable)
        {
          priv->insert_pos = tmp_pos;
          gtk_entry_paste (entry, GDK_SELECTION_PRIMARY);
          return TRUE;
        }
      else
        {
          gtk_widget_error_bell (widget);
        }
    }

  return FALSE;
}

static gint
gtk_entry_button_release (GtkWidget      *widget,
			  GdkEventButton *event)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (widget);
  EntryIconInfo *icon_info = NULL;
  gint i;

  for (i = 0; i < MAX_ICONS; i++)
    {
      icon_info = priv->icons[i];

      if (!icon_info || icon_info->insensitive)
        continue;

      if (event->window == icon_info->window)
        {
          gint width, height;

          width = gdk_window_get_width (icon_info->window);
          height = gdk_window_get_height (icon_info->window);

          icon_info->pressed = FALSE;

          if (should_prelight (entry, i) &&
              event->x >= 0 && event->y >= 0 &&
              event->x < width && event->y < height)
            {
              icon_info->prelight = TRUE;
              gtk_widget_queue_draw (widget);
            }

          if (!icon_info->nonactivatable)
            g_signal_emit (entry, signals[ICON_RELEASE], 0, i, event);

          return TRUE;
        }
    }

  if (event->window != entry->text_area || entry->button != event->button)
    return FALSE;

  if (entry->in_drag)
    {
      gint tmp_pos = gtk_entry_find_position (entry, entry->drag_start_x);

      gtk_editable_set_position (GTK_EDITABLE (entry), tmp_pos);

      entry->in_drag = 0;
    }
  
  entry->button = 0;
  
  gtk_entry_update_primary_selection (entry);
	      
  return TRUE;
}

static gchar *
_gtk_entry_get_selected_text (GtkEntry *entry)
{
  GtkEditable *editable = GTK_EDITABLE (entry);
  gint         start_text, end_text;
  gchar       *text = NULL;

  if (gtk_editable_get_selection_bounds (editable, &start_text, &end_text))
    text = gtk_editable_get_chars (editable, start_text, end_text);

  return text;
}

static gint
gtk_entry_motion_notify (GtkWidget      *widget,
			 GdkEventMotion *event)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (entry);
  EntryIconInfo *icon_info = NULL;
  GdkDragContext *context;
  gint tmp_pos;
  gint i;

  for (i = 0; i < MAX_ICONS; i++)
    {
      icon_info = priv->icons[i];

      if (!icon_info || icon_info->insensitive)
        continue;

      if (event->window == icon_info->window)
        {
          if (icon_info->pressed &&
              icon_info->target_list != NULL &&
              gtk_drag_check_threshold (widget,
                                        priv->start_x,
                                        priv->start_y,
                                        event->x,
                                        event->y))
            {
              icon_info->in_drag = TRUE;
              icon_info->pressed = FALSE;
              context = gtk_drag_begin (widget,
                                        icon_info->target_list,
                                        icon_info->actions,
                                        1,
                                        (GdkEvent*)event);
            }

          return TRUE;
        }
    }

  if (entry->mouse_cursor_obscured)
    {
      GdkCursor *cursor;
      
      cursor = gdk_cursor_new_for_display (gtk_widget_get_display (widget), GDK_XTERM);
      gdk_window_set_cursor (entry->text_area, cursor);
      gdk_cursor_unref (cursor);
      entry->mouse_cursor_obscured = FALSE;
    }

  if (event->window != entry->text_area || entry->button != 1)
    return FALSE;

  if (entry->select_lines)
    return TRUE;

  gdk_event_request_motions (event);

  if (entry->in_drag)
    {
      if (gtk_entry_get_display_mode (entry) == DISPLAY_NORMAL &&
          gtk_drag_check_threshold (widget,
                                    entry->drag_start_x, entry->drag_start_y,
                                    event->x + entry->scroll_offset, event->y))
        {
          GdkDragContext *context;
          GtkTargetList  *target_list = gtk_target_list_new (NULL, 0);
          guint actions = entry->editable ? GDK_ACTION_COPY | GDK_ACTION_MOVE : GDK_ACTION_COPY;
          gchar *text = NULL;
          GdkPixmap *pixmap = NULL;

          gtk_target_list_add_text_targets (target_list, 0);

          text = _gtk_entry_get_selected_text (entry);
          pixmap = _gtk_text_util_create_drag_icon (widget, text, -1);

          context = gtk_drag_begin (widget, target_list, actions,
                                    entry->button, (GdkEvent *)event);
          
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
          g_free (text);

          entry->in_drag = FALSE;
          entry->button = 0;
	  
          gtk_target_list_unref (target_list);
        }
    }
  else
    {
      gint height;

      height = gdk_window_get_height (entry->text_area);

      if (event->y < 0)
	tmp_pos = 0;
      else if (event->y >= height)
	tmp_pos = gtk_entry_buffer_get_length (get_buffer (entry));
      else
	tmp_pos = gtk_entry_find_position (entry, event->x + entry->scroll_offset);
      
      if (entry->select_words) 
	{
	  gint min, max;
	  gint old_min, old_max;
	  gint pos, bound;
	  
	  min = gtk_entry_move_backward_word (entry, tmp_pos, TRUE);
	  max = gtk_entry_move_forward_word (entry, tmp_pos, TRUE);
	  
	  pos = entry->current_pos;
	  bound = entry->selection_bound;

	  old_min = MIN(entry->current_pos, entry->selection_bound);
	  old_max = MAX(entry->current_pos, entry->selection_bound);
	  
	  if (min < old_min)
	    {
	      pos = min;
	      bound = old_max;
	    }
	  else if (old_max < max) 
	    {
	      pos = max;
	      bound = old_min;
	    }
	  else if (pos == old_min) 
	    {
	      if (entry->current_pos != min)
		pos = max;
	    }
	  else 
	    {
	      if (entry->current_pos != max)
		pos = min;
	    }
	
	  gtk_entry_set_positions (entry, pos, bound);
	}
      else
        gtk_entry_set_positions (entry, tmp_pos, -1);
    }
      
  return TRUE;
}

static void
set_invisible_cursor (GdkWindow *window)
{
  GdkDisplay *display;
  GdkCursor *cursor;

  display = gdk_window_get_display (window);
  cursor = gdk_cursor_new_for_display (display, GDK_BLANK_CURSOR);

  gdk_window_set_cursor (window, cursor);

  gdk_cursor_unref (cursor);
}

static void
gtk_entry_obscure_mouse_cursor (GtkEntry *entry)
{
  if (entry->mouse_cursor_obscured)
    return;

  set_invisible_cursor (entry->text_area);
  
  entry->mouse_cursor_obscured = TRUE;  
}

static gint
gtk_entry_key_press (GtkWidget   *widget,
		     GdkEventKey *event)
{
  GtkEntry *entry = GTK_ENTRY (widget);

  gtk_entry_reset_blink_time (entry);
  gtk_entry_pend_cursor_blink (entry);

  if (entry->editable)
    {
      if (gtk_im_context_filter_keypress (entry->im_context, event))
	{
	  gtk_entry_obscure_mouse_cursor (entry);
	  entry->need_im_reset = TRUE;
	  return TRUE;
	}
    }

  if (event->keyval == GDK_Return || 
      event->keyval == GDK_KP_Enter || 
      event->keyval == GDK_ISO_Enter || 
      event->keyval == GDK_Escape)
    {
      GtkEntryCompletion *completion = gtk_entry_get_completion (entry);
      
      if (completion && completion->priv->completion_timeout)
        {
          g_source_remove (completion->priv->completion_timeout);
          completion->priv->completion_timeout = 0;
        }

      _gtk_entry_reset_im_context (entry);
    }

  if (GTK_WIDGET_CLASS (gtk_entry_parent_class)->key_press_event (widget, event))
    /* Activate key bindings
     */
    return TRUE;

  if (!entry->editable && event->length)
    gtk_widget_error_bell (widget);

  return FALSE;
}

static gint
gtk_entry_key_release (GtkWidget   *widget,
		       GdkEventKey *event)
{
  GtkEntry *entry = GTK_ENTRY (widget);

  if (entry->editable)
    {
      if (gtk_im_context_filter_keypress (entry->im_context, event))
	{
	  entry->need_im_reset = TRUE;
	  return TRUE;
	}
    }

  return GTK_WIDGET_CLASS (gtk_entry_parent_class)->key_release_event (widget, event);
}

static gint
gtk_entry_focus_in (GtkWidget     *widget,
		    GdkEventFocus *event)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GdkKeymap *keymap;

  gtk_widget_queue_draw (widget);

  keymap = gdk_keymap_get_for_display (gtk_widget_get_display (widget));

  if (entry->editable)
    {
      entry->need_im_reset = TRUE;
      gtk_im_context_focus_in (entry->im_context);
      keymap_state_changed (keymap, entry);
      g_signal_connect (keymap, "state-changed", 
                        G_CALLBACK (keymap_state_changed), entry);
    }

  g_signal_connect (keymap, "direction-changed",
		    G_CALLBACK (keymap_direction_changed), entry);

  gtk_entry_reset_blink_time (entry);
  gtk_entry_check_cursor_blink (entry);

  return FALSE;
}

static gint
gtk_entry_focus_out (GtkWidget     *widget,
		     GdkEventFocus *event)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryCompletion *completion;
  GdkKeymap *keymap;

  gtk_widget_queue_draw (widget);

  keymap = gdk_keymap_get_for_display (gtk_widget_get_display (widget));

  if (entry->editable)
    {
      entry->need_im_reset = TRUE;
      gtk_im_context_focus_out (entry->im_context);
      remove_capslock_feedback (entry);
    }

  gtk_entry_check_cursor_blink (entry);
  
  g_signal_handlers_disconnect_by_func (keymap, keymap_state_changed, entry);
  g_signal_handlers_disconnect_by_func (keymap, keymap_direction_changed, entry);

  completion = gtk_entry_get_completion (entry);
  if (completion)
    _gtk_entry_completion_popdown (completion);

  return FALSE;
}

static void
gtk_entry_grab_focus (GtkWidget *widget)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  gboolean select_on_focus;

  GTK_WIDGET_CLASS (gtk_entry_parent_class)->grab_focus (widget);

  if (entry->editable && !entry->in_click)
    {
      g_object_get (gtk_widget_get_settings (widget),
                    "gtk-entry-select-on-focus",
                    &select_on_focus,
                    NULL);

      if (select_on_focus)
        gtk_editable_select_region (GTK_EDITABLE (widget), 0, -1);
    }
}

static void
gtk_entry_direction_changed (GtkWidget        *widget,
                             GtkTextDirection  previous_dir)
{
  GtkEntry *entry = GTK_ENTRY (widget);

  gtk_entry_recompute (entry);

  GTK_WIDGET_CLASS (gtk_entry_parent_class)->direction_changed (widget, previous_dir);
}

static void
gtk_entry_state_changed (GtkWidget      *widget,
			 GtkStateType    previous_state)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (widget);
  GdkCursor *cursor;
  gint i;

  if (gtk_widget_get_realized (widget))
    {
      gdk_window_set_background (widget->window, &widget->style->base[gtk_widget_get_state (widget)]);
      gdk_window_set_background (entry->text_area, &widget->style->base[gtk_widget_get_state (widget)]);
      for (i = 0; i < MAX_ICONS; i++)
        {
          EntryIconInfo *icon_info = priv->icons[i];
          if (icon_info && icon_info->window)
            gdk_window_set_background (icon_info->window, &widget->style->base[gtk_widget_get_state (widget)]);
        }

      if (gtk_widget_is_sensitive (widget))
        cursor = gdk_cursor_new_for_display (gtk_widget_get_display (widget), GDK_XTERM);
      else
        cursor = NULL;

      gdk_window_set_cursor (entry->text_area, cursor);

      if (cursor)
        gdk_cursor_unref (cursor);

      entry->mouse_cursor_obscured = FALSE;

      update_cursors (widget);
    }

  if (!gtk_widget_is_sensitive (widget))
    {
      /* Clear any selection */
      gtk_editable_select_region (GTK_EDITABLE (entry), entry->current_pos, entry->current_pos);
    }

  gtk_widget_queue_draw (widget);
}

static void
gtk_entry_screen_changed (GtkWidget *widget,
			  GdkScreen *old_screen)
{
  gtk_entry_recompute (GTK_ENTRY (widget));
}

/* GtkEditable method implementations
 */
static void
gtk_entry_insert_text (GtkEditable *editable,
		       const gchar *new_text,
		       gint         new_text_length,
		       gint        *position)
{
  g_object_ref (editable);

  /*
   * The incoming text may a password or other secret. We make sure
   * not to copy it into temporary buffers.
   */

  g_signal_emit_by_name (editable, "insert-text", new_text, new_text_length, position);

  g_object_unref (editable);
}

static void
gtk_entry_delete_text (GtkEditable *editable,
		       gint         start_pos,
		       gint         end_pos)
{
  g_object_ref (editable);

  g_signal_emit_by_name (editable, "delete-text", start_pos, end_pos);

  g_object_unref (editable);
}

static gchar *    
gtk_entry_get_chars      (GtkEditable   *editable,
			  gint           start_pos,
			  gint           end_pos)
{
  GtkEntry *entry = GTK_ENTRY (editable);
  const gchar *text;
  gint text_length;
  gint start_index, end_index;

  text = gtk_entry_buffer_get_text (get_buffer (entry));
  text_length = gtk_entry_buffer_get_length (get_buffer (entry));

  if (end_pos < 0)
    end_pos = text_length;

  start_pos = MIN (text_length, start_pos);
  end_pos = MIN (text_length, end_pos);

  start_index = g_utf8_offset_to_pointer (text, start_pos) - entry->text;
  end_index = g_utf8_offset_to_pointer (text, end_pos) - entry->text;

  return g_strndup (text + start_index, end_index - start_index);
}

static void
gtk_entry_real_set_position (GtkEditable *editable,
			     gint         position)
{
  GtkEntry *entry = GTK_ENTRY (editable);

  guint length;

  length = gtk_entry_buffer_get_length (get_buffer (entry));
  if (position < 0 || position > length)
    position = length;

  if (position != entry->current_pos ||
      position != entry->selection_bound)
    {
      _gtk_entry_reset_im_context (entry);
      gtk_entry_set_positions (entry, position, position);
    }
}

static gint
gtk_entry_get_position (GtkEditable *editable)
{
  return GTK_ENTRY (editable)->current_pos;
}

static void
gtk_entry_set_selection_bounds (GtkEditable *editable,
				gint         start,
				gint         end)
{
  GtkEntry *entry = GTK_ENTRY (editable);
  guint length;

  length = gtk_entry_buffer_get_length (get_buffer (entry));
  if (start < 0)
    start = length;
  if (end < 0)
    end = length;
  
  _gtk_entry_reset_im_context (entry);

  gtk_entry_set_positions (entry,
			   MIN (end, length),
			   MIN (start, length));

  gtk_entry_update_primary_selection (entry);
}

static gboolean
gtk_entry_get_selection_bounds (GtkEditable *editable,
				gint        *start,
				gint        *end)
{
  GtkEntry *entry = GTK_ENTRY (editable);

  *start = entry->selection_bound;
  *end = entry->current_pos;

  return (entry->selection_bound != entry->current_pos);
}

static void
icon_theme_changed (GtkEntry *entry)
{
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (entry);
  gint i;

  for (i = 0; i < MAX_ICONS; i++)
    {
      EntryIconInfo *icon_info = priv->icons[i];
      if (icon_info != NULL) 
        {
          if (icon_info->storage_type == GTK_IMAGE_ICON_NAME)
            gtk_entry_set_icon_from_icon_name (entry, i, icon_info->icon_name);
          else if (icon_info->storage_type == GTK_IMAGE_STOCK)
            gtk_entry_set_icon_from_stock (entry, i, icon_info->stock_id);
          else if (icon_info->storage_type == GTK_IMAGE_GICON)
            gtk_entry_set_icon_from_gicon (entry, i, icon_info->gicon);
        }
    }

  gtk_widget_queue_draw (GTK_WIDGET (entry));
}

static void
icon_margin_changed (GtkEntry *entry)
{
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (entry);
  GtkBorder border;

  _gtk_entry_effective_inner_border (GTK_ENTRY (entry), &border);

  priv->icon_margin = border.left;
}

static void 
gtk_entry_style_set (GtkWidget *widget,
                     GtkStyle  *previous_style)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (entry);
  gint focus_width;
  gboolean interior_focus;
  gint i;

  gtk_widget_style_get (widget,
			"focus-line-width", &focus_width,
			"interior-focus", &interior_focus,
			NULL);

  priv->focus_width = focus_width;
  priv->interior_focus = interior_focus;

  if (!priv->invisible_char_set)
    entry->invisible_char = find_invisible_char (GTK_WIDGET (entry));

  gtk_entry_recompute (entry);

  if (previous_style && gtk_widget_get_realized (widget))
    {
      gdk_window_set_background (widget->window, &widget->style->base[gtk_widget_get_state (widget)]);
      gdk_window_set_background (entry->text_area, &widget->style->base[gtk_widget_get_state (widget)]);
      for (i = 0; i < MAX_ICONS; i++) 
        {
          EntryIconInfo *icon_info = priv->icons[i];
          if (icon_info && icon_info->window)
            gdk_window_set_background (icon_info->window, &widget->style->base[gtk_widget_get_state (widget)]);
        }
    }

  icon_theme_changed (entry);
  icon_margin_changed (entry);
}

/* GtkCellEditable method implementations
 */
static void
gtk_cell_editable_entry_activated (GtkEntry *entry, gpointer data)
{
  gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (entry));
  gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (entry));
}

static gboolean
gtk_cell_editable_key_press_event (GtkEntry    *entry,
				   GdkEventKey *key_event,
				   gpointer     data)
{
  if (key_event->keyval == GDK_Escape)
    {
      entry->editing_canceled = TRUE;
      gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (entry));
      gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (entry));

      return TRUE;
    }

  /* override focus */
  if (key_event->keyval == GDK_Up || key_event->keyval == GDK_Down)
    {
      gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (entry));
      gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (entry));

      return TRUE;
    }

  return FALSE;
}

static void
gtk_entry_start_editing (GtkCellEditable *cell_editable,
			 GdkEvent        *event)
{
  GTK_ENTRY (cell_editable)->is_cell_renderer = TRUE;

  g_signal_connect (cell_editable, "activate",
		    G_CALLBACK (gtk_cell_editable_entry_activated), NULL);
  g_signal_connect (cell_editable, "key-press-event",
		    G_CALLBACK (gtk_cell_editable_key_press_event), NULL);
}

static void
gtk_entry_password_hint_free (GtkEntryPasswordHint *password_hint)
{
  if (password_hint->source_id)
    g_source_remove (password_hint->source_id);

  g_slice_free (GtkEntryPasswordHint, password_hint);
}


static gboolean
gtk_entry_remove_password_hint (gpointer data)
{
  GtkEntryPasswordHint *password_hint = g_object_get_qdata (data, quark_password_hint);
  password_hint->position = -1;

  /* Force the string to be redrawn, but now without a visible character */
  gtk_entry_recompute (GTK_ENTRY (data));
  return FALSE;
}

/* Default signal handlers
 */
static void
gtk_entry_real_insert_text (GtkEditable *editable,
			    const gchar *new_text,
			    gint         new_text_length,
			    gint        *position)
{
  guint n_inserted;
  gint n_chars;

  n_chars = g_utf8_strlen (new_text, new_text_length);

  /*
   * The actual insertion into the buffer. This will end up firing the
   * following signal handlers: buffer_inserted_text(), buffer_notify_display_text(),
   * buffer_notify_text(), buffer_notify_length()
   */
  begin_change (GTK_ENTRY (editable));

  n_inserted = gtk_entry_buffer_insert_text (get_buffer (GTK_ENTRY (editable)), *position, new_text, n_chars);

  end_change (GTK_ENTRY (editable));

  if (n_inserted != n_chars)
      gtk_widget_error_bell (GTK_WIDGET (editable));

  *position += n_inserted;
}

static void
gtk_entry_real_delete_text (GtkEditable *editable,
                            gint         start_pos,
                            gint         end_pos)
{
  /*
   * The actual deletion from the buffer. This will end up firing the
   * following signal handlers: buffer_deleted_text(), buffer_notify_display_text(),
   * buffer_notify_text(), buffer_notify_length()
   */

  begin_change (GTK_ENTRY (editable));

  gtk_entry_buffer_delete_text (get_buffer (GTK_ENTRY (editable)), start_pos, end_pos - start_pos);

  end_change (GTK_ENTRY (editable));
}

/* GtkEntryBuffer signal handlers
 */
static void
buffer_inserted_text (GtkEntryBuffer *buffer,
                      guint           position,
                      const gchar    *chars,
                      guint           n_chars,
                      GtkEntry       *entry)
{
  guint password_hint_timeout;
  guint current_pos;
  gint selection_bound;

  current_pos = entry->current_pos;
  if (current_pos > position)
    current_pos += n_chars;

  selection_bound = entry->selection_bound;
  if (selection_bound > position)
    selection_bound += n_chars;

  gtk_entry_set_positions (entry, current_pos, selection_bound);

  /* Calculate the password hint if it needs to be displayed. */
  if (n_chars == 1 && !entry->visible)
    {
      g_object_get (gtk_widget_get_settings (GTK_WIDGET (entry)),
                    "gtk-entry-password-hint-timeout", &password_hint_timeout,
                    NULL);

      if (password_hint_timeout > 0)
        {
          GtkEntryPasswordHint *password_hint = g_object_get_qdata (G_OBJECT (entry),
                                                                    quark_password_hint);
          if (!password_hint)
            {
              password_hint = g_slice_new0 (GtkEntryPasswordHint);
              g_object_set_qdata_full (G_OBJECT (entry), quark_password_hint, password_hint,
                                       (GDestroyNotify)gtk_entry_password_hint_free);
            }

          password_hint->position = position;
          if (password_hint->source_id)
            g_source_remove (password_hint->source_id);
          password_hint->source_id = gdk_threads_add_timeout (password_hint_timeout,
                                                              (GSourceFunc)gtk_entry_remove_password_hint, entry);
        }
    }
}

static void
buffer_deleted_text (GtkEntryBuffer *buffer,
                     guint           position,
                     guint           n_chars,
                     GtkEntry       *entry)
{
  guint end_pos = position + n_chars;
  gint selection_bound;
  guint current_pos;

  current_pos = entry->current_pos;
  if (current_pos > position)
    current_pos -= MIN (current_pos, end_pos) - position;

  selection_bound = entry->selection_bound;
  if (selection_bound > position)
    selection_bound -= MIN (selection_bound, end_pos) - position;

  gtk_entry_set_positions (entry, current_pos, selection_bound);

  /* We might have deleted the selection */
  gtk_entry_update_primary_selection (entry);

  /* Disable the password hint if one exists. */
  if (!entry->visible)
    {
      GtkEntryPasswordHint *password_hint = g_object_get_qdata (G_OBJECT (entry),
                                                                quark_password_hint);
      if (password_hint)
        {
          if (password_hint->source_id)
            g_source_remove (password_hint->source_id);
          password_hint->source_id = 0;
          password_hint->position = -1;
        }
    }
}

static void
buffer_notify_text (GtkEntryBuffer *buffer,
                    GParamSpec     *spec,
                    GtkEntry       *entry)
{
  /* COMPAT: Deprecated, not used. This struct field will be removed in GTK+ 3.x */
  entry->text = (gchar*)gtk_entry_buffer_get_text (buffer);

  gtk_entry_recompute (entry);
  emit_changed (entry);
  g_object_notify (G_OBJECT (entry), "text");
}

static void
buffer_notify_length (GtkEntryBuffer *buffer,
                      GParamSpec     *spec,
                      GtkEntry       *entry)
{
  /* COMPAT: Deprecated, not used. This struct field will be removed in GTK+ 3.x */
  entry->text_length = gtk_entry_buffer_get_length (buffer);

  g_object_notify (G_OBJECT (entry), "text-length");
}

static void
buffer_notify_max_length (GtkEntryBuffer *buffer,
                          GParamSpec     *spec,
                          GtkEntry       *entry)
{
  /* COMPAT: Deprecated, not used. This struct field will be removed in GTK+ 3.x */
  entry->text_max_length = gtk_entry_buffer_get_max_length (buffer);

  g_object_notify (G_OBJECT (entry), "max-length");
}

static void
buffer_connect_signals (GtkEntry *entry)
{
  g_signal_connect (get_buffer (entry), "inserted-text", G_CALLBACK (buffer_inserted_text), entry);
  g_signal_connect (get_buffer (entry), "deleted-text", G_CALLBACK (buffer_deleted_text), entry);
  g_signal_connect (get_buffer (entry), "notify::text", G_CALLBACK (buffer_notify_text), entry);
  g_signal_connect (get_buffer (entry), "notify::length", G_CALLBACK (buffer_notify_length), entry);
  g_signal_connect (get_buffer (entry), "notify::max-length", G_CALLBACK (buffer_notify_max_length), entry);
}

static void
buffer_disconnect_signals (GtkEntry *entry)
{
  g_signal_handlers_disconnect_by_func (get_buffer (entry), buffer_inserted_text, entry);
  g_signal_handlers_disconnect_by_func (get_buffer (entry), buffer_deleted_text, entry);
  g_signal_handlers_disconnect_by_func (get_buffer (entry), buffer_notify_text, entry);
  g_signal_handlers_disconnect_by_func (get_buffer (entry), buffer_notify_length, entry);
  g_signal_handlers_disconnect_by_func (get_buffer (entry), buffer_notify_max_length, entry);
}

/* Compute the X position for an offset that corresponds to the "more important
 * cursor position for that offset. We use this when trying to guess to which
 * end of the selection we should go to when the user hits the left or
 * right arrow key.
 */
static gint
get_better_cursor_x (GtkEntry *entry,
		     gint      offset)
{
  GdkKeymap *keymap = gdk_keymap_get_for_display (gtk_widget_get_display (GTK_WIDGET (entry)));
  PangoDirection keymap_direction = gdk_keymap_get_direction (keymap);
  gboolean split_cursor;
  
  PangoLayout *layout = gtk_entry_ensure_layout (entry, TRUE);
  const gchar *text = pango_layout_get_text (layout);
  gint index = g_utf8_offset_to_pointer (text, offset) - text;
  
  PangoRectangle strong_pos, weak_pos;
  
  g_object_get (gtk_widget_get_settings (GTK_WIDGET (entry)),
		"gtk-split-cursor", &split_cursor,
		NULL);

  pango_layout_get_cursor_pos (layout, index, &strong_pos, &weak_pos);

  if (split_cursor)
    return strong_pos.x / PANGO_SCALE;
  else
    return (keymap_direction == entry->resolved_dir) ? strong_pos.x / PANGO_SCALE : weak_pos.x / PANGO_SCALE;
}

static void
gtk_entry_move_cursor (GtkEntry       *entry,
		       GtkMovementStep step,
		       gint            count,
		       gboolean        extend_selection)
{
  gint new_pos = entry->current_pos;
  GtkEntryPrivate *priv;

  _gtk_entry_reset_im_context (entry);

  if (entry->current_pos != entry->selection_bound && !extend_selection)
    {
      /* If we have a current selection and aren't extending it, move to the
       * start/or end of the selection as appropriate
       */
      switch (step)
	{
	case GTK_MOVEMENT_VISUAL_POSITIONS:
	  {
	    gint current_x = get_better_cursor_x (entry, entry->current_pos);
	    gint bound_x = get_better_cursor_x (entry, entry->selection_bound);

	    if (count <= 0)
	      new_pos = current_x < bound_x ? entry->current_pos : entry->selection_bound;
	    else 
	      new_pos = current_x > bound_x ? entry->current_pos : entry->selection_bound;
	  }
	  break;

	case GTK_MOVEMENT_WORDS:
          if (entry->resolved_dir == PANGO_DIRECTION_RTL)
            count *= -1;
          /* Fall through */

	case GTK_MOVEMENT_LOGICAL_POSITIONS:
	  if (count < 0)
	    new_pos = MIN (entry->current_pos, entry->selection_bound);
	  else
	    new_pos = MAX (entry->current_pos, entry->selection_bound);

	  break;

	case GTK_MOVEMENT_DISPLAY_LINE_ENDS:
	case GTK_MOVEMENT_PARAGRAPH_ENDS:
	case GTK_MOVEMENT_BUFFER_ENDS:
	  priv = GTK_ENTRY_GET_PRIVATE (entry);
	  new_pos = count < 0 ? 0 : gtk_entry_buffer_get_length (get_buffer (entry));
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
	  new_pos = gtk_entry_move_logically (entry, new_pos, count);
	  break;

	case GTK_MOVEMENT_VISUAL_POSITIONS:
	  new_pos = gtk_entry_move_visually (entry, new_pos, count);

          if (entry->current_pos == new_pos)
            {
              if (!extend_selection)
                {
                  if (!gtk_widget_keynav_failed (GTK_WIDGET (entry),
                                                 count > 0 ?
                                                 GTK_DIR_RIGHT : GTK_DIR_LEFT))
                    {
                      GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (entry));

                      if (toplevel)
                        gtk_widget_child_focus (toplevel,
                                                count > 0 ?
                                                GTK_DIR_RIGHT : GTK_DIR_LEFT);
                    }
                }
              else
                {
                  gtk_widget_error_bell (GTK_WIDGET (entry));
                }
            }
	  break;

	case GTK_MOVEMENT_WORDS:
          if (entry->resolved_dir == PANGO_DIRECTION_RTL)
            count *= -1;

	  while (count > 0)
	    {
	      new_pos = gtk_entry_move_forward_word (entry, new_pos, FALSE);
	      count--;
	    }

	  while (count < 0)
	    {
	      new_pos = gtk_entry_move_backward_word (entry, new_pos, FALSE);
	      count++;
	    }
          if (entry->current_pos == new_pos)
            gtk_widget_error_bell (GTK_WIDGET (entry));

	  break;

	case GTK_MOVEMENT_DISPLAY_LINE_ENDS:
	case GTK_MOVEMENT_PARAGRAPH_ENDS:
	case GTK_MOVEMENT_BUFFER_ENDS:
	  priv = GTK_ENTRY_GET_PRIVATE (entry);
	  new_pos = count < 0 ? 0 : gtk_entry_buffer_get_length (get_buffer (entry));
          if (entry->current_pos == new_pos)
            gtk_widget_error_bell (GTK_WIDGET (entry));

	  break;

	case GTK_MOVEMENT_DISPLAY_LINES:
	case GTK_MOVEMENT_PARAGRAPHS:
	case GTK_MOVEMENT_PAGES:
	case GTK_MOVEMENT_HORIZONTAL_PAGES:
	  break;
	}
    }

  if (extend_selection)
    gtk_editable_select_region (GTK_EDITABLE (entry), entry->selection_bound, new_pos);
  else
    gtk_editable_set_position (GTK_EDITABLE (entry), new_pos);
  
  gtk_entry_pend_cursor_blink (entry);
}

static void
gtk_entry_insert_at_cursor (GtkEntry    *entry,
			    const gchar *str)
{
  GtkEditable *editable = GTK_EDITABLE (entry);
  gint pos = entry->current_pos;

  if (entry->editable)
    {
      _gtk_entry_reset_im_context (entry);

      gtk_editable_insert_text (editable, str, -1, &pos);
      gtk_editable_set_position (editable, pos);
    }
}

static void
gtk_entry_delete_from_cursor (GtkEntry       *entry,
			      GtkDeleteType   type,
			      gint            count)
{
  GtkEditable *editable = GTK_EDITABLE (entry);
  gint start_pos = entry->current_pos;
  gint end_pos = entry->current_pos;
  gint old_n_bytes = gtk_entry_buffer_get_bytes (get_buffer (entry));
  
  _gtk_entry_reset_im_context (entry);

  if (!entry->editable)
    {
      gtk_widget_error_bell (GTK_WIDGET (entry));
      return;
    }

  if (entry->selection_bound != entry->current_pos)
    {
      gtk_editable_delete_selection (editable);
      return;
    }
  
  switch (type)
    {
    case GTK_DELETE_CHARS:
      end_pos = gtk_entry_move_logically (entry, entry->current_pos, count);
      gtk_editable_delete_text (editable, MIN (start_pos, end_pos), MAX (start_pos, end_pos));
      break;

    case GTK_DELETE_WORDS:
      if (count < 0)
	{
	  /* Move to end of current word, or if not on a word, end of previous word */
	  end_pos = gtk_entry_move_backward_word (entry, end_pos, FALSE);
	  end_pos = gtk_entry_move_forward_word (entry, end_pos, FALSE);
	}
      else if (count > 0)
	{
	  /* Move to beginning of current word, or if not on a word, begining of next word */
	  start_pos = gtk_entry_move_forward_word (entry, start_pos, FALSE);
	  start_pos = gtk_entry_move_backward_word (entry, start_pos, FALSE);
	}
	
      /* Fall through */
    case GTK_DELETE_WORD_ENDS:
      while (count < 0)
	{
	  start_pos = gtk_entry_move_backward_word (entry, start_pos, FALSE);
	  count++;
	}

      while (count > 0)
	{
	  end_pos = gtk_entry_move_forward_word (entry, end_pos, FALSE);
	  count--;
	}

      gtk_editable_delete_text (editable, start_pos, end_pos);
      break;

    case GTK_DELETE_DISPLAY_LINE_ENDS:
    case GTK_DELETE_PARAGRAPH_ENDS:
      if (count < 0)
	gtk_editable_delete_text (editable, 0, entry->current_pos);
      else
	gtk_editable_delete_text (editable, entry->current_pos, -1);

      break;

    case GTK_DELETE_DISPLAY_LINES:
    case GTK_DELETE_PARAGRAPHS:
      gtk_editable_delete_text (editable, 0, -1);  
      break;

    case GTK_DELETE_WHITESPACE:
      gtk_entry_delete_whitespace (entry);
      break;
    }

  if (gtk_entry_buffer_get_bytes (get_buffer (entry)) == old_n_bytes)
    gtk_widget_error_bell (GTK_WIDGET (entry));

  gtk_entry_pend_cursor_blink (entry);
}

static void
gtk_entry_backspace (GtkEntry *entry)
{
  GtkEditable *editable = GTK_EDITABLE (entry);
  gint prev_pos;

  _gtk_entry_reset_im_context (entry);

  if (!entry->editable)
    {
      gtk_widget_error_bell (GTK_WIDGET (entry));
      return;
    }

  if (entry->selection_bound != entry->current_pos)
    {
      gtk_editable_delete_selection (editable);
      return;
    }

  prev_pos = gtk_entry_move_logically (entry, entry->current_pos, -1);

  if (prev_pos < entry->current_pos)
    {
      PangoLayout *layout = gtk_entry_ensure_layout (entry, FALSE);
      PangoLogAttr *log_attrs;
      gint n_attrs;

      pango_layout_get_log_attrs (layout, &log_attrs, &n_attrs);

      /* Deleting parts of characters */
      if (log_attrs[entry->current_pos].backspace_deletes_character)
	{
	  gchar *cluster_text;
	  gchar *normalized_text;
          glong  len;

	  cluster_text = gtk_entry_get_display_text (entry, prev_pos,
	                                             entry->current_pos);
	  normalized_text = g_utf8_normalize (cluster_text,
			  		      strlen (cluster_text),
					      G_NORMALIZE_NFD);
          len = g_utf8_strlen (normalized_text, -1);

          gtk_editable_delete_text (editable, prev_pos, entry->current_pos);
	  if (len > 1)
	    {
	      gint pos = entry->current_pos;

	      gtk_editable_insert_text (editable, normalized_text,
					g_utf8_offset_to_pointer (normalized_text, len - 1) - normalized_text,
					&pos);
	      gtk_editable_set_position (editable, pos);
	    }

	  g_free (normalized_text);
	  g_free (cluster_text);
	}
      else
	{
          gtk_editable_delete_text (editable, prev_pos, entry->current_pos);
	}
      
      g_free (log_attrs);
    }
  else
    {
      gtk_widget_error_bell (GTK_WIDGET (entry));
    }

  gtk_entry_pend_cursor_blink (entry);
}

static void
gtk_entry_copy_clipboard (GtkEntry *entry)
{
  GtkEditable *editable = GTK_EDITABLE (entry);
  gint start, end;
  gchar *str;

  if (gtk_editable_get_selection_bounds (editable, &start, &end))
    {
      if (!entry->visible)
        {
          gtk_widget_error_bell (GTK_WIDGET (entry));
          return;
        }

      str = gtk_entry_get_display_text (entry, start, end);
      gtk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (entry),
							GDK_SELECTION_CLIPBOARD),
			      str, -1);
      g_free (str);
    }
}

static void
gtk_entry_cut_clipboard (GtkEntry *entry)
{
  GtkEditable *editable = GTK_EDITABLE (entry);
  gint start, end;

  if (!entry->visible)
    {
      gtk_widget_error_bell (GTK_WIDGET (entry));
      return;
    }

  gtk_entry_copy_clipboard (entry);

  if (entry->editable)
    {
      if (gtk_editable_get_selection_bounds (editable, &start, &end))
	gtk_editable_delete_text (editable, start, end);
    }
  else
    {
      gtk_widget_error_bell (GTK_WIDGET (entry));
    }
}

static void
gtk_entry_paste_clipboard (GtkEntry *entry)
{
  if (entry->editable)
    gtk_entry_paste (entry, GDK_SELECTION_CLIPBOARD);
  else
    gtk_widget_error_bell (GTK_WIDGET (entry));
}

static void
gtk_entry_delete_cb (GtkEntry *entry)
{
  GtkEditable *editable = GTK_EDITABLE (entry);
  gint start, end;

  if (entry->editable)
    {
      if (gtk_editable_get_selection_bounds (editable, &start, &end))
	gtk_editable_delete_text (editable, start, end);
    }
}

static void
gtk_entry_toggle_overwrite (GtkEntry *entry)
{
  entry->overwrite_mode = !entry->overwrite_mode;
  gtk_entry_pend_cursor_blink (entry);
  gtk_widget_queue_draw (GTK_WIDGET (entry));
}

static void
gtk_entry_select_all (GtkEntry *entry)
{
  gtk_entry_select_line (entry);
}

static void
gtk_entry_real_activate (GtkEntry *entry)
{
  GtkWindow *window;
  GtkWidget *toplevel;
  GtkWidget *widget;

  widget = GTK_WIDGET (entry);

  if (entry->activates_default)
    {
      toplevel = gtk_widget_get_toplevel (widget);
      if (GTK_IS_WINDOW (toplevel))
	{
	  window = GTK_WINDOW (toplevel);
      
	  if (window &&
	      widget != window->default_widget &&
	      !(widget == window->focus_widget &&
		(!window->default_widget || !gtk_widget_get_sensitive (window->default_widget))))
	    gtk_window_activate_default (window);
	}
    }
}

static void
keymap_direction_changed (GdkKeymap *keymap,
                          GtkEntry  *entry)
{
  gtk_entry_recompute (entry);
}

/* IM Context Callbacks
 */

static void
gtk_entry_commit_cb (GtkIMContext *context,
		     const gchar  *str,
		     GtkEntry     *entry)
{
  if (entry->editable)
    gtk_entry_enter_text (entry, str);
}

static void 
gtk_entry_preedit_changed_cb (GtkIMContext *context,
			      GtkEntry     *entry)
{
  if (entry->editable)
    {
      gchar *preedit_string;
      gint cursor_pos;
      
      gtk_im_context_get_preedit_string (entry->im_context,
                                         &preedit_string, NULL,
                                         &cursor_pos);
      g_signal_emit (entry, signals[PREEDIT_CHANGED], 0, preedit_string);
      entry->preedit_length = strlen (preedit_string);
      cursor_pos = CLAMP (cursor_pos, 0, g_utf8_strlen (preedit_string, -1));
      entry->preedit_cursor = cursor_pos;
      g_free (preedit_string);
    
      gtk_entry_recompute (entry);
    }
}

static gboolean
gtk_entry_retrieve_surrounding_cb (GtkIMContext *context,
			       GtkEntry         *entry)
{
  gchar *text;

  /* XXXX ??? does this even make sense when text is not visible? Should we return FALSE? */
  text = gtk_entry_get_display_text (entry, 0, -1);
  gtk_im_context_set_surrounding (context, text, strlen (text), /* Length in bytes */
				  g_utf8_offset_to_pointer (text, entry->current_pos) - text);
  g_free (text);

  return TRUE;
}

static gboolean
gtk_entry_delete_surrounding_cb (GtkIMContext *slave,
				 gint          offset,
				 gint          n_chars,
				 GtkEntry     *entry)
{
  if (entry->editable)
    gtk_editable_delete_text (GTK_EDITABLE (entry),
                              entry->current_pos + offset,
                              entry->current_pos + offset + n_chars);

  return TRUE;
}

/* Internal functions
 */

/* Used for im_commit_cb and inserting Unicode chars */
static void
gtk_entry_enter_text (GtkEntry       *entry,
                      const gchar    *str)
{
  GtkEditable *editable = GTK_EDITABLE (entry);
  gint tmp_pos;
  gboolean old_need_im_reset;
  guint text_length;

  old_need_im_reset = entry->need_im_reset;
  entry->need_im_reset = FALSE;

  if (gtk_editable_get_selection_bounds (editable, NULL, NULL))
    gtk_editable_delete_selection (editable);
  else
    {
      if (entry->overwrite_mode)
        {
          text_length = gtk_entry_buffer_get_length (get_buffer (entry));
          if (entry->current_pos < text_length)
            gtk_entry_delete_from_cursor (entry, GTK_DELETE_CHARS, 1);
        }
    }

  tmp_pos = entry->current_pos;
  gtk_editable_insert_text (editable, str, strlen (str), &tmp_pos);
  gtk_editable_set_position (editable, tmp_pos);

  entry->need_im_reset = old_need_im_reset;
}

/* All changes to entry->current_pos and entry->selection_bound
 * should go through this function.
 */
static void
gtk_entry_set_positions (GtkEntry *entry,
			 gint      current_pos,
			 gint      selection_bound)
{
  gboolean changed = FALSE;

  g_object_freeze_notify (G_OBJECT (entry));
  
  if (current_pos != -1 &&
      entry->current_pos != current_pos)
    {
      entry->current_pos = current_pos;
      changed = TRUE;

      g_object_notify (G_OBJECT (entry), "cursor-position");
    }

  if (selection_bound != -1 &&
      entry->selection_bound != selection_bound)
    {
      entry->selection_bound = selection_bound;
      changed = TRUE;
      
      g_object_notify (G_OBJECT (entry), "selection-bound");
    }

  g_object_thaw_notify (G_OBJECT (entry));

  if (changed) 
    {
      gtk_entry_move_adjustments (entry);
      gtk_entry_recompute (entry);
    }
}

static void
gtk_entry_reset_layout (GtkEntry *entry)
{
  if (entry->cached_layout)
    {
      g_object_unref (entry->cached_layout);
      entry->cached_layout = NULL;
    }
}

static void
update_im_cursor_location (GtkEntry *entry)
{
  GdkRectangle area;
  gint strong_x;
  gint strong_xoffset;
  gint area_width, area_height;

  gtk_entry_get_cursor_locations (entry, CURSOR_STANDARD, &strong_x, NULL);
  gtk_entry_get_text_area_size (entry, NULL, NULL, &area_width, &area_height);

  strong_xoffset = strong_x - entry->scroll_offset;
  if (strong_xoffset < 0)
    {
      strong_xoffset = 0;
    }
  else if (strong_xoffset > area_width)
    {
      strong_xoffset = area_width;
    }
  area.x = strong_xoffset;
  area.y = 0;
  area.width = 0;
  area.height = area_height;

  gtk_im_context_set_cursor_location (entry->im_context, &area);
}

static gboolean
recompute_idle_func (gpointer data)
{
  GtkEntry *entry;

  entry = GTK_ENTRY (data);

  entry->recompute_idle = 0;
  
  if (gtk_widget_has_screen (GTK_WIDGET (entry)))
    {
      gtk_entry_adjust_scroll (entry);
      gtk_entry_queue_draw (entry);
      
      update_im_cursor_location (entry);
    }

  return FALSE;
}

static void
gtk_entry_recompute (GtkEntry *entry)
{
  gtk_entry_reset_layout (entry);
  gtk_entry_check_cursor_blink (entry);
  
  if (!entry->recompute_idle)
    {
      entry->recompute_idle = gdk_threads_add_idle_full (G_PRIORITY_HIGH_IDLE + 15, /* between resize and redraw */
					       recompute_idle_func, entry, NULL); 
    }
}

static PangoLayout *
gtk_entry_create_layout (GtkEntry *entry,
			 gboolean  include_preedit)
{
  GtkWidget *widget = GTK_WIDGET (entry);
  PangoLayout *layout = gtk_widget_create_pango_layout (widget, NULL);
  PangoAttrList *tmp_attrs = pango_attr_list_new ();

  gchar *preedit_string = NULL;
  gint preedit_length = 0;
  PangoAttrList *preedit_attrs = NULL;

  gchar *display;
  guint n_bytes;

  pango_layout_set_single_paragraph_mode (layout, TRUE);

  display = gtk_entry_get_display_text (entry, 0, -1);
  n_bytes = strlen (display);

  if (include_preedit)
    {
      gtk_im_context_get_preedit_string (entry->im_context,
					 &preedit_string, &preedit_attrs, NULL);
      preedit_length = entry->preedit_length;
    }

  if (preedit_length)
    {
      GString *tmp_string = g_string_new (display);
      gint cursor_index = g_utf8_offset_to_pointer (display, entry->current_pos) - display;
      
      g_string_insert (tmp_string, cursor_index, preedit_string);
      
      pango_layout_set_text (layout, tmp_string->str, tmp_string->len);
      
      pango_attr_list_splice (tmp_attrs, preedit_attrs,
			      cursor_index, preedit_length);
      
      g_string_free (tmp_string, TRUE);
    }
  else
    {
      PangoDirection pango_dir;
      
      if (gtk_entry_get_display_mode (entry) == DISPLAY_NORMAL)
	pango_dir = pango_find_base_dir (display, n_bytes);
      else
	pango_dir = PANGO_DIRECTION_NEUTRAL;

      if (pango_dir == PANGO_DIRECTION_NEUTRAL)
        {
          if (gtk_widget_has_focus (widget))
	    {
	      GdkDisplay *display = gtk_widget_get_display (widget);
	      GdkKeymap *keymap = gdk_keymap_get_for_display (display);
	      if (gdk_keymap_get_direction (keymap) == PANGO_DIRECTION_RTL)
		pango_dir = PANGO_DIRECTION_RTL;
	      else
		pango_dir = PANGO_DIRECTION_LTR;
	    }
          else
	    {
	      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
		pango_dir = PANGO_DIRECTION_RTL;
	      else
		pango_dir = PANGO_DIRECTION_LTR;
	    }
        }

      pango_context_set_base_dir (gtk_widget_get_pango_context (widget),
				  pango_dir);

      entry->resolved_dir = pango_dir;

      pango_layout_set_text (layout, display, n_bytes);
    }
      
  pango_layout_set_attributes (layout, tmp_attrs);

  g_free (preedit_string);
  g_free (display);

  if (preedit_attrs)
    pango_attr_list_unref (preedit_attrs);
      
  pango_attr_list_unref (tmp_attrs);

  return layout;
}

static PangoLayout *
gtk_entry_ensure_layout (GtkEntry *entry,
                         gboolean  include_preedit)
{
  if (entry->preedit_length > 0 &&
      !include_preedit != !entry->cache_includes_preedit)
    gtk_entry_reset_layout (entry);

  if (!entry->cached_layout)
    {
      entry->cached_layout = gtk_entry_create_layout (entry, include_preedit);
      entry->cache_includes_preedit = include_preedit;
    }
  
  return entry->cached_layout;
}

static void
get_layout_position (GtkEntry *entry,
                     gint     *x,
                     gint     *y)
{
  PangoLayout *layout;
  PangoRectangle logical_rect;
  gint area_width, area_height;
  GtkBorder inner_border;
  gint y_pos;
  PangoLayoutLine *line;
  
  layout = gtk_entry_ensure_layout (entry, TRUE);

  gtk_entry_get_text_area_size (entry, NULL, NULL, &area_width, &area_height);
  _gtk_entry_effective_inner_border (entry, &inner_border);

  area_height = PANGO_SCALE * (area_height - inner_border.top - inner_border.bottom);

  line = pango_layout_get_lines_readonly (layout)->data;
  pango_layout_line_get_extents (line, NULL, &logical_rect);
  
  /* Align primarily for locale's ascent/descent */
  y_pos = ((area_height - entry->ascent - entry->descent) / 2 + 
           entry->ascent + logical_rect.y);
  
  /* Now see if we need to adjust to fit in actual drawn string */
  if (logical_rect.height > area_height)
    y_pos = (area_height - logical_rect.height) / 2;
  else if (y_pos < 0)
    y_pos = 0;
  else if (y_pos + logical_rect.height > area_height)
    y_pos = area_height - logical_rect.height;
  
  y_pos = inner_border.top + y_pos / PANGO_SCALE;

  if (x)
    *x = inner_border.left - entry->scroll_offset;

  if (y)
    *y = y_pos;
}

static void
draw_text_with_color (GtkEntry *entry, cairo_t *cr, GdkColor *default_color)
{
  PangoLayout *layout = gtk_entry_ensure_layout (entry, TRUE);
  GtkWidget *widget;
  gint x, y;
  gint start_pos, end_pos;

  widget = GTK_WIDGET (entry);

  cairo_save (cr);

  get_layout_position (entry, &x, &y);

  cairo_move_to (cr, x, y);
  gdk_cairo_set_source_color (cr, default_color);
  pango_cairo_show_layout (cr, layout);

  if (gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &start_pos, &end_pos))
    {
      gint *ranges;
      gint n_ranges, i;
      PangoRectangle logical_rect;
      GdkColor *selection_color, *text_color;
      GtkBorder inner_border;

      pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
      gtk_entry_get_pixel_ranges (entry, &ranges, &n_ranges);

      if (gtk_widget_has_focus (widget))
        {
          selection_color = &widget->style->base [GTK_STATE_SELECTED];
          text_color = &widget->style->text [GTK_STATE_SELECTED];
        }
      else
        {
          selection_color = &widget->style->base [GTK_STATE_ACTIVE];
	  text_color = &widget->style->text [GTK_STATE_ACTIVE];
        }

      _gtk_entry_effective_inner_border (entry, &inner_border);

      for (i = 0; i < n_ranges; ++i)
        cairo_rectangle (cr,
        	         inner_border.left - entry->scroll_offset + ranges[2 * i],
			 y,
			 ranges[2 * i + 1],
			 logical_rect.height);

      cairo_clip (cr);
	  
      gdk_cairo_set_source_color (cr, selection_color);
      cairo_paint (cr);

      cairo_move_to (cr, x, y);
      gdk_cairo_set_source_color (cr, text_color);
      pango_cairo_show_layout (cr, layout);
  
      g_free (ranges);
    }
  cairo_restore (cr);
}

static void
gtk_entry_draw_text (GtkEntry *entry)
{
  GtkWidget *widget = GTK_WIDGET (entry);
  cairo_t *cr;

  /* Nothing to display at all */
  if (gtk_entry_get_display_mode (entry) == DISPLAY_BLANK)
    return;
  
  if (gtk_widget_is_drawable (widget))
    {
      GdkColor text_color, bar_text_color;
      gint pos_x, pos_y;
      gint width, height;
      gint progress_x, progress_y, progress_width, progress_height;
      GtkStateType state;

      state = GTK_STATE_SELECTED;
      if (!gtk_widget_get_sensitive (widget))
        state = GTK_STATE_INSENSITIVE;
      text_color = widget->style->text[widget->state];
      bar_text_color = widget->style->fg[state];

      get_progress_area (widget,
                         &progress_x, &progress_y,
                         &progress_width, &progress_height);

      cr = gdk_cairo_create (entry->text_area);

      /* If the color is the same, or the progress area has a zero
       * size, then we only need to draw once. */
      if ((text_color.pixel == bar_text_color.pixel) ||
          ((progress_width == 0) || (progress_height == 0)))
        {
          draw_text_with_color (entry, cr, &text_color);
        }
      else
        {
          width = gdk_window_get_width (entry->text_area);
          height = gdk_window_get_height (entry->text_area);

          cairo_rectangle (cr, 0, 0, width, height);
          cairo_clip (cr);
          cairo_save (cr);

          cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD);
          cairo_rectangle (cr, 0, 0, width, height);

          gdk_window_get_position (entry->text_area, &pos_x, &pos_y);
          progress_x -= pos_x;
          progress_y -= pos_y;

          cairo_rectangle (cr, progress_x, progress_y,
                           progress_width, progress_height);
          cairo_clip (cr);
          cairo_set_fill_rule (cr, CAIRO_FILL_RULE_WINDING);
      
          draw_text_with_color (entry, cr, &text_color);
          cairo_restore (cr);

          cairo_rectangle (cr, progress_x, progress_y,
                           progress_width, progress_height);
          cairo_clip (cr);

          draw_text_with_color (entry, cr, &bar_text_color);
        }

      cairo_destroy (cr);
    }
}

static void
draw_insertion_cursor (GtkEntry      *entry,
		       GdkRectangle  *cursor_location,
		       gboolean       is_primary,
		       PangoDirection direction,
		       gboolean       draw_arrow)
{
  GtkWidget *widget = GTK_WIDGET (entry);
  GtkTextDirection text_dir;

  if (direction == PANGO_DIRECTION_LTR)
    text_dir = GTK_TEXT_DIR_LTR;
  else
    text_dir = GTK_TEXT_DIR_RTL;

  gtk_draw_insertion_cursor (widget, entry->text_area, NULL,
			     cursor_location,
			     is_primary, text_dir, draw_arrow);
}

static void
gtk_entry_draw_cursor (GtkEntry  *entry,
		       CursorType type)
{
  GtkWidget *widget = GTK_WIDGET (entry);
  GdkKeymap *keymap = gdk_keymap_get_for_display (gtk_widget_get_display (GTK_WIDGET (entry)));
  PangoDirection keymap_direction = gdk_keymap_get_direction (keymap);
  
  if (gtk_widget_is_drawable (widget))
    {
      GdkRectangle cursor_location;
      gboolean split_cursor;
      PangoRectangle cursor_rect;
      GtkBorder inner_border;
      gint xoffset;
      gint text_area_height;
      gint cursor_index;
      gboolean block;
      gboolean block_at_line_end;
      PangoLayout *layout;
      const char *text;

      _gtk_entry_effective_inner_border (entry, &inner_border);

      xoffset = inner_border.left - entry->scroll_offset;

      text_area_height = gdk_window_get_height (entry->text_area);

      layout = gtk_entry_ensure_layout (entry, TRUE);
      text = pango_layout_get_text (layout);
      cursor_index = g_utf8_offset_to_pointer (text, entry->current_pos + entry->preedit_cursor) - text;
      if (!entry->overwrite_mode)
        block = FALSE;
      else
        block = _gtk_text_util_get_block_cursor_location (layout,
                                                          cursor_index, &cursor_rect, &block_at_line_end);

      if (!block)
        {
          gint strong_x, weak_x;
          PangoDirection dir1 = PANGO_DIRECTION_NEUTRAL;
          PangoDirection dir2 = PANGO_DIRECTION_NEUTRAL;
          gint x1 = 0;
          gint x2 = 0;

          gtk_entry_get_cursor_locations (entry, type, &strong_x, &weak_x);

          g_object_get (gtk_widget_get_settings (widget),
                        "gtk-split-cursor", &split_cursor,
                        NULL);

          dir1 = entry->resolved_dir;
      
          if (split_cursor)
            {
              x1 = strong_x;

              if (weak_x != strong_x)
                {
                  dir2 = (entry->resolved_dir == PANGO_DIRECTION_LTR) ? PANGO_DIRECTION_RTL : PANGO_DIRECTION_LTR;
                  x2 = weak_x;
                }
            }
          else
            {
              if (keymap_direction == entry->resolved_dir)
                x1 = strong_x;
              else
                x1 = weak_x;
            }

          cursor_location.x = xoffset + x1;
          cursor_location.y = inner_border.top;
          cursor_location.width = 0;
          cursor_location.height = text_area_height - inner_border.top - inner_border.bottom;

          draw_insertion_cursor (entry,
                                 &cursor_location, TRUE, dir1,
                                 dir2 != PANGO_DIRECTION_NEUTRAL);
      
          if (dir2 != PANGO_DIRECTION_NEUTRAL)
            {
              cursor_location.x = xoffset + x2;
              draw_insertion_cursor (entry,
                                     &cursor_location, FALSE, dir2,
                                     TRUE);
            }
        }
      else /* overwrite_mode */
        {
          GdkColor cursor_color;
          GdkRectangle rect;
          cairo_t *cr;
          gint x, y;

          get_layout_position (entry, &x, &y);

          rect.x = PANGO_PIXELS (cursor_rect.x) + x;
          rect.y = PANGO_PIXELS (cursor_rect.y) + y;
          rect.width = PANGO_PIXELS (cursor_rect.width);
          rect.height = PANGO_PIXELS (cursor_rect.height);

          cr = gdk_cairo_create (entry->text_area);

          _gtk_widget_get_cursor_color (widget, &cursor_color);
          gdk_cairo_set_source_color (cr, &cursor_color);
          gdk_cairo_rectangle (cr, &rect);
          cairo_fill (cr);

          if (!block_at_line_end)
            {
              gdk_cairo_rectangle (cr, &rect);
              cairo_clip (cr);
              cairo_move_to (cr, x, y);
              gdk_cairo_set_source_color (cr, &widget->style->base[widget->state]);
              pango_cairo_show_layout (cr, layout);
            }

          cairo_destroy (cr);
        }
    }
}

static void
gtk_entry_queue_draw (GtkEntry *entry)
{
  if (gtk_widget_is_drawable (GTK_WIDGET (entry)))
    gdk_window_invalidate_rect (entry->text_area, NULL, FALSE);
}

void
_gtk_entry_reset_im_context (GtkEntry *entry)
{
  if (entry->need_im_reset)
    {
      entry->need_im_reset = FALSE;
      gtk_im_context_reset (entry->im_context);
    }
}

/**
 * gtk_entry_reset_im_context:
 * @entry: a #GtkEntry
 *
 * Reset the input method context of the entry if needed.
 *
 * This can be necessary in the case where modifying the buffer
 * would confuse on-going input method behavior.
 *
 * Since: 2.22
 */
void
gtk_entry_reset_im_context (GtkEntry *entry)
{
  g_return_if_fail (GTK_IS_ENTRY (entry));

  _gtk_entry_reset_im_context (entry);
}

/**
 * gtk_entry_im_context_filter_keypress:
 * @entry: a #GtkEntry
 * @event: the key event
 *
 * Allow the #GtkEntry input method to internally handle key press
 * and release events. If this function returns %TRUE, then no further
 * processing should be done for this key event. See
 * gtk_im_context_filter_keypress().
 *
 * Note that you are expected to call this function from your handler
 * when overriding key event handling. This is needed in the case when
 * you need to insert your own key handling between the input method
 * and the default key event handling of the #GtkEntry.
 * See gtk_text_view_reset_im_context() for an example of use.
 *
 * Return value: %TRUE if the input method handled the key event.
 *
 * Since: 2.22
 */
gboolean
gtk_entry_im_context_filter_keypress (GtkEntry    *entry,
                                      GdkEventKey *event)
{
  g_return_val_if_fail (GTK_IS_ENTRY (entry), FALSE);

  return gtk_im_context_filter_keypress (entry->im_context, event);
}


static gint
gtk_entry_find_position (GtkEntry *entry,
			 gint      x)
{
  PangoLayout *layout;
  PangoLayoutLine *line;
  gint index;
  gint pos;
  gint trailing;
  const gchar *text;
  gint cursor_index;
  
  layout = gtk_entry_ensure_layout (entry, TRUE);
  text = pango_layout_get_text (layout);
  cursor_index = g_utf8_offset_to_pointer (text, entry->current_pos) - text;
  
  line = pango_layout_get_lines_readonly (layout)->data;
  pango_layout_line_x_to_index (line, x * PANGO_SCALE, &index, &trailing);

  if (index >= cursor_index && entry->preedit_length)
    {
      if (index >= cursor_index + entry->preedit_length)
	index -= entry->preedit_length;
      else
	{
	  index = cursor_index;
	  trailing = 0;
	}
    }

  pos = g_utf8_pointer_to_offset (text, text + index);
  pos += trailing;

  return pos;
}

static void
gtk_entry_get_cursor_locations (GtkEntry   *entry,
				CursorType  type,
				gint       *strong_x,
				gint       *weak_x)
{
  DisplayMode mode = gtk_entry_get_display_mode (entry);

  /* Nothing to display at all, so no cursor is relevant */
  if (mode == DISPLAY_BLANK)
    {
      if (strong_x)
	*strong_x = 0;
      
      if (weak_x)
	*weak_x = 0;
    }
  else
    {
      PangoLayout *layout = gtk_entry_ensure_layout (entry, TRUE);
      const gchar *text = pango_layout_get_text (layout);
      PangoRectangle strong_pos, weak_pos;
      gint index;
  
      if (type == CURSOR_STANDARD)
	{
	  index = g_utf8_offset_to_pointer (text, entry->current_pos + entry->preedit_cursor) - text;
	}
      else /* type == CURSOR_DND */
	{
	  index = g_utf8_offset_to_pointer (text, entry->dnd_position) - text;

	  if (entry->dnd_position > entry->current_pos)
	    {
	      if (mode == DISPLAY_NORMAL)
		index += entry->preedit_length;
	      else
		{
		  gint preedit_len_chars = g_utf8_strlen (text, -1) - gtk_entry_buffer_get_length (get_buffer (entry));
		  index += preedit_len_chars * g_unichar_to_utf8 (entry->invisible_char, NULL);
		}
	    }
	}
      
      pango_layout_get_cursor_pos (layout, index, &strong_pos, &weak_pos);
      
      if (strong_x)
	*strong_x = strong_pos.x / PANGO_SCALE;
      
      if (weak_x)
	*weak_x = weak_pos.x / PANGO_SCALE;
    }
}

static void
gtk_entry_adjust_scroll (GtkEntry *entry)
{
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (entry);
  gint min_offset, max_offset;
  gint text_area_width, text_width;
  GtkBorder inner_border;
  gint strong_x, weak_x;
  gint strong_xoffset, weak_xoffset;
  gfloat xalign;
  PangoLayout *layout;
  PangoLayoutLine *line;
  PangoRectangle logical_rect;

  if (!gtk_widget_get_realized (GTK_WIDGET (entry)))
    return;

  _gtk_entry_effective_inner_border (entry, &inner_border);

  text_area_width = gdk_window_get_width (entry->text_area);
  text_area_width -= inner_border.left + inner_border.right;
  if (text_area_width < 0)
    text_area_width = 0;

  layout = gtk_entry_ensure_layout (entry, TRUE);
  line = pango_layout_get_lines_readonly (layout)->data;

  pango_layout_line_get_extents (line, NULL, &logical_rect);

  /* Display as much text as we can */

  if (entry->resolved_dir == PANGO_DIRECTION_LTR)
      xalign = priv->xalign;
  else
      xalign = 1.0 - priv->xalign;

  text_width = PANGO_PIXELS(logical_rect.width);

  if (text_width > text_area_width)
    {
      min_offset = 0;
      max_offset = text_width - text_area_width;
    }
  else
    {
      min_offset = (text_width - text_area_width) * xalign;
      max_offset = min_offset;
    }

  entry->scroll_offset = CLAMP (entry->scroll_offset, min_offset, max_offset);

  /* And make sure cursors are on screen. Note that the cursor is
   * actually drawn one pixel into the INNER_BORDER space on
   * the right, when the scroll is at the utmost right. This
   * looks better to to me than confining the cursor inside the
   * border entirely, though it means that the cursor gets one
   * pixel closer to the edge of the widget on the right than
   * on the left. This might need changing if one changed
   * INNER_BORDER from 2 to 1, as one would do on a
   * small-screen-real-estate display.
   *
   * We always make sure that the strong cursor is on screen, and
   * put the weak cursor on screen if possible.
   */

  gtk_entry_get_cursor_locations (entry, CURSOR_STANDARD, &strong_x, &weak_x);
  
  strong_xoffset = strong_x - entry->scroll_offset;

  if (strong_xoffset < 0)
    {
      entry->scroll_offset += strong_xoffset;
      strong_xoffset = 0;
    }
  else if (strong_xoffset > text_area_width)
    {
      entry->scroll_offset += strong_xoffset - text_area_width;
      strong_xoffset = text_area_width;
    }

  weak_xoffset = weak_x - entry->scroll_offset;

  if (weak_xoffset < 0 && strong_xoffset - weak_xoffset <= text_area_width)
    {
      entry->scroll_offset += weak_xoffset;
    }
  else if (weak_xoffset > text_area_width &&
	   strong_xoffset - (weak_xoffset - text_area_width) >= 0)
    {
      entry->scroll_offset += weak_xoffset - text_area_width;
    }

  g_object_notify (G_OBJECT (entry), "scroll-offset");
}

static void
gtk_entry_move_adjustments (GtkEntry *entry)
{
  PangoContext *context;
  PangoFontMetrics *metrics;
  gint x, layout_x, border_x, border_y;
  gint char_width;
  GtkAdjustment *adjustment;

  adjustment = g_object_get_qdata (G_OBJECT (entry), quark_cursor_hadjustment);
  if (!adjustment)
    return;

  /* Cursor position, layout offset, border width, and widget allocation */
  gtk_entry_get_cursor_locations (entry, CURSOR_STANDARD, &x, NULL);
  get_layout_position (entry, &layout_x, NULL);
  _gtk_entry_get_borders (entry, &border_x, &border_y);
  x += entry->widget.allocation.x + layout_x + border_x;

  /* Approximate width of a char, so user can see what is ahead/behind */
  context = gtk_widget_get_pango_context (GTK_WIDGET (entry));
  metrics = pango_context_get_metrics (context, 
                                       entry->widget.style->font_desc,
				       pango_context_get_language (context));
  char_width = pango_font_metrics_get_approximate_char_width (metrics) / PANGO_SCALE;

  /* Scroll it */
  gtk_adjustment_clamp_page (adjustment, 
  			     x - (char_width + 1),   /* one char + one pixel before */
			     x + (char_width + 2));  /* one char + cursor + one pixel after */
}

static gint
gtk_entry_move_visually (GtkEntry *entry,
			 gint      start,
			 gint      count)
{
  gint index;
  PangoLayout *layout = gtk_entry_ensure_layout (entry, FALSE);
  const gchar *text;

  text = pango_layout_get_text (layout);
  
  index = g_utf8_offset_to_pointer (text, start) - text;

  while (count != 0)
    {
      int new_index, new_trailing;
      gboolean split_cursor;
      gboolean strong;

      g_object_get (gtk_widget_get_settings (GTK_WIDGET (entry)),
		    "gtk-split-cursor", &split_cursor,
		    NULL);

      if (split_cursor)
	strong = TRUE;
      else
	{
	  GdkKeymap *keymap = gdk_keymap_get_for_display (gtk_widget_get_display (GTK_WIDGET (entry)));
	  PangoDirection keymap_direction = gdk_keymap_get_direction (keymap);

	  strong = keymap_direction == entry->resolved_dir;
	}
      
      if (count > 0)
	{
	  pango_layout_move_cursor_visually (layout, strong, index, 0, 1, &new_index, &new_trailing);
	  count--;
	}
      else
	{
	  pango_layout_move_cursor_visually (layout, strong, index, 0, -1, &new_index, &new_trailing);
	  count++;
	}

      if (new_index < 0)
        index = 0;
      else if (new_index != G_MAXINT)
        index = new_index;
      
      while (new_trailing--)
	index = g_utf8_next_char (text + index) - text;
    }
  
  return g_utf8_pointer_to_offset (text, text + index);
}

static gint
gtk_entry_move_logically (GtkEntry *entry,
			  gint      start,
			  gint      count)
{
  gint new_pos = start;
  guint length;

  length = gtk_entry_buffer_get_length (get_buffer (entry));

  /* Prevent any leak of information */
  if (gtk_entry_get_display_mode (entry) != DISPLAY_NORMAL)
    {
      new_pos = CLAMP (start + count, 0, length);
    }
  else
    {
      PangoLayout *layout = gtk_entry_ensure_layout (entry, FALSE);
      PangoLogAttr *log_attrs;
      gint n_attrs;

      pango_layout_get_log_attrs (layout, &log_attrs, &n_attrs);

      while (count > 0 && new_pos < length)
	{
	  do
	    new_pos++;
	  while (new_pos < length && !log_attrs[new_pos].is_cursor_position);
	  
	  count--;
	}
      while (count < 0 && new_pos > 0)
	{
	  do
	    new_pos--;
	  while (new_pos > 0 && !log_attrs[new_pos].is_cursor_position);
	  
	  count++;
	}
      
      g_free (log_attrs);
    }

  return new_pos;
}

static gint
gtk_entry_move_forward_word (GtkEntry *entry,
			     gint      start,
                             gboolean  allow_whitespace)
{
  gint new_pos = start;
  guint length;

  length = gtk_entry_buffer_get_length (get_buffer (entry));

  /* Prevent any leak of information */
  if (gtk_entry_get_display_mode (entry) != DISPLAY_NORMAL)
    {
      new_pos = length;
    }
  else if (new_pos < length)
    {
      PangoLayout *layout = gtk_entry_ensure_layout (entry, FALSE);
      PangoLogAttr *log_attrs;
      gint n_attrs;

      pango_layout_get_log_attrs (layout, &log_attrs, &n_attrs);
      
      /* Find the next word boundary */
      new_pos++;
      while (new_pos < n_attrs - 1 && !(log_attrs[new_pos].is_word_end ||
                                        (log_attrs[new_pos].is_word_start && allow_whitespace)))
	new_pos++;

      g_free (log_attrs);
    }

  return new_pos;
}


static gint
gtk_entry_move_backward_word (GtkEntry *entry,
			      gint      start,
                              gboolean  allow_whitespace)
{
  gint new_pos = start;

  /* Prevent any leak of information */
  if (gtk_entry_get_display_mode (entry) != DISPLAY_NORMAL)
    {
      new_pos = 0;
    }
  else if (start > 0)
    {
      PangoLayout *layout = gtk_entry_ensure_layout (entry, FALSE);
      PangoLogAttr *log_attrs;
      gint n_attrs;

      pango_layout_get_log_attrs (layout, &log_attrs, &n_attrs);

      new_pos = start - 1;

      /* Find the previous word boundary */
      while (new_pos > 0 && !(log_attrs[new_pos].is_word_start || 
                              (log_attrs[new_pos].is_word_end && allow_whitespace)))
	new_pos--;

      g_free (log_attrs);
    }

  return new_pos;
}

static void
gtk_entry_delete_whitespace (GtkEntry *entry)
{
  PangoLayout *layout = gtk_entry_ensure_layout (entry, FALSE);
  PangoLogAttr *log_attrs;
  gint n_attrs;
  gint start, end;

  pango_layout_get_log_attrs (layout, &log_attrs, &n_attrs);

  start = end = entry->current_pos;
  
  while (start > 0 && log_attrs[start-1].is_white)
    start--;

  while (end < n_attrs && log_attrs[end].is_white)
    end++;

  g_free (log_attrs);

  if (start != end)
    gtk_editable_delete_text (GTK_EDITABLE (entry), start, end);
}


static void
gtk_entry_select_word (GtkEntry *entry)
{
  gint start_pos = gtk_entry_move_backward_word (entry, entry->current_pos, TRUE);
  gint end_pos = gtk_entry_move_forward_word (entry, entry->current_pos, TRUE);

  gtk_editable_select_region (GTK_EDITABLE (entry), start_pos, end_pos);
}

static void
gtk_entry_select_line (GtkEntry *entry)
{
  gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
}

static gint
truncate_multiline (const gchar *text)
{
  gint length;

  for (length = 0;
       text[length] && text[length] != '\n' && text[length] != '\r';
       length++);

  return length;
}

static void
paste_received (GtkClipboard *clipboard,
		const gchar  *text,
		gpointer      data)
{
  GtkEntry *entry = GTK_ENTRY (data);
  GtkEditable *editable = GTK_EDITABLE (entry);
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (entry);
      
  if (entry->button == 2)
    {
      gint pos, start, end;
      pos = priv->insert_pos;
      gtk_editable_get_selection_bounds (editable, &start, &end);
      if (!((start <= pos && pos <= end) || (end <= pos && pos <= start)))
	gtk_editable_select_region (editable, pos, pos);
    }
      
  if (text)
    {
      gint pos, start, end;
      gint length = -1;
      gboolean popup_completion;
      GtkEntryCompletion *completion;

      completion = gtk_entry_get_completion (entry);

      if (entry->truncate_multiline)
        length = truncate_multiline (text);

      /* only complete if the selection is at the end */
      popup_completion = (gtk_entry_buffer_get_length (get_buffer (entry)) ==
                          MAX (entry->current_pos, entry->selection_bound));

      if (completion)
	{
	  if (gtk_widget_get_mapped (completion->priv->popup_window))
	    _gtk_entry_completion_popdown (completion);

          if (!popup_completion && completion->priv->changed_id > 0)
            g_signal_handler_block (entry, completion->priv->changed_id);
	}

      begin_change (entry);
      if (gtk_editable_get_selection_bounds (editable, &start, &end))
        gtk_editable_delete_text (editable, start, end);

      pos = entry->current_pos;
      gtk_editable_insert_text (editable, text, length, &pos);
      gtk_editable_set_position (editable, pos);
      end_change (entry);

      if (completion &&
          !popup_completion && completion->priv->changed_id > 0)
	g_signal_handler_unblock (entry, completion->priv->changed_id);
    }

  g_object_unref (entry);
}

static void
gtk_entry_paste (GtkEntry *entry,
		 GdkAtom   selection)
{
  g_object_ref (entry);
  gtk_clipboard_request_text (gtk_widget_get_clipboard (GTK_WIDGET (entry), selection),
			      paste_received, entry);
}

static void
primary_get_cb (GtkClipboard     *clipboard,
		GtkSelectionData *selection_data,
		guint             info,
		gpointer          data)
{
  GtkEntry *entry = GTK_ENTRY (data);
  gint start, end;
  
  if (gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &start, &end))
    {
      gchar *str = gtk_entry_get_display_text (entry, start, end);
      gtk_selection_data_set_text (selection_data, str, -1);
      g_free (str);
    }
}

static void
primary_clear_cb (GtkClipboard *clipboard,
		  gpointer      data)
{
  GtkEntry *entry = GTK_ENTRY (data);

  gtk_editable_select_region (GTK_EDITABLE (entry), entry->current_pos, entry->current_pos);
}

static void
gtk_entry_update_primary_selection (GtkEntry *entry)
{
  GtkTargetList *list;
  GtkTargetEntry *targets;
  GtkClipboard *clipboard;
  gint start, end;
  gint n_targets;

  if (!gtk_widget_get_realized (GTK_WIDGET (entry)))
    return;

  list = gtk_target_list_new (NULL, 0);
  gtk_target_list_add_text_targets (list, 0);

  targets = gtk_target_table_new_from_list (list, &n_targets);

  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (entry), GDK_SELECTION_PRIMARY);
  
  if (gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &start, &end))
    {
      if (!gtk_clipboard_set_with_owner (clipboard, targets, n_targets,
					 primary_get_cb, primary_clear_cb, G_OBJECT (entry)))
	primary_clear_cb (clipboard, entry);
    }
  else
    {
      if (gtk_clipboard_get_owner (clipboard) == G_OBJECT (entry))
	gtk_clipboard_clear (clipboard);
    }

  gtk_target_table_free (targets, n_targets);
  gtk_target_list_unref (list);
}

static void
gtk_entry_clear (GtkEntry             *entry,
                 GtkEntryIconPosition  icon_pos)
{
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (entry);
  EntryIconInfo *icon_info = priv->icons[icon_pos];

  if (!icon_info || icon_info->storage_type == GTK_IMAGE_EMPTY)
    return;

  g_object_freeze_notify (G_OBJECT (entry));

  /* Explicitly check, as the pointer may become invalidated
   * during destruction.
   */
  if (GDK_IS_WINDOW (icon_info->window))
    gdk_window_hide (icon_info->window);

  if (icon_info->pixbuf)
    {
      g_object_unref (icon_info->pixbuf);
      icon_info->pixbuf = NULL;
    }

  switch (icon_info->storage_type)
    {
    case GTK_IMAGE_PIXBUF:
      g_object_notify (G_OBJECT (entry),
                       icon_pos == GTK_ENTRY_ICON_PRIMARY ? "primary-icon-pixbuf" : "secondary-icon-pixbuf");
      break;

    case GTK_IMAGE_STOCK:
      g_free (icon_info->stock_id);
      icon_info->stock_id = NULL;
      g_object_notify (G_OBJECT (entry),
                       icon_pos == GTK_ENTRY_ICON_PRIMARY ? "primary-icon-stock" : "secondary-icon-stock");
      break;

    case GTK_IMAGE_ICON_NAME:
      g_free (icon_info->icon_name);
      icon_info->icon_name = NULL;
      g_object_notify (G_OBJECT (entry),
                       icon_pos == GTK_ENTRY_ICON_PRIMARY ? "primary-icon-name" : "secondary-icon-name");
      break;

    case GTK_IMAGE_GICON:
      if (icon_info->gicon)
        {
          g_object_unref (icon_info->gicon);
          icon_info->gicon = NULL;
        }
      g_object_notify (G_OBJECT (entry),
                       icon_pos == GTK_ENTRY_ICON_PRIMARY ? "primary-icon-gicon" : "secondary-icon-gicon");
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  icon_info->storage_type = GTK_IMAGE_EMPTY;
  g_object_notify (G_OBJECT (entry),
                   icon_pos == GTK_ENTRY_ICON_PRIMARY ? "primary-icon-storage-type" : "secondary-icon-storage-type");

  g_object_thaw_notify (G_OBJECT (entry));
}

static void
gtk_entry_ensure_pixbuf (GtkEntry             *entry,
                         GtkEntryIconPosition  icon_pos)
{
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (entry);
  EntryIconInfo *icon_info = priv->icons[icon_pos];
  GtkIconInfo *info;
  GtkIconTheme *icon_theme;
  GtkSettings *settings;
  GtkStateType state;
  GtkWidget *widget;
  GdkScreen *screen;
  gint width, height;

  if (!icon_info || icon_info->pixbuf)
    return;

  widget = GTK_WIDGET (entry);

  switch (icon_info->storage_type)
    {
    case GTK_IMAGE_EMPTY:
    case GTK_IMAGE_PIXBUF:
      break;
    case GTK_IMAGE_STOCK:
      state = gtk_widget_get_state (widget);
      gtk_widget_set_state (widget, GTK_STATE_NORMAL);
      icon_info->pixbuf = gtk_widget_render_icon (widget,
                                                  icon_info->stock_id,
                                                  GTK_ICON_SIZE_MENU,
                                                  NULL);
      if (!icon_info->pixbuf)
        icon_info->pixbuf = gtk_widget_render_icon (widget,
                                                    GTK_STOCK_MISSING_IMAGE,
                                                    GTK_ICON_SIZE_MENU,
                                                    NULL);
      gtk_widget_set_state (widget, state);
      break;

    case GTK_IMAGE_ICON_NAME:
      screen = gtk_widget_get_screen (widget);
      if (screen)
        {
          icon_theme = gtk_icon_theme_get_for_screen (screen);
          settings = gtk_settings_get_for_screen (screen);
          
          gtk_icon_size_lookup_for_settings (settings,
                                             GTK_ICON_SIZE_MENU,
                                             &width, &height);

          icon_info->pixbuf = gtk_icon_theme_load_icon (icon_theme,
                                                        icon_info->icon_name,
                                                        MIN (width, height),
                                                        0, NULL);

          if (icon_info->pixbuf == NULL)
            {
              state = gtk_widget_get_state (widget);
              gtk_widget_set_state (widget, GTK_STATE_NORMAL);
              icon_info->pixbuf = gtk_widget_render_icon (widget,
                                                          GTK_STOCK_MISSING_IMAGE,
                                                          GTK_ICON_SIZE_MENU,
                                                          NULL);
              gtk_widget_set_state (widget, state);
            }
        }
      break;

    case GTK_IMAGE_GICON:
      screen = gtk_widget_get_screen (widget);
      if (screen)
        {
          icon_theme = gtk_icon_theme_get_for_screen (screen);
          settings = gtk_settings_get_for_screen (screen);

          gtk_icon_size_lookup_for_settings (settings,
                                             GTK_ICON_SIZE_MENU,
                                             &width, &height);

          info = gtk_icon_theme_lookup_by_gicon (icon_theme,
                                                 icon_info->gicon,
                                                 MIN (width, height), 
                                                 GTK_ICON_LOOKUP_USE_BUILTIN);
          if (info)
            {
              icon_info->pixbuf = gtk_icon_info_load_icon (info, NULL);
              gtk_icon_info_free (info);
            }

          if (icon_info->pixbuf == NULL)
            {
              state = gtk_widget_get_state (widget);
              gtk_widget_set_state (widget, GTK_STATE_NORMAL);
              icon_info->pixbuf = gtk_widget_render_icon (widget,
                                                          GTK_STOCK_MISSING_IMAGE,
                                                          GTK_ICON_SIZE_MENU,
                                                          NULL);
              gtk_widget_set_state (widget, state);
            }
        }
      break;

    default:
      g_assert_not_reached ();
      break;
    }
    
  if (icon_info->pixbuf != NULL && icon_info->window != NULL)
    gdk_window_show_unraised (icon_info->window);
}


/* Public API
 */

/**
 * gtk_entry_new:
 *
 * Creates a new entry.
 *
 * Return value: a new #GtkEntry.
 */
GtkWidget*
gtk_entry_new (void)
{
  return g_object_new (GTK_TYPE_ENTRY, NULL);
}

/**
 * gtk_entry_new_with_buffer:
 * @buffer: The buffer to use for the new #GtkEntry.
 *
 * Creates a new entry with the specified text buffer.
 *
 * Return value: a new #GtkEntry
 *
 * Since: 2.18
 */
GtkWidget*
gtk_entry_new_with_buffer (GtkEntryBuffer *buffer)
{
  g_return_val_if_fail (GTK_IS_ENTRY_BUFFER (buffer), NULL);
  return g_object_new (GTK_TYPE_ENTRY, "buffer", buffer, NULL);
}

/**
 * gtk_entry_new_with_max_length:
 * @max: the maximum length of the entry, or 0 for no maximum.
 *   (other than the maximum length of entries.) The value passed in will
 *   be clamped to the range 0-65536.
 *
 * Creates a new #GtkEntry widget with the given maximum length.
 * 
 * Return value: a new #GtkEntry
 *
 * Deprecated: 2.0: Use gtk_entry_set_max_length() instead.
 **/
GtkWidget*
gtk_entry_new_with_max_length (gint max)
{
  GtkEntry *entry;

  max = CLAMP (max, 0, GTK_ENTRY_BUFFER_MAX_SIZE);

  entry = g_object_new (GTK_TYPE_ENTRY, NULL);
  gtk_entry_buffer_set_max_length (get_buffer (entry), max);

  return GTK_WIDGET (entry);
}


static GtkEntryBuffer*
get_buffer (GtkEntry *entry)
{
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (entry);

  if (priv->buffer == NULL)
    {
      GtkEntryBuffer *buffer;
      buffer = gtk_entry_buffer_new (NULL, 0);
      gtk_entry_set_buffer (entry, buffer);
      g_object_unref (buffer);
    }

  return priv->buffer;
}

/**
 * gtk_entry_get_buffer:
 * @entry: a #GtkEntry
 *
 * Get the #GtkEntryBuffer object which holds the text for
 * this widget.
 *
 * Since: 2.18
 *
 * Returns: (transfer none): A #GtkEntryBuffer object.
 */
GtkEntryBuffer*
gtk_entry_get_buffer (GtkEntry *entry)
{
  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);

  return get_buffer (entry);
}

/**
 * gtk_entry_set_buffer:
 * @entry: a #GtkEntry
 * @buffer: a #GtkEntryBuffer
 *
 * Set the #GtkEntryBuffer object which holds the text for
 * this widget.
 *
 * Since: 2.18
 */
void
gtk_entry_set_buffer (GtkEntry       *entry,
                      GtkEntryBuffer *buffer)
{
  GtkEntryPrivate *priv;
  GObject *obj;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  priv = GTK_ENTRY_GET_PRIVATE (entry);

  if (buffer)
    {
      g_return_if_fail (GTK_IS_ENTRY_BUFFER (buffer));
      g_object_ref (buffer);
    }

  if (priv->buffer)
    {
      buffer_disconnect_signals (entry);
      g_object_unref (priv->buffer);

      /* COMPAT: Deprecated. Not used. Setting these fields no longer necessary in GTK 3.x */
      entry->text = NULL;
      entry->text_length = 0;
      entry->text_max_length = 0;
    }

  priv->buffer = buffer;

  if (priv->buffer)
    {
       buffer_connect_signals (entry);

      /* COMPAT: Deprecated. Not used. Setting these fields no longer necessary in GTK 3.x */
      entry->text = (char*)gtk_entry_buffer_get_text (priv->buffer);
      entry->text_length = gtk_entry_buffer_get_length (priv->buffer);
      entry->text_max_length = gtk_entry_buffer_get_max_length (priv->buffer);
    }

  obj = G_OBJECT (entry);
  g_object_freeze_notify (obj);
  g_object_notify (obj, "buffer");
  g_object_notify (obj, "text");
  g_object_notify (obj, "text-length");
  g_object_notify (obj, "max-length");
  g_object_notify (obj, "visibility");
  g_object_notify (obj, "invisible-char");
  g_object_notify (obj, "invisible-char-set");
  g_object_thaw_notify (obj);

  gtk_editable_set_position (GTK_EDITABLE (entry), 0);
  gtk_entry_recompute (entry);
}

/**
 * gtk_entry_get_text_window:
 * @entry: a #GtkEntry
 *
 * Returns the #GdkWindow which contains the text. This function is
 * useful when drawing something to the entry in an expose-event
 * callback because it enables the callback to distinguish between
 * the text window and entry's icon windows.
 *
 * See also gtk_entry_get_icon_window().
 *
 * Note that GTK+ 3 does not have this function anymore; it has
 * been replaced by gtk_entry_get_text_area().
 *
 * Return value: (transfer none): the entry's text window.
 *
 * Since: 2.20
 **/
GdkWindow *
gtk_entry_get_text_window (GtkEntry *entry)
{
  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);

  return entry->text_area;
}


/**
 * gtk_entry_set_text:
 * @entry: a #GtkEntry
 * @text: the new text
 *
 * Sets the text in the widget to the given
 * value, replacing the current contents.
 *
 * See gtk_entry_buffer_set_text().
 */
void
gtk_entry_set_text (GtkEntry    *entry,
		    const gchar *text)
{
  gint tmp_pos;
  GtkEntryCompletion *completion;
  GtkEntryPrivate *priv;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (text != NULL);
  priv = GTK_ENTRY_GET_PRIVATE (entry);

  /* Actually setting the text will affect the cursor and selection;
   * if the contents don't actually change, this will look odd to the user.
   */
  if (strcmp (gtk_entry_buffer_get_text (get_buffer (entry)), text) == 0)
    return;

  completion = gtk_entry_get_completion (entry);
  if (completion && completion->priv->changed_id > 0)
    g_signal_handler_block (entry, completion->priv->changed_id);

  begin_change (entry);
  gtk_editable_delete_text (GTK_EDITABLE (entry), 0, -1);
  tmp_pos = 0;
  gtk_editable_insert_text (GTK_EDITABLE (entry), text, strlen (text), &tmp_pos);
  end_change (entry);

  if (completion && completion->priv->changed_id > 0)
    g_signal_handler_unblock (entry, completion->priv->changed_id);
}

/**
 * gtk_entry_append_text:
 * @entry: a #GtkEntry
 * @text: the text to append
 *
 * Appends the given text to the contents of the widget.
 *
 * Deprecated: 2.0: Use gtk_editable_insert_text() instead.
 */
void
gtk_entry_append_text (GtkEntry *entry,
		       const gchar *text)
{
  GtkEntryPrivate *priv;
  gint tmp_pos;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (text != NULL);
  priv = GTK_ENTRY_GET_PRIVATE (entry);

  tmp_pos = gtk_entry_buffer_get_length (get_buffer (entry));
  gtk_editable_insert_text (GTK_EDITABLE (entry), text, -1, &tmp_pos);
}

/**
 * gtk_entry_prepend_text:
 * @entry: a #GtkEntry
 * @text: the text to prepend
 *
 * Prepends the given text to the contents of the widget.
 *
 * Deprecated: 2.0: Use gtk_editable_insert_text() instead.
 */
void
gtk_entry_prepend_text (GtkEntry *entry,
			const gchar *text)
{
  gint tmp_pos;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (text != NULL);

  tmp_pos = 0;
  gtk_editable_insert_text (GTK_EDITABLE (entry), text, -1, &tmp_pos);
}

/**
 * gtk_entry_set_position:
 * @entry: a #GtkEntry
 * @position: the position of the cursor. The cursor is displayed
 *    before the character with the given (base 0) index in the widget. 
 *    The value must be less than or equal to the number of characters 
 *    in the widget. A value of -1 indicates that the position should
 *    be set after the last character in the entry. Note that this 
 *    position is in characters, not in bytes.
 *
 * Sets the cursor position in an entry to the given value. 
 *
 * Deprecated: 2.0: Use gtk_editable_set_position() instead.
 */
void
gtk_entry_set_position (GtkEntry *entry,
			gint      position)
{
  g_return_if_fail (GTK_IS_ENTRY (entry));

  gtk_editable_set_position (GTK_EDITABLE (entry), position);
}

/**
 * gtk_entry_set_visibility:
 * @entry: a #GtkEntry
 * @visible: %TRUE if the contents of the entry are displayed
 *           as plaintext
 *
 * Sets whether the contents of the entry are visible or not. 
 * When visibility is set to %FALSE, characters are displayed 
 * as the invisible char, and will also appear that way when 
 * the text in the entry widget is copied elsewhere.
 *
 * By default, GTK+ picks the best invisible character available
 * in the current font, but it can be changed with
 * gtk_entry_set_invisible_char().
 */
void
gtk_entry_set_visibility (GtkEntry *entry,
			  gboolean visible)
{
  g_return_if_fail (GTK_IS_ENTRY (entry));

  visible = visible != FALSE;

  if (entry->visible != visible)
    {
      entry->visible = visible;

      g_object_notify (G_OBJECT (entry), "visibility");
      gtk_entry_recompute (entry);
    }
}

/**
 * gtk_entry_get_visibility:
 * @entry: a #GtkEntry
 *
 * Retrieves whether the text in @entry is visible. See
 * gtk_entry_set_visibility().
 *
 * Return value: %TRUE if the text is currently visible
 **/
gboolean
gtk_entry_get_visibility (GtkEntry *entry)
{
  g_return_val_if_fail (GTK_IS_ENTRY (entry), FALSE);

  return entry->visible;
}

/**
 * gtk_entry_set_invisible_char:
 * @entry: a #GtkEntry
 * @ch: a Unicode character
 * 
 * Sets the character to use in place of the actual text when
 * gtk_entry_set_visibility() has been called to set text visibility
 * to %FALSE. i.e. this is the character used in "password mode" to
 * show the user how many characters have been typed. By default, GTK+
 * picks the best invisible char available in the current font. If you
 * set the invisible char to 0, then the user will get no feedback
 * at all; there will be no text on the screen as they type.
 **/
void
gtk_entry_set_invisible_char (GtkEntry *entry,
                              gunichar  ch)
{
  GtkEntryPrivate *priv;

  g_return_if_fail (GTK_IS_ENTRY (entry));

  priv = GTK_ENTRY_GET_PRIVATE (entry);

  if (!priv->invisible_char_set)
    {
      priv->invisible_char_set = TRUE;
      g_object_notify (G_OBJECT (entry), "invisible-char-set");
    }

  if (ch == entry->invisible_char)
    return;

  entry->invisible_char = ch;
  g_object_notify (G_OBJECT (entry), "invisible-char");
  gtk_entry_recompute (entry);  
}

/**
 * gtk_entry_get_invisible_char:
 * @entry: a #GtkEntry
 *
 * Retrieves the character displayed in place of the real characters
 * for entries with visibility set to false. See gtk_entry_set_invisible_char().
 *
 * Return value: the current invisible char, or 0, if the entry does not
 *               show invisible text at all. 
 **/
gunichar
gtk_entry_get_invisible_char (GtkEntry *entry)
{
  g_return_val_if_fail (GTK_IS_ENTRY (entry), 0);

  return entry->invisible_char;
}

/**
 * gtk_entry_unset_invisible_char:
 * @entry: a #GtkEntry
 *
 * Unsets the invisible char previously set with
 * gtk_entry_set_invisible_char(). So that the
 * default invisible char is used again.
 *
 * Since: 2.16
 **/
void
gtk_entry_unset_invisible_char (GtkEntry *entry)
{
  GtkEntryPrivate *priv;
  gunichar ch;

  g_return_if_fail (GTK_IS_ENTRY (entry));

  priv = GTK_ENTRY_GET_PRIVATE (entry);

  if (!priv->invisible_char_set)
    return;

  priv->invisible_char_set = FALSE;
  ch = find_invisible_char (GTK_WIDGET (entry));

  if (entry->invisible_char != ch)
    {
      entry->invisible_char = ch;
      g_object_notify (G_OBJECT (entry), "invisible-char");
    }

  g_object_notify (G_OBJECT (entry), "invisible-char-set");
  gtk_entry_recompute (entry);
}

/**
 * gtk_entry_set_editable:
 * @entry: a #GtkEntry
 * @editable: %TRUE if the user is allowed to edit the text
 *   in the widget
 *
 * Determines if the user can edit the text in the editable
 * widget or not. 
 *
 * Deprecated: 2.0: Use gtk_editable_set_editable() instead.
 */
void
gtk_entry_set_editable (GtkEntry *entry,
			gboolean  editable)
{
  g_return_if_fail (GTK_IS_ENTRY (entry));

  gtk_editable_set_editable (GTK_EDITABLE (entry), editable);
}

/**
 * gtk_entry_set_overwrite_mode:
 * @entry: a #GtkEntry
 * @overwrite: new value
 * 
 * Sets whether the text is overwritten when typing in the #GtkEntry.
 *
 * Since: 2.14
 **/
void
gtk_entry_set_overwrite_mode (GtkEntry *entry,
                              gboolean  overwrite)
{
  g_return_if_fail (GTK_IS_ENTRY (entry));
  
  if (entry->overwrite_mode == overwrite) 
    return;
  
  gtk_entry_toggle_overwrite (entry);

  g_object_notify (G_OBJECT (entry), "overwrite-mode");
}

/**
 * gtk_entry_get_overwrite_mode:
 * @entry: a #GtkEntry
 * 
 * Gets the value set by gtk_entry_set_overwrite_mode().
 * 
 * Return value: whether the text is overwritten when typing.
 *
 * Since: 2.14
 **/
gboolean
gtk_entry_get_overwrite_mode (GtkEntry *entry)
{
  g_return_val_if_fail (GTK_IS_ENTRY (entry), FALSE);

  return entry->overwrite_mode;
}

/**
 * gtk_entry_get_text:
 * @entry: a #GtkEntry
 *
 * Retrieves the contents of the entry widget.
 * See also gtk_editable_get_chars().
 *
 * This is equivalent to:
 *
 * <informalexample><programlisting>
 * gtk_entry_buffer_get_text (gtk_entry_get_buffer (entry));
 * </programlisting></informalexample>
 *
 * Return value: a pointer to the contents of the widget as a
 *      string. This string points to internally allocated
 *      storage in the widget and must not be freed, modified or
 *      stored.
 **/
const gchar*
gtk_entry_get_text (GtkEntry *entry)
{
  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);
  return gtk_entry_buffer_get_text (get_buffer (entry));
}

/**
 * gtk_entry_select_region:
 * @entry: a #GtkEntry
 * @start: the starting position
 * @end: the end position
 *
 * Selects a region of text. The characters that are selected are 
 * those characters at positions from @start_pos up to, but not 
 * including @end_pos. If @end_pos is negative, then the the characters 
 * selected will be those characters from @start_pos to the end of 
 * the text. 
 *
 * Deprecated: 2.0: Use gtk_editable_select_region() instead.
 */
void       
gtk_entry_select_region  (GtkEntry       *entry,
			  gint            start,
			  gint            end)
{
  gtk_editable_select_region (GTK_EDITABLE (entry), start, end);
}

/**
 * gtk_entry_set_max_length:
 * @entry: a #GtkEntry
 * @max: the maximum length of the entry, or 0 for no maximum.
 *   (other than the maximum length of entries.) The value passed in will
 *   be clamped to the range 0-65536.
 * 
 * Sets the maximum allowed length of the contents of the widget. If
 * the current contents are longer than the given length, then they
 * will be truncated to fit.
 *
 * This is equivalent to:
 *
 * <informalexample><programlisting>
 * gtk_entry_buffer_set_max_length (gtk_entry_get_buffer (entry), max);
 * </programlisting></informalexample>
 **/
void
gtk_entry_set_max_length (GtkEntry     *entry,
                          gint          max)
{
  g_return_if_fail (GTK_IS_ENTRY (entry));
  gtk_entry_buffer_set_max_length (get_buffer (entry), max);
}

/**
 * gtk_entry_get_max_length:
 * @entry: a #GtkEntry
 *
 * Retrieves the maximum allowed length of the text in
 * @entry. See gtk_entry_set_max_length().
 *
 * This is equivalent to:
 *
 * <informalexample><programlisting>
 * gtk_entry_buffer_get_max_length (gtk_entry_get_buffer (entry));
 * </programlisting></informalexample>
 *
 * Return value: the maximum allowed number of characters
 *               in #GtkEntry, or 0 if there is no maximum.
 **/
gint
gtk_entry_get_max_length (GtkEntry *entry)
{
  g_return_val_if_fail (GTK_IS_ENTRY (entry), 0);
  return gtk_entry_buffer_get_max_length (get_buffer (entry));
}

/**
 * gtk_entry_get_text_length:
 * @entry: a #GtkEntry
 *
 * Retrieves the current length of the text in
 * @entry. 
 *
 * This is equivalent to:
 *
 * <informalexample><programlisting>
 * gtk_entry_buffer_get_length (gtk_entry_get_buffer (entry));
 * </programlisting></informalexample>
 *
 * Return value: the current number of characters
 *               in #GtkEntry, or 0 if there are none.
 *
 * Since: 2.14
 **/
guint16
gtk_entry_get_text_length (GtkEntry *entry)
{
  g_return_val_if_fail (GTK_IS_ENTRY (entry), 0);
  return gtk_entry_buffer_get_length (get_buffer (entry));
}

/**
 * gtk_entry_set_activates_default:
 * @entry: a #GtkEntry
 * @setting: %TRUE to activate window's default widget on Enter keypress
 *
 * If @setting is %TRUE, pressing Enter in the @entry will activate the default
 * widget for the window containing the entry. This usually means that
 * the dialog box containing the entry will be closed, since the default
 * widget is usually one of the dialog buttons.
 *
 * (For experts: if @setting is %TRUE, the entry calls
 * gtk_window_activate_default() on the window containing the entry, in
 * the default handler for the #GtkWidget::activate signal.)
 **/
void
gtk_entry_set_activates_default (GtkEntry *entry,
                                 gboolean  setting)
{
  g_return_if_fail (GTK_IS_ENTRY (entry));
  setting = setting != FALSE;

  if (setting != entry->activates_default)
    {
      entry->activates_default = setting;
      g_object_notify (G_OBJECT (entry), "activates-default");
    }
}

/**
 * gtk_entry_get_activates_default:
 * @entry: a #GtkEntry
 * 
 * Retrieves the value set by gtk_entry_set_activates_default().
 * 
 * Return value: %TRUE if the entry will activate the default widget
 **/
gboolean
gtk_entry_get_activates_default (GtkEntry *entry)
{
  g_return_val_if_fail (GTK_IS_ENTRY (entry), FALSE);

  return entry->activates_default;
}

/**
 * gtk_entry_set_width_chars:
 * @entry: a #GtkEntry
 * @n_chars: width in chars
 *
 * Changes the size request of the entry to be about the right size
 * for @n_chars characters. Note that it changes the size
 * <emphasis>request</emphasis>, the size can still be affected by
 * how you pack the widget into containers. If @n_chars is -1, the
 * size reverts to the default entry size.
 **/
void
gtk_entry_set_width_chars (GtkEntry *entry,
                           gint      n_chars)
{
  g_return_if_fail (GTK_IS_ENTRY (entry));

  if (entry->width_chars != n_chars)
    {
      entry->width_chars = n_chars;
      g_object_notify (G_OBJECT (entry), "width-chars");
      gtk_widget_queue_resize (GTK_WIDGET (entry));
    }
}

/**
 * gtk_entry_get_width_chars:
 * @entry: a #GtkEntry
 * 
 * Gets the value set by gtk_entry_set_width_chars().
 * 
 * Return value: number of chars to request space for, or negative if unset
 **/
gint
gtk_entry_get_width_chars (GtkEntry *entry)
{
  g_return_val_if_fail (GTK_IS_ENTRY (entry), 0);

  return entry->width_chars;
}

/**
 * gtk_entry_set_has_frame:
 * @entry: a #GtkEntry
 * @setting: new value
 * 
 * Sets whether the entry has a beveled frame around it.
 **/
void
gtk_entry_set_has_frame (GtkEntry *entry,
                         gboolean  setting)
{
  g_return_if_fail (GTK_IS_ENTRY (entry));

  setting = (setting != FALSE);

  if (entry->has_frame == setting)
    return;

  gtk_widget_queue_resize (GTK_WIDGET (entry));
  entry->has_frame = setting;
  g_object_notify (G_OBJECT (entry), "has-frame");
}

/**
 * gtk_entry_get_has_frame:
 * @entry: a #GtkEntry
 * 
 * Gets the value set by gtk_entry_set_has_frame().
 * 
 * Return value: whether the entry has a beveled frame
 **/
gboolean
gtk_entry_get_has_frame (GtkEntry *entry)
{
  g_return_val_if_fail (GTK_IS_ENTRY (entry), FALSE);

  return entry->has_frame;
}

/**
 * gtk_entry_set_inner_border:
 * @entry: a #GtkEntry
 * @border: (allow-none): a #GtkBorder, or %NULL
 *
 * Sets %entry's inner-border property to %border, or clears it if %NULL
 * is passed. The inner-border is the area around the entry's text, but
 * inside its frame.
 *
 * If set, this property overrides the inner-border style property.
 * Overriding the style-provided border is useful when you want to do
 * in-place editing of some text in a canvas or list widget, where
 * pixel-exact positioning of the entry is important.
 *
 * Since: 2.10
 **/
void
gtk_entry_set_inner_border (GtkEntry        *entry,
                            const GtkBorder *border)
{
  g_return_if_fail (GTK_IS_ENTRY (entry));

  gtk_widget_queue_resize (GTK_WIDGET (entry));

  if (border)
    g_object_set_qdata_full (G_OBJECT (entry), quark_inner_border,
                             gtk_border_copy (border),
                             (GDestroyNotify) gtk_border_free);
  else
    g_object_set_qdata (G_OBJECT (entry), quark_inner_border, NULL);

  g_object_notify (G_OBJECT (entry), "inner-border");
}

/**
 * gtk_entry_get_inner_border:
 * @entry: a #GtkEntry
 *
 * This function returns the entry's #GtkEntry:inner-border property. See
 * gtk_entry_set_inner_border() for more information.
 *
 * Return value: (transfer none): the entry's #GtkBorder, or %NULL if none was set.
 *
 * Since: 2.10
 **/
const GtkBorder *
gtk_entry_get_inner_border (GtkEntry *entry)
{
  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);

  return g_object_get_qdata (G_OBJECT (entry), quark_inner_border);
}

/**
 * gtk_entry_get_layout:
 * @entry: a #GtkEntry
 * 
 * Gets the #PangoLayout used to display the entry.
 * The layout is useful to e.g. convert text positions to
 * pixel positions, in combination with gtk_entry_get_layout_offsets().
 * The returned layout is owned by the entry and must not be 
 * modified or freed by the caller.
 *
 * Keep in mind that the layout text may contain a preedit string, so
 * gtk_entry_layout_index_to_text_index() and
 * gtk_entry_text_index_to_layout_index() are needed to convert byte
 * indices in the layout to byte indices in the entry contents.
 *
 * Return value: (transfer none): the #PangoLayout for this entry
 **/
PangoLayout*
gtk_entry_get_layout (GtkEntry *entry)
{
  PangoLayout *layout;
  
  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);

  layout = gtk_entry_ensure_layout (entry, TRUE);

  return layout;
}


/**
 * gtk_entry_layout_index_to_text_index:
 * @entry: a #GtkEntry
 * @layout_index: byte index into the entry layout text
 * 
 * Converts from a position in the entry contents (returned
 * by gtk_entry_get_text()) to a position in the
 * entry's #PangoLayout (returned by gtk_entry_get_layout(),
 * with text retrieved via pango_layout_get_text()).
 * 
 * Return value: byte index into the entry contents
 **/
gint
gtk_entry_layout_index_to_text_index (GtkEntry *entry,
                                      gint      layout_index)
{
  PangoLayout *layout;
  const gchar *text;
  gint cursor_index;
  
  g_return_val_if_fail (GTK_IS_ENTRY (entry), 0);

  layout = gtk_entry_ensure_layout (entry, TRUE);
  text = pango_layout_get_text (layout);
  cursor_index = g_utf8_offset_to_pointer (text, entry->current_pos) - text;
  
  if (layout_index >= cursor_index && entry->preedit_length)
    {
      if (layout_index >= cursor_index + entry->preedit_length)
	layout_index -= entry->preedit_length;
      else
        layout_index = cursor_index;
    }

  return layout_index;
}

/**
 * gtk_entry_text_index_to_layout_index:
 * @entry: a #GtkEntry
 * @text_index: byte index into the entry contents
 * 
 * Converts from a position in the entry's #PangoLayout (returned by
 * gtk_entry_get_layout()) to a position in the entry contents
 * (returned by gtk_entry_get_text()).
 * 
 * Return value: byte index into the entry layout text
 **/
gint
gtk_entry_text_index_to_layout_index (GtkEntry *entry,
                                      gint      text_index)
{
  PangoLayout *layout;
  const gchar *text;
  gint cursor_index;
  g_return_val_if_fail (GTK_IS_ENTRY (entry), 0);

  layout = gtk_entry_ensure_layout (entry, TRUE);
  text = pango_layout_get_text (layout);
  cursor_index = g_utf8_offset_to_pointer (text, entry->current_pos) - text;
  
  if (text_index > cursor_index)
    text_index += entry->preedit_length;

  return text_index;
}

/**
 * gtk_entry_get_layout_offsets:
 * @entry: a #GtkEntry
 * @x: (out) (allow-none): location to store X offset of layout, or %NULL
 * @y: (out) (allow-none): location to store Y offset of layout, or %NULL
 *
 *
 * Obtains the position of the #PangoLayout used to render text
 * in the entry, in widget coordinates. Useful if you want to line
 * up the text in an entry with some other text, e.g. when using the
 * entry to implement editable cells in a sheet widget.
 *
 * Also useful to convert mouse events into coordinates inside the
 * #PangoLayout, e.g. to take some action if some part of the entry text
 * is clicked.
 * 
 * Note that as the user scrolls around in the entry the offsets will
 * change; you'll need to connect to the "notify::scroll-offset"
 * signal to track this. Remember when using the #PangoLayout
 * functions you need to convert to and from pixels using
 * PANGO_PIXELS() or #PANGO_SCALE.
 *
 * Keep in mind that the layout text may contain a preedit string, so
 * gtk_entry_layout_index_to_text_index() and
 * gtk_entry_text_index_to_layout_index() are needed to convert byte
 * indices in the layout to byte indices in the entry contents.
 **/
void
gtk_entry_get_layout_offsets (GtkEntry *entry,
                              gint     *x,
                              gint     *y)
{
  gint text_area_x, text_area_y;
  
  g_return_if_fail (GTK_IS_ENTRY (entry));

  /* this gets coords relative to text area */
  get_layout_position (entry, x, y);

  /* convert to widget coords */
  gtk_entry_get_text_area_size (entry, &text_area_x, &text_area_y, NULL, NULL);
  
  if (x)
    *x += text_area_x;

  if (y)
    *y += text_area_y;
}


/**
 * gtk_entry_set_alignment:
 * @entry: a #GtkEntry
 * @xalign: The horizontal alignment, from 0 (left) to 1 (right).
 *          Reversed for RTL layouts
 * 
 * Sets the alignment for the contents of the entry. This controls
 * the horizontal positioning of the contents when the displayed
 * text is shorter than the width of the entry.
 *
 * Since: 2.4
 **/
void
gtk_entry_set_alignment (GtkEntry *entry, gfloat xalign)
{
  GtkEntryPrivate *priv;
  
  g_return_if_fail (GTK_IS_ENTRY (entry));

  priv = GTK_ENTRY_GET_PRIVATE (entry);

  if (xalign < 0.0)
    xalign = 0.0;
  else if (xalign > 1.0)
    xalign = 1.0;

  if (xalign != priv->xalign)
    {
      priv->xalign = xalign;

      gtk_entry_recompute (entry);

      g_object_notify (G_OBJECT (entry), "xalign");
    }
}

/**
 * gtk_entry_get_alignment:
 * @entry: a #GtkEntry
 * 
 * Gets the value set by gtk_entry_set_alignment().
 * 
 * Return value: the alignment
 *
 * Since: 2.4
 **/
gfloat
gtk_entry_get_alignment (GtkEntry *entry)
{
  GtkEntryPrivate *priv;
  
  g_return_val_if_fail (GTK_IS_ENTRY (entry), 0.0);

  priv = GTK_ENTRY_GET_PRIVATE (entry);

  return priv->xalign;
}

/**
 * gtk_entry_set_icon_from_pixbuf:
 * @entry: a #GtkEntry
 * @icon_pos: Icon position
 * @pixbuf: (allow-none): A #GdkPixbuf, or %NULL
 *
 * Sets the icon shown in the specified position using a pixbuf.
 *
 * If @pixbuf is %NULL, no icon will be shown in the specified position.
 *
 * Since: 2.16
 */
void
gtk_entry_set_icon_from_pixbuf (GtkEntry             *entry,
                                GtkEntryIconPosition  icon_pos,
                                GdkPixbuf            *pixbuf)
{
  GtkEntryPrivate *priv;
  EntryIconInfo *icon_info;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (IS_VALID_ICON_POSITION (icon_pos));

  priv = GTK_ENTRY_GET_PRIVATE (entry);

  if ((icon_info = priv->icons[icon_pos]) == NULL)
    icon_info = construct_icon_info (GTK_WIDGET (entry), icon_pos);

  g_object_freeze_notify (G_OBJECT (entry));

  if (pixbuf)
    g_object_ref (pixbuf);

  gtk_entry_clear (entry, icon_pos);

  if (pixbuf)
    {
      icon_info->storage_type = GTK_IMAGE_PIXBUF;
      icon_info->pixbuf = pixbuf;

      if (icon_pos == GTK_ENTRY_ICON_PRIMARY)
        {
          g_object_notify (G_OBJECT (entry), "primary-icon-pixbuf");
          g_object_notify (G_OBJECT (entry), "primary-icon-storage-type");
        }
      else
        {
          g_object_notify (G_OBJECT (entry), "secondary-icon-pixbuf");
          g_object_notify (G_OBJECT (entry), "secondary-icon-storage-type");
        }

      if (gtk_widget_get_mapped (GTK_WIDGET (entry)))
          gdk_window_show_unraised (icon_info->window);
    }

  gtk_entry_ensure_pixbuf (entry, icon_pos);
  
  if (gtk_widget_get_visible (GTK_WIDGET (entry)))
    gtk_widget_queue_resize (GTK_WIDGET (entry));

  g_object_thaw_notify (G_OBJECT (entry));
}

/**
 * gtk_entry_set_icon_from_stock:
 * @entry: A #GtkEntry
 * @icon_pos: Icon position
 * @stock_id: (allow-none): The name of the stock item, or %NULL
 *
 * Sets the icon shown in the entry at the specified position from
 * a stock image.
 *
 * If @stock_id is %NULL, no icon will be shown in the specified position.
 *
 * Since: 2.16
 */
void
gtk_entry_set_icon_from_stock (GtkEntry             *entry,
                               GtkEntryIconPosition  icon_pos,
                               const gchar          *stock_id)
{
  GtkEntryPrivate *priv;
  EntryIconInfo *icon_info;
  gchar *new_id;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (IS_VALID_ICON_POSITION (icon_pos));

  priv = GTK_ENTRY_GET_PRIVATE (entry);

  if ((icon_info = priv->icons[icon_pos]) == NULL)
    icon_info = construct_icon_info (GTK_WIDGET (entry), icon_pos);

  g_object_freeze_notify (G_OBJECT (entry));

  gtk_widget_ensure_style (GTK_WIDGET (entry));

  /* need to dup before clearing */
  new_id = g_strdup (stock_id);

  gtk_entry_clear (entry, icon_pos);

  if (new_id != NULL)
    {
      icon_info->storage_type = GTK_IMAGE_STOCK;
      icon_info->stock_id = new_id;

      if (icon_pos == GTK_ENTRY_ICON_PRIMARY)
        {
          g_object_notify (G_OBJECT (entry), "primary-icon-stock");
          g_object_notify (G_OBJECT (entry), "primary-icon-storage-type");
        }
      else
        {
          g_object_notify (G_OBJECT (entry), "secondary-icon-stock");
          g_object_notify (G_OBJECT (entry), "secondary-icon-storage-type");
        }

      if (gtk_widget_get_mapped (GTK_WIDGET (entry)))
          gdk_window_show_unraised (icon_info->window);
    }

  gtk_entry_ensure_pixbuf (entry, icon_pos);

  if (gtk_widget_get_visible (GTK_WIDGET (entry)))
    gtk_widget_queue_resize (GTK_WIDGET (entry));

  g_object_thaw_notify (G_OBJECT (entry));
}

/**
 * gtk_entry_set_icon_from_icon_name:
 * @entry: A #GtkEntry
 * @icon_pos: The position at which to set the icon
 * @icon_name: (allow-none): An icon name, or %NULL
 *
 * Sets the icon shown in the entry at the specified position
 * from the current icon theme.
 *
 * If the icon name isn't known, a "broken image" icon will be displayed
 * instead.
 *
 * If @icon_name is %NULL, no icon will be shown in the specified position.
 *
 * Since: 2.16
 */
void
gtk_entry_set_icon_from_icon_name (GtkEntry             *entry,
                                   GtkEntryIconPosition  icon_pos,
                                   const gchar          *icon_name)
{
  GtkEntryPrivate *priv;
  EntryIconInfo *icon_info;
  gchar *new_name;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (IS_VALID_ICON_POSITION (icon_pos));

  priv = GTK_ENTRY_GET_PRIVATE (entry);

  if ((icon_info = priv->icons[icon_pos]) == NULL)
    icon_info = construct_icon_info (GTK_WIDGET (entry), icon_pos);

  g_object_freeze_notify (G_OBJECT (entry));

  gtk_widget_ensure_style (GTK_WIDGET (entry));

  /* need to dup before clearing */
  new_name = g_strdup (icon_name);

  gtk_entry_clear (entry, icon_pos);

  if (new_name != NULL)
    {
      icon_info->storage_type = GTK_IMAGE_ICON_NAME;
      icon_info->icon_name = new_name;

      if (icon_pos == GTK_ENTRY_ICON_PRIMARY)
        {
          g_object_notify (G_OBJECT (entry), "primary-icon-name");
          g_object_notify (G_OBJECT (entry), "primary-icon-storage-type");
        }
      else
        {
          g_object_notify (G_OBJECT (entry), "secondary-icon-name");
          g_object_notify (G_OBJECT (entry), "secondary-icon-storage-type");
        }

      if (gtk_widget_get_mapped (GTK_WIDGET (entry)))
          gdk_window_show_unraised (icon_info->window);
    }

  gtk_entry_ensure_pixbuf (entry, icon_pos);

  if (gtk_widget_get_visible (GTK_WIDGET (entry)))
    gtk_widget_queue_resize (GTK_WIDGET (entry));

  g_object_thaw_notify (G_OBJECT (entry));
}

/**
 * gtk_entry_set_icon_from_gicon:
 * @entry: A #GtkEntry
 * @icon_pos: The position at which to set the icon
 * @icon: (allow-none): The icon to set, or %NULL
 *
 * Sets the icon shown in the entry at the specified position
 * from the current icon theme.
 * If the icon isn't known, a "broken image" icon will be displayed
 * instead.
 *
 * If @icon is %NULL, no icon will be shown in the specified position.
 *
 * Since: 2.16
 */
void
gtk_entry_set_icon_from_gicon (GtkEntry             *entry,
                               GtkEntryIconPosition  icon_pos,
                               GIcon                *icon)
{
  GtkEntryPrivate *priv;
  EntryIconInfo *icon_info;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (IS_VALID_ICON_POSITION (icon_pos));

  priv = GTK_ENTRY_GET_PRIVATE (entry);

  if ((icon_info = priv->icons[icon_pos]) == NULL)
    icon_info = construct_icon_info (GTK_WIDGET (entry), icon_pos);

  g_object_freeze_notify (G_OBJECT (entry));

  /* need to ref before clearing */
  if (icon)
    g_object_ref (icon);

  gtk_entry_clear (entry, icon_pos);

  if (icon)
    {
      icon_info->storage_type = GTK_IMAGE_GICON;
      icon_info->gicon = icon;

      if (icon_pos == GTK_ENTRY_ICON_PRIMARY)
        {
          g_object_notify (G_OBJECT (entry), "primary-icon-gicon");
          g_object_notify (G_OBJECT (entry), "primary-icon-storage-type");
        }
      else
        {
          g_object_notify (G_OBJECT (entry), "secondary-icon-gicon");
          g_object_notify (G_OBJECT (entry), "secondary-icon-storage-type");
        }

      if (gtk_widget_get_mapped (GTK_WIDGET (entry)))
          gdk_window_show_unraised (icon_info->window);
    }

  gtk_entry_ensure_pixbuf (entry, icon_pos);

  if (gtk_widget_get_visible (GTK_WIDGET (entry)))
    gtk_widget_queue_resize (GTK_WIDGET (entry));

  g_object_thaw_notify (G_OBJECT (entry));
}

/**
 * gtk_entry_set_icon_activatable:
 * @entry: A #GtkEntry
 * @icon_pos: Icon position
 * @activatable: %TRUE if the icon should be activatable
 *
 * Sets whether the icon is activatable.
 *
 * Since: 2.16
 */
void
gtk_entry_set_icon_activatable (GtkEntry             *entry,
                                GtkEntryIconPosition  icon_pos,
                                gboolean              activatable)
{
  GtkEntryPrivate *priv;
  EntryIconInfo *icon_info;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (IS_VALID_ICON_POSITION (icon_pos));

  priv = GTK_ENTRY_GET_PRIVATE (entry);

  if ((icon_info = priv->icons[icon_pos]) == NULL)
    icon_info = construct_icon_info (GTK_WIDGET (entry), icon_pos);

  activatable = activatable != FALSE;

  if (icon_info->nonactivatable != !activatable)
    {
      icon_info->nonactivatable = !activatable;

      if (gtk_widget_get_realized (GTK_WIDGET (entry)))
        update_cursors (GTK_WIDGET (entry));

      g_object_notify (G_OBJECT (entry),
                       icon_pos == GTK_ENTRY_ICON_PRIMARY ? "primary-icon-activatable" : "secondary-icon-activatable");
    }
}

/**
 * gtk_entry_get_icon_activatable:
 * @entry: a #GtkEntry
 * @icon_pos: Icon position
 *
 * Returns whether the icon is activatable.
 *
 * Returns: %TRUE if the icon is activatable.
 *
 * Since: 2.16
 */
gboolean
gtk_entry_get_icon_activatable (GtkEntry             *entry,
                                GtkEntryIconPosition  icon_pos)
{
  GtkEntryPrivate *priv;
  EntryIconInfo *icon_info;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), FALSE);
  g_return_val_if_fail (IS_VALID_ICON_POSITION (icon_pos), FALSE);

  priv = GTK_ENTRY_GET_PRIVATE (entry);
  icon_info = priv->icons[icon_pos];

  return (icon_info != NULL && !icon_info->nonactivatable);
}

/**
 * gtk_entry_get_icon_pixbuf:
 * @entry: A #GtkEntry
 * @icon_pos: Icon position
 *
 * Retrieves the image used for the icon.
 *
 * Unlike the other methods of setting and getting icon data, this
 * method will work regardless of whether the icon was set using a
 * #GdkPixbuf, a #GIcon, a stock item, or an icon name.
 *
 * Returns: (transfer none): A #GdkPixbuf, or %NULL if no icon is
 *     set for this position.
 *
 * Since: 2.16
 */
GdkPixbuf *
gtk_entry_get_icon_pixbuf (GtkEntry             *entry,
                           GtkEntryIconPosition  icon_pos)
{
  GtkEntryPrivate *priv;
  EntryIconInfo *icon_info;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);
  g_return_val_if_fail (IS_VALID_ICON_POSITION (icon_pos), NULL);

  priv = GTK_ENTRY_GET_PRIVATE (entry);
  icon_info = priv->icons[icon_pos];

  if (!icon_info)
    return NULL;

  gtk_entry_ensure_pixbuf (entry, icon_pos);

  return icon_info->pixbuf;
}

/**
 * gtk_entry_get_icon_gicon:
 * @entry: A #GtkEntry
 * @icon_pos: Icon position
 *
 * Retrieves the #GIcon used for the icon, or %NULL if there is
 * no icon or if the icon was set by some other method (e.g., by
 * stock, pixbuf, or icon name).
 *
 * Returns: (transfer none): A #GIcon, or %NULL if no icon is set
 *     or if the icon is not a #GIcon
 *
 * Since: 2.16
 */
GIcon *
gtk_entry_get_icon_gicon (GtkEntry             *entry,
                          GtkEntryIconPosition  icon_pos)
{
  GtkEntryPrivate *priv;
  EntryIconInfo *icon_info;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);
  g_return_val_if_fail (IS_VALID_ICON_POSITION (icon_pos), NULL);

  priv = GTK_ENTRY_GET_PRIVATE (entry);
  icon_info = priv->icons[icon_pos];

  if (!icon_info)
    return NULL;

  return icon_info->storage_type == GTK_IMAGE_GICON ? icon_info->gicon : NULL;
}

/**
 * gtk_entry_get_icon_stock:
 * @entry: A #GtkEntry
 * @icon_pos: Icon position
 *
 * Retrieves the stock id used for the icon, or %NULL if there is
 * no icon or if the icon was set by some other method (e.g., by
 * pixbuf, icon name or gicon).
 *
 * Returns: A stock id, or %NULL if no icon is set or if the icon
 *          wasn't set from a stock id
 *
 * Since: 2.16
 */
const gchar *
gtk_entry_get_icon_stock (GtkEntry             *entry,
                          GtkEntryIconPosition  icon_pos)
{
  GtkEntryPrivate *priv;
  EntryIconInfo *icon_info;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);
  g_return_val_if_fail (IS_VALID_ICON_POSITION (icon_pos), NULL);

  priv = GTK_ENTRY_GET_PRIVATE (entry);
  icon_info = priv->icons[icon_pos];

  if (!icon_info)
    return NULL;

  return icon_info->storage_type == GTK_IMAGE_STOCK ? icon_info->stock_id : NULL;
}

/**
 * gtk_entry_get_icon_name:
 * @entry: A #GtkEntry
 * @icon_pos: Icon position
 *
 * Retrieves the icon name used for the icon, or %NULL if there is
 * no icon or if the icon was set by some other method (e.g., by
 * pixbuf, stock or gicon).
 *
 * Returns: An icon name, or %NULL if no icon is set or if the icon
 *          wasn't set from an icon name
 *
 * Since: 2.16
 */
const gchar *
gtk_entry_get_icon_name (GtkEntry             *entry,
                         GtkEntryIconPosition  icon_pos)
{
  GtkEntryPrivate *priv;
  EntryIconInfo *icon_info;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);
  g_return_val_if_fail (IS_VALID_ICON_POSITION (icon_pos), NULL);

  priv = GTK_ENTRY_GET_PRIVATE (entry);
  icon_info = priv->icons[icon_pos];

  if (!icon_info)
    return NULL;

  return icon_info->storage_type == GTK_IMAGE_ICON_NAME ? icon_info->icon_name : NULL;
}

/**
 * gtk_entry_set_icon_sensitive:
 * @entry: A #GtkEntry
 * @icon_pos: Icon position
 * @sensitive: Specifies whether the icon should appear
 *             sensitive or insensitive
 *
 * Sets the sensitivity for the specified icon.
 *
 * Since: 2.16
 */
void
gtk_entry_set_icon_sensitive (GtkEntry             *entry,
                              GtkEntryIconPosition  icon_pos,
                              gboolean              sensitive)
{
  GtkEntryPrivate *priv;
  EntryIconInfo *icon_info;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (IS_VALID_ICON_POSITION (icon_pos));

  priv = GTK_ENTRY_GET_PRIVATE (entry);

  if ((icon_info = priv->icons[icon_pos]) == NULL)
    icon_info = construct_icon_info (GTK_WIDGET (entry), icon_pos);

  if (icon_info->insensitive != !sensitive)
    {
      icon_info->insensitive = !sensitive;

      icon_info->pressed = FALSE;
      icon_info->prelight = FALSE;

      if (gtk_widget_get_realized (GTK_WIDGET (entry)))
        update_cursors (GTK_WIDGET (entry));

      gtk_widget_queue_draw (GTK_WIDGET (entry));

      g_object_notify (G_OBJECT (entry),
                       icon_pos == GTK_ENTRY_ICON_PRIMARY ? "primary-icon-sensitive" : "secondary-icon-sensitive");
    }
}

/**
 * gtk_entry_get_icon_sensitive:
 * @entry: a #GtkEntry
 * @icon_pos: Icon position
 *
 * Returns whether the icon appears sensitive or insensitive.
 *
 * Returns: %TRUE if the icon is sensitive.
 *
 * Since: 2.16
 */
gboolean
gtk_entry_get_icon_sensitive (GtkEntry             *entry,
                              GtkEntryIconPosition  icon_pos)
{
  GtkEntryPrivate *priv;
  EntryIconInfo *icon_info;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), TRUE);
  g_return_val_if_fail (IS_VALID_ICON_POSITION (icon_pos), TRUE);

  priv = GTK_ENTRY_GET_PRIVATE (entry);
  icon_info = priv->icons[icon_pos];

  return (!icon_info || !icon_info->insensitive);

}

/**
 * gtk_entry_get_icon_storage_type:
 * @entry: a #GtkEntry
 * @icon_pos: Icon position
 *
 * Gets the type of representation being used by the icon
 * to store image data. If the icon has no image data,
 * the return value will be %GTK_IMAGE_EMPTY.
 *
 * Return value: image representation being used
 *
 * Since: 2.16
 **/
GtkImageType
gtk_entry_get_icon_storage_type (GtkEntry             *entry,
                                 GtkEntryIconPosition  icon_pos)
{
  GtkEntryPrivate *priv;
  EntryIconInfo *icon_info;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), GTK_IMAGE_EMPTY);
  g_return_val_if_fail (IS_VALID_ICON_POSITION (icon_pos), GTK_IMAGE_EMPTY);

  priv = GTK_ENTRY_GET_PRIVATE (entry);
  icon_info = priv->icons[icon_pos];

  if (!icon_info)
    return GTK_IMAGE_EMPTY;

  return icon_info->storage_type;
}

/**
 * gtk_entry_get_icon_at_pos:
 * @entry: a #GtkEntry
 * @x: the x coordinate of the position to find
 * @y: the y coordinate of the position to find
 *
 * Finds the icon at the given position and return its index.
 * If @x, @y doesn't lie inside an icon, -1 is returned.
 * This function is intended for use in a #GtkWidget::query-tooltip
 * signal handler.
 *
 * Returns: the index of the icon at the given position, or -1
 *
 * Since: 2.16
 */
gint
gtk_entry_get_icon_at_pos (GtkEntry *entry,
                           gint      x,
                           gint      y)
{
  GtkAllocation primary;
  GtkAllocation secondary;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), -1);

  get_icon_allocations (entry, &primary, &secondary);

  if (primary.x <= x && x < primary.x + primary.width &&
      primary.y <= y && y < primary.y + primary.height)
    return GTK_ENTRY_ICON_PRIMARY;

  if (secondary.x <= x && x < secondary.x + secondary.width &&
      secondary.y <= y && y < secondary.y + secondary.height)
    return GTK_ENTRY_ICON_SECONDARY;

  return -1;
}

/**
 * gtk_entry_set_icon_drag_source:
 * @entry: a #GtkIconEntry
 * @icon_pos: icon position
 * @target_list: the targets (data formats) in which the data can be provided
 * @actions: a bitmask of the allowed drag actions
 *
 * Sets up the icon at the given position so that GTK+ will start a drag
 * operation when the user clicks and drags the icon.
 *
 * To handle the drag operation, you need to connect to the usual
 * #GtkWidget::drag-data-get (or possibly #GtkWidget::drag-data-delete)
 * signal, and use gtk_entry_get_current_icon_drag_source() in
 * your signal handler to find out if the drag was started from
 * an icon.
 *
 * By default, GTK+ uses the icon as the drag icon. You can use the 
 * #GtkWidget::drag-begin signal to set a different icon. Note that you 
 * have to use g_signal_connect_after() to ensure that your signal handler
 * gets executed after the default handler.
 *
 * Since: 2.16
 */
void
gtk_entry_set_icon_drag_source (GtkEntry             *entry,
                                GtkEntryIconPosition  icon_pos,
                                GtkTargetList        *target_list,
                                GdkDragAction         actions)
{
  GtkEntryPrivate *priv;
  EntryIconInfo *icon_info;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (IS_VALID_ICON_POSITION (icon_pos));

  priv = GTK_ENTRY_GET_PRIVATE (entry);

  if ((icon_info = priv->icons[icon_pos]) == NULL)
    icon_info = construct_icon_info (GTK_WIDGET (entry), icon_pos);

  if (icon_info->target_list)
    gtk_target_list_unref (icon_info->target_list);
  icon_info->target_list = target_list;
  if (icon_info->target_list)
    gtk_target_list_ref (icon_info->target_list);

  icon_info->actions = actions;
}

/**
 * gtk_entry_get_current_icon_drag_source:
 * @entry: a #GtkIconEntry
 *
 * Returns the index of the icon which is the source of the current
 * DND operation, or -1.
 *
 * This function is meant to be used in a #GtkWidget::drag-data-get
 * callback.
 *
 * Returns: index of the icon which is the source of the current
 *          DND operation, or -1.
 *
 * Since: 2.16
 */
gint
gtk_entry_get_current_icon_drag_source (GtkEntry *entry)
{
  GtkEntryPrivate *priv;
  EntryIconInfo *icon_info = NULL;
  gint i;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), -1);

  priv = GTK_ENTRY_GET_PRIVATE (entry);

  for (i = 0; i < MAX_ICONS; i++)
    {
      if ((icon_info = priv->icons[i]))
        {
          if (icon_info->in_drag)
            return i;
        }
    }

  return -1;
}

/**
 * gtk_entry_get_icon_window:
 * @entry: A #GtkEntry
 * @icon_pos: Icon position
 *
 * Returns the #GdkWindow which contains the entry's icon at
 * @icon_pos. This function is useful when drawing something to the
 * entry in an expose-event callback because it enables the callback
 * to distinguish between the text window and entry's icon windows.
 *
 * See also gtk_entry_get_text_window().
 *
 * Note that GTK+ 3 does not have this function anymore; it has
 * been replaced by gtk_entry_get_icon_area().
 *
 * Return value: (transfer none): the entry's icon window at @icon_pos.
 *
 * Since: 2.20
 */
GdkWindow  *
gtk_entry_get_icon_window (GtkEntry             *entry,
                           GtkEntryIconPosition  icon_pos)
{
  GtkEntryPrivate *priv;
  EntryIconInfo *icon_info;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);

  priv = GTK_ENTRY_GET_PRIVATE (entry);

  icon_info = priv->icons[icon_pos];

  if (icon_info)
    return icon_info->window;

  return NULL;
}

static void
ensure_has_tooltip (GtkEntry *entry)
{
  gchar *text = gtk_widget_get_tooltip_text (GTK_WIDGET (entry));
  gboolean has_tooltip = text != NULL;

  if (!has_tooltip)
    {
      GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (entry);
      int i;

      for (i = 0; i < MAX_ICONS; i++)
        {
          EntryIconInfo *icon_info = priv->icons[i];

          if (icon_info != NULL && icon_info->tooltip != NULL)
            {
              has_tooltip = TRUE;
              break;
            }
        }
    }
  else
    {
      g_free (text);
    }

  gtk_widget_set_has_tooltip (GTK_WIDGET (entry), has_tooltip);
}

/**
 * gtk_entry_get_icon_tooltip_text:
 * @entry: a #GtkEntry
 * @icon_pos: the icon position
 *
 * Gets the contents of the tooltip on the icon at the specified 
 * position in @entry.
 * 
 * Returns: the tooltip text, or %NULL. Free the returned string
 *     with g_free() when done.
 * 
 * Since: 2.16
 */
gchar *
gtk_entry_get_icon_tooltip_text (GtkEntry             *entry,
                                 GtkEntryIconPosition  icon_pos)
{
  GtkEntryPrivate *priv;
  EntryIconInfo *icon_info;
  gchar *text = NULL;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);
  g_return_val_if_fail (IS_VALID_ICON_POSITION (icon_pos), NULL);

  priv = GTK_ENTRY_GET_PRIVATE (entry);
  icon_info = priv->icons[icon_pos];

  if (!icon_info)
    return NULL;
 
  if (icon_info->tooltip && 
      !pango_parse_markup (icon_info->tooltip, -1, 0, NULL, &text, NULL, NULL))
    g_assert (NULL == text); /* text should still be NULL in case of markup errors */

  return text;
}

/**
 * gtk_entry_set_icon_tooltip_text:
 * @entry: a #GtkEntry
 * @icon_pos: the icon position
 * @tooltip: (allow-none): the contents of the tooltip for the icon, or %NULL
 *
 * Sets @tooltip as the contents of the tooltip for the icon
 * at the specified position.
 *
 * Use %NULL for @tooltip to remove an existing tooltip.
 *
 * See also gtk_widget_set_tooltip_text() and 
 * gtk_entry_set_icon_tooltip_markup().
 *
 * If you unset the widget tooltip via gtk_widget_set_tooltip_text() or
 * gtk_widget_set_tooltip_markup(), this sets GtkWidget:has-tooltip to %FALSE,
 * which suppresses icon tooltips too. You can resolve this by then calling
 * gtk_widget_set_has_tooltip() to set GtkWidget:has-tooltip back to %TRUE, or
 * setting at least one non-empty tooltip on any icon achieves the same result.
 *
 * Since: 2.16
 */
void
gtk_entry_set_icon_tooltip_text (GtkEntry             *entry,
                                 GtkEntryIconPosition  icon_pos,
                                 const gchar          *tooltip)
{
  GtkEntryPrivate *priv;
  EntryIconInfo *icon_info;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (IS_VALID_ICON_POSITION (icon_pos));

  priv = GTK_ENTRY_GET_PRIVATE (entry);

  if ((icon_info = priv->icons[icon_pos]) == NULL)
    icon_info = construct_icon_info (GTK_WIDGET (entry), icon_pos);

  if (icon_info->tooltip)
    g_free (icon_info->tooltip);

  /* Treat an empty string as a NULL string,
   * because an empty string would be useless for a tooltip:
   */
  if (tooltip && tooltip[0] == '\0')
    tooltip = NULL;

  icon_info->tooltip = tooltip ? g_markup_escape_text (tooltip, -1) : NULL;

  ensure_has_tooltip (entry);
}

/**
 * gtk_entry_get_icon_tooltip_markup:
 * @entry: a #GtkEntry
 * @icon_pos: the icon position
 *
 * Gets the contents of the tooltip on the icon at the specified 
 * position in @entry.
 * 
 * Returns: the tooltip text, or %NULL. Free the returned string
 *     with g_free() when done.
 * 
 * Since: 2.16
 */
gchar *
gtk_entry_get_icon_tooltip_markup (GtkEntry             *entry,
                                   GtkEntryIconPosition  icon_pos)
{
  GtkEntryPrivate *priv;
  EntryIconInfo *icon_info;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);
  g_return_val_if_fail (IS_VALID_ICON_POSITION (icon_pos), NULL);

  priv = GTK_ENTRY_GET_PRIVATE (entry);
  icon_info = priv->icons[icon_pos];

  if (!icon_info)
    return NULL;
 
  return g_strdup (icon_info->tooltip);
}

/**
 * gtk_entry_set_icon_tooltip_markup:
 * @entry: a #GtkEntry
 * @icon_pos: the icon position
 * @tooltip: (allow-none): the contents of the tooltip for the icon, or %NULL
 *
 * Sets @tooltip as the contents of the tooltip for the icon at
 * the specified position. @tooltip is assumed to be marked up with
 * the <link linkend="PangoMarkupFormat">Pango text markup language</link>.
 *
 * Use %NULL for @tooltip to remove an existing tooltip.
 *
 * See also gtk_widget_set_tooltip_markup() and 
 * gtk_entry_set_icon_tooltip_text().
 *
 * Since: 2.16
 */
void
gtk_entry_set_icon_tooltip_markup (GtkEntry             *entry,
                                   GtkEntryIconPosition  icon_pos,
                                   const gchar          *tooltip)
{
  GtkEntryPrivate *priv;
  EntryIconInfo *icon_info;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (IS_VALID_ICON_POSITION (icon_pos));

  priv = GTK_ENTRY_GET_PRIVATE (entry);

  if ((icon_info = priv->icons[icon_pos]) == NULL)
    icon_info = construct_icon_info (GTK_WIDGET (entry), icon_pos);

  if (icon_info->tooltip)
    g_free (icon_info->tooltip);

  /* Treat an empty string as a NULL string,
   * because an empty string would be useless for a tooltip:
   */
  if (tooltip && tooltip[0] == '\0')
    tooltip = NULL;

  icon_info->tooltip = g_strdup (tooltip);

  ensure_has_tooltip (entry);
}

static gboolean
gtk_entry_query_tooltip (GtkWidget  *widget,
                         gint        x,
                         gint        y,
                         gboolean    keyboard_tip,
                         GtkTooltip *tooltip)
{
  GtkEntry *entry;
  GtkEntryPrivate *priv;
  EntryIconInfo *icon_info;
  gint icon_pos;

  entry = GTK_ENTRY (widget);
  priv = GTK_ENTRY_GET_PRIVATE (entry);

  if (!keyboard_tip)
    {
      icon_pos = gtk_entry_get_icon_at_pos (entry, x, y);
      if (icon_pos != -1)
        {
          if ((icon_info = priv->icons[icon_pos]) != NULL)
            {
              if (icon_info->tooltip)
                {
                  gtk_tooltip_set_markup (tooltip, icon_info->tooltip);
                  return TRUE;
                }

              return FALSE;
            }
        }
    }

  return GTK_WIDGET_CLASS (gtk_entry_parent_class)->query_tooltip (widget,
                                                                   x, y,
                                                                   keyboard_tip,
                                                                   tooltip);
}


/* Quick hack of a popup menu
 */
static void
activate_cb (GtkWidget *menuitem,
	     GtkEntry  *entry)
{
  const gchar *signal = g_object_get_data (G_OBJECT (menuitem), "gtk-signal");
  g_signal_emit_by_name (entry, signal);
}


static gboolean
gtk_entry_mnemonic_activate (GtkWidget *widget,
			     gboolean   group_cycling)
{
  gtk_widget_grab_focus (widget);
  return TRUE;
}

static void
append_action_signal (GtkEntry     *entry,
		      GtkWidget    *menu,
		      const gchar  *stock_id,
		      const gchar  *signal,
                      gboolean      sensitive)
{
  GtkWidget *menuitem = gtk_image_menu_item_new_from_stock (stock_id, NULL);

  g_object_set_data (G_OBJECT (menuitem), I_("gtk-signal"), (char *)signal);
  g_signal_connect (menuitem, "activate",
		    G_CALLBACK (activate_cb), entry);

  gtk_widget_set_sensitive (menuitem, sensitive);
  
  gtk_widget_show (menuitem);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
}
	
static void
popup_menu_detach (GtkWidget *attach_widget,
		   GtkMenu   *menu)
{
  GTK_ENTRY (attach_widget)->popup_menu = NULL;
}

static void
popup_position_func (GtkMenu   *menu,
                     gint      *x,
                     gint      *y,
                     gboolean  *push_in,
                     gpointer	user_data)
{
  GtkEntry *entry = GTK_ENTRY (user_data);
  GtkWidget *widget = GTK_WIDGET (entry);
  GdkScreen *screen;
  GtkRequisition menu_req;
  GdkRectangle monitor;
  GtkBorder inner_border;
  gint monitor_num, strong_x, height;
 
  g_return_if_fail (gtk_widget_get_realized (widget));

  gdk_window_get_origin (entry->text_area, x, y);

  screen = gtk_widget_get_screen (widget);
  monitor_num = gdk_screen_get_monitor_at_window (screen, entry->text_area);
  if (monitor_num < 0)
    monitor_num = 0;
  gtk_menu_set_monitor (menu, monitor_num);

  gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);
  gtk_widget_size_request (entry->popup_menu, &menu_req);
  height = gdk_window_get_height (entry->text_area);
  gtk_entry_get_cursor_locations (entry, CURSOR_STANDARD, &strong_x, NULL);
  _gtk_entry_effective_inner_border (entry, &inner_border);

  *x += inner_border.left + strong_x - entry->scroll_offset;
  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    *x -= menu_req.width;

  if ((*y + height + menu_req.height) <= monitor.y + monitor.height)
    *y += height;
  else if ((*y - menu_req.height) >= monitor.y)
    *y -= menu_req.height;
  else if (monitor.y + monitor.height - (*y + height) > *y)
    *y += height;
  else
    *y -= menu_req.height;

  *push_in = FALSE;
}

static void
unichar_chosen_func (const char *text,
                     gpointer    data)
{
  GtkEntry *entry = GTK_ENTRY (data);

  if (entry->editable)
    gtk_entry_enter_text (entry, text);
}

typedef struct
{
  GtkEntry *entry;
  gint button;
  guint time;
} PopupInfo;

static void
popup_targets_received (GtkClipboard     *clipboard,
			GtkSelectionData *data,
			gpointer          user_data)
{
  PopupInfo *info = user_data;
  GtkEntry *entry = info->entry;
  
  if (gtk_widget_get_realized (GTK_WIDGET (entry)))
    {
      DisplayMode mode;
      gboolean clipboard_contains_text;
      GtkWidget *menuitem;
      GtkWidget *submenu;
      gboolean show_input_method_menu;
      gboolean show_unicode_menu;
      
      clipboard_contains_text = gtk_selection_data_targets_include_text (data);
      if (entry->popup_menu)
	gtk_widget_destroy (entry->popup_menu);
      
      entry->popup_menu = gtk_menu_new ();
      
      gtk_menu_attach_to_widget (GTK_MENU (entry->popup_menu),
				 GTK_WIDGET (entry),
				 popup_menu_detach);
      
      mode = gtk_entry_get_display_mode (entry);
      append_action_signal (entry, entry->popup_menu, GTK_STOCK_CUT, "cut-clipboard",
			    entry->editable && mode == DISPLAY_NORMAL &&
			    entry->current_pos != entry->selection_bound);

      append_action_signal (entry, entry->popup_menu, GTK_STOCK_COPY, "copy-clipboard",
                            mode == DISPLAY_NORMAL &&
                            entry->current_pos != entry->selection_bound);

      append_action_signal (entry, entry->popup_menu, GTK_STOCK_PASTE, "paste-clipboard",
			    entry->editable && clipboard_contains_text);

      menuitem = gtk_image_menu_item_new_from_stock (GTK_STOCK_DELETE, NULL);
      gtk_widget_set_sensitive (menuitem, entry->editable && entry->current_pos != entry->selection_bound);
      g_signal_connect_swapped (menuitem, "activate",
			        G_CALLBACK (gtk_entry_delete_cb), entry);
      gtk_widget_show (menuitem);
      gtk_menu_shell_append (GTK_MENU_SHELL (entry->popup_menu), menuitem);

      menuitem = gtk_separator_menu_item_new ();
      gtk_widget_show (menuitem);
      gtk_menu_shell_append (GTK_MENU_SHELL (entry->popup_menu), menuitem);
      
      menuitem = gtk_image_menu_item_new_from_stock (GTK_STOCK_SELECT_ALL, NULL);
      g_signal_connect_swapped (menuitem, "activate",
			        G_CALLBACK (gtk_entry_select_all), entry);
      gtk_widget_show (menuitem);
      gtk_menu_shell_append (GTK_MENU_SHELL (entry->popup_menu), menuitem);
      
      g_object_get (gtk_widget_get_settings (GTK_WIDGET (entry)),
                    "gtk-show-input-method-menu", &show_input_method_menu,
                    "gtk-show-unicode-menu", &show_unicode_menu,
                    NULL);

      if (show_input_method_menu || show_unicode_menu)
        {
          menuitem = gtk_separator_menu_item_new ();
          gtk_widget_show (menuitem);
          gtk_menu_shell_append (GTK_MENU_SHELL (entry->popup_menu), menuitem);
        }
      
      if (show_input_method_menu)
        {
          menuitem = gtk_menu_item_new_with_mnemonic (_("Input _Methods"));
          gtk_widget_set_sensitive (menuitem, entry->editable);      
          gtk_widget_show (menuitem);
          submenu = gtk_menu_new ();
          gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), submenu);
          
          gtk_menu_shell_append (GTK_MENU_SHELL (entry->popup_menu), menuitem);
      
          gtk_im_multicontext_append_menuitems (GTK_IM_MULTICONTEXT (entry->im_context),
                                                GTK_MENU_SHELL (submenu));
        }
      
      if (show_unicode_menu)
        {
          menuitem = gtk_menu_item_new_with_mnemonic (_("_Insert Unicode Control Character"));
          gtk_widget_set_sensitive (menuitem, entry->editable);      
          gtk_widget_show (menuitem);
          
          submenu = gtk_menu_new ();
          gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), submenu);
          gtk_menu_shell_append (GTK_MENU_SHELL (entry->popup_menu), menuitem);      
          
          _gtk_text_util_append_special_char_menuitems (GTK_MENU_SHELL (submenu),
                                                        unichar_chosen_func,
                                                        entry);
        }
      
      g_signal_emit (entry,
		     signals[POPULATE_POPUP],
		     0,
		     entry->popup_menu);
  

      if (info->button)
	gtk_menu_popup (GTK_MENU (entry->popup_menu), NULL, NULL,
			NULL, NULL,
			info->button, info->time);
      else
	{
	  gtk_menu_popup (GTK_MENU (entry->popup_menu), NULL, NULL,
			  popup_position_func, entry,
			  info->button, info->time);
	  gtk_menu_shell_select_first (GTK_MENU_SHELL (entry->popup_menu), FALSE);
	}
    }

  g_object_unref (entry);
  g_slice_free (PopupInfo, info);
}
			
static void
gtk_entry_do_popup (GtkEntry       *entry,
                    GdkEventButton *event)
{
  PopupInfo *info = g_slice_new (PopupInfo);

  /* In order to know what entries we should make sensitive, we
   * ask for the current targets of the clipboard, and when
   * we get them, then we actually pop up the menu.
   */
  info->entry = g_object_ref (entry);
  
  if (event)
    {
      info->button = event->button;
      info->time = event->time;
    }
  else
    {
      info->button = 0;
      info->time = gtk_get_current_event_time ();
    }

  gtk_clipboard_request_contents (gtk_widget_get_clipboard (GTK_WIDGET (entry), GDK_SELECTION_CLIPBOARD),
				  gdk_atom_intern_static_string ("TARGETS"),
				  popup_targets_received,
				  info);
}

static gboolean
gtk_entry_popup_menu (GtkWidget *widget)
{
  gtk_entry_do_popup (GTK_ENTRY (widget), NULL);
  return TRUE;
}

static void
gtk_entry_drag_begin (GtkWidget      *widget,
                      GdkDragContext *context)
{
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (widget);
  gint i;

  for (i = 0; i < MAX_ICONS; i++)
    {
      EntryIconInfo *icon_info = priv->icons[i];
      
      if (icon_info != NULL)
        {
          if (icon_info->in_drag) 
            {
              switch (icon_info->storage_type)
                {
                case GTK_IMAGE_STOCK:
                  gtk_drag_set_icon_stock (context, icon_info->stock_id, -2, -2);
                  break;

                case GTK_IMAGE_ICON_NAME:
                  gtk_drag_set_icon_name (context, icon_info->icon_name, -2, -2);
                  break;

                  /* FIXME: No GIcon support for dnd icons */
                case GTK_IMAGE_GICON:
                case GTK_IMAGE_PIXBUF:
                  gtk_drag_set_icon_pixbuf (context, icon_info->pixbuf, -2, -2);
                  break;
                default:
                  g_assert_not_reached ();
                }
            }
        }
    }
}

static void
gtk_entry_drag_end (GtkWidget      *widget,
                    GdkDragContext *context)
{
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (widget);
  gint i;

  for (i = 0; i < MAX_ICONS; i++)
    {
      EntryIconInfo *icon_info = priv->icons[i];

      if (icon_info != NULL)
        icon_info->in_drag = 0;
    }
}

static void
gtk_entry_drag_leave (GtkWidget        *widget,
		      GdkDragContext   *context,
		      guint             time)
{
  GtkEntry *entry = GTK_ENTRY (widget);

  entry->dnd_position = -1;
  gtk_widget_queue_draw (widget);
}

static gboolean
gtk_entry_drag_drop  (GtkWidget        *widget,
		      GdkDragContext   *context,
		      gint              x,
		      gint              y,
		      guint             time)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GdkAtom target = GDK_NONE;
  
  if (entry->editable)
    target = gtk_drag_dest_find_target (widget, context, NULL);

  if (target != GDK_NONE)
    gtk_drag_get_data (widget, context, target, time);
  else
    gtk_drag_finish (context, FALSE, FALSE, time);
  
  return TRUE;
}

static gboolean
gtk_entry_drag_motion (GtkWidget        *widget,
		       GdkDragContext   *context,
		       gint              x,
		       gint              y,
		       guint             time)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkWidget *source_widget;
  GdkDragAction suggested_action;
  gint new_position, old_position;
  gint sel1, sel2;
  
  x -= widget->style->xthickness;
  y -= widget->style->ythickness;
  
  old_position = entry->dnd_position;
  new_position = gtk_entry_find_position (entry, x + entry->scroll_offset);

  if (entry->editable &&
      gtk_drag_dest_find_target (widget, context, NULL) != GDK_NONE)
    {
      source_widget = gtk_drag_get_source_widget (context);
      suggested_action = gdk_drag_context_get_suggested_action (context);

      if (!gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &sel1, &sel2) ||
          new_position < sel1 || new_position > sel2)
        {
          if (source_widget == widget)
	    {
	      /* Default to MOVE, unless the user has
	       * pressed ctrl or alt to affect available actions
	       */
	      if ((gdk_drag_context_get_actions (context) & GDK_ACTION_MOVE) != 0)
	        suggested_action = GDK_ACTION_MOVE;
	    }
              
          entry->dnd_position = new_position;
        }
      else
        {
          if (source_widget == widget)
	    suggested_action = 0;	/* Can't drop in selection where drag started */
          
          entry->dnd_position = -1;
        }
    }
  else
    {
      /* Entry not editable, or no text */
      suggested_action = 0;
      entry->dnd_position = -1;
    }
  
  gdk_drag_status (context, suggested_action, time);
  
  if (entry->dnd_position != old_position)
    gtk_widget_queue_draw (widget);

  return TRUE;
}

static void
gtk_entry_drag_data_received (GtkWidget        *widget,
			      GdkDragContext   *context,
			      gint              x,
			      gint              y,
			      GtkSelectionData *selection_data,
			      guint             info,
			      guint             time)
{
  GtkEntry *entry = GTK_ENTRY (widget);
  GtkEditable *editable = GTK_EDITABLE (widget);
  gchar *str;

  str = (gchar *) gtk_selection_data_get_text (selection_data);

  x -= widget->style->xthickness;
  y -= widget->style->ythickness;

  if (str && entry->editable)
    {
      gint new_position;
      gint sel1, sel2;
      gint length = -1;

      if (entry->truncate_multiline)
        length = truncate_multiline (str);

      new_position = gtk_entry_find_position (entry, x + entry->scroll_offset);

      if (!gtk_editable_get_selection_bounds (editable, &sel1, &sel2) ||
	  new_position < sel1 || new_position > sel2)
	{
	  gtk_editable_insert_text (editable, str, length, &new_position);
	}
      else
	{
	  /* Replacing selection */
          begin_change (entry);
	  gtk_editable_delete_text (editable, sel1, sel2);
	  gtk_editable_insert_text (editable, str, length, &sel1);
          end_change (entry);
	}
      
      gtk_drag_finish (context, TRUE, gdk_drag_context_get_selected_action (context) == GDK_ACTION_MOVE, time);
    }
  else
    {
      /* Drag and drop didn't happen! */
      gtk_drag_finish (context, FALSE, FALSE, time);
    }

  g_free (str);
}

static void
gtk_entry_drag_data_get (GtkWidget        *widget,
			 GdkDragContext   *context,
			 GtkSelectionData *selection_data,
			 guint             info,
			 guint             time)
{
  gint sel_start, sel_end;
  gint i;

  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (widget);
  GtkEditable *editable = GTK_EDITABLE (widget);

  /* If there is an icon drag going on, exit early. */
  for (i = 0; i < MAX_ICONS; i++)
    {
      EntryIconInfo *icon_info = priv->icons[i];

      if (icon_info != NULL)
        {
          if (icon_info->in_drag)
            return;
        }
    }

  if (gtk_editable_get_selection_bounds (editable, &sel_start, &sel_end))
    {
      gchar *str = gtk_entry_get_display_text (GTK_ENTRY (widget), sel_start, sel_end);

      gtk_selection_data_set_text (selection_data, str, -1);
      
      g_free (str);
    }

}

static void
gtk_entry_drag_data_delete (GtkWidget      *widget,
			    GdkDragContext *context)
{
  gint sel_start, sel_end;
  gint i;

  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (widget);
  GtkEditable *editable = GTK_EDITABLE (widget);

  /* If there is an icon drag going on, exit early. */
  for (i = 0; i < MAX_ICONS; i++)
    {
      EntryIconInfo *icon_info = priv->icons[i];

      if (icon_info != NULL)
        {
          if (icon_info->in_drag)
            return;
        }
    }
  
  if (GTK_ENTRY (widget)->editable &&
      gtk_editable_get_selection_bounds (editable, &sel_start, &sel_end))
    gtk_editable_delete_text (editable, sel_start, sel_end);
}

/* We display the cursor when
 *
 *  - the selection is empty, AND
 *  - the widget has focus
 */

#define CURSOR_ON_MULTIPLIER 2
#define CURSOR_OFF_MULTIPLIER 1
#define CURSOR_PEND_MULTIPLIER 3
#define CURSOR_DIVIDER 3

static gboolean
cursor_blinks (GtkEntry *entry)
{
  if (gtk_widget_has_focus (GTK_WIDGET (entry)) &&
      entry->editable &&
      entry->selection_bound == entry->current_pos)
    {
      GtkSettings *settings;
      gboolean blink;

      settings = gtk_widget_get_settings (GTK_WIDGET (entry));
      g_object_get (settings, "gtk-cursor-blink", &blink, NULL);

      return blink;
    }
  else
    return FALSE;
}

static gint
get_cursor_time (GtkEntry *entry)
{
  GtkSettings *settings = gtk_widget_get_settings (GTK_WIDGET (entry));
  gint time;

  g_object_get (settings, "gtk-cursor-blink-time", &time, NULL);

  return time;
}

static gint
get_cursor_blink_timeout (GtkEntry *entry)
{
  GtkSettings *settings = gtk_widget_get_settings (GTK_WIDGET (entry));
  gint timeout;

  g_object_get (settings, "gtk-cursor-blink-timeout", &timeout, NULL);

  return timeout;
}

static void
show_cursor (GtkEntry *entry)
{
  GtkWidget *widget;

  if (!entry->cursor_visible)
    {
      entry->cursor_visible = TRUE;

      widget = GTK_WIDGET (entry);
      if (gtk_widget_has_focus (widget) && entry->selection_bound == entry->current_pos)
	gtk_widget_queue_draw (widget);
    }
}

static void
hide_cursor (GtkEntry *entry)
{
  GtkWidget *widget;

  if (entry->cursor_visible)
    {
      entry->cursor_visible = FALSE;

      widget = GTK_WIDGET (entry);
      if (gtk_widget_has_focus (widget) && entry->selection_bound == entry->current_pos)
	gtk_widget_queue_draw (widget);
    }
}

/*
 * Blink!
 */
static gint
blink_cb (gpointer data)
{
  GtkEntry *entry;
  GtkEntryPrivate *priv; 
  gint blink_timeout;

  entry = GTK_ENTRY (data);
  priv = GTK_ENTRY_GET_PRIVATE (entry);
 
  if (!gtk_widget_has_focus (GTK_WIDGET (entry)))
    {
      g_warning ("GtkEntry - did not receive focus-out-event. If you\n"
		 "connect a handler to this signal, it must return\n"
		 "FALSE so the entry gets the event as well");

      gtk_entry_check_cursor_blink (entry);

      return FALSE;
    }
  
  g_assert (entry->selection_bound == entry->current_pos);
  
  blink_timeout = get_cursor_blink_timeout (entry);
  if (priv->blink_time > 1000 * blink_timeout && 
      blink_timeout < G_MAXINT/1000) 
    {
      /* we've blinked enough without the user doing anything, stop blinking */
      show_cursor (entry);
      entry->blink_timeout = 0;
    } 
  else if (entry->cursor_visible)
    {
      hide_cursor (entry);
      entry->blink_timeout = gdk_threads_add_timeout (get_cursor_time (entry) * CURSOR_OFF_MULTIPLIER / CURSOR_DIVIDER,
					    blink_cb,
					    entry);
    }
  else
    {
      show_cursor (entry);
      priv->blink_time += get_cursor_time (entry);
      entry->blink_timeout = gdk_threads_add_timeout (get_cursor_time (entry) * CURSOR_ON_MULTIPLIER / CURSOR_DIVIDER,
					    blink_cb,
					    entry);
    }

  /* Remove ourselves */
  return FALSE;
}

static void
gtk_entry_check_cursor_blink (GtkEntry *entry)
{
  GtkEntryPrivate *priv; 
  
  priv = GTK_ENTRY_GET_PRIVATE (entry);

  if (cursor_blinks (entry))
    {
      if (!entry->blink_timeout)
	{
	  show_cursor (entry);
	  entry->blink_timeout = gdk_threads_add_timeout (get_cursor_time (entry) * CURSOR_ON_MULTIPLIER / CURSOR_DIVIDER,
						blink_cb,
						entry);
	}
    }
  else
    {
      if (entry->blink_timeout)  
	{ 
	  g_source_remove (entry->blink_timeout);
	  entry->blink_timeout = 0;
	}
      
      entry->cursor_visible = TRUE;
    }
  
}

static void
gtk_entry_pend_cursor_blink (GtkEntry *entry)
{
  if (cursor_blinks (entry))
    {
      if (entry->blink_timeout != 0)
	g_source_remove (entry->blink_timeout);
      
      entry->blink_timeout = gdk_threads_add_timeout (get_cursor_time (entry) * CURSOR_PEND_MULTIPLIER / CURSOR_DIVIDER,
					    blink_cb,
					    entry);
      show_cursor (entry);
    }
}

static void
gtk_entry_reset_blink_time (GtkEntry *entry)
{
  GtkEntryPrivate *priv; 
  
  priv = GTK_ENTRY_GET_PRIVATE (entry);
  
  priv->blink_time = 0;
}


/* completion */
static gint
gtk_entry_completion_timeout (gpointer data)
{
  GtkEntryCompletion *completion = GTK_ENTRY_COMPLETION (data);

  completion->priv->completion_timeout = 0;

  if (completion->priv->filter_model &&
      g_utf8_strlen (gtk_entry_get_text (GTK_ENTRY (completion->priv->entry)), -1)
      >= completion->priv->minimum_key_length)
    {
      gint matches;
      gint actions;
      GtkTreeSelection *s;
      gboolean popup_single;

      gtk_entry_completion_complete (completion);
      matches = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (completion->priv->filter_model), NULL);

      gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (GTK_TREE_VIEW (completion->priv->tree_view)));

      s = gtk_tree_view_get_selection (GTK_TREE_VIEW (completion->priv->action_view));

      gtk_tree_selection_unselect_all (s);

      actions = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (completion->priv->actions), NULL);

      g_object_get (completion, "popup-single-match", &popup_single, NULL);
      if ((matches > (popup_single ? 0: 1)) || actions > 0)
	{ 
	  if (gtk_widget_get_visible (completion->priv->popup_window))
	    _gtk_entry_completion_resize_popup (completion);
          else
	    _gtk_entry_completion_popup (completion);
	}
      else 
	_gtk_entry_completion_popdown (completion);
    }
  else if (gtk_widget_get_visible (completion->priv->popup_window))
    _gtk_entry_completion_popdown (completion);

  return FALSE;
}

static inline gboolean
keyval_is_cursor_move (guint keyval)
{
  if (keyval == GDK_Up || keyval == GDK_KP_Up)
    return TRUE;

  if (keyval == GDK_Down || keyval == GDK_KP_Down)
    return TRUE;

  if (keyval == GDK_Page_Up)
    return TRUE;

  if (keyval == GDK_Page_Down)
    return TRUE;

  return FALSE;
}

static gboolean
gtk_entry_completion_key_press (GtkWidget   *widget,
                                GdkEventKey *event,
                                gpointer     user_data)
{
  gint matches, actions = 0;
  GtkEntryCompletion *completion = GTK_ENTRY_COMPLETION (user_data);

  if (!gtk_widget_get_mapped (completion->priv->popup_window))
    return FALSE;

  matches = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (completion->priv->filter_model), NULL);

  if (completion->priv->actions)
    actions = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (completion->priv->actions), NULL);

  if (keyval_is_cursor_move (event->keyval))
    {
      GtkTreePath *path = NULL;
      
      if (event->keyval == GDK_Up || event->keyval == GDK_KP_Up)
        {
	  if (completion->priv->current_selected < 0)
	    completion->priv->current_selected = matches + actions - 1;
	  else
	    completion->priv->current_selected--;
        }
      else if (event->keyval == GDK_Down || event->keyval == GDK_KP_Down)
        {
          if (completion->priv->current_selected < matches + actions - 1)
	    completion->priv->current_selected++;
	  else
            completion->priv->current_selected = -1;
        }
      else if (event->keyval == GDK_Page_Up)
	{
	  if (completion->priv->current_selected < 0)
	    completion->priv->current_selected = matches + actions - 1;
	  else if (completion->priv->current_selected == 0)
	    completion->priv->current_selected = -1;
	  else if (completion->priv->current_selected < matches) 
	    {
	      completion->priv->current_selected -= 14;
	      if (completion->priv->current_selected < 0)
		completion->priv->current_selected = 0;
	    }
	  else 
	    {
	      completion->priv->current_selected -= 14;
	      if (completion->priv->current_selected < matches - 1)
		completion->priv->current_selected = matches - 1;
	    }
	}
      else if (event->keyval == GDK_Page_Down)
	{
	  if (completion->priv->current_selected < 0)
	    completion->priv->current_selected = 0;
	  else if (completion->priv->current_selected < matches - 1)
	    {
	      completion->priv->current_selected += 14;
	      if (completion->priv->current_selected > matches - 1)
		completion->priv->current_selected = matches - 1;
	    }
	  else if (completion->priv->current_selected == matches + actions - 1)
	    {
	      completion->priv->current_selected = -1;
	    }
	  else
	    {
	      completion->priv->current_selected += 14;
	      if (completion->priv->current_selected > matches + actions - 1)
		completion->priv->current_selected = matches + actions - 1;
	    }
	}

      if (completion->priv->current_selected < 0)
        {
          gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (GTK_TREE_VIEW (completion->priv->tree_view)));
          gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (GTK_TREE_VIEW (completion->priv->action_view)));

          if (completion->priv->inline_selection &&
              completion->priv->completion_prefix)
            {
              gtk_entry_set_text (GTK_ENTRY (completion->priv->entry), 
                                  completion->priv->completion_prefix);
              gtk_editable_set_position (GTK_EDITABLE (widget), -1);
            }
        }
      else if (completion->priv->current_selected < matches)
        {
          gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (GTK_TREE_VIEW (completion->priv->action_view)));

          path = gtk_tree_path_new_from_indices (completion->priv->current_selected, -1);
          gtk_tree_view_set_cursor (GTK_TREE_VIEW (completion->priv->tree_view),
                                    path, NULL, FALSE);

          if (completion->priv->inline_selection)
            {

              GtkTreeIter iter;
              GtkTreeIter child_iter;
              GtkTreeModel *model = NULL;
              GtkTreeSelection *sel;
              gboolean entry_set;

              sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (completion->priv->tree_view));
              if (!gtk_tree_selection_get_selected (sel, &model, &iter))
                return FALSE;

              gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model), &child_iter, &iter);
              model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));
              
              if (completion->priv->completion_prefix == NULL)
                completion->priv->completion_prefix = g_strdup (gtk_entry_get_text (GTK_ENTRY (completion->priv->entry)));

              g_signal_emit_by_name (completion, "cursor-on-match", model,
                                     &child_iter, &entry_set);
            }
        }
      else if (completion->priv->current_selected - matches >= 0)
        {
          gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (GTK_TREE_VIEW (completion->priv->tree_view)));

          path = gtk_tree_path_new_from_indices (completion->priv->current_selected - matches, -1);
          gtk_tree_view_set_cursor (GTK_TREE_VIEW (completion->priv->action_view),
                                    path, NULL, FALSE);

          if (completion->priv->inline_selection &&
              completion->priv->completion_prefix)
            {
              gtk_entry_set_text (GTK_ENTRY (completion->priv->entry), 
                                  completion->priv->completion_prefix);
              gtk_editable_set_position (GTK_EDITABLE (widget), -1);
            }
        }

      gtk_tree_path_free (path);

      return TRUE;
    }
  else if (event->keyval == GDK_Escape ||
           event->keyval == GDK_Left ||
           event->keyval == GDK_KP_Left ||
           event->keyval == GDK_Right ||
           event->keyval == GDK_KP_Right) 
    {
      gboolean retval = TRUE;

      _gtk_entry_reset_im_context (GTK_ENTRY (widget));
      _gtk_entry_completion_popdown (completion);

      if (completion->priv->current_selected < 0)
        {
          retval = FALSE;
          goto keypress_completion_out;
        }
      else if (completion->priv->inline_selection)
        {
          /* Escape rejects the tentative completion */
          if (event->keyval == GDK_Escape)
            {
              if (completion->priv->completion_prefix)
                gtk_entry_set_text (GTK_ENTRY (completion->priv->entry), 
                                    completion->priv->completion_prefix);
              else 
                gtk_entry_set_text (GTK_ENTRY (completion->priv->entry), "");
            }

          /* Move the cursor to the end for Right/Esc, to the
             beginning for Left */
          if (event->keyval == GDK_Right ||
              event->keyval == GDK_KP_Right ||
              event->keyval == GDK_Escape)
            gtk_editable_set_position (GTK_EDITABLE (widget), -1);
          else
            gtk_editable_set_position (GTK_EDITABLE (widget), 0);
        }

keypress_completion_out:
      if (completion->priv->inline_selection)
        {
          g_free (completion->priv->completion_prefix);
          completion->priv->completion_prefix = NULL;
        }

      return retval;
    }
  else if (event->keyval == GDK_Tab || 
	   event->keyval == GDK_KP_Tab ||
	   event->keyval == GDK_ISO_Left_Tab) 
    {
      _gtk_entry_reset_im_context (GTK_ENTRY (widget));
      _gtk_entry_completion_popdown (completion);

      g_free (completion->priv->completion_prefix);
      completion->priv->completion_prefix = NULL;

      return FALSE;
    }
  else if (event->keyval == GDK_ISO_Enter ||
           event->keyval == GDK_KP_Enter ||
	   event->keyval == GDK_Return)
    {
      GtkTreeIter iter;
      GtkTreeModel *model = NULL;
      GtkTreeModel *child_model;
      GtkTreeIter child_iter;
      GtkTreeSelection *sel;
      gboolean retval = TRUE;

      _gtk_entry_reset_im_context (GTK_ENTRY (widget));
      _gtk_entry_completion_popdown (completion);

      if (completion->priv->current_selected < matches)
        {
          gboolean entry_set;

          sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (completion->priv->tree_view));
          if (gtk_tree_selection_get_selected (sel, &model, &iter))
            {
              gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model), &child_iter, &iter);
              child_model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));
              g_signal_handler_block (widget, completion->priv->changed_id);
              g_signal_emit_by_name (completion, "match-selected",
                                     child_model, &child_iter, &entry_set);
              g_signal_handler_unblock (widget, completion->priv->changed_id);

              if (!entry_set)
                {
                  gchar *str = NULL;

                  gtk_tree_model_get (model, &iter,
                                      completion->priv->text_column, &str,
                                      -1);

                  gtk_entry_set_text (GTK_ENTRY (widget), str);

                  /* move the cursor to the end */
                  gtk_editable_set_position (GTK_EDITABLE (widget), -1);

                  g_free (str);
                }
            }
          else
            retval = FALSE;
        }
      else if (completion->priv->current_selected - matches >= 0)
        {
          sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (completion->priv->action_view));
          if (gtk_tree_selection_get_selected (sel, &model, &iter))
            {
              GtkTreePath *path;

              path = gtk_tree_path_new_from_indices (completion->priv->current_selected - matches, -1);
              g_signal_emit_by_name (completion, "action-activated",
                                     gtk_tree_path_get_indices (path)[0]);
              gtk_tree_path_free (path);
            }
          else
            retval = FALSE;
        }

      g_free (completion->priv->completion_prefix);
      completion->priv->completion_prefix = NULL;

      return retval;
    }

  return FALSE;
}

static void
gtk_entry_completion_changed (GtkWidget *entry,
                              gpointer   user_data)
{
  GtkEntryCompletion *completion = GTK_ENTRY_COMPLETION (user_data);

  if (!completion->priv->popup_completion)
    return;

  /* (re)install completion timeout */
  if (completion->priv->completion_timeout)
    {
      g_source_remove (completion->priv->completion_timeout);
      completion->priv->completion_timeout = 0;
    }

  if (!gtk_entry_get_text (GTK_ENTRY (entry)))
    return;

  /* no need to normalize for this test */
  if (completion->priv->minimum_key_length > 0 &&
      strcmp ("", gtk_entry_get_text (GTK_ENTRY (entry))) == 0)
    {
      if (gtk_widget_get_visible (completion->priv->popup_window))
        _gtk_entry_completion_popdown (completion);
      return;
    }

  completion->priv->completion_timeout =
    gdk_threads_add_timeout (COMPLETION_TIMEOUT,
                   gtk_entry_completion_timeout,
                   completion);
}

static gboolean
check_completion_callback (GtkEntryCompletion *completion)
{
  completion->priv->check_completion_idle = NULL;
  
  gtk_entry_completion_complete (completion);
  gtk_entry_completion_insert_prefix (completion);

  return FALSE;
}

static void
clear_completion_callback (GtkEntry   *entry,
			   GParamSpec *pspec)
{
  GtkEntryCompletion *completion = gtk_entry_get_completion (entry);

  if (!completion->priv->inline_completion)
    return;

  if (pspec->name == I_("cursor-position") ||
      pspec->name == I_("selection-bound"))
    completion->priv->has_completion = FALSE;
}

static gboolean
accept_completion_callback (GtkEntry *entry)
{
  GtkEntryCompletion *completion = gtk_entry_get_completion (entry);

  if (!completion->priv->inline_completion)
    return FALSE;

  if (completion->priv->has_completion)
    gtk_editable_set_position (GTK_EDITABLE (entry),
			       gtk_entry_buffer_get_length (get_buffer (entry)));

  return FALSE;
}

static void
completion_insert_text_callback (GtkEntry           *entry,
				 const gchar        *text,
				 gint                length,
				 gint                position,
				 GtkEntryCompletion *completion)
{
  if (!completion->priv->inline_completion)
    return;

  /* idle to update the selection based on the file list */
  if (completion->priv->check_completion_idle == NULL)
    {
      completion->priv->check_completion_idle = g_idle_source_new ();
      g_source_set_priority (completion->priv->check_completion_idle, G_PRIORITY_HIGH);
      g_source_set_closure (completion->priv->check_completion_idle,
			    g_cclosure_new_object (G_CALLBACK (check_completion_callback),
						   G_OBJECT (completion)));
      g_source_attach (completion->priv->check_completion_idle, NULL);
    }
}

static void
disconnect_completion_signals (GtkEntry           *entry,
			       GtkEntryCompletion *completion)
{
  if (completion->priv->changed_id > 0 &&
      g_signal_handler_is_connected (entry, completion->priv->changed_id))
    {
      g_signal_handler_disconnect (entry, completion->priv->changed_id);
      completion->priv->changed_id = 0;
    }
  g_signal_handlers_disconnect_by_func (entry, 
					G_CALLBACK (gtk_entry_completion_key_press), completion);
  if (completion->priv->insert_text_id > 0 &&
      g_signal_handler_is_connected (entry, completion->priv->insert_text_id))
    {
      g_signal_handler_disconnect (entry, completion->priv->insert_text_id);
      completion->priv->insert_text_id = 0;
    }
  g_signal_handlers_disconnect_by_func (entry, 
					G_CALLBACK (completion_insert_text_callback), completion);
  g_signal_handlers_disconnect_by_func (entry, 
					G_CALLBACK (clear_completion_callback), completion);
  g_signal_handlers_disconnect_by_func (entry, 
					G_CALLBACK (accept_completion_callback), completion);
}

static void
connect_completion_signals (GtkEntry           *entry,
			    GtkEntryCompletion *completion)
{
  completion->priv->changed_id =
    g_signal_connect (entry, "changed",
                      G_CALLBACK (gtk_entry_completion_changed), completion);
  g_signal_connect (entry, "key-press-event",
                    G_CALLBACK (gtk_entry_completion_key_press), completion);

  completion->priv->insert_text_id =
    g_signal_connect (entry, "insert-text",
                      G_CALLBACK (completion_insert_text_callback), completion);
  g_signal_connect (entry, "notify",
                    G_CALLBACK (clear_completion_callback), completion);
  g_signal_connect (entry, "activate",
                    G_CALLBACK (accept_completion_callback), completion);
  g_signal_connect (entry, "focus-out-event",
                    G_CALLBACK (accept_completion_callback), completion);
}

/**
 * gtk_entry_set_completion:
 * @entry: A #GtkEntry
 * @completion: (allow-none): The #GtkEntryCompletion or %NULL
 *
 * Sets @completion to be the auxiliary completion object to use with @entry.
 * All further configuration of the completion mechanism is done on
 * @completion using the #GtkEntryCompletion API. Completion is disabled if
 * @completion is set to %NULL.
 *
 * Since: 2.4
 */
void
gtk_entry_set_completion (GtkEntry           *entry,
                          GtkEntryCompletion *completion)
{
  GtkEntryCompletion *old;

  g_return_if_fail (GTK_IS_ENTRY (entry));
  g_return_if_fail (!completion || GTK_IS_ENTRY_COMPLETION (completion));

  old = gtk_entry_get_completion (entry);

  if (old == completion)
    return;
  
  if (old)
    {
      if (old->priv->completion_timeout)
        {
          g_source_remove (old->priv->completion_timeout);
          old->priv->completion_timeout = 0;
        }

      if (old->priv->check_completion_idle)
        {
          g_source_destroy (old->priv->check_completion_idle);
          old->priv->check_completion_idle = NULL;
        }

      if (gtk_widget_get_mapped (old->priv->popup_window))
        _gtk_entry_completion_popdown (old);

      disconnect_completion_signals (entry, old);
      old->priv->entry = NULL;

      g_object_unref (old);
    }

  if (!completion)
    {
      g_object_set_data (G_OBJECT (entry), I_(GTK_ENTRY_COMPLETION_KEY), NULL);
      return;
    }

  /* hook into the entry */
  g_object_ref (completion);

  connect_completion_signals (entry, completion);    
  completion->priv->entry = GTK_WIDGET (entry);
  g_object_set_data (G_OBJECT (entry), I_(GTK_ENTRY_COMPLETION_KEY), completion);
}

/**
 * gtk_entry_get_completion:
 * @entry: A #GtkEntry
 *
 * Returns the auxiliary completion object currently in use by @entry.
 *
 * Return value: (transfer none): The auxiliary completion object currently
 *     in use by @entry.
 *
 * Since: 2.4
 */
GtkEntryCompletion *
gtk_entry_get_completion (GtkEntry *entry)
{
  GtkEntryCompletion *completion;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);

  completion = GTK_ENTRY_COMPLETION (g_object_get_data (G_OBJECT (entry),
                                     GTK_ENTRY_COMPLETION_KEY));

  return completion;
}

/**
 * gtk_entry_set_cursor_hadjustment:
 * @entry: a #GtkEntry
 * @adjustment: an adjustment which should be adjusted when the cursor 
 *              is moved, or %NULL
 *
 * Hooks up an adjustment to the cursor position in an entry, so that when 
 * the cursor is moved, the adjustment is scrolled to show that position. 
 * See gtk_scrolled_window_get_hadjustment() for a typical way of obtaining 
 * the adjustment.
 *
 * The adjustment has to be in pixel units and in the same coordinate system 
 * as the entry. 
 * 
 * Since: 2.12
 */
void
gtk_entry_set_cursor_hadjustment (GtkEntry      *entry,
                                  GtkAdjustment *adjustment)
{
  g_return_if_fail (GTK_IS_ENTRY (entry));
  if (adjustment)
    g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  if (adjustment)
    g_object_ref (adjustment);

  g_object_set_qdata_full (G_OBJECT (entry), 
                           quark_cursor_hadjustment,
                           adjustment, 
                           g_object_unref);
}

/**
 * gtk_entry_get_cursor_hadjustment:
 * @entry: a #GtkEntry
 *
 * Retrieves the horizontal cursor adjustment for the entry. 
 * See gtk_entry_set_cursor_hadjustment().
 *
 * Return value: (transfer none): the horizontal cursor adjustment, or %NULL
 *   if none has been set.
 *
 * Since: 2.12
 */
GtkAdjustment*
gtk_entry_get_cursor_hadjustment (GtkEntry *entry)
{
  g_return_val_if_fail (GTK_IS_ENTRY (entry), NULL);

  return g_object_get_qdata (G_OBJECT (entry), quark_cursor_hadjustment);
}

/**
 * gtk_entry_set_progress_fraction:
 * @entry: a #GtkEntry
 * @fraction: fraction of the task that's been completed
 *
 * Causes the entry's progress indicator to "fill in" the given
 * fraction of the bar. The fraction should be between 0.0 and 1.0,
 * inclusive.
 *
 * Since: 2.16
 */
void
gtk_entry_set_progress_fraction (GtkEntry *entry,
                                 gdouble   fraction)
{
  GtkWidget       *widget;
  GtkEntryPrivate *private;
  gdouble          old_fraction;
  gint x, y, width, height;
  gint old_x, old_y, old_width, old_height;

  g_return_if_fail (GTK_IS_ENTRY (entry));

  widget = GTK_WIDGET (entry);
  private = GTK_ENTRY_GET_PRIVATE (entry);

  if (private->progress_pulse_mode)
    old_fraction = -1;
  else
    old_fraction = private->progress_fraction;

  if (gtk_widget_is_drawable (widget))
    get_progress_area (widget, &old_x, &old_y, &old_width, &old_height);

  fraction = CLAMP (fraction, 0.0, 1.0);

  private->progress_fraction = fraction;
  private->progress_pulse_mode = FALSE;
  private->progress_pulse_current = 0.0;

  if (gtk_widget_is_drawable (widget))
    {
      get_progress_area (widget, &x, &y, &width, &height);

      if ((x != old_x) || (y != old_y) || (width != old_width) || (height != old_height))
        gtk_widget_queue_draw (widget);
    }

  if (fraction != old_fraction)
    g_object_notify (G_OBJECT (entry), "progress-fraction");
}

/**
 * gtk_entry_get_progress_fraction:
 * @entry: a #GtkEntry
 *
 * Returns the current fraction of the task that's been completed.
 * See gtk_entry_set_progress_fraction().
 *
 * Return value: a fraction from 0.0 to 1.0
 *
 * Since: 2.16
 */
gdouble
gtk_entry_get_progress_fraction (GtkEntry *entry)
{
  GtkEntryPrivate *private;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), 0.0);

  private = GTK_ENTRY_GET_PRIVATE (entry);

  return private->progress_fraction;
}

/**
 * gtk_entry_set_progress_pulse_step:
 * @entry: a #GtkEntry
 * @fraction: fraction between 0.0 and 1.0
 *
 * Sets the fraction of total entry width to move the progress
 * bouncing block for each call to gtk_entry_progress_pulse().
 *
 * Since: 2.16
 */
void
gtk_entry_set_progress_pulse_step (GtkEntry *entry,
                                   gdouble   fraction)
{
  GtkEntryPrivate *private;

  g_return_if_fail (GTK_IS_ENTRY (entry));

  private = GTK_ENTRY_GET_PRIVATE (entry);

  fraction = CLAMP (fraction, 0.0, 1.0);

  if (fraction != private->progress_pulse_fraction)
    {
      private->progress_pulse_fraction = fraction;

      gtk_widget_queue_draw (GTK_WIDGET (entry));

      g_object_notify (G_OBJECT (entry), "progress-pulse-step");
    }
}

/**
 * gtk_entry_get_progress_pulse_step:
 * @entry: a #GtkEntry
 *
 * Retrieves the pulse step set with gtk_entry_set_progress_pulse_step().
 *
 * Return value: a fraction from 0.0 to 1.0
 *
 * Since: 2.16
 */
gdouble
gtk_entry_get_progress_pulse_step (GtkEntry *entry)
{
  GtkEntryPrivate *private;

  g_return_val_if_fail (GTK_IS_ENTRY (entry), 0.0);

  private = GTK_ENTRY_GET_PRIVATE (entry);

  return private->progress_pulse_fraction;
}

/**
 * gtk_entry_progress_pulse:
 * @entry: a #GtkEntry
 *
 * Indicates that some progress is made, but you don't know how much.
 * Causes the entry's progress indicator to enter "activity mode,"
 * where a block bounces back and forth. Each call to
 * gtk_entry_progress_pulse() causes the block to move by a little bit
 * (the amount of movement per pulse is determined by
 * gtk_entry_set_progress_pulse_step()).
 *
 * Since: 2.16
 */
void
gtk_entry_progress_pulse (GtkEntry *entry)
{
  GtkEntryPrivate *private;

  g_return_if_fail (GTK_IS_ENTRY (entry));

  private = GTK_ENTRY_GET_PRIVATE (entry);

  if (private->progress_pulse_mode)
    {
      if (private->progress_pulse_way_back)
        {
          private->progress_pulse_current -= private->progress_pulse_fraction;

          if (private->progress_pulse_current < 0.0)
            {
              private->progress_pulse_current = 0.0;
              private->progress_pulse_way_back = FALSE;
            }
        }
      else
        {
          private->progress_pulse_current += private->progress_pulse_fraction;

          if (private->progress_pulse_current > 1.0 - private->progress_pulse_fraction)
            {
              private->progress_pulse_current = 1.0 - private->progress_pulse_fraction;
              private->progress_pulse_way_back = TRUE;
            }
        }
    }
  else
    {
      private->progress_fraction = 0.0;
      private->progress_pulse_mode = TRUE;
      private->progress_pulse_way_back = FALSE;
      private->progress_pulse_current = 0.0;
    }

  gtk_widget_queue_draw (GTK_WIDGET (entry));
}

/* Caps Lock warning for password entries */

static void
show_capslock_feedback (GtkEntry    *entry,
                        const gchar *text)
{
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (entry);

  if (gtk_entry_get_icon_storage_type (entry, GTK_ENTRY_ICON_SECONDARY) == GTK_IMAGE_EMPTY)
    {
      gtk_entry_set_icon_from_stock (entry, GTK_ENTRY_ICON_SECONDARY, GTK_STOCK_CAPS_LOCK_WARNING);
      gtk_entry_set_icon_activatable (entry, GTK_ENTRY_ICON_SECONDARY, FALSE);
      priv->caps_lock_warning_shown = TRUE;
    }

  if (priv->caps_lock_warning_shown)
    gtk_entry_set_icon_tooltip_text (entry, GTK_ENTRY_ICON_SECONDARY, text);
  else
    g_warning ("Can't show Caps Lock warning, since secondary icon is set");
}

static void
remove_capslock_feedback (GtkEntry *entry)
{
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (entry);

  if (priv->caps_lock_warning_shown)
    {
      gtk_entry_set_icon_from_stock (entry, GTK_ENTRY_ICON_SECONDARY, NULL);
      priv->caps_lock_warning_shown = FALSE;
    } 
}

static void
keymap_state_changed (GdkKeymap *keymap, 
                      GtkEntry  *entry)
{
  GtkEntryPrivate *priv = GTK_ENTRY_GET_PRIVATE (entry);
  char *text = NULL;

  if (gtk_entry_get_display_mode (entry) != DISPLAY_NORMAL && priv->caps_lock_warning)
    { 
      if (gdk_keymap_get_caps_lock_state (keymap))
        text = _("Caps Lock is on");
    }

  if (text)
    show_capslock_feedback (entry, text);
  else
    remove_capslock_feedback (entry);
}

#define __GTK_ENTRY_C__
#include "gtkaliasdef.c"
