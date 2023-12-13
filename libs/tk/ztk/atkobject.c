/* ATK -  Accessibility Toolkit
 * Copyright 2001 Sun Microsystems Inc.
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

#include "config.h"

#include <string.h>
#include <locale.h>

#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include "atk.h"
#include "atkmarshal.h"
#include "atkprivate.h"

/**
 * SECTION:atkobject
 * @Short_description: The base object class for the Accessibility Toolkit API.
 * @Title:AtkObject
 *
 * This class is the primary class for accessibility support via the
 * Accessibility ToolKit (ATK).  Objects which are instances of
 * #AtkObject (or instances of AtkObject-derived types) are queried
 * for properties which relate basic (and generic) properties of a UI
 * component such as name and description.  Instances of #AtkObject
 * may also be queried as to whether they implement other ATK
 * interfaces (e.g. #AtkAction, #AtkComponent, etc.), as appropriate
 * to the role which a given UI component plays in a user interface.
 *
 * All UI components in an application which provide useful
 * information or services to the user must provide corresponding
 * #AtkObject instances on request (in GTK+, for instance, usually on
 * a call to #gtk_widget_get_accessible ()), either via ATK support
 * built into the toolkit for the widget class or ancestor class, or
 * in the case of custom widgets, if the inherited #AtkObject
 * implementation is insufficient, via instances of a new #AtkObject
 * subclass.
 *
 * See also: #AtkObjectFactory, #AtkRegistry.  (GTK+ users see also
 * #GtkAccessible).
 *
 */

static GPtrArray *role_names = NULL;

enum
{
  PROP_0,  /* gobject convention */

  PROP_NAME,
  PROP_DESCRIPTION,
  PROP_PARENT,      /* ancestry has changed */
  PROP_VALUE,
  PROP_ROLE,
  PROP_LAYER,
  PROP_MDI_ZORDER,
  PROP_TABLE_CAPTION,
  PROP_TABLE_COLUMN_DESCRIPTION,
  PROP_TABLE_COLUMN_HEADER,
  PROP_TABLE_ROW_DESCRIPTION,
  PROP_TABLE_ROW_HEADER,
  PROP_TABLE_SUMMARY,
  PROP_TABLE_CAPTION_OBJECT,
  PROP_HYPERTEXT_NUM_LINKS,
  PROP_LAST         /* gobject convention */
};

enum {
  CHILDREN_CHANGED,
  FOCUS_EVENT,
  PROPERTY_CHANGE,
  STATE_CHANGE,
  VISIBLE_DATA_CHANGED,
  ACTIVE_DESCENDANT_CHANGED,
  
  LAST_SIGNAL
};

/* These are listed here for extraction by intltool */
#if 0
  N_("invalid")
  N_("accelerator label")
  N_("alert")
  N_("animation")
  N_("arrow")
  N_("calendar")
  N_("canvas")
  N_("check box")
  N_("check menu item")
  N_("color chooser")
  N_("column header")
  N_("combo box")
  N_("dateeditor")
  N_("desktop icon")
  N_("desktop frame")
  N_("dial")
  N_("dialog")
  N_("directory pane")
  N_("drawing area")
  N_("file chooser")
  N_("filler")
  /* I know it looks wrong but that is what Java returns */
  N_("fontchooser")
  N_("frame")
  N_("glass pane")
  N_("html container")
  N_("icon")
  N_("image")
  N_("internal frame")
  N_("label")
  N_("layered pane")
  N_("list")
  N_("list item")
  N_("menu")
  N_("menu bar")
  N_("menu item")
  N_("option pane")
  N_("page tab")
  N_("page tab list")
  N_("panel")
  N_("password text")
  N_("popup menu")
  N_("progress bar")
  N_("push button")
  N_("radio button")
  N_("radio menu item")
  N_("root pane")
  N_("row header")
  N_("scroll bar")
  N_("scroll pane")
  N_("separator")
  N_("slider")
  N_("split pane")
  N_("spin button")
  N_("statusbar")
  N_("table")
  N_("table cell")
  N_("table column header")
  N_("table row header")
  N_("tear off menu item")
  N_("terminal")
  N_("text")
  N_("toggle button")
  N_("tool bar")
  N_("tool tip")
  N_("tree")
  N_("tree table")
  N_("unknown")
  N_("viewport")
  N_("window")
  N_("header")
  N_("footer")
  N_("paragraph")
  N_("ruler")
  N_("application")
  N_("autocomplete")
  N_("edit bar")
  N_("embedded component")
  N_("entry")
  N_("chart")
  N_("caption")
  N_("document frame")
  N_("heading")
  N_("page")
  N_("section")
  N_("redundant object")
  N_("form")
  N_("link")
  N_("input method window")
  N_("table row")
  N_("tree item")
  N_("document spreadsheet")
  N_("document presentation")
  N_("document text")
  N_("document web")
  N_("document email")
  N_("comment")
  N_("list box")
  N_("grouping")
  N_("image map")
  N_("notification")
  N_("info bar")
  N_("level bar")
  N_("title bar")
  N_("block quote")
  N_("audio")
  N_("video")
  N_("definition")
  N_("article")
  N_("landmark")
  N_("log")
  N_("marquee")
  N_("math")
  N_("rating")
  N_("timer")
  N_("description list")
  N_("description term")
  N_("description value")
#endif /* 0 */

static void            atk_object_class_init        (AtkObjectClass  *klass);
static void            atk_object_init              (AtkObject       *accessible,
                                                     AtkObjectClass  *klass);
static AtkRelationSet* atk_object_real_ref_relation_set 
                                                    (AtkObject       *accessible);
static void            atk_object_real_initialize   (AtkObject       *accessible,
                                                     gpointer        data);
static void            atk_object_real_set_property (GObject         *object,
                                                     guint            prop_id,
                                                     const GValue    *value,
                                                     GParamSpec      *pspec);
static void            atk_object_real_get_property (GObject         *object,
                                                     guint            prop_id,
                                                     GValue          *value,
                                                     GParamSpec      *pspec);
static void            atk_object_finalize          (GObject         *object);
static const gchar*    atk_object_real_get_name     (AtkObject       *object);
static const gchar*    atk_object_real_get_description
                                                   (AtkObject       *object);
