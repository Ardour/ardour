/* ATK -  Accessibility Toolkit
 * Copyright 2001, 2002, 2003 Sun Microsystems Inc.
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
#include <glib/gi18n-lib.h>

#include "atkvalue.h"
#include "atkmarshal.h"
#include "atk-enum-types.h"
#include "atkprivate.h"

/**
 * SECTION:atkvalue
 * @Short_description: The ATK interface implemented by valuators and
 *  components which display or select a value from a bounded range of
 *  values.
 * @Title:AtkValue
 *
 * #AtkValue should be implemented for components which either display
 * a value from a bounded range, or which allow the user to specify a
 * value from a bounded range, or both. For instance, most sliders and
 * range controls, as well as dials, should have #AtkObject
 * representations which implement #AtkValue on the component's
 * behalf. #AtKValues may be read-only, in which case attempts to
 * alter the value return would fail.
 *
 * <refsect1 id="current-value-text">
 * <title>On the subject of current value text</title>
 * <para>
 * In addition to providing the current value, implementors can
 * optionally provide an end-user-consumable textual description
 * associated with this value. This description should be included
 * when the numeric value fails to convey the full, on-screen
 * representation seen by users.
 * </para>
 *
 * <example>
 * <title>Password strength</title>
 * A password strength meter whose value changes as the user types
 * their new password. Red is used for values less than 4.0, yellow
 * for values between 4.0 and 7.0, and green for values greater than
 * 7.0. In this instance, value text should be provided by the
 * implementor. Appropriate value text would be "weak", "acceptable,"
 * and "strong" respectively.
 * </example>
 *
 * A level bar whose value changes to reflect the battery charge. The
 * color remains the same regardless of the charge and there is no
 * on-screen text reflecting the fullness of the battery. In this
 * case, because the position within the bar is the only indication
 * the user has of the current charge, value text should not be
 * provided by the implementor.
 *
 * <refsect2 id="implementor-notes">
 * <title>Implementor Notes</title>
 * <para>
 * Implementors should bear in mind that assistive technologies will
 * likely prefer the value text provided over the numeric value when
 * presenting a widget's value. As a result, strings not intended for
 * end users should not be exposed in the value text, and strings
 * which are exposed should be localized. In the case of widgets which
 * display value text on screen, for instance through a separate label
 * in close proximity to the value-displaying widget, it is still
 * expected that implementors will expose the value text using the
 * above API.
 * </para>
 *
 * <para>
 * #AtkValue should NOT be implemented for widgets whose displayed
 * value is not reflective of a meaningful amount. For instance, a
 * progress pulse indicator whose value alternates between 0.0 and 1.0
 * to indicate that some process is still taking place should not
 * implement #AtkValue because the current value does not reflect
 * progress towards completion.
 * </para>
 * </refsect2>
 * </refsect1>
 *
 * <refsect1 id="ranges">
 * <title>On the subject of ranges</title>
 * <para>
 * In addition to providing the minimum and maximum values,
 * implementors can optionally provide details about subranges
 * associated with the widget. These details should be provided by the
 * implementor when both of the following are communicated visually to
 * the end user:
 * </para>
 * <itemizedlist>
 *   <listitem>The existence of distinct ranges such as "weak",
 *   "acceptable", and "strong" indicated by color, bar tick marks,
 *   and/or on-screen text.</listitem>
 *   <listitem>Where the current value stands within a given subrange,
 *   for instance illustrating progression from very "weak" towards
 *   nearly "acceptable" through changes in shade and/or position on
 *   the bar within the "weak" subrange.</listitem>
 * </itemizedlist>
 * <para>
 * If both of the above do not apply to the widget, it should be
 * sufficient to expose the numeric value, along with the value text
 * if appropriate, to make the widget accessible.
 * </para>
 *
 * <refsect2 id="ranges-implementor-notes">
 * <title>Implementor Notes</title>
 * <para>
 * If providing subrange details is deemed necessary, all possible
 * values of the widget are expected to fall within one of the
 * subranges defined by the implementor.
 * </para>
 * </refsect2>
 * </refsect1>
 *
 * <refsect1 id="localization">
 * <title>On the subject of localization of end-user-consumable text
 * values</title>
 * <para>
 * Because value text and subrange descriptors are human-consumable,
 * implementors are expected to provide localized strings which can be
 * directly presented to end users via their assistive technology. In
 * order to simplify this for implementors, implementors can use
 * atk_value_type_get_localized_name() with the following
 * already-localized constants for commonly-needed values can be used:
 * </para>
 *
 * <itemizedlist>
 *   <listitem>ATK_VALUE_VERY_WEAK</listitem>
 *   <listitem>ATK_VALUE_WEAK</listitem>
 *   <listitem>ATK_VALUE_ACCEPTABLE</listitem>
 *   <listitem>ATK_VALUE_STRONG</listitem>
 *   <listitem>ATK_VALUE_VERY_STRONG</listitem>
 *   <listitem>ATK_VALUE_VERY_LOW</listitem>
 *   <listitem>ATK_VALUE_LOW</listitem>
 *   <listitem>ATK_VALUE_MEDIUM</listitem>
 *   <listitem>ATK_VALUE_HIGH</listitem>
 *   <listitem>ATK_VALUE_VERY_HIGH</listitem>
 *   <listitem>ATK_VALUE_VERY_BAD</listitem>
 *   <listitem>ATK_VALUE_BAD</listitem>
 *   <listitem>ATK_VALUE_GOOD</listitem>
 *   <listitem>ATK_VALUE_VERY_GOOD</listitem>
 *   <listitem>ATK_VALUE_BEST</listitem>
 *   <listitem>ATK_VALUE_SUBSUBOPTIMAL</listitem>
 *   <listitem>ATK_VALUE_SUBOPTIMAL</listitem>
 *   <listitem>ATK_VALUE_OPTIMAL</listitem>
 * </itemizedlist>
 * <para>
 * Proposals for additional constants, along with their use cases,
 * should be submitted to the GNOME Accessibility Team.
 * </para>
 * </refsect1>
 *
 * <refsect1 id="changes">
 * <title>On the subject of changes</title>
 * <para>
 * Note that if there is a textual description associated with the new
 * numeric value, that description should be included regardless of
 * whether or not it has also changed.
 * </para>
 * </refsect1>
 */

