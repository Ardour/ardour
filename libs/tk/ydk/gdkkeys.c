/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2000 Red Hat, Inc.
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

#include "gdkdisplay.h"
#include "gdkkeys.h"
#include "gdkalias.h"

enum {
  DIRECTION_CHANGED,
  KEYS_CHANGED,
  STATE_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GdkKeymap, gdk_keymap, G_TYPE_OBJECT)

static void
gdk_keymap_class_init (GdkKeymapClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /**
   * GdkKeymap::direction-changed:
   * @keymap: the object on which the signal is emitted
   * 
   * The ::direction-changed signal gets emitted when the direction of
   * the keymap changes. 
   *
   * Since: 2.0
   */
  signals[DIRECTION_CHANGED] =
    g_signal_new ("direction-changed",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GdkKeymapClass, direction_changed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE,
		  0);
  /**
   * GdkKeymap::keys-changed:
   * @keymap: the object on which the signal is emitted
   *
   * The ::keys-changed signal is emitted when the mapping represented by
   * @keymap changes.
   *
   * Since: 2.2
   */
  signals[KEYS_CHANGED] =
    g_signal_new ("keys-changed",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GdkKeymapClass, keys_changed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE,
		  0);

  /**
   * GdkKeymap::state-changed:
   * @keymap: the object on which the signal is emitted
   *
   * The ::state-changed signal is emitted when the state of the
   * keyboard changes, e.g when Caps Lock is turned on or off.
   * See gdk_keymap_get_caps_lock_state().
   *
   * Since: 2.16
   */
  signals[STATE_CHANGED] =
    g_signal_new ("state_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GdkKeymapClass, state_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 
                  0);
}

static void
gdk_keymap_init (GdkKeymap *keymap)
{
}

/* Other key-handling stuff
 */

#ifndef HAVE_XCONVERTCASE
#include "gdkkeysyms.h"

/* compatibility function from X11R6.3, since XConvertCase is not
 * supplied by X11R5.
 */
/**
 * gdk_keyval_convert_case:
 * @symbol: a keyval
 * @lower: (out): return location for lowercase version of @symbol
 * @upper: (out): return location for uppercase version of @symbol
 *
 * Obtains the upper- and lower-case versions of the keyval @symbol.
 * Examples of keyvals are #GDK_a, #GDK_Enter, #GDK_F1, etc.
 *
 **/