static AtkObject*      atk_object_real_get_parent  (AtkObject       *object);
static AtkRole         atk_object_real_get_role    (AtkObject       *object);
static AtkLayer        atk_object_real_get_layer   (AtkObject       *object);
static AtkStateSet*    atk_object_real_ref_state_set
                                                   (AtkObject       *object);
static void            atk_object_real_set_name    (AtkObject       *object,
                                                    const gchar     *name);
static void            atk_object_real_set_description
                                                   (AtkObject       *object,
                                                    const gchar     *description);
static void            atk_object_real_set_parent  (AtkObject       *object,
                                                    AtkObject       *parent);
static void            atk_object_real_set_role    (AtkObject       *object,
                                                    AtkRole         role);
static void            atk_object_notify           (GObject         *obj,
                                                    GParamSpec      *pspec);
static const gchar*    atk_object_real_get_object_locale
                                                   (AtkObject       *object);

static guint atk_object_signals[LAST_SIGNAL] = { 0, };

static gpointer parent_class = NULL;

static const gchar* const atk_object_name_property_name = "accessible-name";
static const gchar* const atk_object_name_property_description = "accessible-description";
static const gchar* const atk_object_name_property_parent = "accessible-parent";
static const gchar* const atk_object_name_property_value = "accessible-value";
static const gchar* const atk_object_name_property_role = "accessible-role";
static const gchar* const atk_object_name_property_component_layer = "accessible-component-layer";
static const gchar* const atk_object_name_property_component_mdi_zorder = "accessible-component-mdi-zorder";
static const gchar* const atk_object_name_property_table_caption = "accessible-table-caption";
static const gchar* const atk_object_name_property_table_column_description = "accessible-table-column-description";
static const gchar* const atk_object_name_property_table_column_header = "accessible-table-column-header";
static const gchar* const atk_object_name_property_table_row_description = "accessible-table-row-description";
static const gchar* const atk_object_name_property_table_row_header = "accessible-table-row-header";
static const gchar* const atk_object_name_property_table_summary = "accessible-table-summary";
static const gchar* const atk_object_name_property_table_caption_object = "accessible-table-caption-object";
static const gchar* const atk_object_name_property_hypertext_num_links = "accessible-hypertext-nlinks";

static void
initialize_role_names ()
{
  GTypeClass *enum_class;
  GEnumValue *enum_value;
  int i;
  gchar *role_name = NULL;

  if (role_names)
    return;

  role_names = g_ptr_array_new ();
  enum_class = g_type_class_ref (ATK_TYPE_ROLE);
  if (!G_IS_ENUM_CLASS(enum_class))
    return;

  for (i = 0; i < ATK_ROLE_LAST_DEFINED; i++)
    {
      enum_value = g_enum_get_value (G_ENUM_CLASS (enum_class), i);
      role_name = g_strdup (enum_value->value_nick);
      // We want the role names to be in the format "check button" and not "check-button"
      _compact_name (role_name);
      g_ptr_array_add (role_names, role_name);
    }

  g_type_class_unref (enum_class);

}

GType
atk_object_get_type (void)
{
  static GType type = 0;

  if (!type)
    {
      static const GTypeInfo typeInfo =
      {
        sizeof (AtkObjectClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) atk_object_class_init,
        (GClassFinalizeFunc) NULL,
        NULL,
        sizeof (AtkObject),
        0,
        (GInstanceInitFunc) atk_object_init,
      } ;
      type = g_type_register_static (G_TYPE_OBJECT, "AtkObject", &typeInfo, 0) ;
    }
  return type;
}

