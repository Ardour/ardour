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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>

#include "gdk.h"
#include "gdkx.h"

#include "gdkprivate-x11.h"
#include "gdkinternals.h"
#include "gdkdisplay-x11.h"
#include "gdkkeysyms.h"
#include "gdkalias.h"

#ifdef HAVE_XKB
#include <X11/XKBlib.h>

/* OSF-4.0 is apparently missing this macro
 */
#  ifndef XkbKeySymEntry
#    define XkbKeySymEntry(d,k,sl,g) \
 	(XkbKeySym(d,k,((XkbKeyGroupsWidth(d,k)*(g))+(sl))))
#  endif
#endif /* HAVE_XKB */

typedef struct _GdkKeymapX11   GdkKeymapX11;
typedef struct _GdkKeymapClass GdkKeymapX11Class;

#define GDK_TYPE_KEYMAP_X11          (gdk_keymap_x11_get_type ())
#define GDK_KEYMAP_X11(object)       (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_KEYMAP_X11, GdkKeymapX11))
#define GDK_IS_KEYMAP_X11(object)    (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_KEYMAP_X11))

typedef struct _DirectionCacheEntry DirectionCacheEntry;

struct _DirectionCacheEntry
{
  guint serial;
  Atom group_atom;
  PangoDirection direction;
};

struct _GdkKeymapX11
{
  GdkKeymap     parent_instance;

  gint min_keycode;
  gint max_keycode;
  KeySym* keymap;
  gint keysyms_per_keycode;
  XModifierKeymap* mod_keymap;
  guint lock_keysym;
  GdkModifierType group_switch_mask;
  GdkModifierType num_lock_mask;
  GdkModifierType modmap[8];
  PangoDirection current_direction;
  guint sun_keypad      : 1;
  guint have_direction  : 1;
  guint caps_lock_state : 1;
  guint current_serial;

#ifdef HAVE_XKB
  XkbDescPtr xkb_desc;
  /* We cache the directions */
  Atom current_group_atom;
  guint current_cache_serial;
  /* A cache of size four should be more than enough, people usually
   * have two groups around, and the xkb limit is four.  It still
   * works correct for more than four groups.  It's just the
   * cache.
   */
  DirectionCacheEntry group_direction_cache[4];
#endif
};

#define KEYMAP_USE_XKB(keymap) GDK_DISPLAY_X11 ((keymap)->display)->use_xkb
#define KEYMAP_XDISPLAY(keymap) GDK_DISPLAY_XDISPLAY ((keymap)->display)

static GType gdk_keymap_x11_get_type   (void);
static void  gdk_keymap_x11_class_init (GdkKeymapX11Class *klass);
static void  gdk_keymap_x11_init       (GdkKeymapX11      *keymap);
static void  gdk_keymap_x11_finalize   (GObject           *object);

static GdkKeymapClass *parent_class = NULL;

static GType
gdk_keymap_x11_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      const GTypeInfo object_info =
	{
	  sizeof (GdkKeymapClass),
	  (GBaseInitFunc) NULL,
	  (GBaseFinalizeFunc) NULL,
	  (GClassInitFunc) gdk_keymap_x11_class_init,
	  NULL,           /* class_finalize */
	  NULL,           /* class_data */
	  sizeof (GdkKeymapX11),
	  0,              /* n_preallocs */
	  (GInstanceInitFunc) gdk_keymap_x11_init,
	};
      
      object_type = g_type_register_static (GDK_TYPE_KEYMAP,
                                            g_intern_static_string ("GdkKeymapX11"),
                                            &object_info, 0);
    }
  
  return object_type;
}

static void
gdk_keymap_x11_class_init (GdkKeymapX11Class *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_keymap_x11_finalize;
}

static void
gdk_keymap_x11_init (GdkKeymapX11 *keymap)
{
  keymap->min_keycode = 0;
  keymap->max_keycode = 0;

  keymap->keymap = NULL;
  keymap->keysyms_per_keycode = 0;
  keymap->mod_keymap = NULL;
  
  keymap->num_lock_mask = 0;
  keymap->sun_keypad = FALSE;
  keymap->group_switch_mask = 0;
  keymap->lock_keysym = GDK_Caps_Lock;
  keymap->have_direction = FALSE;
  keymap->current_serial = 0;

#ifdef HAVE_XKB
  keymap->xkb_desc = NULL;
  keymap->current_group_atom = 0;
  keymap->current_cache_serial = 0;
#endif

}

static void
gdk_keymap_x11_finalize (GObject *object)
{
  GdkKeymapX11 *keymap_x11 = GDK_KEYMAP_X11 (object);

  if (keymap_x11->keymap)
    XFree (keymap_x11->keymap);

  if (keymap_x11->mod_keymap)
    XFreeModifiermap (keymap_x11->mod_keymap);

#ifdef HAVE_XKB
  if (keymap_x11->xkb_desc)
    XkbFreeKeyboard (keymap_x11->xkb_desc, XkbAllComponentsMask, True);
#endif

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static inline void
update_keyrange (GdkKeymapX11 *keymap_x11)
{
  if (keymap_x11->max_keycode == 0)
    XDisplayKeycodes (KEYMAP_XDISPLAY (GDK_KEYMAP (keymap_x11)),
		      &keymap_x11->min_keycode, &keymap_x11->max_keycode);
}

#ifdef HAVE_XKB

static void
update_modmap (Display      *display,
	       GdkKeymapX11 *keymap_x11)
{
  static struct {
    const gchar *name;
    Atom atom;
    GdkModifierType mask;
  } vmods[] = {
    { "Meta", 0, GDK_META_MASK },
    { "Super", 0, GDK_SUPER_MASK },
    { "Hyper", 0, GDK_HYPER_MASK },
    { NULL, 0, 0 }
  };

  gint i, j, k;

  if (!vmods[0].atom)
    for (i = 0; vmods[i].name; i++)
      vmods[i].atom = XInternAtom (display, vmods[i].name, FALSE);

  for (i = 0; i < 8; i++)
    keymap_x11->modmap[i] = 1 << i;

  for (i = 0; i < XkbNumVirtualMods; i++)
    {
      for (j = 0; vmods[j].atom; j++)
	{
	  if (keymap_x11->xkb_desc->names->vmods[i] == vmods[j].atom)
	    {
	      for (k = 0; k < 8; k++)
		{
		  if (keymap_x11->xkb_desc->server->vmods[i] & (1 << k))
		    keymap_x11->modmap[k] |= vmods[j].mask;
		}
	    }
	}
    }
}

static XkbDescPtr
get_xkb (GdkKeymapX11 *keymap_x11)
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (GDK_KEYMAP (keymap_x11)->display);
  Display *xdisplay = display_x11->xdisplay;
  
  update_keyrange (keymap_x11);
  
  if (keymap_x11->xkb_desc == NULL)
    {
      keymap_x11->xkb_desc = XkbGetMap (xdisplay, XkbKeySymsMask | XkbKeyTypesMask | XkbModifierMapMask | XkbVirtualModsMask, XkbUseCoreKbd);
      if (keymap_x11->xkb_desc == NULL)
        {
	  g_error ("Failed to get keymap");
          return NULL;
        }

      XkbGetNames (xdisplay, XkbGroupNamesMask | XkbVirtualModNamesMask, keymap_x11->xkb_desc);

      update_modmap (xdisplay, keymap_x11);
    }
  else if (keymap_x11->current_serial != display_x11->keymap_serial)
    {
      XkbGetUpdatedMap (xdisplay, XkbKeySymsMask | XkbKeyTypesMask | XkbModifierMapMask | XkbVirtualModsMask,
			keymap_x11->xkb_desc);
      XkbGetNames (xdisplay, XkbGroupNamesMask | XkbVirtualModNamesMask, keymap_x11->xkb_desc);

      update_modmap (xdisplay, keymap_x11);
    }

  keymap_x11->current_serial = display_x11->keymap_serial;

  return keymap_x11->xkb_desc;
}
#endif /* HAVE_XKB */

