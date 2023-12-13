/* ATK -  Accessibility Toolkit
 *
 * Copyright (C) 2012 Igalia, S.L.
 *
 * Author: Alejandro Piñeiro Iglesias <apinheiro@igalia.com>
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
 */

#include "config.h"

#include "atk.h"

/**
 * SECTION:atkversion
 * @Short_description: Variables and functions to check the ATK version
 * @Title: Versioning macros
 *
 * ATK provides a set of macros and methods for checking the version
 * of the library at compile and run time.
 */

/**
 * atk_get_major_version:
 *
 * Returns the major version number of the ATK library.  (e.g. in ATK
 * version 2.7.4 this is 2.)
 *
 * This function is in the library, so it represents the ATK library
 * your code is running against. In contrast, the #ATK_MAJOR_VERSION
 * macro represents the major version of the ATK headers you have
 * included when compiling your code.
 *
 * Returns: the major version number of the ATK library
 *
 * Since: 2.8
 */
guint
atk_get_major_version (void)
{
  return ATK_MAJOR_VERSION;
}

/**
 * atk_get_minor_version:
 *
 * Returns the minor version number of the ATK library.  (e.g. in ATK
 * version 2.7.4 this is 7.)
 *
 * This function is in the library, so it represents the ATK library
 * your code is are running against. In contrast, the
 * #ATK_MINOR_VERSION macro represents the minor version of the ATK
 * headers you have included when compiling your code.
 *
 * Returns: the minor version number of the ATK library
 *
 * Since: 2.8
 */
guint
atk_get_minor_version (void)
{
  return ATK_MINOR_VERSION;
}

/**
 * atk_get_micro_version:
 *
 * Returns the micro version number of the ATK library.  (e.g. in ATK
 * version 2.7.4 this is 4.)
 *
 * This function is in the library, so it represents the ATK library
 * your code is are running against. In contrast, the
 * #ATK_MICRO_VERSION macro represents the micro version of the ATK
 * headers you have included when compiling your code.
 *
 * Returns: the micro version number of the ATK library
 *
 * Since: 2.8
 */
guint
atk_get_micro_version (void)
{
  return ATK_MICRO_VERSION;
}

/**
 * atk_get_binary_age:
 *
 * Returns the binary age as passed to libtool when building the ATK
 * library the process is running against.
 *
 * Returns: the binary age of the ATK library
 *
 * Since: 2.8
 */
guint
atk_get_binary_age (void)
{
  return ATK_BINARY_AGE;
}

/**
 * atk_get_interface_age:
 *
 * Returns the interface age as passed to libtool when building the
 * ATK library the process is running against.
 *
 * Returns: the interface age of the ATK library
 *
 * Since: 2.8
 */
guint
atk_get_interface_age (void)
{
  return ATK_INTERFACE_AGE;
}