static GPtrArray *value_type_names = NULL;

enum {
  VALUE_CHANGED,
  LAST_SIGNAL
};

/* These are listed here for extraction by intltool */
#if 0
/* Translators: This string describes a range within value-related
 * widgets such as a password-strength meter. Note that what such a
 * widget presents is controlled by application developers. Thus
 * assistive technologies such as screen readers are expected to
 * present this string alone or as a token in a list.
 */
N_("very weak")
/* Translators: This string describes a range within value-related
 * widgets such as a password-strength meter. Note that what such a
 * widget presents is controlled by application developers. Thus
 * assistive technologies such as screen readers are expected to
 * present this string alone or as a token in a list.
 */
N_("weak")
/* Translators: This string describes a range within value-related
 * widgets such as a password-strength meter. Note that what such a
 * widget presents is controlled by application developers. Thus
 * assistive technologies such as screen readers are expected to
 * present this string alone or as a token in a list.
 */
N_("acceptable")
/* Translators: This string describes a range within value-related
 * widgets such as a password-strength meter. Note that what such a
 * widget presents is controlled by application developers. Thus
 * assistive technologies such as screen readers are expected to
 * present this string alone or as a token in a list.
 */
N_("strong")
/* Translators: This string describes a range within value-related
 * widgets such as a password-strength meter. Note that what such a
 * widget presents is controlled by application developers. Thus
 * assistive technologies such as screen readers are expected to
 * present this string alone or as a token in a list.
 */
N_("very strong")
/* Translators: This string describes a range within value-related
 * widgets such as a volume slider. Note that what such a widget
 * presents (e.g. temperature, volume, price) is controlled by
 * application developers. Thus assistive technologies such as screen
 * readers are expected to present this string alone or as a token in
 * a list.
 */
N_("very low")
/* Translators: This string describes a range within value-related
 * widgets such as a volume slider. Note that what such a widget
 * presents (e.g. temperature, volume, price) is controlled by
 * application developers. Thus assistive technologies such as screen
 * readers are expected to present this string alone or as a token in
 * a list.
 */
N_("medium")
/* Translators: This string describes a range within value-related
 * widgets such as a volume slider. Note that what such a widget
 * presents (e.g. temperature, volume, price) is controlled by
 * application developers. Thus assistive technologies such as screen
 * readers are expected to present this string alone or as a token in
 * a list.
 */
N_("high")
/* Translators: This string describes a range within value-related
 * widgets such as a volume slider. Note that what such a widget
 * presents (e.g. temperature, volume, price) is controlled by
 * application developers. Thus assistive technologies such as screen
 * readers are expected to present this string alone or as a token in
 * a list.
 */
N_("very high")
/* Translators: This string describes a range within value-related
 * widgets such as a hard drive usage. Note that what such a widget
 * presents (e.g. hard drive usage, network traffic) is controlled by
 * application developers. Thus assistive technologies such as screen
 * readers are expected to present this string alone or as a token in
 * a list.
 */