/* Whether we were able to turn on detectable-autorepeat using
 * XkbSetDetectableAutorepeat. If FALSE, we'll fall back
 * to checking the next event with XPending().
 */

/** 
 * gdk_keymap_get_for_display:
 * @display: the #GdkDisplay.
 * @returns: the #GdkKeymap attached to @display.
 *
 * Returns the #GdkKeymap attached to @display.
 *
 * Since: 2.2
 **/
GdkKeymap*
gdk_keymap_get_for_display (GdkDisplay *display)
{
  GdkDisplayX11 *display_x11;
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  display_x11 = GDK_DISPLAY_X11 (display);
  
  if (!display_x11->keymap)
    display_x11->keymap = g_object_new (gdk_keymap_x11_get_type (), NULL);

  display_x11->keymap->display = display;

  return display_x11->keymap;
}

/* Find the index of the group/level pair within the keysyms for a key.
 * We round up the number of keysyms per keycode to the next even number,
 * otherwise we lose a whole group of keys
 */
#define KEYSYM_INDEX(keymap_impl, group, level) \
  (2 * ((group) % (gint)((keymap_impl->keysyms_per_keycode + 1) / 2)) + (level))
#define KEYSYM_IS_KEYPAD(s) (((s) >= 0xff80 && (s) <= 0xffbd) || \
                             ((s) >= 0x11000000 && (s) <= 0x1100ffff))

static gint
get_symbol (const KeySym *syms,
	    GdkKeymapX11 *keymap_x11,
	    gint group,
	    gint level)
{
  gint index;

  index = KEYSYM_INDEX(keymap_x11, group, level);
  if (index >= keymap_x11->keysyms_per_keycode)
      return NoSymbol;

  return syms[index];
}

static void
set_symbol (KeySym       *syms, 
	    GdkKeymapX11 *keymap_x11, 
	    gint          group, 
	    gint          level, 
	    KeySym        sym)
{
  gint index;

  index = KEYSYM_INDEX(keymap_x11, group, level);
  if (index >= keymap_x11->keysyms_per_keycode)
      return;

  syms[index] = sym;
}

static void
update_keymaps (GdkKeymapX11 *keymap_x11)
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (GDK_KEYMAP (keymap_x11)->display);
  Display *xdisplay = display_x11->xdisplay;
  
#ifdef HAVE_XKB
  g_assert (!KEYMAP_USE_XKB (GDK_KEYMAP (keymap_x11)));
#endif
  
  if (keymap_x11->keymap == NULL ||
      keymap_x11->current_serial != display_x11->keymap_serial)
    {
      gint i;
      gint map_size;
      gint keycode;

      keymap_x11->current_serial = display_x11->keymap_serial;
      
      update_keyrange (keymap_x11);
      
      if (keymap_x11->keymap)
        XFree (keymap_x11->keymap);

      if (keymap_x11->mod_keymap)
        XFreeModifiermap (keymap_x11->mod_keymap);
      
      keymap_x11->keymap = XGetKeyboardMapping (xdisplay, keymap_x11->min_keycode,
						keymap_x11->max_keycode - keymap_x11->min_keycode + 1,
						&keymap_x11->keysyms_per_keycode);


      /* GDK_ISO_Left_Tab, as usually configured through XKB, really messes
       * up the whole idea of "consumed modifiers" because shift is consumed.
       * However, <shift>Tab is not usually GDK_ISO_Left_Tab without XKB,
       * we we fudge the map here.
       */
      keycode = keymap_x11->min_keycode;
      while (keycode <= keymap_x11->max_keycode)
        {
          KeySym *syms = keymap_x11->keymap + (keycode - keymap_x11->min_keycode) * keymap_x11->keysyms_per_keycode;
	  /* Check both groups */
	  for (i = 0 ; i < 2 ; i++)
	    {
	      if (get_symbol (syms, keymap_x11, i, 0) == GDK_Tab)
		set_symbol (syms, keymap_x11, i, 1, GDK_ISO_Left_Tab);
	    }

          /*
           * If there is one keysym and the key symbol has upper and lower
           * case variants fudge the keymap
           */
          if (get_symbol (syms, keymap_x11, 0, 1) == 0)
            {
              guint lower;
              guint upper;

              gdk_keyval_convert_case (get_symbol (syms, keymap_x11, 0, 0), &lower, &upper);
              if (lower != upper)
                {
		  set_symbol (syms, keymap_x11, 0, 0, lower);
		  set_symbol (syms, keymap_x11, 0, 1, upper);
                }
            }
      
          
          ++keycode;
        }

      keymap_x11->mod_keymap = XGetModifierMapping (xdisplay);

      keymap_x11->lock_keysym = GDK_VoidSymbol;
      keymap_x11->group_switch_mask = 0;
      keymap_x11->num_lock_mask = 0;

      for (i = 0; i < 8; i++)
	keymap_x11->modmap[i] = 1 << i;
      
      /* There are 8 sets of modifiers, with each set containing
       * max_keypermod keycodes.
       */
      map_size = 8 * keymap_x11->mod_keymap->max_keypermod;
      for (i = 0; i < map_size; i++)
        {
          /* Get the key code at this point in the map. */
          gint keycode = keymap_x11->mod_keymap->modifiermap[i];
          gint j;
          KeySym *syms;
	  guint mask;

          /* Ignore invalid keycodes. */
          if (keycode < keymap_x11->min_keycode ||
              keycode > keymap_x11->max_keycode)
            continue;

          syms = keymap_x11->keymap + (keycode - keymap_x11->min_keycode) * keymap_x11->keysyms_per_keycode;

	  mask = 0;
	  for (j = 0; j < keymap_x11->keysyms_per_keycode; j++)
	    {
	      if (syms[j] == GDK_Meta_L ||
		  syms[j] == GDK_Meta_R)
		mask |= GDK_META_MASK;
	      else if (syms[j] == GDK_Hyper_L ||
		       syms[j] == GDK_Hyper_R)
		mask |= GDK_HYPER_MASK;
	      else if (syms[j] == GDK_Super_L ||
		       syms[j] == GDK_Super_R)
		mask |= GDK_SUPER_MASK;
	    }

	  keymap_x11->modmap[i/keymap_x11->mod_keymap->max_keypermod] |= mask;

          /* The fourth modifier, GDK_MOD1_MASK is 1 << 3.
	   * Each group of max_keypermod entries refers to the same modifier.
           */
          mask = 1 << (i / keymap_x11->mod_keymap->max_keypermod);

          switch (mask)
            {
            case GDK_LOCK_MASK:
              /* Get the Lock keysym.  If any keysym bound to the Lock modifier
               * is Caps_Lock, we will interpret the modifier as Caps_Lock;
               * otherwise, if any is bound to Shift_Lock, we will interpret
               * the modifier as Shift_Lock. Otherwise, the lock modifier
	       * has no effect.
               */
	      for (j = 0; j < keymap_x11->keysyms_per_keycode; j++)
		{
		  if (syms[j] == GDK_Caps_Lock)
		    keymap_x11->lock_keysym = GDK_Caps_Lock;
		  else if (syms[j] == GDK_Shift_Lock &&
			   keymap_x11->lock_keysym == GDK_VoidSymbol)
		    keymap_x11->lock_keysym = GDK_Shift_Lock;
		}
              break;

            case GDK_CONTROL_MASK:
            case GDK_SHIFT_MASK:
            case GDK_MOD1_MASK:
              /* Some keyboard maps are known to map Mode_Switch as an
               * extra Mod1 key. In circumstances like that, it won't be
               * used to switch groups.
               */
              break;

            default:
              /* Find the Mode_Switch and Num_Lock modifiers. */
              for (j = 0; j < keymap_x11->keysyms_per_keycode; j++)
                {
                  if (syms[j] == GDK_Mode_switch)
                    {
                      /* This modifier swaps groups */
                      keymap_x11->group_switch_mask |= mask;
                    }
                  else if (syms[j] == GDK_Num_Lock)
                    {
                      /* This modifier is used for Num_Lock */
                      keymap_x11->num_lock_mask |= mask;
                    }
                }
              break;
            }
        }

      /* Hack: The Sun X server puts the keysym to use when the Num Lock
       * modifier is on in the third element of the keysym array, instead
       * of the second.
       */
      if ((strcmp (ServerVendor (xdisplay), "Sun Microsystems, Inc.") == 0) &&
          (keymap_x11->keysyms_per_keycode > 2))
        keymap_x11->sun_keypad = TRUE;
      else
        keymap_x11->sun_keypad = FALSE;
    }
}

