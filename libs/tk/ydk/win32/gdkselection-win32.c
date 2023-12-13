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
#include <string.h>
#include <stdlib.h>

#include "gdkproperty.h"
#include "gdkselection.h"
#include "gdkdisplay.h"
#include "gdkprivate-win32.h"

/* We emulate the GDK_SELECTION window properties of windows (as used
 * in the X11 backend) by using a hash table from window handles to
 * GdkSelProp structs.
 */

typedef struct {
  guchar *data;
  gsize length;
  gint format;
  GdkAtom type;
} GdkSelProp;

static GHashTable *sel_prop_table = NULL;

static GdkSelProp *dropfiles_prop = NULL;

/* We store the owner of each selection in this table. Obviously, this only
 * is valid intra-app, and in fact it is necessary for the intra-app DND to work.
 */
static GHashTable *sel_owner_table = NULL;

/* GdkAtoms for well-known image formats */
static GdkAtom *known_pixbuf_formats;
static int n_known_pixbuf_formats;

/* GdkAtoms for well-known text formats */
static GdkAtom text_plain;
static GdkAtom text_plain_charset_utf_8;
static GdkAtom text_plain_charset_CP1252;

void
_gdk_win32_selection_init (void)
{
  GSList *pixbuf_formats;
  GSList *rover;

  sel_prop_table = g_hash_table_new (NULL, NULL);
  sel_owner_table = g_hash_table_new (NULL, NULL);
  _format_atom_table = g_hash_table_new (NULL, NULL);

  pixbuf_formats = gdk_pixbuf_get_formats ();

  n_known_pixbuf_formats = 0;
  for (rover = pixbuf_formats; rover != NULL; rover = rover->next)
    {
      gchar **mime_types =
	gdk_pixbuf_format_get_mime_types ((GdkPixbufFormat *) rover->data);

      gchar **mime_type;

      for (mime_type = mime_types; *mime_type != NULL; mime_type++)
	n_known_pixbuf_formats++;
    }

  known_pixbuf_formats = g_new (GdkAtom, n_known_pixbuf_formats);

  n_known_pixbuf_formats = 0;
  for (rover = pixbuf_formats; rover != NULL; rover = rover->next)
    {
      gchar **mime_types =
	gdk_pixbuf_format_get_mime_types ((GdkPixbufFormat *) rover->data);

      gchar **mime_type;

      for (mime_type = mime_types; *mime_type != NULL; mime_type++)
	known_pixbuf_formats[n_known_pixbuf_formats++] = gdk_atom_intern (*mime_type, FALSE);
    }

  g_slist_free (pixbuf_formats);
  
  text_plain = gdk_atom_intern ("text/plain", FALSE);
  text_plain_charset_utf_8= gdk_atom_intern ("text/plain;charset=utf-8", FALSE);
  text_plain_charset_CP1252 = gdk_atom_intern ("text/plain;charset=CP1252", FALSE);

  g_hash_table_replace (_format_atom_table,
			GINT_TO_POINTER (_cf_png),
			_image_png);

  g_hash_table_replace (_format_atom_table,
			GINT_TO_POINTER (CF_DIB),
			_image_bmp);
}

/* The specifications for COMPOUND_TEXT and STRING specify that C0 and
 * C1 are not allowed except for \n and \t, however the X conversions
 * routines for COMPOUND_TEXT only enforce this in one direction,
 * causing cut-and-paste of \r and \r\n separated text to fail.
 * This routine strips out all non-allowed C0 and C1 characters
 * from the input string and also canonicalizes \r, and \r\n to \n
 */
static gchar * 
sanitize_utf8 (const gchar *src,
	       gint         length)
{
  GString *result = g_string_sized_new (length + 1);
  const gchar *p = src;
  const gchar *endp = src + length;

  while (p < endp)
    {
      if (*p == '\r')
	{
	  p++;
	  if (*p == '\n')
	    p++;

	  g_string_append_c (result, '\n');
	}
      else
	{
	  gunichar ch = g_utf8_get_char (p);
	  char buf[7];
	  gint buflen;
	  
	  if (!((ch < 0x20 && ch != '\t' && ch != '\n') || (ch >= 0x7f && ch < 0xa0)))
	    {
	      buflen = g_unichar_to_utf8 (ch, buf);
	      g_string_append_len (result, buf, buflen);
	    }

	  p = g_utf8_next_char (p);
	}
    }
  g_string_append_c (result, '\0');

  return g_string_free (result, FALSE);
}

static gchar *
_gdk_utf8_to_string_target_internal (const gchar *str,
				     gint         length)
{
  GError *error = NULL;
  
  gchar *tmp_str = sanitize_utf8 (str, length);
  gchar *result =  g_convert_with_fallback (tmp_str, -1,
					    "ISO-8859-1", "UTF-8",
					    NULL, NULL, NULL, &error);
  if (!result)
    {
      g_warning ("Error converting from UTF-8 to STRING: %s",
		 error->message);
      g_error_free (error);
    }
  
  g_free (tmp_str);
  return result;
}