static void
atk_object_class_init (AtkObjectClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->set_property = atk_object_real_set_property;
  gobject_class->get_property = atk_object_real_get_property;
  gobject_class->finalize = atk_object_finalize;
  gobject_class->notify = atk_object_notify;

  klass->get_name = atk_object_real_get_name;
  klass->get_description = atk_object_real_get_description;
  klass->get_parent = atk_object_real_get_parent;
  klass->get_n_children = NULL;
  klass->ref_child = NULL;
  klass->get_index_in_parent = NULL;
  klass->ref_relation_set = atk_object_real_ref_relation_set;
  klass->get_role = atk_object_real_get_role;
  klass->get_layer = atk_object_real_get_layer;
  klass->get_mdi_zorder = NULL;
  klass->initialize = atk_object_real_initialize;
  klass->ref_state_set = atk_object_real_ref_state_set;
  klass->set_name = atk_object_real_set_name;
  klass->set_description = atk_object_real_set_description;
  klass->set_parent = atk_object_real_set_parent;
  klass->set_role = atk_object_real_set_role;
  klass->get_object_locale = atk_object_real_get_object_locale;

  /*
   * We do not define default signal handlers here
   */
  klass->children_changed = NULL;
  klass->focus_event = NULL;
  klass->property_change = NULL;
  klass->visible_data_changed = NULL;
  klass->active_descendant_changed = NULL;

  _gettext_initialization ();

  g_object_class_install_property (gobject_class,
                                   PROP_NAME,
                                   g_param_spec_string (atk_object_name_property_name,
                                                        _("Accessible Name"),
                                                        _("Object instance\'s name formatted for assistive technology access"),
                                                        NULL,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_DESCRIPTION,
                                   g_param_spec_string (atk_object_name_property_description,
                                                        _("Accessible Description"),
                                                        _("Description of an object, formatted for assistive technology access"),
                                                        NULL,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_PARENT,
                                   g_param_spec_object (atk_object_name_property_parent,
                                                        _("Accessible Parent"),
                                                        _("Parent of the current accessible as returned by atk_object_get_parent()"),
                                                        ATK_TYPE_OBJECT,
                                                        G_PARAM_READWRITE));

  /**
   * AtkObject:accessible-value:
   *
   * Numeric value of this object, in case being and AtkValue.
   *
   * Deprecated: Since 2.12. Use atk_value_get_value_and_text() to get
   * the value, and value-changed signal to be notified on their value
   * changes.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_VALUE,
                                   g_param_spec_double (atk_object_name_property_value,
                                                        _("Accessible Value"),
                                                        _("Is used to notify that the value has changed"),
                                                        0.0,
                                                        G_MAXDOUBLE,
                                                        0.0,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_ROLE,
                                   g_param_spec_int    (atk_object_name_property_role,
                                                        _("Accessible Role"),
                                                        _("The accessible role of this object"),
                                                        0,
                                                        G_MAXINT,
                                                        ATK_ROLE_UNKNOWN,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_LAYER,
                                   g_param_spec_int    (atk_object_name_property_component_layer,
                                                        _("Accessible Layer"),
                                                        _("The accessible layer of this object"),
                                                        0,
                                                        G_MAXINT,
                                                        0,
                                                        G_PARAM_READABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_MDI_ZORDER,
                                   g_param_spec_int    (atk_object_name_property_component_mdi_zorder,
                                                        _("Accessible MDI Value"),
                                                        _("The accessible MDI value of this object"),
                                                        G_MININT,
                                                        G_MAXINT,
                                                        G_MININT,
                                                        G_PARAM_READABLE));

  /**
   * AtkObject:accessible-table-caption:
   *
   * Table caption.
   *
   * Deprecated: Since 1.3. Use table-caption-object instead.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_TABLE_CAPTION,
                                   g_param_spec_string (atk_object_name_property_table_caption,
                                                        _("Accessible Table Caption"),
                                                        _("Is used to notify that the table caption has changed; this property should not be used. accessible-table-caption-object should be used instead"),
                                                        NULL,
                                                        G_PARAM_READWRITE));
  /**
   * AtkObject:accessible-table-column-header:
   *
   * Accessible table column header.
   *
   * Deprecated: Since 2.12. Use atk_table_get_column_header() and
   * atk_table_set_column_header() instead.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_TABLE_COLUMN_HEADER,
                                   g_param_spec_object (atk_object_name_property_table_column_header,
                                                        _("Accessible Table Column Header"),
                                                        _("Is used to notify that the table column header has changed"),
                                                        ATK_TYPE_OBJECT,
                                                        G_PARAM_READWRITE));

  /**
   * AtkObject:accessible-table-column-description:
   *
   * Accessible table column description.
   *
   * Deprecated: Since 2.12. Use atk_table_get_column_description()
   * and atk_table_set_column_description() instead.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_TABLE_COLUMN_DESCRIPTION,
                                   g_param_spec_string (atk_object_name_property_table_column_description,
                                                        _("Accessible Table Column Description"),
                                                        _("Is used to notify that the table column description has changed"),
                                                        NULL,
                                                        G_PARAM_READWRITE));

  /**
   * AtkObject:accessible-table-row-header:
   *
   * Accessible table row header.
   *
   * Deprecated: Since 2.12. Use atk_table_get_row_header() and
   * atk_table_set_row_header() instead.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_TABLE_ROW_HEADER,
                                   g_param_spec_object (atk_object_name_property_table_row_header,
                                                        _("Accessible Table Row Header"),
                                                        _("Is used to notify that the table row header has changed"),
                                                        ATK_TYPE_OBJECT,
                                                        G_PARAM_READWRITE));
  /**
   * AtkObject:accessible-table-row-description:
   *
   * Accessible table row description.
   *
   * Deprecated: Since 2.12. Use atk_table_get_row_description() and
   * atk_table_set_row_description() instead.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_TABLE_ROW_DESCRIPTION,
                                   g_param_spec_string (atk_object_name_property_table_row_description,
                                                        _("Accessible Table Row Description"),
                                                        _("Is used to notify that the table row description has changed"),
                                                        NULL,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_TABLE_SUMMARY,
                                   g_param_spec_object (atk_object_name_property_table_summary,
                                                        _("Accessible Table Summary"),
                                                        _("Is used to notify that the table summary has changed"),
                                                        ATK_TYPE_OBJECT,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_TABLE_CAPTION_OBJECT,
                                   g_param_spec_object (atk_object_name_property_table_caption_object,
                                                        _("Accessible Table Caption Object"),
                                                        _("Is used to notify that the table caption has changed"),
                                                        ATK_TYPE_OBJECT,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_HYPERTEXT_NUM_LINKS,
                                   g_param_spec_int    (atk_object_name_property_hypertext_num_links,
                                                        _("Number of Accessible Hypertext Links"),
                                                        _("The number of links which the current AtkHypertext has"),
                                                        0,
                                                        G_MAXINT,
                                                        0,
                                                        G_PARAM_READABLE));

  /**
   * AtkObject::children-changed:
   * @atkobject: the object which received the signal.
   * @arg1: The index of the added or removed child. The value can be
   * -1. This is used if the value is not known by the implementor
   * when the child is added/removed or irrelevant.
   * @arg2: A gpointer to the child AtkObject which was added or
   * removed. If the child was removed, it is possible that it is not
   * available for the implementor. In that case this pointer can be
   * NULL.
   *
   * The signal "children-changed" is emitted when a child is added or
   * removed form an object. It supports two details: "add" and
   * "remove"
   */
  atk_object_signals[CHILDREN_CHANGED] =
    g_signal_new ("children_changed",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
		  G_STRUCT_OFFSET (AtkObjectClass, children_changed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__UINT_POINTER,
		  G_TYPE_NONE,
		  2, G_TYPE_UINT, G_TYPE_POINTER);

  /**
   * AtkObject::focus-event:
   * @atkobject: the object which received the signal
   * @arg1: a boolean value which indicates whether the object gained
   * or lost focus.
   *
   * The signal "focus-event" is emitted when an object gained or lost
   * focus.
   *
   * Deprecated: Since 2.9.4. Use #AtkObject::state-change signal instead.
   */
  atk_object_signals[FOCUS_EVENT] =
    g_signal_new ("focus_event",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (AtkObjectClass, focus_event), 
		  NULL, NULL,
		  g_cclosure_marshal_VOID__BOOLEAN,
		  G_TYPE_NONE,
		  1, G_TYPE_BOOLEAN);
  /**
   * AtkObject::property-change:
   * @atkobject: the object which received the signal.
   * @arg1: an #AtkPropertyValues containing the new value of the
   *   property which changed.
   *
   * The signal "property-change" is emitted when an object's property
   * value changes. @arg1 contains an #AtkPropertyValues with the name
   * and the new value of the property whose value has changed. Note
   * that, as with GObject notify, getting this signal does not
   * guarantee that the value of the property has actually changed; it
   * may also be emitted when the setter of the property is called to
   * reinstate the previous value.
   *
   * Toolkit implementor note: ATK implementors should use
   * g_object_notify() to emit property-changed
   * notifications. #AtkObject::property-changed is needed by the
   * implementation of atk_add_global_event_listener() because GObject
   * notify doesn't support emission hooks.
   */
  atk_object_signals[PROPERTY_CHANGE] =
    g_signal_new ("property_change",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  G_STRUCT_OFFSET (AtkObjectClass, property_change),
                  (GSignalAccumulator) NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1,
                  G_TYPE_POINTER);

  /**
   * AtkObject::state-change:
   * @atkobject: the object which received the signal.
   * @arg1: The name of the state which has changed
   * @arg2: A boolean which indicates whether the state has been set or unset.
   *
   * The "state-change" signal is emitted when an object's state
   * changes.  The detail value identifies the state type which has
   * changed.
   */
  atk_object_signals[STATE_CHANGE] =
    g_signal_new ("state_change",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  G_STRUCT_OFFSET (AtkObjectClass, state_change),
                  (GSignalAccumulator) NULL, NULL,
                  atk_marshal_VOID__STRING_BOOLEAN,
                  G_TYPE_NONE, 2,
                  G_TYPE_STRING,
                  G_TYPE_BOOLEAN);

  /**
   * AtkObject::visible-data-changed:
   * @atkobject: the object which received the signal.
   *
   * The "visible-data-changed" signal is emitted when the visual
   * appearance of the object changed.
   */
  atk_object_signals[VISIBLE_DATA_CHANGED] =
    g_signal_new ("visible_data_changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (AtkObjectClass, visible_data_changed),
                  (GSignalAccumulator) NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * AtkObject::active-descendant-changed:
   * @atkobject: the object which received the signal.
   * @arg1: the newly focused object.
   *
   * The "active-descendant-changed" signal is emitted by an object
   * which has the state ATK_STATE_MANAGES_DESCENDANTS when the focus
   * object in the object changes. For instance, a table will emit the
   * signal when the cell in the table which has focus changes.
   */
  atk_object_signals[ACTIVE_DESCENDANT_CHANGED] =
    g_signal_new ("active_descendant_changed",
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
		  G_STRUCT_OFFSET (AtkObjectClass, active_descendant_changed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__POINTER,
		  G_TYPE_NONE,
		  1, G_TYPE_POINTER);
}

static void
atk_object_init  (AtkObject        *accessible,
                  AtkObjectClass   *klass)
{
  accessible->name = NULL;
  accessible->description = NULL;
  accessible->accessible_parent = NULL;
  accessible->relation_set = atk_relation_set_new();
  accessible->role = ATK_ROLE_UNKNOWN;
}

GType
atk_implementor_get_type (void)
{
  static GType type = 0;

  if (!type)
    {
      static const GTypeInfo typeInfo =
      {
        sizeof (AtkImplementorIface),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
      } ;

      type = g_type_register_static (G_TYPE_INTERFACE, "AtkImplementorIface", &typeInfo, 0) ;
    }

  return type;
}

/**
 * atk_object_get_name:
 * @accessible: an #AtkObject
 *
 * Gets the accessible name of the accessible.
 *
 * Returns: a character string representing the accessible name of the object.
 **/
const gchar*
atk_object_get_name (AtkObject *accessible)
{
  AtkObjectClass *klass;

  g_return_val_if_fail (ATK_IS_OBJECT (accessible), NULL);

  klass = ATK_OBJECT_GET_CLASS (accessible);
  if (klass->get_name)
    return (klass->get_name) (accessible);
  else
    return NULL;
}

/**
 * atk_object_get_description:
 * @accessible: an #AtkObject
 *
 * Gets the accessible description of the accessible.
 *
 * Returns: a character string representing the accessible description
 * of the accessible.
 *
 **/
const gchar*
atk_object_get_description (AtkObject *accessible)
{
  AtkObjectClass *klass;

  g_return_val_if_fail (ATK_IS_OBJECT (accessible), NULL);

  klass = ATK_OBJECT_GET_CLASS (accessible);
  if (klass->get_description)
    return (klass->get_description) (accessible);
  else
    return NULL;
}

/**
 * atk_object_get_parent:
 * @accessible: an #AtkObject
 *
 * Gets the accessible parent of the accessible. By default this is
 * the one assigned with atk_object_set_parent(), but it is assumed
 * that ATK implementors have ways to get the parent of the object
 * without the need of assigning it manually with
 * atk_object_set_parent(), and will return it with this method.
 *
 * If you are only interested on the parent assigned with
 * atk_object_set_parent(), use atk_object_peek_parent().
 *
 * Returns: (transfer none): an #AtkObject representing the accessible
 * parent of the accessible
 **/
AtkObject*
atk_object_get_parent (AtkObject *accessible)
{
  AtkObjectClass *klass;

  g_return_val_if_fail (ATK_IS_OBJECT (accessible), NULL);

  klass = ATK_OBJECT_GET_CLASS (accessible);
  if (klass->get_parent)
    return (klass->get_parent) (accessible);
  else
    return NULL;
}

/**
 * atk_object_peek_parent:
 * @accessible: an #AtkObject
 *
 * Gets the accessible parent of the accessible, if it has been
 * manually assigned with atk_object_set_parent. Otherwise, this
 * function returns %NULL.
 *
 * This method is intended as an utility for ATK implementors, and not
 * to be exposed to accessible tools. See atk_object_get_parent() for
 * further reference.
 *
 * Returns: (transfer none): an #AtkObject representing the accessible
 * parent of the accessible if assigned
 **/
AtkObject*
atk_object_peek_parent (AtkObject *accessible)
{
  return accessible->accessible_parent;
}

/**
 * atk_object_get_n_accessible_children:
 * @accessible: an #AtkObject
 *
 * Gets the number of accessible children of the accessible.
 *
 * Returns: an integer representing the number of accessible children
 * of the accessible.
 **/
gint
atk_object_get_n_accessible_children (AtkObject *accessible)
{
  AtkObjectClass *klass;

  g_return_val_if_fail (ATK_IS_OBJECT (accessible), 0);

  klass = ATK_OBJECT_GET_CLASS (accessible);
  if (klass->get_n_children)
    return (klass->get_n_children) (accessible);
  else
    return 0;
}

/**
 * atk_object_ref_accessible_child:
 * @accessible: an #AtkObject
 * @i: a gint representing the position of the child, starting from 0
 *
 * Gets a reference to the specified accessible child of the object.
 * The accessible children are 0-based so the first accessible child is
 * at index 0, the second at index 1 and so on.
 *
 * Returns: (transfer full): an #AtkObject representing the specified
 * accessible child of the accessible.
 **/
AtkObject*
atk_object_ref_accessible_child (AtkObject   *accessible,
                                 gint        i)
{
  AtkObjectClass *klass;

  g_return_val_if_fail (ATK_IS_OBJECT (accessible), NULL);

  klass = ATK_OBJECT_GET_CLASS (accessible);
  if (klass->ref_child)
    return (klass->ref_child) (accessible, i);
  else
    return NULL;
}

/**
 * atk_object_ref_relation_set:
 * @accessible: an #AtkObject
 *
 * Gets the #AtkRelationSet associated with the object.
 *
 * Returns: (transfer full): an #AtkRelationSet representing the relation set
 * of the object.
 **/
AtkRelationSet*
atk_object_ref_relation_set (AtkObject *accessible)
{
  AtkObjectClass *klass;

  g_return_val_if_fail (ATK_IS_OBJECT (accessible), NULL);

  klass = ATK_OBJECT_GET_CLASS (accessible);
  if (klass->ref_relation_set)
    return (klass->ref_relation_set) (accessible);
  else
    return NULL;
}

/**
 * atk_role_register:
 * @name: a character string describing the new role.
 *
 * Registers the role specified by @name. @name must be a meaningful
 * name. So it should not be empty, or consisting on whitespaces.
 *
 * Deprecated: Since 2.12. If your application/toolkit doesn't find a
 * suitable role for a specific object defined at #AtkRole, please
 * submit a bug in order to add a new role to the specification.
 *
 * Returns: an #AtkRole for the new role if added
 * properly. ATK_ROLE_INVALID in case of error.
 **/
AtkRole
atk_role_register (const gchar *name)
{
  gboolean valid = FALSE;
  gint i = 0;
  glong length = g_utf8_strlen (name, -1);

  for (i=0; i < length; i++) {
    if (name[i]!=' ') {
      valid = TRUE;
      break;
    }
  }

  if (!valid)
    return ATK_ROLE_INVALID;

  if (!role_names)
    initialize_role_names ();

  g_ptr_array_add (role_names, g_strdup (name));
  return role_names->len - 1;
}

/**
 * atk_object_get_role:
 * @accessible: an #AtkObject
 *
 * Gets the role of the accessible.
 *
 * Returns: an #AtkRole which is the role of the accessible
 **/
AtkRole
atk_object_get_role (AtkObject *accessible) 
{
  AtkObjectClass *klass;

  g_return_val_if_fail (ATK_IS_OBJECT (accessible), ATK_ROLE_UNKNOWN);

  klass = ATK_OBJECT_GET_CLASS (accessible);
  if (klass->get_role)
    return (klass->get_role) (accessible);
  else
    return ATK_ROLE_UNKNOWN;
}

/**
 * atk_object_get_layer:
 * @accessible: an #AtkObject
 *
 * Gets the layer of the accessible.
 *
 * Deprecated: Use atk_component_get_layer instead.
 *
 * Returns: an #AtkLayer which is the layer of the accessible
 **/
AtkLayer
atk_object_get_layer (AtkObject *accessible) 
{
  AtkObjectClass *klass;

  g_return_val_if_fail (ATK_IS_OBJECT (accessible), ATK_LAYER_INVALID);

  klass = ATK_OBJECT_GET_CLASS (accessible);
  if (klass->get_layer)
    return (klass->get_layer) (accessible);
  else
    return ATK_LAYER_INVALID;
}

/**
 * atk_object_get_mdi_zorder:
 * @accessible: an #AtkObject
 *
 * Gets the zorder of the accessible. The value G_MININT will be returned 
 * if the layer of the accessible is not ATK_LAYER_MDI.
 *
 * Deprecated: Use atk_component_get_mdi_zorder instead.
 *
 * Returns: a gint which is the zorder of the accessible, i.e. the depth at 
 * which the component is shown in relation to other components in the same 
 * container.
 *
 **/
gint
atk_object_get_mdi_zorder (AtkObject *accessible) 
{
  AtkObjectClass *klass;

  g_return_val_if_fail (ATK_IS_OBJECT (accessible), G_MININT);

  klass = ATK_OBJECT_GET_CLASS (accessible);
  if (klass->get_mdi_zorder)
    return (klass->get_mdi_zorder) (accessible);
  else
    return G_MININT;
}

/**
 * atk_object_ref_state_set:
 * @accessible: an #AtkObject
 *
 * Gets a reference to the state set of the accessible; the caller must
 * unreference it when it is no longer needed.
 *
 * Returns: (transfer full): a reference to an #AtkStateSet which is the state
 * set of the accessible
 **/
AtkStateSet*
atk_object_ref_state_set (AtkObject *accessible) 
{
  AtkObjectClass *klass;

  g_return_val_if_fail (ATK_IS_OBJECT (accessible), NULL);

  klass = ATK_OBJECT_GET_CLASS (accessible);
  if (klass->ref_state_set)
    return (klass->ref_state_set) (accessible);
  else
    return NULL;
}

/**
 * atk_object_get_index_in_parent:
 * @accessible: an #AtkObject
 *
 * Gets the 0-based index of this accessible in its parent; returns -1 if the
 * accessible does not have an accessible parent.
 *
 * Returns: an integer which is the index of the accessible in its parent
 **/
gint
atk_object_get_index_in_parent (AtkObject *accessible)
{
  AtkObjectClass *klass;

  g_return_val_if_fail (ATK_OBJECT (accessible), -1);

  klass = ATK_OBJECT_GET_CLASS (accessible);
  if (klass->get_index_in_parent)
    return (klass->get_index_in_parent) (accessible);
  else
    return -1;
}

/**
 * atk_object_set_name:
 * @accessible: an #AtkObject
 * @name: a character string to be set as the accessible name
 *
 * Sets the accessible name of the accessible. You can't set the name
 * to NULL. This is reserved for the initial value. In this aspect
 * NULL is similar to ATK_ROLE_UNKNOWN. If you want to set the name to
 * a empty value you can use "".
 **/
void
atk_object_set_name (AtkObject    *accessible,
                     const gchar  *name)
{
  AtkObjectClass *klass;
  gboolean notify = FALSE;

  g_return_if_fail (ATK_IS_OBJECT (accessible));
  g_return_if_fail (name != NULL);

  klass = ATK_OBJECT_GET_CLASS (accessible);
  if (klass->set_name)
    {
      /* Do not notify for initial name setting. See bug 665870 */
      notify = (accessible->name != NULL);

      (klass->set_name) (accessible, name);
      if (notify)
        g_object_notify (G_OBJECT (accessible), atk_object_name_property_name);
    }
}

/**
 * atk_object_set_description:
 * @accessible: an #AtkObject
 * @description: a character string to be set as the accessible description
 *
 * Sets the accessible description of the accessible. You can't set
 * the description to NULL. This is reserved for the initial value. In
 * this aspect NULL is similar to ATK_ROLE_UNKNOWN. If you want to set
 * the name to a empty value you can use "".
 **/
void
atk_object_set_description (AtkObject   *accessible,
                            const gchar *description)
{
  AtkObjectClass *klass;
  gboolean notify = FALSE;

  g_return_if_fail (ATK_IS_OBJECT (accessible));
  g_return_if_fail (description != NULL);

  klass = ATK_OBJECT_GET_CLASS (accessible);
  if (klass->set_description)
    {
      /* Do not notify for initial name setting. See bug 665870 */
      notify = (accessible->description != NULL);

      (klass->set_description) (accessible, description);
      if (notify)
        g_object_notify (G_OBJECT (accessible),
                         atk_object_name_property_description);
    }
}

/**
 * atk_object_set_parent:
 * @accessible: an #AtkObject
 * @parent: an #AtkObject to be set as the accessible parent
 *
 * Sets the accessible parent of the accessible. @parent can be NULL.
 **/
void
atk_object_set_parent (AtkObject *accessible,
                       AtkObject *parent)
{
  AtkObjectClass *klass;

  g_return_if_fail (ATK_IS_OBJECT (accessible));

  klass = ATK_OBJECT_GET_CLASS (accessible);
  if (klass->set_parent)
    {
      (klass->set_parent) (accessible, parent);
      g_object_notify (G_OBJECT (accessible), atk_object_name_property_parent);
    }
}

/**
 * atk_object_set_role:
 * @accessible: an #AtkObject
 * @role: an #AtkRole to be set as the role
 *
 * Sets the role of the accessible.
 **/
void
atk_object_set_role (AtkObject *accessible, 
                     AtkRole   role)
{
  AtkObjectClass *klass;
  AtkRole old_role;

  g_return_if_fail (ATK_IS_OBJECT (accessible));

  klass = ATK_OBJECT_GET_CLASS (accessible);
  if (klass->set_role)
    {
      old_role = atk_object_get_role (accessible);
      if (old_role != role)
        {
          (klass->set_role) (accessible, role);
          if (old_role != ATK_ROLE_UNKNOWN)
          /* Do not notify for initial role setting */
            g_object_notify (G_OBJECT (accessible), atk_object_name_property_role);
        }
    }
}

/**
 * atk_object_connect_property_change_handler:
 * @accessible: an #AtkObject
 * @handler: a function to be called when a property changes its value
 *
 * Deprecated: Since 2.12. Connect directly to property-change or
 * notify signals.
 *
 * Returns: a #guint which is the handler id used in 
 * atk_object_remove_property_change_handler()
 **/
guint
atk_object_connect_property_change_handler (AtkObject *accessible,
                                            AtkPropertyChangeHandler *handler)
{
  AtkObjectClass *klass;

  g_return_val_if_fail (ATK_IS_OBJECT (accessible), 0);
  g_return_val_if_fail ((handler != NULL), 0);

  klass = ATK_OBJECT_GET_CLASS (accessible);
  if (klass->connect_property_change_handler)
    return (klass->connect_property_change_handler) (accessible, handler);
  else
    return 0;
}

/**
 * atk_object_remove_property_change_handler:
 * @accessible: an #AtkObject
 * @handler_id: a guint which identifies the handler to be removed.
 *
 * Deprecated: Since 2.12.
 *
 * Removes a property change handler.
 **/
void
atk_object_remove_property_change_handler  (AtkObject *accessible,
                                            guint      handler_id)
{
  AtkObjectClass *klass;

  g_return_if_fail (ATK_IS_OBJECT (accessible));

  klass = ATK_OBJECT_GET_CLASS (accessible);
  if (klass->remove_property_change_handler)
    (klass->remove_property_change_handler) (accessible, handler_id);
}

/**
 * atk_object_notify_state_change:
 * @accessible: an #AtkObject
 * @state: an #AtkState whose state is changed
 * @value: a gboolean which indicates whether the state is being set on or off
 * 
 * Emits a state-change signal for the specified state. 
 **/
void
atk_object_notify_state_change (AtkObject *accessible,
                                AtkState  state,
                                gboolean  value)
{
  const gchar* name;

  g_return_if_fail (ATK_IS_OBJECT (accessible));

  name = atk_state_type_get_name (state);
  g_signal_emit (accessible, atk_object_signals[STATE_CHANGE],
                 g_quark_from_string (name),
                 name, value, NULL);
}

/**
 * atk_implementor_ref_accessible:
 * @implementor: The #GObject instance which should implement #AtkImplementorIface
 * if a non-null return value is required.
 * 
 * Gets a reference to an object's #AtkObject implementation, if
 * the object implements #AtkObjectIface
 *
 * Returns: (transfer full): a reference to an object's #AtkObject
 * implementation
 */
AtkObject *
atk_implementor_ref_accessible (AtkImplementor *implementor)
{
  AtkImplementorIface *iface;
  AtkObject           *accessible = NULL;

  g_return_val_if_fail (ATK_IS_IMPLEMENTOR (implementor), NULL);

  iface = ATK_IMPLEMENTOR_GET_IFACE (implementor);

  if (iface != NULL) 
    accessible =  iface->ref_accessible (implementor);

  g_return_val_if_fail ((accessible != NULL), NULL);

  return accessible;
}

    	
/**
 * atk_object_get_attributes:
 * @accessible: An #AtkObject.
 *
 * Get a list of properties applied to this object as a whole, as an #AtkAttributeSet consisting of 
 * name-value pairs. As such these attributes may be considered weakly-typed properties or annotations, 
 * as distinct from strongly-typed object data available via other get/set methods.
 * Not all objects have explicit "name-value pair" #AtkAttributeSet properties.
 *
 * Since: 1.12
 *
 * Returns: (transfer full): an #AtkAttributeSet consisting of all
 * explicit properties/annotations applied to the object, or an empty
 * set if the object has no name-value pair attributes assigned to
 * it. This #atkattributeset should be freed by a call to
 * atk_attribute_set_free().
 */
AtkAttributeSet *
atk_object_get_attributes (AtkObject                  *accessible)
{
  AtkObjectClass *klass;

  g_return_val_if_fail (ATK_IS_OBJECT (accessible), NULL);

  klass = ATK_OBJECT_GET_CLASS (accessible);
  if (klass->get_attributes)
    return (klass->get_attributes) (accessible); 
  else 
    return NULL;
	
}

static AtkRelationSet*
atk_object_real_ref_relation_set (AtkObject *accessible)
{
  g_return_val_if_fail (accessible->relation_set, NULL);
  g_object_ref (accessible->relation_set); 

  return accessible->relation_set;
}

static void
atk_object_real_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  AtkObject *accessible;

  accessible = ATK_OBJECT (object);

  switch (prop_id)
    {
    case PROP_NAME:
      atk_object_set_name (accessible, g_value_get_string (value));
      break;
    case PROP_DESCRIPTION:
      atk_object_set_description (accessible, g_value_get_string (value));
      break;
    case PROP_ROLE:
      atk_object_set_role (accessible, g_value_get_int (value));
      break;
    case PROP_PARENT:
      atk_object_set_parent (accessible, g_value_get_object (value));
      break;
    case PROP_VALUE:
      if (ATK_IS_VALUE (accessible))
        atk_value_set_current_value (ATK_VALUE (accessible), value);
      break;
    case PROP_TABLE_SUMMARY:
      if (ATK_IS_TABLE (accessible))
        atk_table_set_summary (ATK_TABLE (accessible), g_value_get_object (value));
      break;
    case PROP_TABLE_CAPTION_OBJECT:
      if (ATK_IS_TABLE (accessible))
        atk_table_set_caption (ATK_TABLE (accessible), g_value_get_object (value));
      break;
    default:
      break;
    }
}

static void
atk_object_real_get_property (GObject      *object,
                              guint         prop_id,
                              GValue       *value,
                              GParamSpec   *pspec)
{
  AtkObject *accessible;

  accessible = ATK_OBJECT (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, atk_object_get_name (accessible));
      break;
    case PROP_DESCRIPTION:
      g_value_set_string (value, atk_object_get_description (accessible));
      break;
    case PROP_ROLE:
      g_value_set_int (value, atk_object_get_role (accessible));
      break;
    case PROP_LAYER:
      if (ATK_IS_COMPONENT (accessible))
        g_value_set_int (value, atk_component_get_layer (ATK_COMPONENT (accessible)));
      break;
    case PROP_MDI_ZORDER:
      if (ATK_IS_COMPONENT (accessible))
        g_value_set_int (value, atk_component_get_mdi_zorder (ATK_COMPONENT (accessible)));
      break;
    case PROP_PARENT:
      g_value_set_object (value, atk_object_get_parent (accessible));
      break;
    case PROP_VALUE:
      if (ATK_IS_VALUE (accessible))
        atk_value_get_current_value (ATK_VALUE (accessible), value);
      break;
    case PROP_TABLE_SUMMARY:
      if (ATK_IS_TABLE (accessible))
        g_value_set_object (value, atk_table_get_summary (ATK_TABLE (accessible)));
      break;
    case PROP_TABLE_CAPTION_OBJECT:
      if (ATK_IS_TABLE (accessible))
        g_value_set_object (value, atk_table_get_caption (ATK_TABLE (accessible)));
      break;
    case PROP_HYPERTEXT_NUM_LINKS:
      if (ATK_IS_HYPERTEXT (accessible))
        g_value_set_int (value, atk_hypertext_get_n_links (ATK_HYPERTEXT (accessible)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
atk_object_finalize (GObject *object)
{
  AtkObject        *accessible;

  g_return_if_fail (ATK_IS_OBJECT (object));

  accessible = ATK_OBJECT (object);

  g_free (accessible->name);
  g_free (accessible->description);

  /*
   * Free memory allocated for relation set if it have been allocated.
   */
  if (accessible->relation_set)
    g_object_unref (accessible->relation_set);

  if (accessible->accessible_parent)
    g_object_unref (accessible->accessible_parent);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static const gchar*
atk_object_real_get_name (AtkObject *object)
{
  return object->name;
}

static const gchar*
atk_object_real_get_description (AtkObject *object)
{
  return object->description;
}

static AtkObject*
atk_object_real_get_parent (AtkObject       *object)
{
  return atk_object_peek_parent (object);
}

static AtkRole
atk_object_real_get_role (AtkObject       *object)
{
  return object->role;
}

static AtkLayer
atk_object_real_get_layer (AtkObject       *object)
{
  return object->layer;
}

static AtkStateSet*
atk_object_real_ref_state_set (AtkObject *accessible) 
{
  AtkStateSet *state_set;
  AtkObject *focus_object;

  state_set = atk_state_set_new ();

  focus_object = atk_get_focus_object ();
  if (focus_object == accessible)
    atk_state_set_add_state (state_set, ATK_STATE_FOCUSED);

  return state_set; 
}

static void
atk_object_real_set_name (AtkObject       *object,
                          const gchar     *name)
{
  g_free (object->name);
  object->name = g_strdup (name);
}

static void
atk_object_real_set_description (AtkObject       *object,
                                 const gchar     *description)
{
  g_free (object->description);
  object->description = g_strdup (description);
}

static void
atk_object_real_set_parent (AtkObject       *object,
                            AtkObject       *parent)
{
  if (object->accessible_parent)
    g_object_unref (object->accessible_parent);

  object->accessible_parent = parent;
  if (object->accessible_parent)
    g_object_ref (object->accessible_parent);
}

static void
atk_object_real_set_role (AtkObject *object,
                          AtkRole   role)
{
  object->role = role;
}

/**
 * atk_object_initialize:
 * @accessible: a #AtkObject
 * @data: a #gpointer which identifies the object for which the AtkObject was created.
 *
 * This function is called when implementing subclasses of #AtkObject.
 * It does initialization required for the new object. It is intended
 * that this function should called only in the ..._new() functions used
 * to create an instance of a subclass of #AtkObject
 **/
void
atk_object_initialize (AtkObject  *accessible,
                       gpointer   data)
{
  AtkObjectClass *klass;

  g_return_if_fail (ATK_IS_OBJECT (accessible));

  klass = ATK_OBJECT_GET_CLASS (accessible);
  if (klass->initialize)
    klass->initialize (accessible, data);
}

/*
 * This function is a signal handler for notify signal which gets emitted
 * when a property changes value.
 *
 * It constructs an AtkPropertyValues structure and emits a "property_changed"
 * signal which causes the user specified AtkPropertyChangeHandler
 * to be called.
 */
static void
atk_object_notify (GObject     *obj,
                   GParamSpec  *pspec)
{
  AtkPropertyValues values = { NULL, };

  g_value_init (&values.new_value, pspec->value_type);
  g_object_get_property (obj, pspec->name, &values.new_value);
  values.property_name = pspec->name;
  g_signal_emit (obj, atk_object_signals[PROPERTY_CHANGE],
                 g_quark_from_string (pspec->name),
                 &values, NULL);
  g_value_unset (&values.new_value);
}

/**
 * atk_role_get_name:
 * @role: The #AtkRole whose name is required
 *
 * Gets the description string describing the #AtkRole @role.
 *
 * Returns: the string describing the AtkRole
 */
const gchar*
atk_role_get_name (AtkRole role)
{
  g_return_val_if_fail (role >= 0, NULL);

  if (!role_names)
    initialize_role_names ();

  if (role < role_names->len)
    return g_ptr_array_index (role_names, role);

  return NULL;
}

/**
 * atk_role_get_localized_name:
 * @role: The #AtkRole whose localized name is required
 *
 * Gets the localized description string describing the #AtkRole @role.
 *
 * Returns: the localized string describing the AtkRole
 **/
const gchar*
atk_role_get_localized_name (AtkRole role)
{
  _gettext_initialization ();

  return dgettext (GETTEXT_PACKAGE, atk_role_get_name (role));
}

static const gchar*
atk_object_real_get_object_locale (AtkObject *object)
{
  return setlocale (LC_MESSAGES, NULL);
}

/**
 * atk_object_get_object_locale:
 * @accessible: an #AtkObject
 *
 * Gets a UTF-8 string indicating the POSIX-style LC_MESSAGES locale
 * of @accessible.
 *
 * Since: 2.8
 *
 * Returns: a UTF-8 string indicating the POSIX-style LC_MESSAGES
 *          locale of @accessible.
 **/
const gchar*
atk_object_get_object_locale (AtkObject *accessible)
{
  AtkObjectClass *klass;

  g_return_val_if_fail (ATK_IS_OBJECT (accessible), NULL);

  klass = ATK_OBJECT_GET_CLASS (accessible);
  if (klass->get_object_locale)
    return (klass->get_object_locale) (accessible);
  else
    return NULL;
}


/**
 * atk_role_for_name:
 * @name: a string which is the (non-localized) name of an ATK role.
 *
 * Get the #AtkRole type corresponding to a rolew name.
 *
 * Returns: the #AtkRole enumerated type corresponding to the specified name,
 *          or #ATK_ROLE_INVALID if no matching role is found.
 **/
AtkRole
atk_role_for_name (const gchar *name)
{
  AtkRole role = ATK_ROLE_INVALID;
  gint i;

  g_return_val_if_fail (name, ATK_ROLE_INVALID);

  if (!role_names)
    initialize_role_names ();

  for (i = 0; i < role_names->len; i++)
    {
      gchar *role_name = (gchar *)g_ptr_array_index (role_names, i);

      g_return_val_if_fail (role_name, ATK_ROLE_INVALID);

      if (strcmp (name, role_name) == 0)
        {
          role = i;
          break;
        }
    }

  return role;
}

/**
 * atk_object_add_relationship:
 * @object: The #AtkObject to which an AtkRelation is to be added. 
 * @relationship: The #AtkRelationType of the relation
 * @target: The #AtkObject which is to be the target of the relation.
 *
 * Adds a relationship of the specified type with the specified target.
 *
 * Returns: TRUE if the relationship is added.
 **/
gboolean
atk_object_add_relationship (AtkObject       *object,
                             AtkRelationType relationship,
                             AtkObject       *target)
{
  AtkObject *array[1];
  AtkRelation *relation;

  g_return_val_if_fail (ATK_IS_OBJECT (object), FALSE);
  g_return_val_if_fail (ATK_IS_OBJECT (target), FALSE);

  if (atk_relation_set_contains_target (object->relation_set,
                                        relationship, target))
    return FALSE;

  array[0] = target;
  relation = atk_relation_new (array, 1, relationship);
  atk_relation_set_add (object->relation_set, relation);
  g_object_unref (relation);

  return TRUE;
}

/**
 * atk_object_remove_relationship:
 * @object: The #AtkObject from which an AtkRelation is to be removed. 
 * @relationship: The #AtkRelationType of the relation
 * @target: The #AtkObject which is the target of the relation to be removed.
 *
 * Removes a relationship of the specified type with the specified target.
 *
 * Returns: TRUE if the relationship is removed.
 **/
gboolean
atk_object_remove_relationship (AtkObject       *object,
                                AtkRelationType relationship,
                                AtkObject       *target)
{
  gboolean ret = FALSE;
  AtkRelation *relation;
  GPtrArray *array;

  g_return_val_if_fail (ATK_IS_OBJECT (object), FALSE);
  g_return_val_if_fail (ATK_IS_OBJECT (target), FALSE);

  relation = atk_relation_set_get_relation_by_type (object->relation_set, relationship);

  if (relation)
    {
      ret = atk_relation_remove_target (relation, target);
      array = atk_relation_get_target (relation);
      if (!array || array->len == 0)
        atk_relation_set_remove (object->relation_set, relation);
    }
  return ret;
}

static void
atk_object_real_initialize (AtkObject *accessible,
                            gpointer  data)
{
  return;
}