static const KeySym*
get_keymap (GdkKeymapX11 *keymap_x11)
{
  update_keymaps (keymap_x11);
  
  return keymap_x11->keymap;
}

#define GET_EFFECTIVE_KEYMAP(keymap) get_effective_keymap ((keymap), G_STRFUNC)

static GdkKeymap *
get_effective_keymap (GdkKeymap  *keymap,
		      const char *function)
{
  if (!keymap)
    {
      GDK_NOTE (MULTIHEAD,
		g_message ("reverting to default display keymap in %s",
			   function));
      return gdk_keymap_get_default ();
    }

  return keymap;
}

#if HAVE_XKB
static PangoDirection
get_direction (XkbDescRec *xkb, 
	       gint group)
{
  gint code;

  gint rtl_minus_ltr = 0; /* total number of RTL keysyms minus LTR ones */

  for (code = xkb->min_key_code; code <= xkb->max_key_code; code++)
    {
      gint level = 0;
      KeySym sym = XkbKeySymEntry (xkb, code, level, group);
      PangoDirection dir = pango_unichar_direction (gdk_keyval_to_unicode (sym));

      switch (dir)
	{
	case PANGO_DIRECTION_RTL:
	  rtl_minus_ltr++;
	  break;
	case PANGO_DIRECTION_LTR:
	  rtl_minus_ltr--;
	  break;
	default:
	  break;
	}
    }

  if (rtl_minus_ltr > 0)
    return PANGO_DIRECTION_RTL;
  else
    return PANGO_DIRECTION_LTR;
}

static PangoDirection
get_direction_from_cache (GdkKeymapX11 *keymap_x11,
			  XkbDescPtr xkb,
			  gint group)
{
  Atom group_atom = xkb->names->groups[group];

  gboolean cache_hit = FALSE;
  DirectionCacheEntry *cache = keymap_x11->group_direction_cache;

  PangoDirection direction = PANGO_DIRECTION_NEUTRAL;
  gint i;

  if (keymap_x11->have_direction)
    {
      /* lookup in cache */
      for (i = 0; i < G_N_ELEMENTS (keymap_x11->group_direction_cache); i++)
      {
	if (cache[i].group_atom == group_atom)
	  {
	    cache_hit = TRUE;
	    cache[i].serial = keymap_x11->current_cache_serial++; /* freshen */
	    direction = cache[i].direction;
	    group_atom = cache[i].group_atom;
	    break;
	  }
      }
    }
  else
    {
      /* initialize cache */
      for (i = 0; i < G_N_ELEMENTS (keymap_x11->group_direction_cache); i++)
	{
	  cache[i].group_atom = 0;
	  cache[i].serial = keymap_x11->current_cache_serial;
	}
      keymap_x11->current_cache_serial++;
    }

  /* insert in cache */
  if (!cache_hit)
    {
      gint oldest = 0;

      direction = get_direction (xkb, group);

      /* remove the oldest entry */
      for (i = 0; i < G_N_ELEMENTS (keymap_x11->group_direction_cache); i++)
	{
	  if (cache[i].serial < cache[oldest].serial)
	    oldest = i;
	}
      
      cache[oldest].group_atom = group_atom;
      cache[oldest].direction = direction;
      cache[oldest].serial = keymap_x11->current_cache_serial++;
    }

  return direction;
}

static int
get_num_groups (GdkKeymap *keymap,
		XkbDescPtr xkb)
{
      Display *display = KEYMAP_XDISPLAY (keymap);
      XkbGetControls(display, XkbSlowKeysMask, xkb);
      XkbGetUpdatedMap (display, XkbKeySymsMask | XkbKeyTypesMask |
			XkbModifierMapMask | XkbVirtualModsMask, xkb);
      return xkb->ctrls->num_groups;
}

static gboolean
update_direction (GdkKeymapX11 *keymap_x11,
		  gint          group)
{
  XkbDescPtr xkb = get_xkb (keymap_x11);
  Atom group_atom;
  gboolean had_direction;
  PangoDirection old_direction;
      
  had_direction = keymap_x11->have_direction;
  old_direction = keymap_x11->current_direction;

  group_atom = xkb->names->groups[group];

  /* a group change? */
  if (!keymap_x11->have_direction || keymap_x11->current_group_atom != group_atom)
    {
      keymap_x11->current_direction = get_direction_from_cache (keymap_x11, xkb, group);
      keymap_x11->current_group_atom = group_atom;
      keymap_x11->have_direction = TRUE;
    }

  return !had_direction || old_direction != keymap_x11->current_direction;
}

static gboolean
update_lock_state (GdkKeymapX11 *keymap_x11,
                   gint          locked_mods)
{
  gboolean caps_lock_state;
  
  caps_lock_state = keymap_x11->caps_lock_state;

  keymap_x11->caps_lock_state = (locked_mods & GDK_LOCK_MASK) != 0;
  
  return caps_lock_state != keymap_x11->caps_lock_state;
}

/* keep this in sync with the XkbSelectEventDetails() call 
 * in gdk_display_open()
 */
void
_gdk_keymap_state_changed (GdkDisplay *display,
			   XEvent     *xevent)
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);
  XkbEvent *xkb_event = (XkbEvent *)xevent;
  
  if (display_x11->keymap)
    {
      GdkKeymapX11 *keymap_x11 = GDK_KEYMAP_X11 (display_x11->keymap);
      
      if (update_direction (keymap_x11, XkbStateGroup (&xkb_event->state)))
	g_signal_emit_by_name (keymap_x11, "direction-changed");      

      if (update_lock_state (keymap_x11, xkb_event->state.locked_mods))
	g_signal_emit_by_name (keymap_x11, "state-changed");      
    }
}

#endif /* HAVE_XKB */

void
_gdk_keymap_keys_changed (GdkDisplay *display)
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);
  
  ++display_x11->keymap_serial;
  
  if (display_x11->keymap)
    g_signal_emit_by_name (display_x11->keymap, "keys_changed", 0);
}