N_("very bad")
/* Translators: This string describes a range within value-related
 * widgets such as a hard drive usage. Note that what such a widget
 * presents (e.g. hard drive usage, network traffic) is controlled by
 * application developers. Thus assistive technologies such as screen
 * readers are expected to present this string alone or as a token in
 * a list.
 */
N_("bad")
/* Translators: This string describes a range within value-related
 * widgets such as a hard drive usage. Note that what such a widget
 * presents (e.g. hard drive usage, network traffic) is controlled by
 * application developers. Thus assistive technologies such as screen
 * readers are expected to present this string alone or as a token in
 * a list.
 */
N_("good")
/* Translators: This string describes a range within value-related
 * widgets such as a hard drive usage. Note that what such a widget
 * presents (e.g. hard drive usage, network traffic) is controlled by
 * application developers. Thus assistive technologies such as screen
 * readers are expected to present this string alone or as a token in
 * a list.
 */
N_("very good")
/* Translators: This string describes a range within value-related
 * widgets such as a hard drive usage. Note that what such a widget
 * presents (e.g. hard drive usage, network traffic) is controlled by
 * application developers. Thus assistive technologies such as screen
 * readers are expected to present this string alone or as a token in
 * a list.
 */
N_("best")
#endif

static void atk_value_base_init (AtkValueIface *class);

static guint atk_value_signals[LAST_SIGNAL] = {0};

GType
atk_value_get_type (void)
{
  static GType type = 0;

  if (!type) {
    GTypeInfo tinfo =
    {
      sizeof (AtkValueIface),
      (GBaseInitFunc) atk_value_base_init,
      (GBaseFinalizeFunc) NULL,

    };

    type = g_type_register_static (G_TYPE_INTERFACE, "AtkValue", &tinfo, 0);
  }

  return type;
}

static void
atk_value_base_init (AtkValueIface *class)
{
  static gboolean initialized = FALSE;
  if (!initialized)
    {
      /**
       * AtkValue::value-changed:
       * @atkvalue: the object on which the signal was emitted.
       * @value: the new value in a numerical form.
       * @text: human readable text alternative (also called
       * description) of this object. NULL if not available.
       *
       * The 'value-changed' signal is emitted when the current value
       * that represent the object changes. @value is the numerical
       * representation of this new value.  @text is the human
       * readable text alternative of @value, and can be NULL if it is
       * not available. Note that if there is a textual description
       * associated with the new numeric value, that description
       * should be included regardless of whether or not it has also
       * changed.
       *
       * Example: a password meter whose value changes as the user
       * types their new password. Appropiate value text would be
       * "weak", "acceptable" and "strong".
       *
       * Since: 2.12
       */
      atk_value_signals[VALUE_CHANGED] =
        g_signal_new ("value_changed",
                      ATK_TYPE_VALUE,
                      G_SIGNAL_RUN_LAST,
                      0,
                      (GSignalAccumulator) NULL, NULL,
                      atk_marshal_VOID__DOUBLE_STRING,
                      G_TYPE_NONE,
                      2, G_TYPE_DOUBLE, G_TYPE_STRING);

      initialized = TRUE;
    }
}

/**
 * atk_value_get_current_value:
 * @obj: a GObject instance that implements AtkValueIface
 * @value: a #GValue representing the current accessible value
 *
 * Gets the value of this object.
 *
 * Deprecated: Since 2.12. Use atk_value_get_value_and_text()
 * instead.
 **/
void
atk_value_get_current_value (AtkValue *obj,
                             GValue   *value)
{
  AtkValueIface *iface;

  g_return_if_fail (value != NULL);
  g_return_if_fail (ATK_IS_VALUE (obj));

  iface = ATK_VALUE_GET_IFACE (obj);

  if (iface->get_current_value)
    {
      if (G_IS_VALUE (value))
        g_value_unset (value);
      else
        memset (value, 0, sizeof (*value));

      (iface->get_current_value) (obj, value);
    }
}

/**
 * atk_value_get_maximum_value:
 * @obj: a GObject instance that implements AtkValueIface
 * @value: a #GValue representing the maximum accessible value
 *
 * Gets the maximum value of this object.
 *
 * Deprecated: Since 2.12. Use atk_value_get_range() instead.
 **/
