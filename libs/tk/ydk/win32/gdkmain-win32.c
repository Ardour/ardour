/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-2002 Tor Lillqvist
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

#include <glib/gprintf.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <io.h>

#include "gdk.h"
#include "gdkregion-generic.h"
#include "gdkkeysyms.h"
#include "gdkinternals.h"
#include "gdkintl.h"
#include "gdkprivate-win32.h"
#include "gdkinput-win32.h"

#include <objbase.h>

#include <imm.h>

static gboolean gdk_synchronize = FALSE;

static gboolean dummy;

const GOptionEntry _gdk_windowing_args[] = {
  { "sync", 0, 0, G_OPTION_ARG_NONE, &gdk_synchronize, 
    /* Description of --sync in --help output */              N_("Don't batch GDI requests"), NULL },
  { "no-wintab", 0, 0, G_OPTION_ARG_NONE, &_gdk_input_ignore_wintab, 
    /* Description of --no-wintab in --help output */         N_("Don't use the Wintab API for tablet support"), NULL },
  { "ignore-wintab", 0, 0, G_OPTION_ARG_NONE, &_gdk_input_ignore_wintab, 
    /* Description of --ignore-wintab in --help output */     N_("Same as --no-wintab"), NULL },
  { "use-wintab", 0, 0, G_OPTION_ARG_NONE, &dummy,
    /* Description of --use-wintab in --help output */     N_("Do use the Wintab API [default]"), NULL },
  { "max-colors", 0, 0, G_OPTION_ARG_INT, &_gdk_max_colors, 
    /* Description of --max-colors=COLORS in --help output */ N_("Size of the palette in 8 bit mode"), 
    /* Placeholder in --max-colors=COLORS in --help output */ N_("COLORS") },
  { NULL }
};

int __stdcall
DllMain (HINSTANCE hinstDLL,
	 DWORD     dwReason,
	 LPVOID    reserved)
{
  _gdk_dll_hinstance = hinstDLL;

  return TRUE;
}

void
_gdk_windowing_init (void)
{
  gchar buf[10];

  if (getenv ("GDK_IGNORE_WINTAB") != NULL)
    _gdk_input_ignore_wintab = TRUE;
  else if (getenv ("GDK_USE_WINTAB") != NULL)
    _gdk_input_ignore_wintab = FALSE;

  if (gdk_synchronize)
    GdiSetBatchLimit (1);

  _gdk_app_hmodule = GetModuleHandle (NULL);
  _gdk_display_hdc = CreateDC ("DISPLAY", NULL, NULL, NULL);
  _gdk_input_locale = GetKeyboardLayout (0);
  _gdk_win32_keymap_set_active_layout (GDK_WIN32_KEYMAP (gdk_keymap_get_default ()), _gdk_input_locale);
  _gdk_input_locale_is_ime = ImmIsIME (_gdk_input_locale);
  GetLocaleInfo (MAKELCID (LOWORD (_gdk_input_locale), SORT_DEFAULT),
		 LOCALE_IDEFAULTANSICODEPAGE,
		 buf, sizeof (buf));
  _gdk_input_codepage = atoi (buf);
  GDK_NOTE (EVENTS, g_print ("input_locale:%p, codepage:%d\n",
			     _gdk_input_locale, _gdk_input_codepage));

  CoInitialize (NULL);

  _gdk_selection = gdk_atom_intern_static_string ("GDK_SELECTION");
  _wm_transient_for = gdk_atom_intern_static_string ("WM_TRANSIENT_FOR");
  _targets = gdk_atom_intern_static_string ("TARGETS");
  _delete = gdk_atom_intern_static_string ("DELETE");
  _save_targets = gdk_atom_intern_static_string ("SAVE_TARGETS");
  _utf8_string = gdk_atom_intern_static_string ("UTF8_STRING");
  _text = gdk_atom_intern_static_string ("TEXT");
  _compound_text = gdk_atom_intern_static_string ("COMPOUND_TEXT");
  _text_uri_list = gdk_atom_intern_static_string ("text/uri-list");
  _text_html = gdk_atom_intern_static_string ("text/html");
  _image_png = gdk_atom_intern_static_string ("image/png");
  _image_jpeg = gdk_atom_intern_static_string ("image/jpeg");
  _image_bmp = gdk_atom_intern_static_string ("image/bmp");
  _image_gif = gdk_atom_intern_static_string ("image/gif");

  _local_dnd = gdk_atom_intern_static_string ("LocalDndSelection");
  _gdk_win32_dropfiles = gdk_atom_intern_static_string ("DROPFILES_DND");
  _gdk_ole2_dnd = gdk_atom_intern_static_string ("OLE2_DND");

  /* MS Office 2007, at least, offers images in common file formats
   * using clipboard format names like "PNG" and "JFIF". So we follow
   * the lead and map the GDK target name "image/png" to the clipboard
   * format name "PNG" etc.
   */
  _cf_png = RegisterClipboardFormat ("PNG");
  _cf_jfif = RegisterClipboardFormat ("JFIF");
  _cf_gif = RegisterClipboardFormat ("GIF");

  _cf_url = RegisterClipboardFormat ("UniformResourceLocatorW");
  _cf_html_format = RegisterClipboardFormat ("HTML Format");
  _cf_text_html = RegisterClipboardFormat ("text/html");

  _gdk_win32_selection_init ();
}