static void
selection_property_store (GdkWindow *owner,
			  GdkAtom    type,
			  gint       format,
			  guchar    *data,
			  gint       length)
{
  GdkSelProp *prop;

  g_return_if_fail (type != GDK_TARGET_STRING);

  prop = g_hash_table_lookup (sel_prop_table, GDK_WINDOW_HWND (owner));

  if (prop != NULL)
    {
      g_free (prop->data);
      g_free (prop);
      g_hash_table_remove (sel_prop_table, GDK_WINDOW_HWND (owner));
    }

  prop = g_new (GdkSelProp, 1);

  prop->data = data;
  prop->length = length;
  prop->format = format;
  prop->type = type;

  g_hash_table_insert (sel_prop_table, GDK_WINDOW_HWND (owner), prop);
}

void
_gdk_dropfiles_store (gchar *data)
{
  if (data != NULL)
    {
      g_assert (dropfiles_prop == NULL);

      dropfiles_prop = g_new (GdkSelProp, 1);
      dropfiles_prop->data = (guchar *) data;
      dropfiles_prop->length = strlen (data) + 1;
      dropfiles_prop->format = 8;
      dropfiles_prop->type = _text_uri_list;
    }
  else
    {
      if (dropfiles_prop != NULL)
	{
	  g_free (dropfiles_prop->data);
	  g_free (dropfiles_prop);
	}
      dropfiles_prop = NULL;
    }
}

static gchar *
get_mapped_gdk_atom_name (GdkAtom gdk_target)
{
  if (gdk_target == _image_png)
    return g_strdup ("PNG");

  if (gdk_target == _image_jpeg)
    return g_strdup ("JFIF");
  
  if (gdk_target == _image_gif)
    return g_strdup ("GIF");
  
  return gdk_atom_name (gdk_target);
}

gboolean
gdk_selection_owner_set_for_display (GdkDisplay *display,
                                     GdkWindow  *owner,
                                     GdkAtom     selection,
                                     guint32     time,
                                     gboolean    send_event)
{
  HWND hwnd;
  GdkEvent tmp_event;

  g_return_val_if_fail (display == _gdk_display, FALSE);
  g_return_val_if_fail (selection != GDK_NONE, FALSE);

  GDK_NOTE (DND, {
      gchar *sel_name = gdk_atom_name (selection);

      g_print ("gdk_selection_owner_set_for_display: %p %s\n",
	       (owner ? GDK_WINDOW_HWND (owner) : NULL),
	       sel_name);
      g_free (sel_name);
    });

  if (selection != GDK_SELECTION_CLIPBOARD)
    {
      if (owner != NULL)
	g_hash_table_insert (sel_owner_table, selection, GDK_WINDOW_HWND (owner));
      else
	g_hash_table_remove (sel_owner_table, selection);
      return TRUE;
    }

  /* Rest of this function handles the CLIPBOARD selection */
  if (owner != NULL)
    {
      if (GDK_WINDOW_DESTROYED (owner))
	return FALSE;

      hwnd = GDK_WINDOW_HWND (owner);
    }
  else
    hwnd = NULL;

  if (!API_CALL (OpenClipboard, (hwnd)))
    return FALSE;

  _ignore_destroy_clipboard = TRUE;
  GDK_NOTE (DND, g_print ("... EmptyClipboard()\n"));
  if (!API_CALL (EmptyClipboard, ()))
    {
      _ignore_destroy_clipboard = FALSE;
      API_CALL (CloseClipboard, ());
      return FALSE;
    }
  _ignore_destroy_clipboard = FALSE;

  if (!API_CALL (CloseClipboard, ()))
    return FALSE;

  if (owner != NULL)
    {
      /* Send ourselves a selection request message so that
       * gdk_property_change will be called to store the clipboard
       * data.
       */
      GDK_NOTE (DND, g_print ("... sending GDK_SELECTION_REQUEST to ourselves\n"));
      tmp_event.selection.type = GDK_SELECTION_REQUEST;
      tmp_event.selection.window = owner;
      tmp_event.selection.send_event = FALSE;
      tmp_event.selection.selection = selection;
      tmp_event.selection.target = _utf8_string;
      tmp_event.selection.property = _gdk_selection;
      tmp_event.selection.requestor = hwnd;
      tmp_event.selection.time = time;

      gdk_event_put (&tmp_event);
    }

  return TRUE;
}

GdkWindow*
gdk_selection_owner_get_for_display (GdkDisplay *display,
                                     GdkAtom     selection)
{
  GdkWindow *window;

  g_return_val_if_fail (display == _gdk_display, NULL);
  g_return_val_if_fail (selection != GDK_NONE, NULL);

  if (selection == GDK_SELECTION_CLIPBOARD)
    {
      HWND owner = GetClipboardOwner ();

      if (owner == NULL)
	return NULL;

      return gdk_win32_handle_table_lookup ((GdkNativeWindow) owner);
    }

  window = gdk_window_lookup ((GdkNativeWindow) g_hash_table_lookup (sel_owner_table, selection));

  GDK_NOTE (DND, {
      gchar *sel_name = gdk_atom_name (selection);
      
      g_print ("gdk_selection_owner_get: %s = %p\n",
	       sel_name,
	       (window ? GDK_WINDOW_HWND (window) : NULL));
      g_free (sel_name);
    });

  return window;
}