void
atk_value_get_maximum_value  (AtkValue *obj,
                              GValue   *value)
{
  AtkValueIface *iface;

  g_return_if_fail (value != NULL);
  g_return_if_fail (ATK_IS_VALUE (obj));

  iface = ATK_VALUE_GET_IFACE (obj);

  if (iface->get_maximum_value)
    {
      if (G_IS_VALUE (value))
        g_value_unset (value);
      else
        memset (value, 0, sizeof (*value));

      (iface->get_maximum_value) (obj, value);
    }
}

/**
 * atk_value_get_minimum_value:
 * @obj: a GObject instance that implements AtkValueIface
 * @value: a #GValue representing the minimum accessible value
 *
 * Gets the minimum value of this object.
 *
 * Deprecated: Since 2.12. Use atk_value_get_range() instead.
 **/
void
atk_value_get_minimum_value (AtkValue *obj,
                             GValue   *value)
{
  AtkValueIface *iface;

  g_return_if_fail (value != NULL);
  g_return_if_fail (ATK_IS_VALUE (obj));

  iface = ATK_VALUE_GET_IFACE (obj);

  if (iface->get_minimum_value)
    {
      if (G_IS_VALUE (value))
        g_value_unset (value);
      else
        memset (value, 0, sizeof (*value));

      (iface->get_minimum_value) (obj, value);
    }
}

/**
 * atk_value_get_minimum_increment:
 * @obj: a GObject instance that implements AtkValueIface
 * @value: a #GValue representing the minimum increment by which the accessible value may be changed
 *
 * Gets the minimum increment by which the value of this object may be changed.  If zero,
 * the minimum increment is undefined, which may mean that it is limited only by the 
 * floating point precision of the platform.
 *
 * Since: 1.12
 *
 * Deprecated: Since 2.12. Use atk_value_get_increment() instead.
 **/
void
atk_value_get_minimum_increment (AtkValue *obj,
                             GValue   *value)
{
  AtkValueIface *iface;

  g_return_if_fail (value != NULL);
  g_return_if_fail (ATK_IS_VALUE (obj));

  iface = ATK_VALUE_GET_IFACE (obj);

  if (iface->get_minimum_increment)
    {
      if (G_IS_VALUE (value))
        g_value_unset (value);
      else
        memset (value, 0, sizeof (*value));

      (iface->get_minimum_increment) (obj, value);
    }
}

/**
 * atk_value_set_current_value:
 * @obj: a GObject instance that implements AtkValueIface
 * @value: a #GValue which is the desired new accessible value.
 *
 * Sets the value of this object.
 *
 * Returns: %TRUE if new value is successfully set, %FALSE otherwise.
 *
 * Deprecated: Since 2.12. Use atk_value_set_value() instead.
 **/
gboolean
atk_value_set_current_value (AtkValue       *obj, 
                             const GValue   *value)
{
  AtkValueIface *iface;

  g_return_val_if_fail (ATK_IS_VALUE (obj), FALSE);
  g_return_val_if_fail (G_IS_VALUE (value), FALSE);

  iface = ATK_VALUE_GET_IFACE (obj);

  if (iface->set_current_value)
    return (iface->set_current_value) (obj, value);
  else
    return FALSE;
}


/**
 * atk_value_get_value_and_text:
 * @obj: a GObject instance that implements AtkValueIface
 * @value: (out): address of #gdouble to put the current value of @obj
 * @text: (out) (allow-none): address of #gchar to put the human
 * readable text alternative for @value
 *
 * Gets the current value and the human readable text alternative of
 * @obj. @text is a newly created string, that must be freed by the
 * caller. Can be NULL if not descriptor is available.
 *
 * Since: 2.12
 **/

void
atk_value_get_value_and_text (AtkValue *obj,
                              gdouble *value,
                              gchar  **text)
{
  AtkValueIface *iface;

  g_return_if_fail (ATK_IS_VALUE (obj));

  iface = ATK_VALUE_GET_IFACE (obj);

  if (iface->get_value_and_text)
    {
      (iface->get_value_and_text) (obj, value, text);
    }
}

/**
 * atk_value_get_range:
 * @obj: a GObject instance that implements AtkValueIface
 *
 * Gets the range of this object.
 *
 * Returns: (nullable) (transfer full): a newly allocated #AtkRange
 * that represents the minimum, maximum and descriptor (if available)
 * of @obj. NULL if that range is not defined.
 *
 * Since: 2.12
 **/
AtkRange*
atk_value_get_range (AtkValue *obj)
{
  AtkValueIface *iface;

  g_return_val_if_fail (ATK_IS_VALUE (obj), NULL);

  iface = ATK_VALUE_GET_IFACE (obj);

  if (iface->get_range)
    {
      return (iface->get_range) (obj);
    }
  else
    return NULL;
}