void
_gdk_win32_api_failed (const gchar *where,
		      const gchar *api)
{
  gchar *msg = g_win32_error_message (GetLastError ());
  g_warning ("%s: %s failed: %s", where, api, msg);
  g_free (msg);
}

void
_gdk_other_api_failed (const gchar *where,
		      const gchar *api)
{
  g_warning ("%s: %s failed", where, api);
}

void
gdk_set_use_xshm (gboolean use_xshm)
{
  /* Always on */
}

gboolean
gdk_get_use_xshm (void)
{
  return TRUE;
}

gint
gdk_screen_get_width (GdkScreen *screen)
{
  return GDK_WINDOW_OBJECT (_gdk_root)->width;
}

gint
gdk_screen_get_height (GdkScreen *screen)
{
  return GDK_WINDOW_OBJECT (_gdk_root)->height;
}
gint
gdk_screen_get_width_mm (GdkScreen *screen)
{
  return (double) gdk_screen_get_width (screen) / GetDeviceCaps (_gdk_display_hdc, LOGPIXELSX) * 25.4;
}

gint
gdk_screen_get_height_mm (GdkScreen *screen)
{
  return (double) gdk_screen_get_height (screen) / GetDeviceCaps (_gdk_display_hdc, LOGPIXELSY) * 25.4;
}

void
_gdk_windowing_display_set_sm_client_id (GdkDisplay  *display,
					 const gchar *sm_client_id)
{
  g_warning("gdk_set_sm_client_id %s", sm_client_id ? sm_client_id : "NULL");
}

void
gdk_display_beep (GdkDisplay *display)
{
  g_return_if_fail (display == gdk_display_get_default());
  if (!MessageBeep (-1))
    Beep (1000, 50);
}

void
_gdk_windowing_exit (void)
{
  _gdk_win32_dnd_exit ();
  CoUninitialize ();
  DeleteDC (_gdk_display_hdc);
  _gdk_display_hdc = NULL;
}

gchar *
gdk_get_display (void)
{
  return g_strdup (gdk_display_get_name (gdk_display_get_default ()));
}

void
gdk_error_trap_push (void)
{
}

gint
gdk_error_trap_pop (void)
{
  return 0;
}

void
gdk_notify_startup_complete (void)
{
}

void
gdk_notify_startup_complete_with_id (const gchar* startup_id)
{
}

void          
gdk_window_set_startup_id (GdkWindow   *window,
			   const gchar *startup_id)
{
}

#ifdef G_ENABLE_DEBUG

/*
 * Like g_strdup_printf, but to a static buffer. Return value does not
 * have to be g_free()d. The buffer is of bounded size and reused
 * cyclically. Thus the return value is valid only until that part of
 * the buffer happens to get reused. This doesn't matter as this
 * function's return value is used in debugging output right after the call,
 * and the return value isn't used after that.
 */
static gchar *
static_printf (const gchar *format,
	       ...)
{
  static gchar buf[10000];
  gchar *msg;
  static gchar *bufp = buf;
  gchar *retval;
  va_list args;

  va_start (args, format);
  msg = g_strdup_vprintf (format, args);
  va_end (args);

  g_assert (strlen (msg) < sizeof (buf));

  if (bufp + strlen (msg) + 1 > buf + sizeof (buf))
    bufp = buf;
  retval = bufp;

  strcpy (bufp, msg);
  bufp += strlen (msg) + 1;
  g_free (msg);

  return retval;
}

gchar *
_gdk_win32_color_to_string (const GdkColor *color)
{
  return static_printf ("(%.04x,%.04x,%.04x):%.06x",
			color->red, color->green,
			color->blue, color->pixel);
}

void
_gdk_win32_print_paletteentries (const PALETTEENTRY *pep,
				const int           nentries)
{
  char buf[20];
  int i;

  for (i = 0; i < nentries; i++)
    g_print ("  %3d %02x:  %02x %02x %02x%s\n",
	     i, i,
	     pep[i].peRed, pep[i].peGreen, pep[i].peBlue,
	     (pep[i].peFlags == 0 ? "" :
	      (pep[i].peFlags == PC_EXPLICIT ? " PC_EXPLICIT" :
	       (pep[i].peFlags == PC_NOCOLLAPSE ? " PC_NOCOLLAPSE" :
		(pep[i].peFlags == PC_RESERVED ? " PC_RESERVED" :
		 (g_sprintf (buf, " %d", pep[i].peFlags), buf))))));
}

void
_gdk_win32_print_system_palette (void)
{
  PALETTEENTRY *pe;
  int k;

  k = GetSystemPaletteEntries (_gdk_display_hdc, 0, 0, NULL);
  pe = g_new (PALETTEENTRY, k);
  k = GetSystemPaletteEntries (_gdk_display_hdc, 0, k, pe);

  if (!k)
    g_print ("GetSystemPaletteEntries failed: %s\n",
	     g_win32_error_message (GetLastError ()));
  else
    {
      g_print ("System palette: %d entries\n", k);
      _gdk_win32_print_paletteentries (pe, k);
    }
  g_free (pe);
}