static void
generate_selection_notify (GdkWindow *requestor,
			   GdkAtom    selection,
			   GdkAtom    target,
			   GdkAtom    property,
			   guint32    time)
{
  GdkEvent tmp_event;

  tmp_event.selection.type = GDK_SELECTION_NOTIFY;
  tmp_event.selection.window = requestor;
  tmp_event.selection.send_event = FALSE;
  tmp_event.selection.selection = selection;
  tmp_event.selection.target = target;
  tmp_event.selection.property = property;
  tmp_event.selection.requestor = 0;
  tmp_event.selection.time = time;

  gdk_event_put (&tmp_event);
}

void
gdk_selection_convert (GdkWindow *requestor,
		       GdkAtom    selection,
		       GdkAtom    target,
		       guint32    time)
{
  HGLOBAL hdata;
  GdkAtom property = _gdk_selection;

  g_return_if_fail (selection != GDK_NONE);
  g_return_if_fail (requestor != NULL);

  if (GDK_WINDOW_DESTROYED (requestor))
    return;

  GDK_NOTE (DND, {
      gchar *sel_name = gdk_atom_name (selection);
      gchar *tgt_name = gdk_atom_name (target);
      
      g_print ("gdk_selection_convert: %p %s %s\n",
	       GDK_WINDOW_HWND (requestor),
	       sel_name, tgt_name);
      g_free (sel_name);
      g_free (tgt_name);
    });

  if (selection == GDK_SELECTION_CLIPBOARD && target == _targets)
    {
      gint ntargets, fmt;
      GdkAtom *targets;
      gboolean has_text = FALSE;
      gboolean has_png = FALSE;
      gboolean has_bmp = FALSE;

      if (!API_CALL (OpenClipboard, (GDK_WINDOW_HWND (requestor))))
	return;

      targets = g_new (GdkAtom, CountClipboardFormats ());
      ntargets = 0;

      for (fmt = 0; 0 != (fmt = EnumClipboardFormats (fmt)); )
	{
	  if (fmt == _cf_png)
	    {
	      targets[ntargets++] = _image_png;
	      has_png = TRUE;
	    }
	}

      for (fmt = 0; 0 != (fmt = EnumClipboardFormats (fmt)); )
	{
	  gchar sFormat[80];

	  if (fmt == CF_UNICODETEXT || fmt == CF_TEXT)
	    {
	      /* Advertise text to GDK always as UTF8_STRING */
	      if (!has_text)
		targets[ntargets++] = _utf8_string;
	      has_text = TRUE;
	    }
	  else if (fmt == _cf_png)
	    {
	      /* Already handled above */
	    }
	  else if (fmt == CF_DIB ||
		   fmt == CF_DIBV5)
	    {
	      /* Don't bother telling that a bitmap is present if there is
	       * also PNG, which is much more reliable in transferring
	       * transparency.
	       */
	      if (!has_bmp && !has_png)
		targets[ntargets++] = _image_bmp;
	      has_bmp = TRUE;
	    }
	  else if (fmt == _cf_jfif)
	    {
	      /* Ditto for JPEG */
	      if (!has_png)
		targets[ntargets++] = _image_jpeg;
	    }
	  else if (fmt == _cf_gif)
	    {
	      /* Ditto for GIF.
	       */
	      if (!has_png)
		targets[ntargets++] = _image_gif;
	    }
	  else if (GetClipboardFormatName (fmt, sFormat, 80) > 0)
	    {
	      if (strcmp (sFormat, "image/bmp") == 0 ||
		  strcmp (sFormat, "image/x-bmp") == 0 ||
		  strcmp (sFormat, "image/x-MS-bmp") == 0 ||
		  strcmp (sFormat, "image/x-icon") == 0 ||
		  strcmp (sFormat, "image/x-ico") == 0 ||
		  strcmp (sFormat, "image/x-win-bitmap") == 0)
		{
		  /* Ignore these (from older GTK+ versions
		   * presumably), as the same image in the CF_DIB
		   * format will also be on the clipboard anyway.
		   */
		}
	      else
		targets[ntargets++] = gdk_atom_intern (sFormat, FALSE);
            }
        }

      GDK_NOTE (DND, {
	  int i;
	  
	  g_print ("... ");
	  for (i = 0; i < ntargets; i++)
	    {
	      gchar *atom_name = gdk_atom_name (targets[i]);

	      g_print ("%s", atom_name);
	      g_free (atom_name);
	      if (i < ntargets - 1)
		g_print (", ");
	    }
	  g_print ("\n");
	});

      if (ntargets > 0)
	selection_property_store (requestor, GDK_SELECTION_TYPE_ATOM,
				  32, (guchar *) targets,
				  ntargets * sizeof (GdkAtom));
      else
	property = GDK_NONE;

      API_CALL (CloseClipboard, ());
    }
  else if (selection == GDK_SELECTION_CLIPBOARD && target == _utf8_string)
    {
      /* Converting the CLIPBOARD selection means he wants the
       * contents of the clipboard. Get the clipboard data, and store
       * it for later.
       */
      if (!API_CALL (OpenClipboard, (GDK_WINDOW_HWND (requestor))))
	return;

      if ((hdata = GetClipboardData (CF_UNICODETEXT)) != NULL)
	{
	  wchar_t *ptr, *p, *q;
	  guchar *data;
	  glong length, wclen;

	  if ((ptr = GlobalLock (hdata)) != NULL)
	    {
	      length = GlobalSize (hdata);

	      GDK_NOTE (DND, g_print ("... CF_UNICODETEXT: %ld bytes\n",
				      length));

	      /* Strip out \r */
	      p = ptr;
	      q = ptr;
	      wclen = 0;
	      while (p < ptr + length / 2)
		{
		  if (*p != '\r')
		    {
		      *q++ = *p;
		      wclen++;
		    }
		  p++;
		}

	      data = g_utf16_to_utf8 (ptr, wclen, NULL, NULL, NULL);

	      if (data)
		selection_property_store (requestor, _utf8_string, 8,
					  data, strlen (data) + 1);
	      GlobalUnlock (hdata);
	    }
	}
      else
	property = GDK_NONE;

      API_CALL (CloseClipboard, ());
    }
  else if (selection == GDK_SELECTION_CLIPBOARD && target == _image_bmp)
    {
      if (!API_CALL (OpenClipboard, (GDK_WINDOW_HWND (requestor))))
	return;

      if ((hdata = GetClipboardData (CF_DIB)) != NULL)
        {
          BITMAPINFOHEADER *bi;

          if ((bi = GlobalLock (hdata)) != NULL)
            {
	      /* Need to add a BMP file header so gdk-pixbuf can load
	       * it.
	       *
	       * If the data is from Mozilla Firefox or IE7, and
	       * starts with an "old fashioned" BITMAPINFOHEADER,
	       * i.e. with biSize==40, and biCompression == BI_RGB and
	       * biBitCount==32, we assume that the "extra" byte in
	       * each pixel in fact is alpha.
	       *
	       * The gdk-pixbuf bmp loader doesn't trust 32-bit BI_RGB
	       * bitmaps to in fact have alpha, so we have to convince
	       * it by changing the bitmap header to a version 5
	       * BI_BITFIELDS one with explicit alpha mask indicated.
	       *
	       * The RGB bytes that are in bitmaps on the clipboard
	       * originating from Firefox or IE7 seem to be
	       * premultiplied with alpha. The gdk-pixbuf bmp loader
	       * of course doesn't expect that, so we have to undo the
	       * premultiplication before feeding the bitmap to the
	       * bmp loader.
	       *
	       * Note that for some reason the bmp loader used to want
	       * the alpha bytes in its input to actually be
	       * 255-alpha, but here we assume that this has been
	       * fixed before this is committed.
	       */
              BITMAPFILEHEADER *bf;
	      gpointer data;
	      gint data_length = GlobalSize (hdata);
	      gint new_length;
	      gboolean make_dibv5 = FALSE;

	      GDK_NOTE (DND, g_print ("... CF_DIB: %d bytes\n", data_length));

	      if (bi->biSize == sizeof (BITMAPINFOHEADER) &&
		  bi->biPlanes == 1 &&
		  bi->biBitCount == 32 &&
		  bi->biCompression == BI_RGB &&
#if 0
		  /* Maybe check explicitly for Mozilla or IE7?
		   *
		   * If the clipboard format
		   * application/x-moz-nativeimage is present, that is
		   * a reliable indicator that the data is offered by
		   * Mozilla one would think. For IE7,
		   * UniformResourceLocatorW is presumably not that
		   * uniqie, so probably need to do some
		   * GetClipboardOwner(), GetWindowThreadProcessId(),
		   * OpenProcess(), GetModuleFileNameEx() dance to
		   * check?
		   */
		  (IsClipboardFormatAvailable
		   (RegisterClipboardFormat ("application/x-moz-nativeimage")) ||
		   IsClipboardFormatAvailable
		   (RegisterClipboardFormat ("UniformResourceLocatorW"))) &&
#endif
		  TRUE)
		{
		  /* We turn the BITMAPINFOHEADER into a
		   * BITMAPV5HEADER before feeding it to gdk-pixbuf.
		   */
		  new_length = (data_length +
				sizeof (BITMAPFILEHEADER) +
				(sizeof (BITMAPV5HEADER) - sizeof (BITMAPINFOHEADER)));
		  make_dibv5 = TRUE;
		}
	      else
		{
		  new_length = data_length + sizeof (BITMAPFILEHEADER);
		}
	      
              data = g_try_malloc (new_length);

              if (data)
                {
                  bf = (BITMAPFILEHEADER *)data;
                  bf->bfType = 0x4d42; /* "BM" */
                  bf->bfSize = new_length;
                  bf->bfReserved1 = 0;
                  bf->bfReserved2 = 0;

		  if (make_dibv5)
		    {
		      BITMAPV5HEADER *bV5 = (BITMAPV5HEADER *) ((char *) data + sizeof (BITMAPFILEHEADER));
		      guchar *p;
		      int i;

		      bV5->bV5Size = sizeof (BITMAPV5HEADER);
		      bV5->bV5Width = bi->biWidth;
		      bV5->bV5Height = bi->biHeight;
		      bV5->bV5Planes = 1;
		      bV5->bV5BitCount = 32;
		      bV5->bV5Compression = BI_BITFIELDS;
		      bV5->bV5SizeImage = 4 * bV5->bV5Width * ABS (bV5->bV5Height);
		      bV5->bV5XPelsPerMeter = bi->biXPelsPerMeter;
		      bV5->bV5YPelsPerMeter = bi->biYPelsPerMeter;
		      bV5->bV5ClrUsed = 0;
		      bV5->bV5ClrImportant = 0;
		      /* Now the added mask fields */
		      bV5->bV5RedMask   = 0x00ff0000;
		      bV5->bV5GreenMask = 0x0000ff00;
		      bV5->bV5BlueMask  = 0x000000ff;
		      bV5->bV5AlphaMask = 0xff000000;
		      ((char *) &bV5->bV5CSType)[3] = 's';
		      ((char *) &bV5->bV5CSType)[2] = 'R';
		      ((char *) &bV5->bV5CSType)[1] = 'G';
		      ((char *) &bV5->bV5CSType)[0] = 'B';
		      /* Ignore colorspace and profile fields */
		      bV5->bV5Intent = LCS_GM_GRAPHICS;
		      bV5->bV5Reserved = 0;

		      bf->bfOffBits = (sizeof (BITMAPFILEHEADER) +
				       bV5->bV5Size);

		      p = ((guchar *) data) + sizeof (BITMAPFILEHEADER) + sizeof (BITMAPV5HEADER);
		      memcpy (p, ((char *) bi) + bi->biSize,
			      data_length - sizeof (BITMAPINFOHEADER));

		      for (i = 0; i < bV5->bV5SizeImage/4; i++)
			{
			  if (p[3] != 0)
			    {
			      gdouble inverse_alpha = 255./p[3];
			      
			      p[0] = p[0] * inverse_alpha + 0.5;
			      p[1] = p[1] * inverse_alpha + 0.5;
			      p[2] = p[2] * inverse_alpha + 0.5;
			    }

			  p += 4;
			}
		    }
		  else
		    {
		      bf->bfOffBits = (sizeof (BITMAPFILEHEADER) +
				       bi->biSize +
				       bi->biClrUsed * sizeof (RGBQUAD));

		      if (bi->biCompression == BI_BITFIELDS && bi->biBitCount >= 16)
		        {
                          /* Screenshots taken with PrintScreen or
                           * Alt + PrintScreen are found on the clipboard in
                           * this format. In this case the BITMAPINFOHEADER is
                           * followed by three DWORD specifying the masks of the
                           * red green and blue components, so adjust the offset
                           * accordingly. */
		          bf->bfOffBits += (3 * sizeof (DWORD));
		        }

		      memcpy ((char *) data + sizeof (BITMAPFILEHEADER),
			      bi,
			      data_length);
		    }

	          selection_property_store (requestor, _image_bmp, 8,
					    data, new_length);
                }
	      GlobalUnlock (hdata);
            }
      }

      API_CALL (CloseClipboard, ());
    }
  else if (selection == GDK_SELECTION_CLIPBOARD)
    {
      gchar *mapped_target_name;
      UINT fmt = 0;

      if (!API_CALL (OpenClipboard, (GDK_WINDOW_HWND (requestor))))
	return;

      mapped_target_name = get_mapped_gdk_atom_name (target);

      /* Check if it's available. We could simply call
       * GetClipboardData (RegisterClipboardFormat (targetname)), but
       * the global custom format ID space is limited,
       * (0xC000~0xFFFF), and we better not waste an format ID if we
       * are just a requestor.
       */
      for ( ; 0 != (fmt = EnumClipboardFormats (fmt)); )
        {
          char sFormat[80];

          if (GetClipboardFormatName (fmt, sFormat, 80) > 0 && 
              strcmp (sFormat, mapped_target_name) == 0)
            {
              if ((hdata = GetClipboardData (fmt)) != NULL)
	        {
	          /* Simply get it without conversion */
                  guchar *ptr;
                  gint length;

                  if ((ptr = GlobalLock (hdata)) != NULL)
                    {
                      length = GlobalSize (hdata);
	      
                      GDK_NOTE (DND, g_print ("... %s: %d bytes\n", mapped_target_name, length));
	      
                      selection_property_store (requestor, target, 8,
						g_memdup (ptr, length), length);
	              GlobalUnlock (hdata);
                      break;
                    }
                }
            }
        }
      g_free (mapped_target_name);
      API_CALL (CloseClipboard, ());
    }
  else if (selection == _gdk_win32_dropfiles)
    {
      /* This means he wants the names of the dropped files.
       * gdk_dropfiles_filter already has stored the text/uri-list
       * data temporarily in dropfiles_prop.
       */
      if (dropfiles_prop != NULL)
	{
	  selection_property_store
	    (requestor, dropfiles_prop->type, dropfiles_prop->format,
	     dropfiles_prop->data, dropfiles_prop->length);
	  g_free (dropfiles_prop);
	  dropfiles_prop = NULL;
	}
    }
  else
    property = GDK_NONE;

  /* Generate a selection notify message so that we actually fetch the
   * data (if property == _gdk_selection) or indicating failure (if
   * property == GDK_NONE).
   */
  generate_selection_notify (requestor, selection, target, property, time);
}