void
gdk_keyval_convert_case (guint symbol,
			 guint *lower,
			 guint *upper)
{
  guint xlower = symbol;
  guint xupper = symbol;

  /* Check for directly encoded 24-bit UCS characters: */
  if ((symbol & 0xff000000) == 0x01000000)
    {
      if (lower)
	*lower = gdk_unicode_to_keyval (g_unichar_tolower (symbol & 0x00ffffff));
      if (upper)
	*upper = gdk_unicode_to_keyval (g_unichar_toupper (symbol & 0x00ffffff));
      return;
    }

  switch (symbol >> 8)
    {
    case 0: /* Latin 1 */
      if ((symbol >= GDK_A) && (symbol <= GDK_Z))
	xlower += (GDK_a - GDK_A);
      else if ((symbol >= GDK_a) && (symbol <= GDK_z))
	xupper -= (GDK_a - GDK_A);
      else if ((symbol >= GDK_Agrave) && (symbol <= GDK_Odiaeresis))
	xlower += (GDK_agrave - GDK_Agrave);
      else if ((symbol >= GDK_agrave) && (symbol <= GDK_odiaeresis))
	xupper -= (GDK_agrave - GDK_Agrave);
      else if ((symbol >= GDK_Ooblique) && (symbol <= GDK_Thorn))
	xlower += (GDK_oslash - GDK_Ooblique);
      else if ((symbol >= GDK_oslash) && (symbol <= GDK_thorn))
	xupper -= (GDK_oslash - GDK_Ooblique);
      break;
      
    case 1: /* Latin 2 */
      /* Assume the KeySym is a legal value (ignore discontinuities) */
      if (symbol == GDK_Aogonek)
	xlower = GDK_aogonek;
      else if (symbol >= GDK_Lstroke && symbol <= GDK_Sacute)
	xlower += (GDK_lstroke - GDK_Lstroke);
      else if (symbol >= GDK_Scaron && symbol <= GDK_Zacute)
	xlower += (GDK_scaron - GDK_Scaron);
      else if (symbol >= GDK_Zcaron && symbol <= GDK_Zabovedot)
	xlower += (GDK_zcaron - GDK_Zcaron);
      else if (symbol == GDK_aogonek)
	xupper = GDK_Aogonek;
      else if (symbol >= GDK_lstroke && symbol <= GDK_sacute)
	xupper -= (GDK_lstroke - GDK_Lstroke);
      else if (symbol >= GDK_scaron && symbol <= GDK_zacute)
	xupper -= (GDK_scaron - GDK_Scaron);
      else if (symbol >= GDK_zcaron && symbol <= GDK_zabovedot)
	xupper -= (GDK_zcaron - GDK_Zcaron);
      else if (symbol >= GDK_Racute && symbol <= GDK_Tcedilla)
	xlower += (GDK_racute - GDK_Racute);
      else if (symbol >= GDK_racute && symbol <= GDK_tcedilla)
	xupper -= (GDK_racute - GDK_Racute);
      break;
      
    case 2: /* Latin 3 */
      /* Assume the KeySym is a legal value (ignore discontinuities) */
      if (symbol >= GDK_Hstroke && symbol <= GDK_Hcircumflex)
	xlower += (GDK_hstroke - GDK_Hstroke);
      else if (symbol >= GDK_Gbreve && symbol <= GDK_Jcircumflex)
	xlower += (GDK_gbreve - GDK_Gbreve);
      else if (symbol >= GDK_hstroke && symbol <= GDK_hcircumflex)
	xupper -= (GDK_hstroke - GDK_Hstroke);
      else if (symbol >= GDK_gbreve && symbol <= GDK_jcircumflex)
	xupper -= (GDK_gbreve - GDK_Gbreve);
      else if (symbol >= GDK_Cabovedot && symbol <= GDK_Scircumflex)
	xlower += (GDK_cabovedot - GDK_Cabovedot);
      else if (symbol >= GDK_cabovedot && symbol <= GDK_scircumflex)
	xupper -= (GDK_cabovedot - GDK_Cabovedot);
      break;
      
    case 3: /* Latin 4 */
      /* Assume the KeySym is a legal value (ignore discontinuities) */
      if (symbol >= GDK_Rcedilla && symbol <= GDK_Tslash)
	xlower += (GDK_rcedilla - GDK_Rcedilla);
      else if (symbol >= GDK_rcedilla && symbol <= GDK_tslash)
	xupper -= (GDK_rcedilla - GDK_Rcedilla);
      else if (symbol == GDK_ENG)
	xlower = GDK_eng;
      else if (symbol == GDK_eng)
	xupper = GDK_ENG;
      else if (symbol >= GDK_Amacron && symbol <= GDK_Umacron)
	xlower += (GDK_amacron - GDK_Amacron);
      else if (symbol >= GDK_amacron && symbol <= GDK_umacron)
	xupper -= (GDK_amacron - GDK_Amacron);
      break;
      
    case 6: /* Cyrillic */
      /* Assume the KeySym is a legal value (ignore discontinuities) */
      if (symbol >= GDK_Serbian_DJE && symbol <= GDK_Serbian_DZE)
	xlower -= (GDK_Serbian_DJE - GDK_Serbian_dje);
      else if (symbol >= GDK_Serbian_dje && symbol <= GDK_Serbian_dze)
	xupper += (GDK_Serbian_DJE - GDK_Serbian_dje);
      else if (symbol >= GDK_Cyrillic_YU && symbol <= GDK_Cyrillic_HARDSIGN)
	xlower -= (GDK_Cyrillic_YU - GDK_Cyrillic_yu);
      else if (symbol >= GDK_Cyrillic_yu && symbol <= GDK_Cyrillic_hardsign)
	xupper += (GDK_Cyrillic_YU - GDK_Cyrillic_yu);
      break;
      
    case 7: /* Greek */
      /* Assume the KeySym is a legal value (ignore discontinuities) */
      if (symbol >= GDK_Greek_ALPHAaccent && symbol <= GDK_Greek_OMEGAaccent)
	xlower += (GDK_Greek_alphaaccent - GDK_Greek_ALPHAaccent);
      else if (symbol >= GDK_Greek_alphaaccent && symbol <= GDK_Greek_omegaaccent &&
	       symbol != GDK_Greek_iotaaccentdieresis &&
	       symbol != GDK_Greek_upsilonaccentdieresis)
	xupper -= (GDK_Greek_alphaaccent - GDK_Greek_ALPHAaccent);
      else if (symbol >= GDK_Greek_ALPHA && symbol <= GDK_Greek_OMEGA)
	xlower += (GDK_Greek_alpha - GDK_Greek_ALPHA);
      else if (symbol >= GDK_Greek_alpha && symbol <= GDK_Greek_omega &&
	       symbol != GDK_Greek_finalsmallsigma)
	xupper -= (GDK_Greek_alpha - GDK_Greek_ALPHA);
      break;
    }

  if (lower)
    *lower = xlower;
  if (upper)
    *upper = xupper;
}
#endif

guint
gdk_keyval_to_upper (guint keyval)
{
  guint result;
  
  gdk_keyval_convert_case (keyval, NULL, &result);

  return result;
}

guint
gdk_keyval_to_lower (guint keyval)
{
  guint result;
  
  gdk_keyval_convert_case (keyval, &result, NULL);

  return result;
}

gboolean
gdk_keyval_is_upper (guint keyval)
{
  if (keyval)
    {
      guint upper_val = 0;
      
      gdk_keyval_convert_case (keyval, NULL, &upper_val);
      return upper_val == keyval;
    }
  return FALSE;
}

gboolean
gdk_keyval_is_lower (guint keyval)
{
  if (keyval)
    {
      guint lower_val = 0;
      
      gdk_keyval_convert_case (keyval, &lower_val, NULL);
      return lower_val == keyval;
    }
  return FALSE;
}

/** 
 * gdk_keymap_get_default:
 * @returns: the #GdkKeymap attached to the default display.
 *
 * Returns the #GdkKeymap attached to the default display.
 **/
GdkKeymap*
gdk_keymap_get_default (void)
{
  return gdk_keymap_get_for_display (gdk_display_get_default ());
}

#define __GDK_KEYS_C__
#include "gdkaliasdef.c"