/** 
 * gdk_keymap_get_direction:
 * @keymap: a #GdkKeymap or %NULL to use the default keymap
 *
 * Returns the direction of effective layout of the keymap.
 *
 * Note that passing %NULL for @keymap is deprecated and will stop
 * to work in GTK+ 3.0. Use gdk_keymap_get_for_display() instead.
 *
 * Returns: %PANGO_DIRECTION_LTR or %PANGO_DIRECTION_RTL
 *   if it can determine the direction. %PANGO_DIRECTION_NEUTRAL
 *   otherwise.
 **/
PangoDirection
gdk_keymap_get_direction (GdkKeymap *keymap)
{
  keymap = GET_EFFECTIVE_KEYMAP (keymap);
  
#if HAVE_XKB
  if (KEYMAP_USE_XKB (keymap))
    {
      GdkKeymapX11 *keymap_x11 = GDK_KEYMAP_X11 (keymap);

      if (!keymap_x11->have_direction)
	{
	  GdkDisplay *display = GDK_KEYMAP (keymap_x11)->display;
	  XkbStateRec state_rec;

	  XkbGetState (GDK_DISPLAY_XDISPLAY (display), XkbUseCoreKbd, 
		       &state_rec);
	  update_direction (keymap_x11, XkbStateGroup (&state_rec));
	}
  
      return keymap_x11->current_direction;
    }
  else
#endif /* HAVE_XKB */
    return PANGO_DIRECTION_NEUTRAL;
}

/** 
 * gdk_keymap_have_bidi_layouts:
 * @keymap: a #GdkKeymap or %NULL to use the default keymap
 *
 * Determines if keyboard layouts for both right-to-left and left-to-right
 * languages are in use.
 *
 * Note that passing %NULL for @keymap is deprecated and will stop
 * to work in GTK+ 3.0. Use gdk_keymap_get_for_display() instead.
 *
 * Returns: %TRUE if there are layouts in both directions, %FALSE otherwise
 *
 * Since: 2.12
 **/
gboolean
gdk_keymap_have_bidi_layouts (GdkKeymap *keymap)
{
  keymap = GET_EFFECTIVE_KEYMAP (keymap);

#if HAVE_XKB
  if (KEYMAP_USE_XKB (keymap))
    {
      GdkKeymapX11 *keymap_x11 = GDK_KEYMAP_X11 (keymap);
      XkbDescPtr xkb = get_xkb (keymap_x11);
      int num_groups = get_num_groups (keymap, xkb);

      int i;
      gboolean have_ltr_keyboard = FALSE;
      gboolean have_rtl_keyboard = FALSE;

      for (i = 0; i < num_groups; i++)
      {
	if (get_direction_from_cache (keymap_x11, xkb, i) == PANGO_DIRECTION_RTL)
	  have_rtl_keyboard = TRUE;
	else
	  have_ltr_keyboard = TRUE;
      }

      return have_ltr_keyboard && have_rtl_keyboard;
    }
  else
#endif /* HAVE_XKB */
    return FALSE;
}

/**
 * gdk_keymap_get_caps_lock_state:
 * @keymap: a #GdkKeymap
 *
 * Returns whether the Caps Lock modifer is locked. 
 *
 * Returns: %TRUE if Caps Lock is on
 *
 * Since: 2.16
 */
gboolean
gdk_keymap_get_caps_lock_state (GdkKeymap *keymap)
{
  GdkKeymapX11 *keymap_x11;
  
  keymap = GET_EFFECTIVE_KEYMAP (keymap);
  
  keymap_x11 = GDK_KEYMAP_X11 (keymap);
  
  return keymap_x11->caps_lock_state;
}


/**
 * gdk_keymap_get_entries_for_keyval:
 * @keymap: (allow-none): a #GdkKeymap, or %NULL to use the default keymap
 * @keyval: a keyval, such as %GDK_a, %GDK_Up, %GDK_Return, etc.
 * @keys: (out): return location for an array of #GdkKeymapKey
 * @n_keys: (out): return location for number of elements in returned array
 *
 * Obtains a list of keycode/group/level combinations that will
 * generate @keyval. Groups and levels are two kinds of keyboard mode;
 * in general, the level determines whether the top or bottom symbol
 * on a key is used, and the group determines whether the left or
 * right symbol is used. On US keyboards, the shift key changes the
 * keyboard level, and there are no groups. A group switch key might
 * convert a keyboard between Hebrew to English modes, for example.
 * #GdkEventKey contains a %group field that indicates the active
 * keyboard group. The level is computed from the modifier mask.
 * The returned array should be freed
 * with g_free().
 *
 * Note that passing %NULL for @keymap is deprecated and will stop
 * to work in GTK+ 3.0. Use gdk_keymap_get_for_display() instead.
 *
 * Return value: %TRUE if keys were found and returned
 **/
gboolean
gdk_keymap_get_entries_for_keyval (GdkKeymap     *keymap,
                                   guint          keyval,
                                   GdkKeymapKey **keys,
                                   gint          *n_keys)
{
  GArray *retval;
  GdkKeymapX11 *keymap_x11;

  g_return_val_if_fail (keymap == NULL || GDK_IS_KEYMAP (keymap), FALSE);
  g_return_val_if_fail (keys != NULL, FALSE);
  g_return_val_if_fail (n_keys != NULL, FALSE);
  g_return_val_if_fail (keyval != 0, FALSE);

  keymap = GET_EFFECTIVE_KEYMAP (keymap);
  keymap_x11 = GDK_KEYMAP_X11 (keymap);
  
  retval = g_array_new (FALSE, FALSE, sizeof (GdkKeymapKey));

#ifdef HAVE_XKB
  if (KEYMAP_USE_XKB (keymap))
    {
      /* See sec 15.3.4 in XKB docs */

      XkbDescRec *xkb = get_xkb (keymap_x11);
      gint keycode;
      
      keycode = keymap_x11->min_keycode;

      while (keycode <= keymap_x11->max_keycode)
        {
          gint max_shift_levels = XkbKeyGroupsWidth (xkb, keycode); /* "key width" */
          gint group = 0;
          gint level = 0;
          gint total_syms = XkbKeyNumSyms (xkb, keycode);
          gint i = 0;
          KeySym *entry;

          /* entry is an array with all syms for group 0, all
           * syms for group 1, etc. and for each group the
           * shift level syms are in order
           */
          entry = XkbKeySymsPtr (xkb, keycode);

          while (i < total_syms)
            {
              /* check out our cool loop invariant */
              g_assert (i == (group * max_shift_levels + level));

              if (entry[i] == keyval)
                {
                  /* Found a match */
                  GdkKeymapKey key;

                  key.keycode = keycode;
                  key.group = group;
                  key.level = level;

                  g_array_append_val (retval, key);

                  g_assert (XkbKeySymEntry (xkb, keycode, level, group) == 
			    keyval);
                }

              ++level;

              if (level == max_shift_levels)
                {
                  level = 0;
                  ++group;
                }

              ++i;
            }

          ++keycode;
        }
    }
  else
#endif
    {
      const KeySym *map = get_keymap (keymap_x11);
      gint keycode;
      
      keycode = keymap_x11->min_keycode;
      while (keycode <= keymap_x11->max_keycode)
        {
          const KeySym *syms = map + (keycode - keymap_x11->min_keycode) * keymap_x11->keysyms_per_keycode;
          gint i = 0;

          while (i < keymap_x11->keysyms_per_keycode)
            {
              if (syms[i] == keyval)
                {
                  /* found a match */
                  GdkKeymapKey key;

                  key.keycode = keycode;

                  /* The "classic" non-XKB keymap has 2 levels per group */
                  key.group = i / 2;
                  key.level = i % 2;

                  g_array_append_val (retval, key);
                }
              
              ++i;
            }
          
          ++keycode;
        }
    }

  if (retval->len > 0)
    {
      *keys = (GdkKeymapKey*) retval->data;
      *n_keys = retval->len;
    }
  else
    {
      *keys = NULL;
      *n_keys = 0;
    }
      
  g_array_free (retval, retval->len > 0 ? FALSE : TRUE);

  return *n_keys > 0;
}