gint
gdk_selection_property_get (GdkWindow  *requestor,
			    guchar    **data,
			    GdkAtom    *ret_type,
			    gint       *ret_format)
{
  GdkSelProp *prop;

  g_return_val_if_fail (requestor != NULL, 0);
  g_return_val_if_fail (GDK_IS_WINDOW (requestor), 0);

  if (GDK_WINDOW_DESTROYED (requestor))
    return 0;
  
  GDK_NOTE (DND, g_print ("gdk_selection_property_get: %p",
			   GDK_WINDOW_HWND (requestor)));

  prop = g_hash_table_lookup (sel_prop_table, GDK_WINDOW_HWND (requestor));

  if (prop == NULL)
    {
      GDK_NOTE (DND, g_print (" (nothing)\n"));
      *data = NULL;

      return 0;
    }

  *data = g_malloc (prop->length + 1);
  (*data)[prop->length] = '\0';
  if (prop->length > 0)
    memmove (*data, prop->data, prop->length);

  GDK_NOTE (DND, {
      gchar *type_name = gdk_atom_name (prop->type);

      g_print (" %s format:%d length:%d\n", type_name, prop->format, prop->length);
      g_free (type_name);
    });

  if (ret_type)
    *ret_type = prop->type;

  if (ret_format)
    *ret_format = prop->format;

  return prop->length;
}

