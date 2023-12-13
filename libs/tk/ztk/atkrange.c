/* ATK -  Accessibility Toolkit
 * Copyright 2014 Igalia S.L.
 *
 * Author: Alejandro Piñeiro Iglesias <apinheiro@igalia.com>
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

#include "atkvalue.h"

/**
 * SECTION:atkrange
 * @Short_description: A given range or subrange, to be used with #AtkValue
 * @Title:AtkRange
 *
 * #AtkRange are used on #AtkValue, in order to represent the full
 * range of a given component (for example an slider or a range
 * control), or to define each individual subrange this full range is
 * splitted if available. See #AtkValue documentation for further
 * details.
 */

struct _AtkRange {
  gdouble lower;
  gdouble upper;
  gchar *description;
};

/**
 * atk_range_copy:
 * @src: #AtkRange to copy
 *
 * Returns a new #AtkRange that is a exact copy of @src
 *
 * Since: 2.12
 *
 * Returns: (transfer full): a new #AtkRange copy of @src
 */
AtkRange *
atk_range_copy (AtkRange *src)
{
  g_return_val_if_fail (src != NULL, NULL);

  return atk_range_new (src->lower,
                        src->upper,
                        src->description);
}

/**
 * atk_range_free:
 * @range: #AtkRange to free
 *
 * Free @range
 *
 * Since: 2.12
 */
void
atk_range_free (AtkRange *range)
{
  g_return_if_fail (range != NULL);

  if (range->description)
    g_free (range->description);

  g_slice_free (AtkRange, range);
}

G_DEFINE_BOXED_TYPE (AtkRange, atk_range, atk_range_copy,
                     atk_range_free)


/**
 * atk_range_new:
 * @lower_limit: inferior limit for this range
 * @upper_limit: superior limit for this range
 * @description: human readable description of this range.
 *
 * Creates a new #AtkRange.
 *
 * Since: 2.12
 *
 * Returns: (transfer full): a new #AtkRange
 *
 */
AtkRange*
atk_range_new  (gdouble   lower_limit,
                gdouble   upper_limit,
                const gchar *description)
{
  AtkRange *range;

  range = g_slice_new0 (AtkRange);

  range->lower = lower_limit;
  range->upper = upper_limit;
  if (description != NULL)
    range->description = g_strdup (description);

  return range;
}

/**
 * atk_range_get_lower_limit:
 * @range: an #AtkRange
 *
 * Returns the lower limit of @range
 *
 * Since: 2.12
 *
 * Returns: the lower limit of @range
 */
gdouble
atk_range_get_lower_limit  (AtkRange *range)
{
  g_return_val_if_fail (range != NULL, 0);

  return range->lower;
}

/**
 * atk_range_get_upper_limit:
 * @range: an #AtkRange
 *
 * Returns the upper limit of @range
 *
 * Since: 2.12
 *
 * Returns: the upper limit of @range
 */
gdouble
atk_range_get_upper_limit (AtkRange *range)
{
  g_return_val_if_fail (range != NULL, 0);

  return range->upper;
}

/**
 * atk_range_get_description:
 * @range: an #AtkRange
 *
 * Returns the human readable description of @range
 *
 * Since: 2.12
 *
 * Returns: the human-readable description of @range
 */
const gchar*
atk_range_get_description  (AtkRange *range)
{
  g_return_val_if_fail (range != NULL, NULL);

  return range->description;
}