/**
 * atk_value_get_increment:
 * @obj: a GObject instance that implements AtkValueIface
 *
 * Gets the minimum increment by which the value of this object may be
 * changed.  If zero, the minimum increment is undefined, which may
 * mean that it is limited only by the floating point precision of the
 * platform.
 *
 * Return value: the minimum increment by which the value of this
 * object may be changed. zero if undefined.
 *
 * Since: 2.12
 **/
gdouble
atk_value_get_increment (AtkValue *obj)
{
  AtkValueIface *iface;

  g_return_val_if_fail (ATK_IS_VALUE (obj), 0);

  iface = ATK_VALUE_GET_IFACE (obj);

  if (iface->get_increment)
    {
      return (iface->get_increment) (obj);
    }
  else
    return 0;
}


/**
 * atk_value_get_sub_ranges:
 * @obj: a GObject instance that implements AtkValueIface
 *
 * Gets the list of subranges defined for this object. See #AtkValue
 * introduction for examples of subranges and when to expose them.
 *
 * Returns: (element-type AtkRange) (transfer full): an #GSList of
 * #AtkRange which each of the subranges defined for this object. Free
 * the returns list with g_slist_free().
 *
 * Since: 2.12
 **/
GSList*
atk_value_get_sub_ranges (AtkValue *obj)
{
  AtkValueIface *iface;

  g_return_val_if_fail (ATK_IS_VALUE (obj), NULL);

  iface = ATK_VALUE_GET_IFACE (obj);

  if (iface->get_sub_ranges)
    {
      return (iface->get_sub_ranges) (obj);
    }
  else
    return NULL;
}

/**
 * atk_value_set_value:
 * @obj: a GObject instance that implements AtkValueIface
 * @new_value: a double which is the desired new accessible value.
 *
 * Sets the value of this object.
 *
 * This method is intended to provide a way to change the value of the
 * object. In any case, it is possible that the value can't be
 * modified (ie: a read-only component). If the value changes due this
 * call, it is possible that the text could change, and will trigger
 * an #AtkValue::value-changed signal emission.
 *
 * Note for implementors: the deprecated atk_value_set_current_value()
 * method returned TRUE or FALSE depending if the value was assigned
 * or not. In the practice several implementors were not able to
 * decide it, and returned TRUE in any case. For that reason it is not
 * required anymore to return if the value was properly assigned or
 * not.
 *
 * Since: 2.12
 **/
void
atk_value_set_value (AtkValue     *obj,
                     const gdouble new_value)
{
  AtkValueIface *iface;

  g_return_if_fail (ATK_IS_VALUE (obj));

  iface = ATK_VALUE_GET_IFACE (obj);

  if (iface->set_value)
    {
      (iface->set_value) (obj, new_value);
    }
}

static void
initialize_value_type_names ()
{
  GTypeClass *enum_class;
  GEnumValue *enum_value;
  int i;
  gchar *value_type_name = NULL;

  if (value_type_names)
    return;

  value_type_names = g_ptr_array_new ();
  enum_class = g_type_class_ref (ATK_TYPE_VALUE_TYPE);
  if (!G_IS_ENUM_CLASS(enum_class))
    return;

  for (i = 0; i < ATK_VALUE_LAST_DEFINED; i++)
    {
      enum_value = g_enum_get_value (G_ENUM_CLASS (enum_class), i);
      value_type_name = g_strdup (enum_value->value_nick);
      _compact_name (value_type_name);
      g_ptr_array_add (value_type_names, value_type_name);
    }

  g_type_class_unref (enum_class);
}

/**
 * atk_value_type_get_name:
 * @role: The #AtkValueType whose name is required
 *
 * Gets the description string describing the #AtkValueType @value_type.
 *
 * Returns: the string describing the #AtkValueType
 */
const gchar*
atk_value_type_get_name (AtkValueType value_type)
{
  g_return_val_if_fail (value_type >= 0, NULL);

  if (!value_type_names)
    initialize_value_type_names ();

  if (value_type < value_type_names->len)
    return g_ptr_array_index (value_type_names, value_type);

  return NULL;
}

/**
 * atk_value_type_get_localized_name:
 * @value_type: The #AtkValueType whose localized name is required
 *
 * Gets the localized description string describing the #AtkValueType @value_type.
 *
 * Returns: the localized string describing the #AtkValueType
 **/
const gchar*
atk_value_type_get_localized_name (AtkValueType value_type)
{
  _gettext_initialization ();

  return dgettext (GETTEXT_PACKAGE, atk_value_type_get_name (value_type));
}