static gint
palette_size (HPALETTE hpal)
{
  WORD npal = 0;

  if (!GetObject (hpal, sizeof (npal), &npal))
    WIN32_GDI_FAILED ("GetObject (HPALETTE)");

  return npal;
}

void
_gdk_win32_print_hpalette (HPALETTE hpal)
{
  PALETTEENTRY *pe;
  gint n, npal;

  npal = palette_size (hpal);
  pe = g_new (PALETTEENTRY, npal);
  n = GetPaletteEntries (hpal, 0, npal, pe);

  if (!n)
    g_print ("HPALETTE %p: GetPaletteEntries failed: %s\n",
	     hpal, g_win32_error_message (GetLastError ()));
  else
    {
      g_print ("HPALETTE %p: %d (%d) entries\n", hpal, n, npal);
      _gdk_win32_print_paletteentries (pe, n);
    }
  g_free (pe);
}

void
_gdk_win32_print_dc (HDC hdc)
{
  HGDIOBJ obj;
  LOGBRUSH logbrush;
  EXTLOGPEN extlogpen;
  HRGN hrgn;
  RECT rect;
  int flag;

  g_print ("%p:\n", hdc);
  obj = GetCurrentObject (hdc, OBJ_BRUSH);
  GetObject (obj, sizeof (LOGBRUSH), &logbrush);
  g_print ("brush: %s color=%06lx hatch=%p\n",
	   _gdk_win32_lbstyle_to_string (logbrush.lbStyle),
	   logbrush.lbColor, (gpointer) logbrush.lbHatch);
  obj = GetCurrentObject (hdc, OBJ_PEN);
  GetObject (obj, sizeof (EXTLOGPEN), &extlogpen);
  g_print ("pen: %s %s %s %s w=%d %s\n",
	   _gdk_win32_pstype_to_string (extlogpen.elpPenStyle),
	   _gdk_win32_psstyle_to_string (extlogpen.elpPenStyle),
	   _gdk_win32_psendcap_to_string (extlogpen.elpPenStyle),
	   _gdk_win32_psjoin_to_string (extlogpen.elpPenStyle),
	   (int) extlogpen.elpWidth,
	   _gdk_win32_lbstyle_to_string (extlogpen.elpBrushStyle));
  g_print ("rop2: %s textcolor=%06lx\n",
	   _gdk_win32_rop2_to_string (GetROP2 (hdc)),
	   GetTextColor (hdc));
  hrgn = CreateRectRgn (0, 0, 0, 0);
  if ((flag = GetClipRgn (hdc, hrgn)) == -1)
    WIN32_API_FAILED ("GetClipRgn");
  else if (flag == 0)
    g_print ("no clip region\n");
  else if (flag == 1)
    {
      GetRgnBox (hrgn, &rect);
      g_print ("clip region: %p bbox: %s\n",
	       hrgn, _gdk_win32_rect_to_string (&rect));
    }
  DeleteObject (hrgn);
}

gchar *
_gdk_win32_cap_style_to_string (GdkCapStyle cap_style)
{
  switch (cap_style)
    {
#define CASE(x) case GDK_CAP_##x: return #x
    CASE (NOT_LAST);
    CASE (BUTT);
    CASE (ROUND);
    CASE (PROJECTING);
#undef CASE
    default: return static_printf ("illegal_%d", cap_style);
    }
  /* NOTREACHED */
  return NULL;
}

gchar *
_gdk_win32_fill_style_to_string (GdkFill fill)
{
  switch (fill)
    {
#define CASE(x) case GDK_##x: return #x
    CASE (SOLID);
    CASE (TILED);
    CASE (STIPPLED);
    CASE (OPAQUE_STIPPLED);
#undef CASE
    default: return static_printf ("illegal_%d", fill);
    }
  /* NOTREACHED */
  return NULL;
}

gchar *
_gdk_win32_function_to_string (GdkFunction function)
{
  switch (function)
    {
#define CASE(x) case GDK_##x: return #x
    CASE (COPY);
    CASE (INVERT);
    CASE (XOR);
    CASE (CLEAR);
    CASE (AND);
    CASE (AND_REVERSE);
    CASE (AND_INVERT);
    CASE (NOOP);
    CASE (OR);
    CASE (EQUIV);
    CASE (OR_REVERSE);
    CASE (COPY_INVERT);
    CASE (OR_INVERT);
    CASE (NAND);
    CASE (SET);
#undef CASE
    default: return static_printf ("illegal_%d", function);
    }
  /* NOTREACHED */
  return NULL; 
}

gchar *
_gdk_win32_join_style_to_string (GdkJoinStyle join_style)
{
  switch (join_style)
    {
#define CASE(x) case GDK_JOIN_##x: return #x
    CASE (MITER);
    CASE (ROUND);
    CASE (BEVEL);
#undef CASE
    default: return static_printf ("illegal_%d", join_style);
    }
  /* NOTREACHED */
  return NULL; 
}