void
_gdk_selection_property_delete (GdkWindow *window)
{
  GdkSelProp *prop;

  GDK_NOTE (DND, g_print ("_gdk_selection_property_delete: %p (no-op)\n",
			   GDK_WINDOW_HWND (window)));

#if 1 /* without this we can only paste the first image from clipboard */
  prop = g_hash_table_lookup (sel_prop_table, GDK_WINDOW_HWND (window));
  if (prop != NULL)
    {
      g_free (prop->data);
      g_free (prop);
      g_hash_table_remove (sel_prop_table, GDK_WINDOW_HWND (window));
    }
#endif
}

void
gdk_selection_send_notify_for_display (GdkDisplay      *display,
                                       GdkNativeWindow  requestor,
                                       GdkAtom     	selection,
                                       GdkAtom     	target,
                                       GdkAtom     	property,
                                       guint32     	time)
{
  g_return_if_fail (display == _gdk_display);

  GDK_NOTE (DND, {
      gchar *sel_name = gdk_atom_name (selection);
      gchar *tgt_name = gdk_atom_name (target);
      gchar *prop_name = gdk_atom_name (property);
      
      g_print ("gdk_selection_send_notify_for_display: %p %s %s %s (no-op)\n",
	       requestor, sel_name, tgt_name, prop_name);
      g_free (sel_name);
      g_free (tgt_name);
      g_free (prop_name);
    });
}