/**
 * gdk_keymap_get_entries_for_keycode:
 * @keymap: (allow-none): a #GdkKeymap or %NULL to use the default keymap
 * @hardware_keycode: a keycode
 * @keys: (out): return location for array of #GdkKeymapKey, or %NULL
 * @keyvals: (out): return location for array of keyvals, or %NULL
 * @n_entries: length of @keys and @keyvals
 *
 * Returns the keyvals bound to @hardware_keycode.
 * The Nth #GdkKeymapKey in @keys is bound to the Nth
 * keyval in @keyvals. Free the returned arrays with g_free().
 * When a keycode is pressed by the user, the keyval from
 * this list of entries is selected by considering the effective
 * keyboard group and level. See gdk_keymap_translate_keyboard_state().
 *
 * Note that passing %NULL for @keymap is deprecated and will stop
 * to work in GTK+ 3.0. Use gdk_keymap_get_for_display() instead.
 *
 * Returns: %TRUE if there were any entries
 **/
gboolean
gdk_keymap_get_entries_for_keycode (GdkKeymap     *keymap,
                                    guint          hardware_keycode,
                                    GdkKeymapKey **keys,
                                    guint        **keyvals,
                                    gint          *n_entries)
{
  GdkKeymapX11 *keymap_x11;
  
  GArray *key_array;
  GArray *keyval_array;

  g_return_val_if_fail (keymap == NULL || GDK_IS_KEYMAP (keymap), FALSE);
  g_return_val_if_fail (n_entries != NULL, FALSE);

  keymap = GET_EFFECTIVE_KEYMAP (keymap);
  keymap_x11 = GDK_KEYMAP_X11 (keymap);

  update_keyrange (keymap_x11);

  if (hardware_keycode < keymap_x11->min_keycode ||
      hardware_keycode > keymap_x11->max_keycode)
    {
      if (keys)
        *keys = NULL;
      if (keyvals)
        *keyvals = NULL;

      *n_entries = 0;
      return FALSE;
    }
  
  if (keys)
    key_array = g_array_new (FALSE, FALSE, sizeof (GdkKeymapKey));
  else
    key_array = NULL;
  
  if (keyvals)
    keyval_array = g_array_new (FALSE, FALSE, sizeof (guint));
  else
    keyval_array = NULL;
  
#ifdef HAVE_XKB
  if (KEYMAP_USE_XKB (keymap))
    {
      /* See sec 15.3.4 in XKB docs */

      XkbDescRec *xkb = get_xkb (keymap_x11);
      gint max_shift_levels;
      gint group = 0;
      gint level = 0;
      gint total_syms;
      gint i = 0;
      KeySym *entry;
      
      max_shift_levels = XkbKeyGroupsWidth (xkb, hardware_keycode); /* "key width" */
      total_syms = XkbKeyNumSyms (xkb, hardware_keycode);

      /* entry is an array with all syms for group 0, all
       * syms for group 1, etc. and for each group the
       * shift level syms are in order
       */
      entry = XkbKeySymsPtr (xkb, hardware_keycode);

      while (i < total_syms)
        {          
          /* check out our cool loop invariant */          
          g_assert (i == (group * max_shift_levels + level));

          if (key_array)
            {
              GdkKeymapKey key;
              
              key.keycode = hardware_keycode;
              key.group = group;
              key.level = level;
              
              g_array_append_val (key_array, key);
            }

          if (keyval_array)
            g_array_append_val (keyval_array, entry[i]);
          
          ++level;
          
          if (level == max_shift_levels)
            {
              level = 0;
              ++group;
            }
          
          ++i;
        }
    }
  else
#endif
    {
      const KeySym *map = get_keymap (keymap_x11);
      const KeySym *syms;
      gint i = 0;

      syms = map + (hardware_keycode - keymap_x11->min_keycode) * keymap_x11->keysyms_per_keycode;

      while (i < keymap_x11->keysyms_per_keycode)
        {
          if (key_array)
            {
              GdkKeymapKey key;
          
              key.keycode = hardware_keycode;
              
              /* The "classic" non-XKB keymap has 2 levels per group */
              key.group = i / 2;
              key.level = i % 2;
              
              g_array_append_val (key_array, key);
            }

          if (keyval_array)
            g_array_append_val (keyval_array, syms[i]);
          
          ++i;
        }
    }

  *n_entries = 0;

  if (keys)
    {
      *n_entries = key_array->len;
      *keys = (GdkKeymapKey*) g_array_free (key_array, FALSE);
    }
  
  if (keyvals)
    {
      *n_entries = keyval_array->len;
      *keyvals = (guint*) g_array_free (keyval_array, FALSE);
    }

  return *n_entries > 0;
}


/**
 * gdk_keymap_lookup_key:
 * @keymap: a #GdkKeymap or %NULL to use the default keymap
 * @key: a #GdkKeymapKey with keycode, group, and level initialized
 * 
 * Looks up the keyval mapped to a keycode/group/level triplet.
 * If no keyval is bound to @key, returns 0. For normal user input,
 * you want to use gdk_keymap_translate_keyboard_state() instead of
 * this function, since the effective group/level may not be
 * the same as the current keyboard state.
 * 
 * Note that passing %NULL for @keymap is deprecated and will stop
 * to work in GTK+ 3.0. Use gdk_keymap_get_for_display() instead.
 *
 * Return value: a keyval, or 0 if none was mapped to the given @key
 **/
guint
gdk_keymap_lookup_key (GdkKeymap          *keymap,
                       const GdkKeymapKey *key)
{
  GdkKeymapX11 *keymap_x11;
  
  g_return_val_if_fail (keymap == NULL || GDK_IS_KEYMAP (keymap), 0);
  g_return_val_if_fail (key != NULL, 0);
  g_return_val_if_fail (key->group < 4, 0);

  keymap = GET_EFFECTIVE_KEYMAP (keymap);
  keymap_x11 = GDK_KEYMAP_X11 (keymap);
  
#ifdef HAVE_XKB
  if (KEYMAP_USE_XKB (keymap))
    {
      XkbDescRec *xkb = get_xkb (keymap_x11);
      
      return XkbKeySymEntry (xkb, key->keycode, key->level, key->group);
    }
  else
#endif
    {
      const KeySym *map = get_keymap (keymap_x11);
      const KeySym *syms = map + (key->keycode - keymap_x11->min_keycode) * keymap_x11->keysyms_per_keycode;
      return get_symbol (syms, keymap_x11, key->group, key->level);
    }
}

#ifdef HAVE_XKB
/* This is copied straight from XFree86 Xlib, to:
 *  - add the group and level return.
 *  - change the interpretation of mods_rtrn as described
 *    in the docs for gdk_keymap_translate_keyboard_state()
 * It's unchanged for ease of diff against the Xlib sources; don't
 * reformat it.
 */