gchar *
_gdk_win32_line_style_to_string (GdkLineStyle line_style)
{
  switch (line_style)
    {
#define CASE(x) case GDK_LINE_##x: return #x
    CASE(SOLID);
    CASE(ON_OFF_DASH);  
    CASE(DOUBLE_DASH);  
#undef CASE
    default: return static_printf ("illegal_%d", line_style);
    }
  /* NOTREACHED */
  return NULL; 
}

gchar *
_gdk_win32_drag_protocol_to_string (GdkDragProtocol protocol)
{
  switch (protocol)
    {
#define CASE(x) case GDK_DRAG_PROTO_##x: return #x
      CASE (MOTIF);
      CASE (XDND);
      CASE (ROOTWIN);
      CASE (NONE);
      CASE (WIN32_DROPFILES);
      CASE (OLE2);
      CASE (LOCAL);
#undef CASE
    default: return static_printf ("illegal_%d", protocol);
    }
  /* NOTREACHED */
  return NULL; 
}

gchar *
_gdk_win32_gcvalues_mask_to_string (GdkGCValuesMask mask)
{
  gchar buf[400];
  gchar *bufp = buf;
  gchar *s = "";

  buf[0] = '\0';

#define BIT(x) 						\
  if (mask & GDK_GC_##x) 				\
    (bufp += g_sprintf (bufp, "%s" #x, s), s = "|")

  BIT (FOREGROUND);
  BIT (BACKGROUND);
  BIT (FONT);
  BIT (FUNCTION);
  BIT (FILL);
  BIT (TILE);
  BIT (STIPPLE);
  BIT (CLIP_MASK);
  BIT (SUBWINDOW);
  BIT (TS_X_ORIGIN);
  BIT (TS_Y_ORIGIN);
  BIT (CLIP_X_ORIGIN);
  BIT (CLIP_Y_ORIGIN);
  BIT (EXPOSURES);
  BIT (LINE_WIDTH);
  BIT (LINE_STYLE);
  BIT (CAP_STYLE);
  BIT (JOIN_STYLE);
#undef BIT

  return static_printf ("%s", buf);  
}

gchar *
_gdk_win32_window_state_to_string (GdkWindowState state)
{
  gchar buf[100];
  gchar *bufp = buf;
  gchar *s = "";

  buf[0] = '\0';

#define BIT(x)						\
  if (state & GDK_WINDOW_STATE_ ## x)			\
    (bufp += sprintf (bufp, "%s" #x, s), s = "|")

  /* For clarity, also show the complement of WITHDRAWN, i.e. "MAPPED" */
  if (!(state & GDK_WINDOW_STATE_WITHDRAWN))
    (bufp += sprintf (bufp, "MAPPED"), s = "|");

  BIT (WITHDRAWN);
  BIT (ICONIFIED);
  BIT (MAXIMIZED);
  BIT (STICKY);
#undef BIT

  return static_printf ("%s", buf);  
}

gchar *
_gdk_win32_window_style_to_string (LONG style)
{
  gchar buf[1000];
  gchar *bufp = buf;
  gchar *s = "";

  buf[0] = '\0';

#define BIT(x)						\
  if (style & WS_ ## x)					\
    (bufp += sprintf (bufp, "%s" #x, s), s = "|")

  /* Note that many of the WS_* macros are in face several bits.
   * Handle just the individual bits here. Sort as in w32api's
   * winuser.h.
   */
  BIT (BORDER);
  BIT (CHILD);
  BIT (CLIPCHILDREN);
  BIT (CLIPSIBLINGS);
  BIT (DISABLED);
  BIT (DLGFRAME);
  BIT (GROUP);
  BIT (HSCROLL);
  BIT (ICONIC);
  BIT (MAXIMIZE);
  BIT (MAXIMIZEBOX);
  BIT (MINIMIZE);
  BIT (MINIMIZEBOX);
  BIT (POPUP);
  BIT (SIZEBOX);
  BIT (SYSMENU);
  BIT (TABSTOP);
  BIT (THICKFRAME);
  BIT (VISIBLE);
  BIT (VSCROLL);
#undef BIT

  return static_printf ("%s", buf);  
}

gchar *
_gdk_win32_window_exstyle_to_string (LONG style)
{
  gchar buf[1000];
  gchar *bufp = buf;
  gchar *s = "";

  buf[0] = '\0';

#define BIT(x)						\
  if (style & WS_EX_ ## x)				\
    (bufp += sprintf (bufp, "%s" #x, s), s = "|")

  /* Note that many of the WS_EX_* macros are in face several bits.
   * Handle just the individual bits here. Sort as in w32api's
   * winuser.h.
   */
  BIT (ACCEPTFILES);
  BIT (APPWINDOW);
  BIT (CLIENTEDGE);
#ifndef WS_EX_COMPOSITED
#  define WS_EX_COMPOSITED 0x02000000L
#endif
  BIT (COMPOSITED);
  BIT (CONTEXTHELP);
  BIT (CONTROLPARENT);
  BIT (DLGMODALFRAME);
  BIT (LAYERED);
  BIT (LAYOUTRTL);
  BIT (LEFTSCROLLBAR);
  BIT (MDICHILD);
  BIT (NOACTIVATE);
  BIT (NOINHERITLAYOUT);
  BIT (NOPARENTNOTIFY);
  BIT (RIGHT);
  BIT (RTLREADING);
  BIT (STATICEDGE);
  BIT (TOOLWINDOW);
  BIT (TOPMOST);
  BIT (TRANSPARENT);
  BIT (WINDOWEDGE);
#undef BIT

  return static_printf ("%s", buf);  
}

gchar *
_gdk_win32_window_pos_bits_to_string (UINT flags)
{
  gchar buf[1000];
  gchar *bufp = buf;
  gchar *s = "";

  buf[0] = '\0';

#define BIT(x)						\
  if (flags & SWP_ ## x)				\
    (bufp += sprintf (bufp, "%s" #x, s), s = "|")

  BIT (DRAWFRAME);
  BIT (FRAMECHANGED);
  BIT (HIDEWINDOW);
  BIT (NOACTIVATE);
  BIT (NOCOPYBITS);
  BIT (NOMOVE);
  BIT (NOSIZE);
  BIT (NOREDRAW);
  BIT (NOZORDER);
  BIT (SHOWWINDOW);
  BIT (NOOWNERZORDER);
  BIT (NOSENDCHANGING);
  BIT (DEFERERASE);
  BIT (ASYNCWINDOWPOS);
#undef BIT

  return static_printf ("%s", buf);  
}

gchar *
_gdk_win32_drag_action_to_string (GdkDragAction actions)
{
  gchar buf[100];
  gchar *bufp = buf;
  gchar *s = "";

  buf[0] = '\0';

#define BIT(x)						\
  if (actions & GDK_ACTION_ ## x)				\
    (bufp += sprintf (bufp, "%s" #x, s), s = "|")

  BIT (DEFAULT);
  BIT (COPY);
  BIT (MOVE);
  BIT (LINK);
  BIT (PRIVATE);
  BIT (ASK);
#undef BIT

  return static_printf ("%s", buf);  
}

gchar *
_gdk_win32_rop2_to_string (int rop2)
{
  switch (rop2)
    {
#define CASE(x) case R2_##x: return #x
      CASE (BLACK);
      CASE (COPYPEN);
      CASE (MASKNOTPEN);
      CASE (MASKPEN);
      CASE (MASKPENNOT);
      CASE (MERGENOTPEN);
      CASE (MERGEPEN);
      CASE (MERGEPENNOT);
      CASE (NOP);
      CASE (NOT);
      CASE (NOTCOPYPEN);
      CASE (NOTMASKPEN);
      CASE (NOTMERGEPEN);
      CASE (NOTXORPEN);
      CASE (WHITE);
      CASE (XORPEN);
#undef CASE
    default: return static_printf ("illegal_%x", rop2);
    }
  /* NOTREACHED */
  return NULL;
}

gchar *
_gdk_win32_lbstyle_to_string (UINT brush_style)
{
  switch (brush_style)
    {
#define CASE(x) case BS_##x: return #x
      CASE (DIBPATTERN);
      CASE (DIBPATTERNPT);
      CASE (HATCHED);
      CASE (HOLLOW);
      CASE (PATTERN);
      CASE (SOLID);
#undef CASE
    default: return static_printf ("illegal_%d", brush_style);
    }
  /* NOTREACHED */
  return NULL;
}

gchar *
_gdk_win32_pstype_to_string (DWORD pen_style)
{
  switch (pen_style & PS_TYPE_MASK)
    {
    case PS_GEOMETRIC: return "GEOMETRIC";
    case PS_COSMETIC: return "COSMETIC";
    default: return static_printf ("illegal_%d", pen_style & PS_TYPE_MASK);
    }
  /* NOTREACHED */
  return NULL;
}

gchar *
_gdk_win32_psstyle_to_string (DWORD pen_style)
{
  switch (pen_style & PS_STYLE_MASK)
    {
#define CASE(x) case PS_##x: return #x
      CASE (ALTERNATE);
      CASE (SOLID);
      CASE (DASH);
      CASE (DOT);
      CASE (DASHDOT);
      CASE (DASHDOTDOT);
      CASE (NULL);
      CASE (USERSTYLE);
      CASE (INSIDEFRAME);
#undef CASE
    default: return static_printf ("illegal_%d", pen_style & PS_STYLE_MASK);
    }
  /* NOTREACHED */
  return NULL;
}

gchar *
_gdk_win32_psendcap_to_string (DWORD pen_style)
{
  switch (pen_style & PS_ENDCAP_MASK)
    {
#define CASE(x) case PS_ENDCAP_##x: return #x
      CASE (ROUND);
      CASE (SQUARE);
      CASE (FLAT);
#undef CASE
    default: return static_printf ("illegal_%d", pen_style & PS_ENDCAP_MASK);
    }
  /* NOTREACHED */
  return NULL;
}

gchar *
_gdk_win32_psjoin_to_string (DWORD pen_style)
{
  switch (pen_style & PS_JOIN_MASK)
    {
#define CASE(x) case PS_JOIN_##x: return #x
      CASE (ROUND);
      CASE (BEVEL);
      CASE (MITER);
#undef CASE
    default: return static_printf ("illegal_%d", pen_style & PS_JOIN_MASK);
    }
  /* NOTREACHED */
  return NULL;
}

gchar *
_gdk_win32_message_to_string (UINT msg)
{
  switch (msg)
    {
#define CASE(x) case x: return #x
      CASE (WM_NULL);
      CASE (WM_CREATE);
      CASE (WM_DESTROY);
      CASE (WM_MOVE);
      CASE (WM_SIZE);
      CASE (WM_ACTIVATE);
      CASE (WM_SETFOCUS);
      CASE (WM_KILLFOCUS);
      CASE (WM_ENABLE);
      CASE (WM_SETREDRAW);
      CASE (WM_SETTEXT);
      CASE (WM_GETTEXT);
      CASE (WM_GETTEXTLENGTH);
      CASE (WM_PAINT);
      CASE (WM_CLOSE);
      CASE (WM_QUERYENDSESSION);
      CASE (WM_QUERYOPEN);
      CASE (WM_ENDSESSION);
      CASE (WM_QUIT);
      CASE (WM_ERASEBKGND);
      CASE (WM_SYSCOLORCHANGE);
      CASE (WM_SHOWWINDOW);
      CASE (WM_WININICHANGE);
      CASE (WM_DEVMODECHANGE);
      CASE (WM_ACTIVATEAPP);
      CASE (WM_FONTCHANGE);
      CASE (WM_TIMECHANGE);
      CASE (WM_CANCELMODE);
      CASE (WM_SETCURSOR);
      CASE (WM_MOUSEACTIVATE);
      CASE (WM_CHILDACTIVATE);
      CASE (WM_QUEUESYNC);
      CASE (WM_GETMINMAXINFO);
      CASE (WM_PAINTICON);
      CASE (WM_ICONERASEBKGND);
      CASE (WM_NEXTDLGCTL);
      CASE (WM_SPOOLERSTATUS);
      CASE (WM_DRAWITEM);
      CASE (WM_MEASUREITEM);
      CASE (WM_DELETEITEM);
      CASE (WM_VKEYTOITEM);
      CASE (WM_CHARTOITEM);
      CASE (WM_SETFONT);
      CASE (WM_GETFONT);
      CASE (WM_SETHOTKEY);
      CASE (WM_GETHOTKEY);
      CASE (WM_QUERYDRAGICON);
      CASE (WM_COMPAREITEM);
      CASE (WM_GETOBJECT);
      CASE (WM_COMPACTING);
      CASE (WM_WINDOWPOSCHANGING);
      CASE (WM_WINDOWPOSCHANGED);
      CASE (WM_POWER);
      CASE (WM_COPYDATA);
      CASE (WM_CANCELJOURNAL);
      CASE (WM_NOTIFY);
      CASE (WM_INPUTLANGCHANGEREQUEST);
      CASE (WM_INPUTLANGCHANGE);
      CASE (WM_TCARD);
      CASE (WM_HELP);
      CASE (WM_USERCHANGED);
      CASE (WM_NOTIFYFORMAT);
      CASE (WM_CONTEXTMENU);
      CASE (WM_STYLECHANGING);
      CASE (WM_STYLECHANGED);
      CASE (WM_DISPLAYCHANGE);
      CASE (WM_GETICON);
      CASE (WM_SETICON);
      CASE (WM_NCCREATE);
      CASE (WM_NCDESTROY);
      CASE (WM_NCCALCSIZE);
      CASE (WM_NCHITTEST);
      CASE (WM_NCPAINT);
      CASE (WM_NCACTIVATE);
      CASE (WM_GETDLGCODE);
      CASE (WM_SYNCPAINT);
      CASE (WM_NCMOUSEMOVE);
      CASE (WM_NCLBUTTONDOWN);
      CASE (WM_NCLBUTTONUP);
      CASE (WM_NCLBUTTONDBLCLK);
      CASE (WM_NCRBUTTONDOWN);
      CASE (WM_NCRBUTTONUP);
      CASE (WM_NCRBUTTONDBLCLK);
      CASE (WM_NCMBUTTONDOWN);
      CASE (WM_NCMBUTTONUP);
      CASE (WM_NCMBUTTONDBLCLK);
      CASE (WM_NCXBUTTONDOWN);
      CASE (WM_NCXBUTTONUP);
      CASE (WM_NCXBUTTONDBLCLK);
      CASE (WM_KEYDOWN);
      CASE (WM_KEYUP);
      CASE (WM_CHAR);
      CASE (WM_DEADCHAR);
      CASE (WM_SYSKEYDOWN);
      CASE (WM_SYSKEYUP);
      CASE (WM_SYSCHAR);
      CASE (WM_SYSDEADCHAR);
      CASE (WM_KEYLAST);
      CASE (WM_IME_STARTCOMPOSITION);
      CASE (WM_IME_ENDCOMPOSITION);
      CASE (WM_IME_COMPOSITION);
      CASE (WM_INITDIALOG);
      CASE (WM_COMMAND);
      CASE (WM_SYSCOMMAND);
      CASE (WM_TIMER);
      CASE (WM_HSCROLL);
      CASE (WM_VSCROLL);
      CASE (WM_INITMENU);
      CASE (WM_INITMENUPOPUP);
      CASE (WM_MENUSELECT);
      CASE (WM_MENUCHAR);
      CASE (WM_ENTERIDLE);
      CASE (WM_MENURBUTTONUP);
      CASE (WM_MENUDRAG);
      CASE (WM_MENUGETOBJECT);
      CASE (WM_UNINITMENUPOPUP);
      CASE (WM_MENUCOMMAND);
      CASE (WM_CHANGEUISTATE);
      CASE (WM_UPDATEUISTATE);
      CASE (WM_QUERYUISTATE);
      CASE (WM_CTLCOLORMSGBOX);
      CASE (WM_CTLCOLOREDIT);
      CASE (WM_CTLCOLORLISTBOX);
      CASE (WM_CTLCOLORBTN);
      CASE (WM_CTLCOLORDLG);
      CASE (WM_CTLCOLORSCROLLBAR);
      CASE (WM_CTLCOLORSTATIC);
      CASE (WM_MOUSEMOVE);
      CASE (WM_LBUTTONDOWN);
      CASE (WM_LBUTTONUP);
      CASE (WM_LBUTTONDBLCLK);
      CASE (WM_RBUTTONDOWN);
      CASE (WM_RBUTTONUP);
      CASE (WM_RBUTTONDBLCLK);
      CASE (WM_MBUTTONDOWN);
      CASE (WM_MBUTTONUP);
      CASE (WM_MBUTTONDBLCLK);
      CASE (WM_MOUSEWHEEL);
      CASE (WM_MOUSEHWHEEL);
      CASE (WM_XBUTTONDOWN);
      CASE (WM_XBUTTONUP);
      CASE (WM_XBUTTONDBLCLK);
      CASE (WM_PARENTNOTIFY);
      CASE (WM_ENTERMENULOOP);
      CASE (WM_EXITMENULOOP);
      CASE (WM_NEXTMENU);
      CASE (WM_SIZING);
      CASE (WM_CAPTURECHANGED);
      CASE (WM_MOVING);
      CASE (WM_POWERBROADCAST);
      CASE (WM_DEVICECHANGE);
      CASE (WM_MDICREATE);
      CASE (WM_MDIDESTROY);
      CASE (WM_MDIACTIVATE);
      CASE (WM_MDIRESTORE);
      CASE (WM_MDINEXT);
      CASE (WM_MDIMAXIMIZE);
      CASE (WM_MDITILE);
      CASE (WM_MDICASCADE);
      CASE (WM_MDIICONARRANGE);
      CASE (WM_MDIGETACTIVE);
      CASE (WM_MDISETMENU);
      CASE (WM_ENTERSIZEMOVE);
      CASE (WM_EXITSIZEMOVE);
      CASE (WM_DROPFILES);
      CASE (WM_MDIREFRESHMENU);
      CASE (WM_IME_SETCONTEXT);
      CASE (WM_IME_NOTIFY);
      CASE (WM_IME_CONTROL);
      CASE (WM_IME_COMPOSITIONFULL);
      CASE (WM_IME_SELECT);
      CASE (WM_IME_CHAR);
      CASE (WM_IME_REQUEST);
      CASE (WM_IME_KEYDOWN);
      CASE (WM_IME_KEYUP);
      CASE (WM_MOUSEHOVER);
      CASE (WM_MOUSELEAVE);
      CASE (WM_NCMOUSEHOVER);
      CASE (WM_NCMOUSELEAVE);
      CASE (WM_CUT);
      CASE (WM_COPY);
      CASE (WM_PASTE);
      CASE (WM_CLEAR);
      CASE (WM_UNDO);
      CASE (WM_RENDERFORMAT);
      CASE (WM_RENDERALLFORMATS);
      CASE (WM_DESTROYCLIPBOARD);
      CASE (WM_DRAWCLIPBOARD);
      CASE (WM_PAINTCLIPBOARD);
      CASE (WM_VSCROLLCLIPBOARD);
      CASE (WM_SIZECLIPBOARD);
      CASE (WM_ASKCBFORMATNAME);
      CASE (WM_CHANGECBCHAIN);
      CASE (WM_HSCROLLCLIPBOARD);
      CASE (WM_QUERYNEWPALETTE);
      CASE (WM_PALETTEISCHANGING);
      CASE (WM_PALETTECHANGED);
      CASE (WM_HOTKEY);
      CASE (WM_PRINT);
      CASE (WM_PRINTCLIENT);
      CASE (WM_APPCOMMAND);
      CASE (WM_HANDHELDFIRST);
      CASE (WM_HANDHELDLAST);
      CASE (WM_AFXFIRST);
      CASE (WM_AFXLAST);
      CASE (WM_PENWINFIRST);
      CASE (WM_PENWINLAST);
      CASE (WM_APP);
      CASE (WT_PACKET);
      CASE (WT_CSRCHANGE);
      CASE (WT_PROXIMITY);
#undef CASE
    default:
      if (msg >= WM_HANDHELDFIRST && msg <= WM_HANDHELDLAST)
	return static_printf ("WM_HANDHELDFIRST+%d", msg - WM_HANDHELDFIRST);
      else if (msg >= WM_AFXFIRST && msg <= WM_AFXLAST)
	return static_printf ("WM_AFXFIRST+%d", msg - WM_AFXFIRST);
      else if (msg >= WM_PENWINFIRST && msg <= WM_PENWINLAST)
	return static_printf ("WM_PENWINFIRST+%d", msg - WM_PENWINFIRST);
      else if (msg >= WM_USER && msg <= 0x7FFF)
	return static_printf ("WM_USER+%d", msg - WM_USER);
      else if (msg >= 0xC000 && msg <= 0xFFFF)
	return static_printf ("reg-%#x", msg);
      else
	return static_printf ("unk-%#x", msg);
    }
  /* NOTREACHED */
  return NULL;
}

gchar *
_gdk_win32_key_to_string (LONG lParam)
{
  char buf[100];
  gchar *keyname_utf8;

  if (GetKeyNameText (lParam, buf, sizeof (buf)) &&
      (keyname_utf8 = g_locale_to_utf8 (buf, -1, NULL, NULL, NULL)) != NULL)
    {
      gchar *retval = static_printf ("%s", keyname_utf8);

      g_free (keyname_utf8);

      return retval;
    }

  return static_printf ("unk-%#lx", lParam);
}
      
gchar *
_gdk_win32_cf_to_string (UINT format)
{
  char buf[100];

  switch (format)
    {
#define CASE(x) case CF_##x: return "CF_" #x
      CASE (BITMAP);
      CASE (DIB);
      CASE (DIBV5);
      CASE (DIF);
      CASE (DSPBITMAP);
      CASE (DSPENHMETAFILE);
      CASE (DSPMETAFILEPICT);
      CASE (DSPTEXT);
      CASE (ENHMETAFILE);
      CASE (HDROP);
      CASE (LOCALE);
      CASE (METAFILEPICT);
      CASE (OEMTEXT);
      CASE (OWNERDISPLAY);
      CASE (PALETTE);
      CASE (PENDATA);
      CASE (RIFF);
      CASE (SYLK);
      CASE (TEXT);
      CASE (WAVE);
      CASE (TIFF);
      CASE (UNICODETEXT);
    default:
      if (format >= CF_GDIOBJFIRST &&
	  format <= CF_GDIOBJLAST)
	return static_printf ("CF_GDIOBJ%d", format - CF_GDIOBJFIRST);
      if (format >= CF_PRIVATEFIRST &&
	  format <= CF_PRIVATELAST)
	return static_printf ("CF_PRIVATE%d", format - CF_PRIVATEFIRST);
      if (GetClipboardFormatName (format, buf, sizeof (buf)))
	return static_printf ("'%s'", buf);
      else
	return static_printf ("unk-%#lx", format);
    }
}
      
gchar *
_gdk_win32_data_to_string (const guchar *data,
			   int           nbytes)
{
  GString *s = g_string_new ("");
  int i;
  gchar *retval;

  for (i = 0; i < nbytes; i++)
    if (data[i] >=' ' && data[i] <= '~')
      g_string_append_printf (s, "%c  ", data[i]);
    else
      g_string_append_printf (s, "%02X ", data[i]);

  retval = static_printf ("%s", s->str);
  g_string_free (s, TRUE);

  return retval;
}

gchar *
_gdk_win32_rect_to_string (const RECT *rect)
{
  return static_printf ("%ldx%ld@%+ld%+ld",
			(rect->right - rect->left), (rect->bottom - rect->top),
			rect->left, rect->top);
}

gchar *
_gdk_win32_gdkrectangle_to_string (const GdkRectangle *rect)
{
  return static_printf ("%dx%d@%+d%+d",
			rect->width, rect->height,
			rect->x, rect->y);
}

gchar *
_gdk_win32_gdkregion_to_string (const GdkRegion *rgn)
{
  return static_printf ("%dx%d@%+d%+d",
			(rgn->extents.x2 - rgn->extents.x1),
			(rgn->extents.y2 - rgn->extents.y1),
			rgn->extents.x1, rgn->extents.y1);
}

gchar *
_gdk_win32_drawable_description (GdkDrawable *d)
{
  gint width, height, depth;

  g_return_val_if_fail (GDK_IS_DRAWABLE (d), NULL);

  gdk_drawable_get_size (d, &width, &height);
  depth = gdk_drawable_get_depth (d);

  return static_printf ("%s:%p:%dx%dx%d",
			G_OBJECT_TYPE_NAME (d),
			GDK_DRAWABLE_HANDLE (d),
			width, height, depth);
}

#endif /* G_ENABLE_DEBUG */