/* It's hard to say whether implementing this actually is of any use
 * on the Win32 platform? gtk calls only
 * gdk_text_property_to_utf8_list_for_display().
 */
gint
gdk_text_property_to_text_list_for_display (GdkDisplay   *display,
					    GdkAtom       encoding,
					    gint          format, 
					    const guchar *text,
					    gint          length,
					    gchar      ***list)
{
  gchar *result;
  const gchar *charset;
  gchar *source_charset;

  g_return_val_if_fail (display == _gdk_display, 0);

  GDK_NOTE (DND, {
      gchar *enc_name = gdk_atom_name (encoding);
      
      g_print ("gdk_text_property_to_text_list_for_display: %s %d %.20s %d\n",
	       enc_name, format, text, length);
      g_free (enc_name);
    });
    
  if (!list)
    return 0;

  if (encoding == GDK_TARGET_STRING)
    source_charset = g_strdup ("ISO-8859-1");
  else if (encoding == _utf8_string)
    source_charset = g_strdup ("UTF-8");
  else
    source_charset = gdk_atom_name (encoding);
    
  g_get_charset (&charset);

  result = g_convert (text, length, charset, source_charset,
		      NULL, NULL, NULL);
  g_free (source_charset);

  if (!result)
    return 0;

  *list = g_new (gchar *, 1);
  **list = result;
  
  return 1;
}