static Bool
MyEnhancedXkbTranslateKeyCode(register XkbDescPtr     xkb,
                              KeyCode                 key,
                              register unsigned int   mods,
                              unsigned int *          mods_rtrn,
                              KeySym *                keysym_rtrn,
                              int *                   group_rtrn,
                              int *                   level_rtrn)
{
    XkbKeyTypeRec *type;
    int col,nKeyGroups;
    unsigned preserve,effectiveGroup;
    KeySym *syms;

    if (mods_rtrn!=NULL)
        *mods_rtrn = 0;

    nKeyGroups= XkbKeyNumGroups(xkb,key);
    if ((!XkbKeycodeInRange(xkb,key))||(nKeyGroups==0)) {
        if (keysym_rtrn!=NULL)
            *keysym_rtrn = NoSymbol;
        return False;
    }

    syms = XkbKeySymsPtr(xkb,key);

    /* find the offset of the effective group */
    col = 0;
    effectiveGroup= XkbGroupForCoreState(mods);
    if ( effectiveGroup>=nKeyGroups ) {
        unsigned groupInfo= XkbKeyGroupInfo(xkb,key);
        switch (XkbOutOfRangeGroupAction(groupInfo)) {
            default:
                effectiveGroup %= nKeyGroups;
                break;
            case XkbClampIntoRange:
                effectiveGroup = nKeyGroups-1;
                break;
            case XkbRedirectIntoRange:
                effectiveGroup = XkbOutOfRangeGroupNumber(groupInfo);
                if (effectiveGroup>=nKeyGroups)
                    effectiveGroup= 0;
                break;
        }
    }
    col= effectiveGroup*XkbKeyGroupsWidth(xkb,key);
    type = XkbKeyKeyType(xkb,key,effectiveGroup);

    preserve= 0;
    if (type->map) { /* find the column (shift level) within the group */
        register int i;
        register XkbKTMapEntryPtr entry;
	/* ---- Begin section modified for GDK  ---- */
	int found = 0;
	
        for (i=0,entry=type->map;i<type->map_count;i++,entry++) {
            if (!entry->active || syms[col+entry->level] == syms[col])
              continue;
	    if (mods_rtrn) {
		int bits = 0;
		unsigned long tmp = entry->mods.mask;
		while (tmp) {
		    if ((tmp & 1) == 1)
			bits++;
		    tmp >>= 1;
		}
                /* We always add one-modifiers levels to mods_rtrn since
                 * they can't wipe out bits in the state unless the
                 * level would be triggered. But not if they don't change
                 * the symbol (otherwise we can't discriminate Shift-F10
                 * and F10 anymore). And don't add modifiers that are
                 * explicitly marked as preserved, either.
                 */
		if (bits == 1 || (mods&type->mods.mask)==entry->mods.mask)
                  {
                    if (type->preserve)
                      *mods_rtrn |= (entry->mods.mask & ~type->preserve[i].mask);
                    else
                      *mods_rtrn |= entry->mods.mask;
                  }
	    }

            if (!found && ((mods&type->mods.mask) == entry->mods.mask)) {
                col+= entry->level;
                if (type->preserve)
                    preserve= type->preserve[i].mask;

                if (level_rtrn)
                  *level_rtrn = entry->level;

                found = 1;
            }
        }
	/* ---- End section modified for GDK ---- */
    }

    if (keysym_rtrn!=NULL)
        *keysym_rtrn= syms[col];
    if (mods_rtrn) {
	/* ---- Begin section modified for GDK  ---- */
        *mods_rtrn &= ~preserve;
	/* ---- End section modified for GDK ---- */
	
        /* ---- Begin stuff GDK comments out of the original Xlib version ---- */
        /* This is commented out because xkb_info is a private struct */

#if 0
        /* The Motif VTS doesn't get the help callback called if help
         * is bound to Shift+<whatever>, and it appears as though it 
         * is XkbTranslateKeyCode that is causing the problem.  The 
         * core X version of XTranslateKey always OR's in ShiftMask 
         * and LockMask for mods_rtrn, so this "fix" keeps this behavior 
         * and solves the VTS problem.
         */
        if ((xkb->dpy)&&(xkb->dpy->xkb_info)&&
            (xkb->dpy->xkb_info->xlib_ctrls&XkbLC_AlwaysConsumeShiftAndLock)) {            *mods_rtrn|= (ShiftMask|LockMask);
        }
#endif
        
        /* ---- End stuff GDK comments out of the original Xlib version ---- */
    }

    /* ---- Begin stuff GDK adds to the original Xlib version ---- */

    if (group_rtrn)
      *group_rtrn = effectiveGroup;
    
    /* ---- End stuff GDK adds to the original Xlib version ---- */
    
    return (syms[col] != NoSymbol);
}
#endif /* HAVE_XKB */

/* Translates from keycode/state to keysymbol using the traditional interpretation
 * of the keyboard map. See section 12.7 of the Xlib reference manual
 */
static guint
translate_keysym (GdkKeymapX11   *keymap_x11,
		  guint           hardware_keycode,
		  gint            group,
		  GdkModifierType state,
		  gint           *effective_group,
		  gint           *effective_level)
{
  const KeySym *map = get_keymap (keymap_x11);
  const KeySym *syms = map + (hardware_keycode - keymap_x11->min_keycode) * keymap_x11->keysyms_per_keycode;

#define SYM(k,g,l) get_symbol (syms, k,g,l)

  GdkModifierType shift_modifiers;
  gint shift_level;
  guint tmp_keyval;
  gint num_lock_index;

  shift_modifiers = GDK_SHIFT_MASK;
  if (keymap_x11->lock_keysym == GDK_Shift_Lock)
    shift_modifiers |= GDK_LOCK_MASK;

  /* Fall back to the first group if the passed in group is empty
   */
  if (!(SYM (keymap_x11, group, 0) || SYM (keymap_x11, group, 1)) &&
      (SYM (keymap_x11, 0, 0) || SYM (keymap_x11, 0, 1)))
    group = 0;

  /* Hack: On Sun, the Num Lock modifier uses the third element in the
   * keysym array, and Mode_Switch does not apply for a keypad key.
   */
  if (keymap_x11->sun_keypad)
    {
      num_lock_index = 2;
      
      if (group != 0)
	{
	  gint i;
	  
	  for (i = 0; i < keymap_x11->keysyms_per_keycode; i++)
	    if (KEYSYM_IS_KEYPAD (SYM (keymap_x11, 0, i)))
	      group = 0;
	}
    }
  else
    num_lock_index = 1;

  if ((state & keymap_x11->num_lock_mask) &&
      KEYSYM_IS_KEYPAD (SYM (keymap_x11, group, num_lock_index)))
    {
      /* Shift, Shift_Lock cancel Num_Lock
       */
      shift_level = (state & shift_modifiers) ? 0 : num_lock_index;
      if (!SYM (keymap_x11, group, shift_level) && SYM (keymap_x11, group, 0))
        shift_level = 0;

       tmp_keyval = SYM (keymap_x11, group, shift_level);
    }
  else
    {
      /* Fall back to the first level if no symbol for the level
       * we were passed.
       */
      shift_level = (state & shift_modifiers) ? 1 : 0;
      if (!SYM (keymap_x11, group, shift_level) && SYM (keymap_x11, group, 0))
	shift_level = 0;
  
      tmp_keyval = SYM (keymap_x11, group, shift_level);
      
      if (keymap_x11->lock_keysym == GDK_Caps_Lock && (state & GDK_LOCK_MASK) != 0)
	{
	  guint upper = gdk_keyval_to_upper (tmp_keyval);
	  if (upper != tmp_keyval)
	    tmp_keyval = upper;
	}
    }

  if (effective_group)
    *effective_group = group;
      
  if (effective_level)
    *effective_level = shift_level;

  return tmp_keyval;
  
#undef SYM
}