void
gdk_free_text_list (gchar **list)
{
  g_return_if_fail (list != NULL);

  g_free (*list);
  g_free (list);
}

static gint
make_list (const gchar  *text,
	   gint          length,
	   gboolean      latin1,
	   gchar      ***list)
{
  GSList *strings = NULL;
  gint n_strings = 0;
  gint i;
  const gchar *p = text;
  const gchar *q;
  GSList *tmp_list;
  GError *error = NULL;

  while (p < text + length)
    {
      gchar *str;
      
      q = p;
      while (*q && q < text + length)
	q++;

      if (latin1)
	{
	  str = g_convert (p, q - p,
			   "UTF-8", "ISO-8859-1",
			   NULL, NULL, &error);

	  if (!str)
	    {
	      g_warning ("Error converting selection from STRING: %s",
			 error->message);
	      g_error_free (error);
	    }
	}
      else
	str = g_strndup (p, q - p);

      if (str)
	{
	  strings = g_slist_prepend (strings, str);
	  n_strings++;
	}

      p = q + 1;
    }

  if (list)
    *list = g_new (gchar *, n_strings + 1);

  (*list)[n_strings] = NULL;
  
  i = n_strings;
  tmp_list = strings;
  while (tmp_list)
    {
      if (list)
	(*list)[--i] = tmp_list->data;
      else
	g_free (tmp_list->data);

      tmp_list = tmp_list->next;
    }

  g_slist_free (strings);

  return n_strings;
}

gint 
gdk_text_property_to_utf8_list_for_display (GdkDisplay    *display,
                                            GdkAtom        encoding,
                                            gint           format,
                                            const guchar  *text,
                                            gint           length,
                                            gchar       ***list)
{
  g_return_val_if_fail (text != NULL, 0);
  g_return_val_if_fail (length >= 0, 0);
  g_return_val_if_fail (display == _gdk_display, 0);

  if (encoding == GDK_TARGET_STRING)
    {
      return make_list ((gchar *)text, length, TRUE, list);
    }
  else if (encoding == _utf8_string)
    {
      return make_list ((gchar *)text, length, FALSE, list);
    }
  else
    {
      gchar *enc_name = gdk_atom_name (encoding);

      g_warning ("gdk_text_property_to_utf8_list_for_display: encoding %s not handled\n", enc_name);
      g_free (enc_name);

      if (list)
	*list = NULL;

      return 0;
    }
}

gint
gdk_string_to_compound_text_for_display (GdkDisplay  *display,
					 const gchar *str,
					 GdkAtom     *encoding,
					 gint        *format,
					 guchar     **ctext,
					 gint        *length)
{
  g_return_val_if_fail (str != NULL, 0);
  g_return_val_if_fail (length >= 0, 0);
  g_return_val_if_fail (display == _gdk_display, 0);

  GDK_NOTE (DND, g_print ("gdk_string_to_compound_text_for_display: %.20s\n", str));

  /* Always fail on Win32. No COMPOUND_TEXT support. */

  if (encoding)
    *encoding = GDK_NONE;

  if (format)
    *format = 0;

  if (ctext)
    *ctext = NULL;

  if (length)
    *length = 0;

  return -1;
}

gchar *
gdk_utf8_to_string_target (const gchar *str)
{
  return _gdk_utf8_to_string_target_internal (str, strlen (str));
}

gboolean
gdk_utf8_to_compound_text_for_display (GdkDisplay  *display,
                                       const gchar *str,
                                       GdkAtom     *encoding,
                                       gint        *format,
                                       guchar     **ctext,
                                       gint        *length)
{
  g_return_val_if_fail (str != NULL, FALSE);
  g_return_val_if_fail (display == _gdk_display, FALSE);

  GDK_NOTE (DND, g_print ("gdk_utf8_to_compound_text_for_display: %.20s\n", str));

  /* Always fail on Win32. No COMPOUND_TEXT support. */

  if (encoding)
    *encoding = GDK_NONE;

  if (format)
    *format = 0;
  
  if (ctext)
    *ctext = NULL;

  if (length)
    *length = 0;

  return FALSE;
}

void
gdk_free_compound_text (guchar *ctext)
{
  /* As we never generate anything claimed to be COMPOUND_TEXT, this
   * should never be called. Or if it is called, ctext should be the
   * NULL returned for conversions to COMPOUND_TEXT above.
   */
  g_return_if_fail (ctext == NULL);
}

/* This function is called from gtk_selection_add_target() and
 * gtk_selection_add_targets() in gtkselection.c. It is this function
 * that takes care of setting those clipboard formats for which we use
 * delayed rendering. Formats copied directly to the clipboard are
 * handled in gdk_property_change() in gdkproperty-win32.c.
 */

void
gdk_win32_selection_add_targets (GdkWindow  *owner,
				 GdkAtom     selection,
				 gint	     n_targets,
				 GdkAtom    *targets)
{
  HWND hwnd = NULL;
  gboolean has_image = FALSE;
  gint i;

  GDK_NOTE (DND, {
      gchar *sel_name = gdk_atom_name (selection);
      
      g_print ("gdk_win32_selection_add_targets: %p: %s: ",
	       owner ? GDK_WINDOW_HWND (owner) : NULL,
	       sel_name);
      g_free (sel_name);

      for (i = 0; i < n_targets; i++)
	{
	  gchar *tgt_name = gdk_atom_name (targets[i]);

	  g_print ("%s", tgt_name);
	  g_free (tgt_name);
	  if (i < n_targets - 1)
	    g_print (", ");
	}
      g_print ("\n");
    });

  if (selection != GDK_SELECTION_CLIPBOARD)
    return;

  if (owner != NULL)
    {
      if (GDK_WINDOW_DESTROYED (owner))
	return;
      hwnd = GDK_WINDOW_HWND (owner);
    }

  if (!API_CALL (OpenClipboard, (hwnd)))
    return;

  /* We have a very simple strategy: If some kind of pixmap image
   * format is being added, actually advertise just PNG and DIB. PNG
   * is our preferred format because it can losslessly represent any
   * image that gdk-pixbuf formats in general can, even with alpha,
   * unambiguously. CF_DIB is also advertised because of the general
   * support for it in Windows software, but note that alpha won't be
   * handled.
   */
  for (i = 0; !has_image && i < n_targets; ++i)
    {
      UINT cf;
      gchar *target_name;
      int j;
      
      for (j = 0; j < n_known_pixbuf_formats; j++)
	if (targets[i] == known_pixbuf_formats[j])
	  {
	    if (!has_image)
	      {
		GDK_NOTE (DND, g_print ("... SetClipboardData(PNG,NULL)\n"));
		SetClipboardData (_cf_png, NULL);

		GDK_NOTE (DND, g_print ("... SetClipboardData(CF_DIB,NULL)\n"));
		SetClipboardData (CF_DIB, NULL);

		has_image = TRUE;
	      }
	    break;
	  }
      
      /* If it is one of the pixmap formats, already handled or not
       * needed.
       */
      if (j < n_known_pixbuf_formats)
	continue;

      /* We don't bother registering and advertising clipboard formats
       * that are X11 specific or no non-GTK+ apps will have ever
       * heard of, and when there are equivalent clipboard formats
       * that are commonly used.
       */
      if (targets[i] == _save_targets ||
	  targets[i] == _utf8_string ||
	  targets[i] == GDK_TARGET_STRING ||
	  targets[i] == _compound_text ||
	  targets[i] == _text ||
	  targets[i] == text_plain_charset_utf_8 ||
	  targets[i] == text_plain_charset_CP1252 ||
	  targets[i] == text_plain)
	continue;

      target_name = gdk_atom_name (targets[i]);

      if (g_str_has_prefix (target_name, "text/plain;charset="))
	{
	  g_free (target_name);
	  continue;
	}

      cf = RegisterClipboardFormat (target_name);

      g_hash_table_replace (_format_atom_table,
			    GINT_TO_POINTER (cf),
			    targets[i]);
      
      GDK_NOTE (DND, g_print ("... SetClipboardData(%s,NULL)\n",
			      _gdk_win32_cf_to_string (cf)));
      SetClipboardData (cf, NULL);

      g_free (target_name);
    }
  API_CALL (CloseClipboard, ());
}

/* Convert from types such as "image/jpg" or "image/png" to DIB using
 * gdk-pixbuf so that image copied from GTK+ apps can be pasted in
 * native apps like mspaint.exe
 */
HGLOBAL
_gdk_win32_selection_convert_to_dib (HGLOBAL  hdata,
				     GdkAtom  target)
{
  GDK_NOTE (DND, {
      gchar *target_name = gdk_atom_name (target);

      g_print ("_gdk_win32_selection_convert_to_dib: %p %s\n",
	       hdata, target_name);
      g_free (target_name);
    });

  if (target == _image_bmp)
    {
      HGLOBAL hdatanew;
      SIZE_T size;
      guchar *ptr;

      g_return_val_if_fail (GlobalSize (hdata) >= sizeof (BITMAPFILEHEADER), NULL);

      /* No conversion is needed, just strip the BITMAPFILEHEADER */
      size = GlobalSize (hdata) - sizeof (BITMAPFILEHEADER);
      ptr = GlobalLock (hdata);

      memmove (ptr, ptr + sizeof (BITMAPFILEHEADER), size);
      GlobalUnlock (hdata);

      if ((hdatanew = GlobalReAlloc (hdata, size, GMEM_MOVEABLE)) == NULL)
	{
	  WIN32_API_FAILED ("GlobalReAlloc");
	  GlobalFree (hdata); /* The old hdata is not freed if error */
	}
      return hdatanew;
    }

  g_warning ("Should not happen: We provide some image format but not CF_DIB and CF_DIB is requested.");

  return NULL;
}