/**
 * gdk_keymap_translate_keyboard_state:
 * @keymap: (allow-none): a #GdkKeymap, or %NULL to use the default
 * @hardware_keycode: a keycode
 * @state: a modifier state
 * @group: active keyboard group
 * @keyval: (out) (allow-none): return location for keyval, or %NULL
 * @effective_group: (out) (allow-none): return location for effective group, or %NULL
 * @level: (out) (allow-none):  return location for level, or %NULL
 * @consumed_modifiers: (out) (allow-none):  return location for modifiers that were used to
 *     determine the group or level, or %NULL
 *
 * Translates the contents of a #GdkEventKey into a keyval, effective
 * group, and level. Modifiers that affected the translation and
 * are thus unavailable for application use are returned in
 * @consumed_modifiers.  See <xref linkend="key-group-explanation"/> for an explanation of
 * groups and levels.  The @effective_group is the group that was
 * actually used for the translation; some keys such as Enter are not
 * affected by the active keyboard group. The @level is derived from
 * @state. For convenience, #GdkEventKey already contains the translated
 * keyval, so this function isn't as useful as you might think.
 *
 * <note><para>
 * @consumed_modifiers gives modifiers that should be masked out
 * from @state when comparing this key press to a hot key. For
 * instance, on a US keyboard, the <literal>plus</literal>
 * symbol is shifted, so when comparing a key press to a
 * <literal>&lt;Control&gt;plus</literal> accelerator &lt;Shift&gt; should
 * be masked out.
 * </para>
 * <informalexample><programlisting>
 * &sol;* We want to ignore irrelevant modifiers like ScrollLock *&sol;
 * &num;define ALL_ACCELS_MASK (GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_MOD1_MASK)
 * gdk_keymap_translate_keyboard_state (keymap, event->hardware_keycode,
 *                                      event->state, event->group,
 *                                      &amp;keyval, NULL, NULL, &amp;consumed);
 * if (keyval == GDK_PLUS &&
 *     (event->state &amp; ~consumed &amp; ALL_ACCELS_MASK) == GDK_CONTROL_MASK)
 *   &sol;* Control was pressed *&sol;
 * </programlisting></informalexample>
 * <para>
 * An older interpretation @consumed_modifiers was that it contained
 * all modifiers that might affect the translation of the key;
 * this allowed accelerators to be stored with irrelevant consumed
 * modifiers, by doing:</para>
 * <informalexample><programlisting>
 * &sol;* XXX Don't do this XXX *&sol;
 * if (keyval == accel_keyval &&
 *     (event->state &amp; ~consumed &amp; ALL_ACCELS_MASK) == (accel_mods &amp; ~consumed))
 *   &sol;* Accelerator was pressed *&sol;
 * </programlisting></informalexample>
 * <para>
 * However, this did not work if multi-modifier combinations were
 * used in the keymap, since, for instance, <literal>&lt;Control&gt;</literal>
 * would be masked out even if only <literal>&lt;Control&gt;&lt;Alt&gt;</literal>
 * was used in the keymap. To support this usage as well as well as
 * possible, all <emphasis>single modifier</emphasis> combinations
 * that could affect the key for any combination of modifiers will
 * be returned in @consumed_modifiers; multi-modifier combinations
 * are returned only when actually found in @state. When you store
 * accelerators, you should always store them with consumed modifiers
 * removed. Store <literal>&lt;Control&gt;plus</literal>,
 * not <literal>&lt;Control&gt;&lt;Shift&gt;plus</literal>,
 * </para></note>
 * 
 * Note that passing %NULL for @keymap is deprecated and will stop
 * to work in GTK+ 3.0. Use gdk_keymap_get_for_display() instead.
 *
 * Return value: %TRUE if there was a keyval bound to the keycode/state/group
 **/
gboolean
gdk_keymap_translate_keyboard_state (GdkKeymap       *keymap,
                                     guint            hardware_keycode,
                                     GdkModifierType  state,
                                     gint             group,
                                     guint           *keyval,
                                     gint            *effective_group,
                                     gint            *level,
                                     GdkModifierType *consumed_modifiers)
{
  GdkKeymapX11 *keymap_x11;
  KeySym tmp_keyval = NoSymbol;
  guint tmp_modifiers;

  g_return_val_if_fail (keymap == NULL || GDK_IS_KEYMAP (keymap), FALSE);
  g_return_val_if_fail (group < 4, FALSE);

  keymap = GET_EFFECTIVE_KEYMAP (keymap);  
  keymap_x11 = GDK_KEYMAP_X11 (keymap);

  if (keyval)
    *keyval = NoSymbol;
  if (effective_group)
    *effective_group = 0;
  if (level)
    *level = 0;
  if (consumed_modifiers)
    *consumed_modifiers = 0;

  update_keyrange (keymap_x11);
  
  if (hardware_keycode < keymap_x11->min_keycode ||
      hardware_keycode > keymap_x11->max_keycode)
    return FALSE;

#ifdef HAVE_XKB
  if (KEYMAP_USE_XKB (keymap))
    {
      XkbDescRec *xkb = get_xkb (keymap_x11);

      /* replace bits 13 and 14 with the provided group */
      state &= ~(1 << 13 | 1 << 14);
      state |= group << 13;
      
      MyEnhancedXkbTranslateKeyCode (xkb,
                                     hardware_keycode,
                                     state,
                                     &tmp_modifiers,
                                     &tmp_keyval,
                                     effective_group,
                                     level);

      if (state & ~tmp_modifiers & LockMask)
	tmp_keyval = gdk_keyval_to_upper (tmp_keyval);

      /* We need to augment the consumed modifiers with LockMask, since
       * we handle that ourselves, and also with the group bits
       */
      tmp_modifiers |= LockMask | 1 << 13 | 1 << 14;
    }
  else
#endif
    {
      GdkModifierType bit;
      
      tmp_modifiers = 0;

      /* We see what modifiers matter by trying the translation with
       * and without each possible modifier
       */
      for (bit = GDK_SHIFT_MASK; bit < GDK_BUTTON1_MASK; bit <<= 1)
	{
	  /* Handling of the group here is a bit funky; a traditional
	   * X keyboard map can have more than two groups, but no way
	   * of accessing the extra groups is defined. We allow a
	   * caller to pass in any group to this function, but we 
	   * only can represent switching between group 0 and 1 in
	   * consumed modifiers.
	   */
	  if (translate_keysym (keymap_x11, hardware_keycode,
				(bit == keymap_x11->group_switch_mask) ? 0 : group,
				state & ~bit,
				NULL, NULL) !=
	      translate_keysym (keymap_x11, hardware_keycode,
				(bit == keymap_x11->group_switch_mask) ? 1 : group,
				state | bit,
				NULL, NULL))
	    tmp_modifiers |= bit;
	}
      
      tmp_keyval = translate_keysym (keymap_x11, hardware_keycode,
				     group, state,
				     level, effective_group);
    }

  if (consumed_modifiers)
    *consumed_modifiers = tmp_modifiers;
				
  if (keyval)
    *keyval = tmp_keyval;

  return tmp_keyval != NoSymbol;
}


/* Key handling not part of the keymap */

gchar*
gdk_keyval_name (guint	      keyval)
{
  switch (keyval)
    {
    case GDK_Page_Up:
      return "Page_Up";
    case GDK_Page_Down:
      return "Page_Down";
    case GDK_KP_Page_Up:
      return "KP_Page_Up";
    case GDK_KP_Page_Down:
      return "KP_Page_Down";
    }
  
  return XKeysymToString (keyval);
}

guint
gdk_keyval_from_name (const gchar *keyval_name)
{
  g_return_val_if_fail (keyval_name != NULL, 0);
  
  return XStringToKeysym (keyval_name);
}

#ifdef HAVE_XCONVERTCASE
void
gdk_keyval_convert_case (guint symbol,
			 guint *lower,
			 guint *upper)
{
  KeySym xlower = 0;
  KeySym xupper = 0;

  /* Check for directly encoded 24-bit UCS characters: */
  if ((symbol & 0xff000000) == 0x01000000)
    {
      if (lower)
	*lower = gdk_unicode_to_keyval (g_unichar_tolower (symbol & 0x00ffffff));
      if (upper)
	*upper = gdk_unicode_to_keyval (g_unichar_toupper (symbol & 0x00ffffff));
      return;
    }
  
  if (symbol)
    XConvertCase (symbol, &xlower, &xupper);

  if (lower)
    *lower = xlower;
  if (upper)
    *upper = xupper;
}  
#endif /* HAVE_XCONVERTCASE */

gint
_gdk_x11_get_group_for_state (GdkDisplay      *display,
			      GdkModifierType  state)
{
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);
  
#ifdef HAVE_XKB
  if (display_x11->use_xkb)
    {
      return XkbGroupForCoreState (state);
    }
  else
#endif
    {
      GdkKeymapX11 *keymap_impl = GDK_KEYMAP_X11 (gdk_keymap_get_for_display (display));
      update_keymaps (keymap_impl);
      return (state & keymap_impl->group_switch_mask) ? 1 : 0;
    }
}

void
_gdk_keymap_add_virtual_modifiers_compat (GdkKeymap       *keymap,
				          GdkModifierType *modifiers)
{
  GdkKeymapX11 *keymap_x11;
  int i;
  
  keymap = GET_EFFECTIVE_KEYMAP (keymap);
  keymap_x11 = GDK_KEYMAP_X11 (keymap);

  /* See comment in add_virtual_modifiers() */
  for (i = 4; i < 8; i++)
    {
      if ((1 << i) & *modifiers)
        {
	  if (keymap_x11->modmap[i] & GDK_SUPER_MASK)
	    *modifiers |= GDK_SUPER_MASK;
	  else if (keymap_x11->modmap[i] & GDK_HYPER_MASK)
	    *modifiers |= GDK_HYPER_MASK;
	  else if (keymap_x11->modmap[i] & GDK_META_MASK)
	    *modifiers |= GDK_META_MASK;
        }
    }
}

/**
 * gdk_keymap_add_virtual_modifiers:
 * @keymap: a #GdkKeymap
 * @state: pointer to the modifier mask to change
 *
 * Adds virtual modifiers (i.e. Super, Hyper and Meta) which correspond
 * to the real modifiers (i.e Mod2, Mod3, ...) in @modifiers.
 * are set in @state to their non-virtual counterparts (i.e. Mod2,
 * Mod3,...) and set the corresponding bits in @state.
 *
 * GDK already does this before delivering key events, but for
 * compatibility reasons, it only sets the first virtual modifier
 * it finds, whereas this function sets all matching virtual modifiers.
 *
 * This function is useful when matching key events against
 * accelerators.
 *
 * Since: 2.20
 */
void
gdk_keymap_add_virtual_modifiers (GdkKeymap       *keymap,
			          GdkModifierType *state)
{
  GdkKeymapX11 *keymap_x11;
  int i;

  keymap = GET_EFFECTIVE_KEYMAP (keymap);
  keymap_x11 = GDK_KEYMAP_X11 (keymap);

  /*  This loop used to start at 3, which included MOD1 in the
   *  virtual mapping. However, all of GTK+ treats MOD1 as a
   *  synonym for Alt, and does not expect it to be mapped around,
   *  therefore it's more sane to simply treat MOD1 like SHIFT and
   *  CONTROL, which are not mappable either.
   */
  for (i = 4; i < 8; i++)
    {
      if ((1 << i) & *state)
        {
	  if (keymap_x11->modmap[i] & GDK_SUPER_MASK)
	    *state |= GDK_SUPER_MASK;
	  if (keymap_x11->modmap[i] & GDK_HYPER_MASK)
	    *state |= GDK_HYPER_MASK;
	  if (keymap_x11->modmap[i] & GDK_META_MASK)
	    *state |= GDK_META_MASK;
        }
    }
}

gboolean
_gdk_keymap_key_is_modifier (GdkKeymap *keymap,
			     guint      keycode)
{
  GdkKeymapX11 *keymap_x11;
  gint i;

  keymap = GET_EFFECTIVE_KEYMAP (keymap);  
  keymap_x11 = GDK_KEYMAP_X11 (keymap);

  update_keyrange (keymap_x11);
  if (keycode < keymap_x11->min_keycode ||
      keycode > keymap_x11->max_keycode)
    return FALSE;

#ifdef HAVE_XKB
  if (KEYMAP_USE_XKB (keymap))
    {
      XkbDescRec *xkb = get_xkb (keymap_x11);
      
      if (xkb->map->modmap && xkb->map->modmap[keycode] != 0)
	return TRUE;
    }
  else
#endif
    {
      for (i = 0; i < 8 * keymap_x11->mod_keymap->max_keypermod; i++)
	{
	  if (keycode == keymap_x11->mod_keymap->modifiermap[i])
	    return TRUE;
	}
    }

  return FALSE;
}

/**
 * gdk_keymap_map_virtual_modifiers:
 * @keymap: a #GdkKeymap
 * @state: pointer to the modifier state to map
 *
 * Maps the virtual modifiers (i.e. Super, Hyper and Meta) which
 * are set in @state to their non-virtual counterparts (i.e. Mod2,
 * Mod3,...) and set the corresponding bits in @state.
 *
 * This function is useful when matching key events against
 * accelerators.
 *
 * Returns: %TRUE if no virtual modifiers were mapped to the
 *     same non-virtual modifier. Note that %FALSE is also returned
 *     if a virtual modifier is mapped to a non-virtual modifier that
 *     was already set in @state.
 *
 * Since: 2.20
 */
gboolean
gdk_keymap_map_virtual_modifiers (GdkKeymap       *keymap,
                                  GdkModifierType *state)
{
  GdkKeymapX11 *keymap_x11;
  const guint vmods[] = {
    GDK_SUPER_MASK, GDK_HYPER_MASK, GDK_META_MASK
  };
  int i, j;
  gboolean retval;

  keymap = GET_EFFECTIVE_KEYMAP (keymap);
  keymap_x11 = GDK_KEYMAP_X11 (keymap);

  if (KEYMAP_USE_XKB (keymap))
    get_xkb (keymap_x11);

  retval = TRUE;

  for (j = 0; j < 3; j++)
    {
      if (*state & vmods[j])
        {
          /* See comment in add_virtual_modifiers() */
          for (i = 4; i < 8; i++)
            {
              if (keymap_x11->modmap[i] & vmods[j])
                {
                  if (*state & (1 << i))
                    retval = FALSE;
                  else
                    *state |= 1 << i;
                }
            }
        }
    }

  return retval;
}


#define __GDK_KEYS_X11_C__
#include "gdkaliasdef.c"
