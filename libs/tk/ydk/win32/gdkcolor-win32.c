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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gdkcolor.h"
#include "gdkscreen.h"
#include "gdkinternals.h"
#include "gdkprivate-win32.h"

static gint     gdk_colormap_match_color (GdkColormap      *cmap,
					  GdkColor         *color,
					  const gchar      *available);
static void     gdk_colormap_init        (GdkColormap      *colormap);
static void     gdk_colormap_class_init  (GdkColormapClass *klass);
static void     gdk_colormap_finalize    (GObject          *object);

static gpointer parent_class = NULL;

GType
gdk_colormap_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      const GTypeInfo object_info =
      {
        sizeof (GdkColormapClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_colormap_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkColormap),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gdk_colormap_init,
      };
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "GdkColormap",
                                            &object_info, 0);
    }
  
  return object_type;
}

static void
gdk_colormap_init (GdkColormap *colormap)
{
  GdkColormapPrivateWin32 *private;

  private = g_new (GdkColormapPrivateWin32, 1);

  colormap->windowing_data = private;
  
  private->hpal = NULL;
  private->current_size = 0;
  private->use = NULL;
  private->hash = NULL;
  private->info = NULL;

  colormap->size = 0;
  colormap->colors = NULL;
}

static void
gdk_colormap_class_init (GdkColormapClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_colormap_finalize;
}

static void
gdk_colormap_finalize (GObject *object)
{
  GdkColormap *colormap = GDK_COLORMAP (object);
  GdkColormapPrivateWin32 *private = GDK_WIN32_COLORMAP_DATA (colormap);

  GDI_CALL (DeleteObject, (private->hpal));

  if (private->hash)
    g_hash_table_destroy (private->hash);
  
  g_free (private->info);
  g_free (colormap->colors);
  g_free (private);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/* Mimics XAllocColorCells. Allocate read/write color cells. */

static gboolean
alloc_color_cells (GdkColormap    *cmap,
		   gboolean        contig,
		   unsigned long   plane_masks_return[],
		   unsigned int    nplanes,
		   unsigned long   pixels_return[],
		   unsigned int	   npixels)
{
  GdkColormapPrivateWin32 *cmapp = GDK_WIN32_COLORMAP_DATA (cmap);
  gint i, nfree, iret, start = 0;

  GDK_NOTE (COLORMAP, g_print ("alloc_color_cells: cmap=%p contig=%s npl=%d npix=%d",
			       cmapp, contig ? "TRUE" : "FALSE",
			       nplanes, npixels));

  switch (cmap->visual->type)
    {
    case GDK_VISUAL_GRAYSCALE:
    case GDK_VISUAL_PSEUDO_COLOR:
      nfree = 0;
      for (i = 0; i < cmap->size && nfree < npixels; i++)
	if (cmapp->use[i] == GDK_WIN32_PE_AVAILABLE &&
	    (!contig ||
	     (nfree == 0 || cmapp->use[i-1] == GDK_WIN32_PE_AVAILABLE)))
	  {
	    if (nfree == 0)
	      start = i;
	    nfree++;
	  }
	else if (contig)
	  nfree = 0;

      if (npixels > nfree)
	{
	  GDK_NOTE (COLORMAP, g_print ("... nope (%d > %d)\n",
				       npixels, nfree));
	  return FALSE;
	}
      else
	GDK_NOTE (COLORMAP, g_print ("... ok\n"));

      iret = 0;
      for (i = start; i < cmap->size && iret < npixels; i++)
	if (cmapp->use[i] == GDK_WIN32_PE_AVAILABLE)
	  {
	    cmapp->use[i] = GDK_WIN32_PE_INUSE;
	    pixels_return[iret] = i;
	    iret++;
	  }
      g_assert (iret == npixels);
      break;

    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

/* The following functions are originally from Tk8.0, but heavily
   modified.  Here are tk's licensing terms. I hope these terms don't
   conflict with the GNU Lesser General Public License? They
   shouldn't, as they are looser that the GLPL, yes? */

/*
This software is copyrighted by the Regents of the University of
California, Sun Microsystems, Inc., and other parties.  The following
terms apply to all files associated with the software unless explicitly
disclaimed in individual files.

The authors hereby grant permission to use, copy, modify, distribute,
and license this software and its documentation for any purpose, provided
that existing copyright notices are retained in all copies and that this
notice is included verbatim in any distributions. No written agreement,
license, or royalty fee is required for any of the authorized uses.
Modifications to this software may be copyrighted by their authors
and need not follow the licensing terms described here, provided that
the new terms are clearly indicated on the first page of each file where
they apply.

IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY
FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY
DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE
IS PROVIDED ON AN "AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE
NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
MODIFICATIONS.

GOVERNMENT USE: If you are acquiring this software on behalf of the
U.S. government, the Government shall have only "Restricted Rights"
in the software and related documentation as defined in the Federal 
Acquisition Regulations (FARs) in Clause 52.227.19 (c) (2).  If you
are acquiring the software on behalf of the Department of Defense, the
software shall be classified as "Commercial Computer Software" and the
Government shall have only "Restricted Rights" as defined in Clause
252.227-7013 (c) (1) of DFARs.  Notwithstanding the foregoing, the
authors grant the U.S. Government and others acting in its behalf
permission to use and distribute the software in accordance with the
terms specified in this license.
*/

/* Mimics XAllocColor. Allocate a read-only colormap entry. */

static int
alloc_color (GdkColormap  *cmap,
	     PALETTEENTRY *color,
	     guint        *pixelp)
{
  PALETTEENTRY entry, close_entry;
  COLORREF new_pixel;
  UINT index;
  GdkColormapPrivateWin32 *cmapp = GDK_WIN32_COLORMAP_DATA (cmap);
  gint i;
    
  entry = *color;
  entry.peFlags = 0;

  new_pixel = RGB (entry.peRed, entry.peGreen, entry.peBlue);

  switch (cmap->visual->type)
    {
    case GDK_VISUAL_PSEUDO_COLOR:
      /* Find the nearest existing palette entry. */
      index = GetNearestPaletteIndex (cmapp->hpal, new_pixel);
      GetPaletteEntries (cmapp->hpal, index, 1, &close_entry);

      GDK_NOTE (COLORMAP,
		g_print ("alloc_color: new_pixel=%06lx index=%d=%02x close=%06lx\n",
			 new_pixel, index, index,
			 RGB (close_entry.peRed, close_entry.peGreen, close_entry.peBlue)));

      if (new_pixel != RGB (close_entry.peRed, close_entry.peGreen,
			    close_entry.peBlue))
	{
	  /* Not a perfect match. */
	  if (cmapp->use[index] == GDK_WIN32_PE_AVAILABLE)
	    {
	      /* It was a nonused entry anyway, so we can use it, and
	       * set it to the correct color.
	       */
	      GDK_NOTE (COLORMAP, g_print ("... was free\n"));
	      GDI_CALL (SetPaletteEntries, (cmapp->hpal, index, 1, &entry));
	    }
	  else
	    {
	      /* The close entry found is in use, so search for a
	       * available slot.
	       */
	      gboolean done = FALSE;
	      for (i = 0; i < cmap->size; i++)
		if (cmapp->use[i] == GDK_WIN32_PE_AVAILABLE)
		  {
		    /* An available slot, use it. */
		    GDK_NOTE (COLORMAP,
			      g_print ("... use free slot %d%s\n",
				       i, (i >= cmapp->current_size) ?
				       ", will resize palette" : ""));
		    if (i >= cmapp->current_size)
		      {
			if (!ResizePalette (cmapp->hpal, i + 1))
			  {
			    WIN32_GDI_FAILED ("ResizePalette");
			    break;
			  }
			cmapp->current_size = i + 1;
		      }
		    if (!SetPaletteEntries (cmapp->hpal, i, 1, &entry))
		      {
			WIN32_GDI_FAILED ("SetPaletteEntries");
			i = cmap->size;
		      }
		    else
		      {
			done = TRUE;
			index = i;
		      }
		    break;
		  }
	      if (!done)
		{
		  /* No free slots available, or failed to resize
		   * palette or set palette entry.
		   */
		  GDK_NOTE (COLORMAP, g_print ("... failure\n"));
		  return FALSE;
		}
	    }
	}
      else
	{
	  /* We got a match, so use it. */
	}

      *pixelp = index;
      cmapp->use[index] = GDK_WIN32_PE_INUSE;
      GDK_NOTE (COLORMAP, g_print ("alloc_color: %p: "
				   "index=%3d=%02x for %02x %02x %02x: "
				   "%02x %02x %02x\n",
				   cmapp->hpal, index, index,
				   entry.peRed, entry.peGreen, entry.peBlue,
				   color->peRed, color->peGreen, color->peBlue));
      return TRUE;

    case GDK_VISUAL_STATIC_COLOR:
      /* Find the nearest existing palette entry. */
      index = GetNearestPaletteIndex (cmapp->hpal, new_pixel);
      GetPaletteEntries (cmapp->hpal, index, 1, &close_entry);
      *color = close_entry;
      *pixelp = index;
      GDK_NOTE (COLORMAP, g_print ("alloc_color %p: "
				   "index=%3d=%02x for %02x %02x %02x: "
				   "%02x %02x %02x\n",
				   cmapp->hpal, index, index,
				   entry.peRed, entry.peGreen, entry.peBlue,
				   color->peRed, color->peGreen, color->peBlue));
      return TRUE;

    case GDK_VISUAL_TRUE_COLOR:
      /* Determine what color will actually be used on non-colormap systems. */

      *pixelp = GetNearestColor (_gdk_display_hdc, new_pixel);
      color->peRed = GetRValue (*pixelp);
      color->peGreen = GetGValue (*pixelp);
      color->peBlue = GetBValue (*pixelp);
      return TRUE;

    default:
      g_assert_not_reached ();
      return FALSE;
    }
}

/* Mimics XFreeColors. */

static void
free_colors (GdkColormap *cmap,
	     gulong  	 *pixels,
	     gint    	  npixels,
	     gulong  	  planes)
{
  PALETTEENTRY pe;
  GdkColormapPrivateWin32 *cmapp = GDK_WIN32_COLORMAP_DATA (cmap);
  gint i;
#ifdef G_ENABLE_DEBUG
  gint set_black_count = 0;
#endif
  gboolean *cleared_entries;

  cleared_entries = g_new0 (gboolean, cmap->size);

  /* We don't have to do anything for non-palette devices. */
  
  switch (cmap->visual->type)
    {
    case GDK_VISUAL_GRAYSCALE:
    case GDK_VISUAL_PSEUDO_COLOR:
      for (i = 0; i < npixels; i++)
	{
	  if (pixels[i] >= cmap->size)
	    ; /* Nothing */
	  else if (cmapp->use[pixels[i]] == GDK_WIN32_PE_STATIC)
	    ; /* Nothing either*/
	  else
	    {
	      cmapp->use[pixels[i]] = GDK_WIN32_PE_AVAILABLE;
	      cleared_entries[pixels[i]] = TRUE;
	    }
	}
      for (i = cmapp->current_size - 1; i >= 0; i--)
	if (cmapp->use[i] != GDK_WIN32_PE_AVAILABLE)
	  break;
      if (i < cmapp->current_size - 1)
	{
	  GDK_NOTE (COLORMAP, g_print ("free_colors: hpal=%p resize=%d\n",
				       cmapp->hpal, i + 1));
	  if (!ResizePalette (cmapp->hpal, i + 1))
	    WIN32_GDI_FAILED ("ResizePalette");
	  else
	    cmapp->current_size = i + 1;
	}
      pe.peRed = pe.peGreen = pe.peBlue = pe.peFlags = 0;
      for (i = 0; i < cmapp->current_size; i++)
	{
	  if (cleared_entries[i])
	    {
	      GDI_CALL (SetPaletteEntries, (cmapp->hpal, i, 1, &pe));
	      GDK_NOTE (COLORMAP, set_black_count++);
	    }
	}
#if 0
      GDK_NOTE (COLORMAP, _gdk_win32_print_hpalette (cmapp->hpal));
#else
      GDK_NOTE (COLORMAP, (set_black_count > 0 ?
			   g_print ("free_colors: %d (%d) set to black\n",
				    set_black_count, cmapp->current_size)
			   : (void) 0));
#endif
      g_free (cleared_entries);

      break;

    default:
      g_assert_not_reached ();
    }
}

/* Mimics XCreateColormap. */

static void
create_colormap (GdkColormap *cmap,
		 gboolean     writeable)
{
  struct {
    LOGPALETTE pal;
    PALETTEENTRY pe[256-1];
  } lp;
  HPALETTE hpal;
  GdkColormapPrivateWin32 *cmapp = GDK_WIN32_COLORMAP_DATA (cmap);
  gint i;

  /* Allocate a starting palette with all the static colors. */
  hpal = GetStockObject (DEFAULT_PALETTE);
  lp.pal.palVersion = 0x300;
  lp.pal.palNumEntries = GetPaletteEntries (hpal, 0, 256, lp.pal.palPalEntry);

  if (cmap->visual->type == GDK_VISUAL_STATIC_COLOR &&
      cmap->visual->depth == 4)
    {
      /* Use only 16 colors */
      for (i = 8; i < 16; i++)
	lp.pal.palPalEntry[i] = lp.pal.palPalEntry[i+4];
      lp.pal.palNumEntries = 16;
    }

  for (i = 0; i < lp.pal.palNumEntries; i++)
    lp.pal.palPalEntry[i].peFlags = 0;
  GDK_NOTE (COLORMAP, (g_print ("Default palette %p: %d entries\n",
				hpal, lp.pal.palNumEntries),
		       _gdk_win32_print_paletteentries (lp.pal.palPalEntry,
						       lp.pal.palNumEntries)));
  DeleteObject (hpal);
  
  /* For writeable colormaps, allow all 256 entries to be set. They won't
   * set all 256 system palette entries anyhow, of course, but we shouldn't
   * let the app see that, I think.
   */
  if (writeable)
    cmapp->current_size = 0;
  else
    cmapp->current_size = lp.pal.palNumEntries;

  cmapp->private_val = writeable;

  if (!(cmapp->hpal = CreatePalette (&lp.pal)))
    WIN32_GDI_FAILED ("CreatePalette");
  else
    GDK_NOTE (COLORMAP, g_print ("Created palette %p\n", cmapp->hpal));

  switch (cmap->visual->type)
    {
    case GDK_VISUAL_PSEUDO_COLOR:
      cmapp->use = g_new (GdkWin32PalEntryState, cmap->size);

      /* Mark static colors in use. */
      for (i = 0; i < cmapp->current_size; i++)
	{
	  cmapp->use[i] = GDK_WIN32_PE_STATIC;
	  cmapp->info[i].ref_count = G_MAXUINT/2;
	}
      /* Mark rest not in use */
      for (; i < cmap->size; i++)
	cmapp->use[i] = GDK_WIN32_PE_AVAILABLE;
      break;

    default:
      break;
    }
}

static void
sync_colors (GdkColormap *colormap)
{
  PALETTEENTRY *pe;
  GdkColormapPrivateWin32 *private = GDK_WIN32_COLORMAP_DATA (colormap);
  gint nlookup;
  gint i;
  
  pe = g_new (PALETTEENTRY, colormap->size);
  nlookup = GetPaletteEntries (private->hpal, 0, colormap->size, pe);
	  
  GDK_NOTE (COLORMAP, (g_print ("sync_colors: %p hpal=%p: %d entries\n",
				private, private->hpal, nlookup),
		       _gdk_win32_print_paletteentries (pe, nlookup)));
	  
  for (i = 0; i < nlookup; i++)
    {
      colormap->colors[i].pixel = i;
      colormap->colors[i].red = (pe[i].peRed * 65535) / 255;
      colormap->colors[i].green = (pe[i].peGreen * 65535) / 255;
      colormap->colors[i].blue = (pe[i].peBlue * 65535) / 255;
    }
  
  for ( ; i < colormap->size; i++)
    {
      colormap->colors[i].pixel = i;
      colormap->colors[i].red = 0;
      colormap->colors[i].green = 0;
      colormap->colors[i].blue = 0;
    }
  
  g_free (pe);
}

GdkColormap*
gdk_colormap_new (GdkVisual *visual,
		  gboolean   private_cmap)
{
  GdkColormap *colormap;
  GdkColormapPrivateWin32 *private;

  g_return_val_if_fail (visual != NULL, NULL);

  colormap = g_object_new (gdk_colormap_get_type (), NULL);
  private = GDK_WIN32_COLORMAP_DATA (colormap);

  colormap->visual = visual;

  colormap->size = visual->colormap_size;

  switch (visual->type)
    {
    case GDK_VISUAL_GRAYSCALE:
    case GDK_VISUAL_PSEUDO_COLOR:
      private->info = g_new0 (GdkColorInfo, colormap->size);
      colormap->colors = g_new (GdkColor, colormap->size);
      
      private->hash = g_hash_table_new ((GHashFunc) gdk_color_hash,
					(GEqualFunc) gdk_color_equal);
      
      create_colormap (colormap, private_cmap);

      if (private_cmap)
	{
	  sync_colors (colormap);
#if 0 /* XXX is this needed or not? Seems redundant */
	  gdk_colormap_change (colormap, colormap->size);
#endif
	}
      break;

    case GDK_VISUAL_STATIC_GRAY:
    case GDK_VISUAL_STATIC_COLOR:
      create_colormap (colormap, FALSE);
      colormap->colors = g_new (GdkColor, colormap->size);
      sync_colors (colormap);
      break;

    case GDK_VISUAL_TRUE_COLOR:
      break;

    default:
      g_assert_not_reached ();
    }

  return colormap;
}

GdkColormap*
gdk_screen_get_system_colormap (GdkScreen *screen)
{
  static GdkColormap *colormap = NULL;
  GdkColormapPrivateWin32 *private;

  g_return_val_if_fail (screen == _gdk_screen, NULL);

  if (!colormap)
    {
      colormap = g_object_new (gdk_colormap_get_type (), NULL);
      private = GDK_WIN32_COLORMAP_DATA (colormap);

      colormap->visual = gdk_visual_get_system ();

      colormap->size = colormap->visual->colormap_size;

      private->private_val = FALSE;

      switch (colormap->visual->type)
	{
	case GDK_VISUAL_GRAYSCALE:
	case GDK_VISUAL_PSEUDO_COLOR:
	  private->info = g_new0 (GdkColorInfo, colormap->size);
	  private->hash = g_hash_table_new ((GHashFunc) gdk_color_hash,
					    (GEqualFunc) gdk_color_equal);
	  /* Fallthrough */

	case GDK_VISUAL_STATIC_GRAY:
	case GDK_VISUAL_STATIC_COLOR:
	  create_colormap (colormap, FALSE);

	  colormap->colors = g_new (GdkColor, colormap->size);
	  sync_colors (colormap);
	  break;

	case GDK_VISUAL_TRUE_COLOR:
	  break;

	default:
	  g_assert_not_reached ();
	}
    }

  return colormap;
}

gint
gdk_colormap_get_system_size (void)
{
  return gdk_colormap_get_system ()->size;
}

void
gdk_colormap_change (GdkColormap *colormap,
		     gint         ncolors)
{
  GdkColormapPrivateWin32 *cmapp;
  PALETTEENTRY *pe;
  int i;

  g_return_if_fail (GDK_IS_COLORMAP (colormap));

  cmapp = GDK_WIN32_COLORMAP_DATA (colormap);

  GDK_NOTE (COLORMAP, g_print ("gdk_colormap_change: hpal=%p ncolors=%d\n",
			       cmapp->hpal, ncolors));

  switch (colormap->visual->type)
    {
    case GDK_VISUAL_GRAYSCALE:
    case GDK_VISUAL_PSEUDO_COLOR:
      pe = g_new (PALETTEENTRY, ncolors);

      for (i = 0; i < ncolors; i++)
	{
	  pe[i].peRed = (colormap->colors[i].red >> 8);
	  pe[i].peGreen = (colormap->colors[i].green >> 8);
	  pe[i].peBlue = (colormap->colors[i].blue >> 8);
	  pe[i].peFlags = 0;
	}

      GDI_CALL (SetPaletteEntries, (cmapp->hpal, 0, ncolors, pe));
      g_free (pe);
      break;

    default:
      break;
    }
}

gboolean
gdk_colors_alloc (GdkColormap   *colormap,
		  gboolean       contiguous,
		  gulong        *planes,
		  gint           nplanes,
		  gulong        *pixels,
		  gint           npixels)
{
  GdkColormapPrivateWin32 *private;
  gint return_val;
  gint i;

  g_return_val_if_fail (GDK_IS_COLORMAP (colormap), FALSE);

  private = GDK_WIN32_COLORMAP_DATA (colormap);

  return_val = alloc_color_cells (colormap, contiguous,
				  planes, nplanes, pixels, npixels);

  if (return_val)
    {
      for (i = 0; i < npixels; i++)
	{
	  private->info[pixels[i]].ref_count++;
	  private->info[pixels[i]].flags |= GDK_COLOR_WRITEABLE;
	}
    }

  return return_val != 0;
}

void
gdk_colors_free (GdkColormap *colormap,
		 gulong      *in_pixels,
		 gint         in_npixels,
		 gulong       planes)
{
  GdkColormapPrivateWin32 *private;
  gulong *pixels;
  gint npixels = 0;
  gint i;

  g_return_if_fail (GDK_IS_COLORMAP (colormap));
  g_return_if_fail (in_pixels != NULL);

  private = GDK_WIN32_COLORMAP_DATA (colormap);

  if ((colormap->visual->type != GDK_VISUAL_PSEUDO_COLOR) &&
      (colormap->visual->type != GDK_VISUAL_GRAYSCALE))
    return;
  
  pixels = g_new (gulong, in_npixels);

  for (i = 0; i < in_npixels; i++)
    {
      gulong pixel = in_pixels[i];
      
      if (private->use[pixel] == GDK_WIN32_PE_STATIC)
	continue;

      if (private->info[pixel].ref_count)
	{
	  private->info[pixel].ref_count--;

	  if (private->info[pixel].ref_count == 0)
	    {
	      pixels[npixels++] = pixel;
	      if (!(private->info[pixel].flags & GDK_COLOR_WRITEABLE))
		g_hash_table_remove (private->hash, &colormap->colors[pixel]);
	      private->info[pixel].flags = 0;
	    }
	}
    }

  if (npixels)
    free_colors (colormap, pixels, npixels, planes);

  g_free (pixels);
}

void
gdk_colormap_free_colors (GdkColormap    *colormap,
			  const GdkColor *colors,
			  gint            ncolors)
{
  gulong *pixels;
  gint i;

  g_return_if_fail (GDK_IS_COLORMAP (colormap));
  g_return_if_fail (colors != NULL);

  if ((colormap->visual->type != GDK_VISUAL_PSEUDO_COLOR) &&
      (colormap->visual->type != GDK_VISUAL_GRAYSCALE))
    return;

  pixels = g_new (gulong, ncolors);

  for (i = 0; i < ncolors; i++)
    pixels[i] =  colors[i].pixel;

  gdk_colors_free (colormap, pixels, ncolors, 0);

  g_free (pixels);
}

/********************
 * Color allocation *
 ********************/

/* Try to allocate a single color using alloc_color. If it succeeds,
 * cache the result in our colormap, and store in ret.
 */
static gboolean 
gdk_colormap_alloc1 (GdkColormap *colormap,
		     GdkColor    *color,
		     GdkColor    *ret)
{
  GdkColormapPrivateWin32 *private;
  PALETTEENTRY pe;

  private = GDK_WIN32_COLORMAP_DATA (colormap);

  pe.peRed = color->red >> 8;
  pe.peGreen = color->green >> 8;
  pe.peBlue = color->blue >> 8;

  if (alloc_color (colormap, &pe, &ret->pixel))
    {
      ret->red = (pe.peRed * 65535) / 255;
      ret->green = (pe.peGreen * 65535) / 255;
      ret->blue = (pe.peBlue * 65535) / 255;
      
      if ((guint) ret->pixel < colormap->size)
	{
	  if (private->info[ret->pixel].ref_count) /* got a duplicate */
	    {
	    }
	  else
	    {
	      colormap->colors[ret->pixel] = *color;
	      colormap->colors[ret->pixel].pixel = ret->pixel;
	      private->info[ret->pixel].ref_count = 1;

	      g_hash_table_insert (private->hash,
				   &colormap->colors[ret->pixel],
				   &colormap->colors[ret->pixel]);
	    }
	}
      return TRUE;
    }
  return FALSE;
}

static gint
gdk_colormap_alloc_colors_writeable (GdkColormap *colormap,
				     GdkColor    *colors,
				     gint         ncolors,
				     gboolean     writeable,
				     gboolean     best_match,
				     gboolean    *success)
{
  GdkColormapPrivateWin32 *private;
  gulong *pixels;
  gboolean status;
  gint i, index;

  private = GDK_WIN32_COLORMAP_DATA (colormap);

  if (private->private_val)
    {
      index = 0;
      for (i=0; i<ncolors; i++)
	{
	  while ((index < colormap->size) &&
		 (private->info[index].ref_count != 0))
	    index++;
	  
	  if (index < colormap->size)
	    {
	      colors[i].pixel = index;
	      success[i] = TRUE;
	      private->info[index].ref_count++;
	      private->info[i].flags |= GDK_COLOR_WRITEABLE;
	    }
	  else
	    break;
	}
      return i;
    }
  else
    {
      pixels = g_new (gulong, ncolors);

      /* Allocation of a writeable color cells */
      status =  alloc_color_cells (colormap, FALSE, NULL, 0, pixels, ncolors);
      if (status)
	{
	  for (i = 0; i < ncolors; i++)
	    {
	      colors[i].pixel = pixels[i];
	      private->info[pixels[i]].ref_count++;
	      private->info[pixels[i]].flags |= GDK_COLOR_WRITEABLE;
	    }
	}
      
      g_free (pixels);

      return status ? ncolors : 0; 
    }
}

static gint
gdk_colormap_alloc_colors_private (GdkColormap *colormap,
				   GdkColor    *colors,
				   gint         ncolors,
				   gboolean     writeable,
				   gboolean     best_match,
				   gboolean    *success)
{
  GdkColormapPrivateWin32 *cmapp;
  gint i, index;
  PALETTEENTRY pe;
  gint nremaining = 0;
  
  cmapp = GDK_WIN32_COLORMAP_DATA (colormap);
  index = -1;

  /* First, store the colors we have room for */

  index = 0;
  for (i = 0; i < ncolors; i++)
    {
      if (!success[i])
	{
	  while ((index < colormap->size) &&
		 (cmapp->info[index].ref_count != 0))
	    index++;

	  if (index < colormap->size)
	    {
	      if (index >= cmapp->current_size)
		{
		  if (!ResizePalette (cmapp->hpal, index + 1))
		    {
		      WIN32_GDI_FAILED ("ResizePalette");
		      nremaining++;
		    }
		  else
		    cmapp->current_size = index + 1;
		}
	      if (index < cmapp->current_size)
		{
		  pe.peRed = colors[i].red >> 8;
		  pe.peBlue = colors[i].blue >> 8;
		  pe.peGreen = colors[i].green >> 8;
		  pe.peFlags = 0;
		  
		  if (!SetPaletteEntries (cmapp->hpal, index, 1, &pe))
		    {
		      WIN32_GDI_FAILED ("SetPaletteEntries");
		      nremaining++;
		    }
		  else
		    {
		      success[i] = TRUE;

		      colors[i].pixel = index;
		      colormap->colors[index] = colors[i];
		      cmapp->info[index].ref_count++;
		    }
		}
	    }
	  else
	    nremaining++;
	}
    }
  
  if (nremaining > 0 && best_match)
    {
      /* Get best matches for remaining colors */

      gchar *available = g_new (gchar, colormap->size);
      for (i = 0; i < colormap->size; i++)
	available[i] = TRUE;

      for (i=0; i<ncolors; i++)
	{
	  if (!success[i])
	    {
	      index = gdk_colormap_match_color (colormap, 
						&colors[i], 
						available);
	      if (index != -1)
		{
		  colors[i] = colormap->colors[index];
		  cmapp->info[index].ref_count++;

		  success[i] = TRUE;
		  nremaining--;
		}
	    }
	}
      g_free (available);
    }

  return (ncolors - nremaining);
}

static gint
gdk_colormap_alloc_colors_shared (GdkColormap *colormap,
				  GdkColor    *colors,
				  gint         ncolors,
				  gboolean     writeable,
				  gboolean     best_match,
				  gboolean    *success)
{
  GdkColormapPrivateWin32 *private;
  gint i, index;
  gint nremaining = 0;
  gint nfailed = 0;

  private = GDK_WIN32_COLORMAP_DATA (colormap);
  index = -1;

  for (i = 0; i < ncolors; i++)
    {
      if (!success[i])
	{
	  if (gdk_colormap_alloc1 (colormap, &colors[i], &colors[i]))
	    success[i] = TRUE;
	  else
	    nremaining++;
	}
    }


  if (nremaining > 0 && best_match)
    {
      gchar *available = g_new (gchar, colormap->size);
      for (i = 0; i < colormap->size; i++)
	available[i] = ((private->info[i].ref_count == 0) ||
			!(private->info[i].flags & GDK_COLOR_WRITEABLE));
      while (nremaining > 0)
	{
	  for (i = 0; i < ncolors; i++)
	    {
	      if (!success[i])
		{
		  index = gdk_colormap_match_color (colormap, &colors[i], available);
		  if (index != -1)
		    {
		      if (private->info[index].ref_count)
			{
			  private->info[index].ref_count++;
			  colors[i] = colormap->colors[index];
			  success[i] = TRUE;
			  nremaining--;
			}
		      else
			{
			  if (gdk_colormap_alloc1 (colormap, 
						   &colormap->colors[index],
						   &colors[i]))
			    {
			      success[i] = TRUE;
			      nremaining--;
			      break;
			    }
			  else
			    {
			      available[index] = FALSE;
			    }
			}
		    }
		  else
		    {
		      nfailed++;
		      nremaining--;
		      success[i] = 2; /* flag as permanent failure */
		    }
		}
	    }
	}
      g_free (available);
    }

  /* Change back the values we flagged as permanent failures */
  if (nfailed > 0)
    {
      for (i = 0; i < ncolors; i++)
	if (success[i] == 2)
	  success[i] = FALSE;
      nremaining = nfailed;
    }
  
  return (ncolors - nremaining);
}

static gint
gdk_colormap_alloc_colors_pseudocolor (GdkColormap *colormap,
				       GdkColor    *colors,
				       gint         ncolors,
				       gboolean     writeable,
				       gboolean     best_match,
				       gboolean    *success)
{
  GdkColormapPrivateWin32 *private;
  GdkColor *lookup_color;
  gint i;
  gint nremaining = 0;

  private = GDK_WIN32_COLORMAP_DATA (colormap);

  /* Check for an exact match among previously allocated colors */

  for (i = 0; i < ncolors; i++)
    {
      if (!success[i])
	{
	  lookup_color = g_hash_table_lookup (private->hash, &colors[i]);
	  if (lookup_color)
	    {
	      private->info[lookup_color->pixel].ref_count++;
	      colors[i].pixel = lookup_color->pixel;
	      success[i] = TRUE;
	    }
	  else
	    nremaining++;
	}
    }

  /* If that failed, we try to allocate a new color, or approxmiate
   * with what we can get if best_match is TRUE.
   */
  if (nremaining > 0)
    {
      if (private->private_val)
	return gdk_colormap_alloc_colors_private (colormap, colors, ncolors, writeable, best_match, success);
      else
	return gdk_colormap_alloc_colors_shared (colormap, colors, ncolors, writeable, best_match, success);
    }
  else
    return 0;
}

gint
gdk_colormap_alloc_colors (GdkColormap *colormap,
			   GdkColor    *colors,
			   gint         ncolors,
			   gboolean     writeable,
			   gboolean     best_match,
			   gboolean    *success)
{
  GdkColormapPrivateWin32 *private;
  GdkVisual *visual;
  gint i;
  gint nremaining = 0;
  PALETTEENTRY pe;

  g_return_val_if_fail (GDK_IS_COLORMAP (colormap), FALSE);
  g_return_val_if_fail (colors != NULL, FALSE);
  g_return_val_if_fail (success != NULL, ncolors);

  private = GDK_WIN32_COLORMAP_DATA (colormap);

  for (i = 0; i < ncolors; i++)
    success[i] = FALSE;

  switch (colormap->visual->type)
    {
    case GDK_VISUAL_PSEUDO_COLOR:
    case GDK_VISUAL_GRAYSCALE:
      if (writeable)
	return gdk_colormap_alloc_colors_writeable (colormap, colors, ncolors,
						    writeable, best_match, success);
      else
	return gdk_colormap_alloc_colors_pseudocolor (colormap, colors, ncolors,
						    writeable, best_match, success);
      break;

    case GDK_VISUAL_TRUE_COLOR:
      visual = colormap->visual;

      for (i = 0; i < ncolors; i++)
	{
	  colors[i].pixel =
	    (((colors[i].red >> (16 - visual->red_prec)) << visual->red_shift) +
	     ((colors[i].green >> (16 - visual->green_prec)) << visual->green_shift) +
	     ((colors[i].blue >> (16 - visual->blue_prec)) << visual->blue_shift));
	  success[i] = TRUE;
	}
      break;

    case GDK_VISUAL_STATIC_GRAY:
    case GDK_VISUAL_STATIC_COLOR:
      for (i = 0; i < ncolors; i++)
	{
	  pe.peRed = colors[i].red >> 8;
	  pe.peGreen = colors[i].green >> 8;
	  pe.peBlue = colors[i].blue >> 8;
	  if (alloc_color (colormap, &pe, &colors[i].pixel))
	    success[i] = TRUE;
	  else
	    nremaining++;
	}
      break;

    case GDK_VISUAL_DIRECT_COLOR:
      g_assert_not_reached ();
    }

  return nremaining;
}

void
gdk_colormap_query_color (GdkColormap *colormap,
			  gulong       pixel,
			  GdkColor    *result)
{
  GdkVisual *visual;

  g_return_if_fail (GDK_IS_COLORMAP (colormap));
  
  visual = gdk_colormap_get_visual (colormap);

  switch (visual->type)
    {
    case GDK_VISUAL_DIRECT_COLOR:
    case GDK_VISUAL_TRUE_COLOR:
      result->red = 65535. * (double)((pixel & visual->red_mask) >> visual->red_shift) / ((1 << visual->red_prec) - 1);
      result->green = 65535. * (double)((pixel & visual->green_mask) >> visual->green_shift) / ((1 << visual->green_prec) - 1);
      result->blue = 65535. * (double)((pixel & visual->blue_mask) >> visual->blue_shift) / ((1 << visual->blue_prec) - 1);
      break;

    case GDK_VISUAL_STATIC_GRAY:
    case GDK_VISUAL_GRAYSCALE:
      result->red = result->green = result->blue = 65535. * (double)pixel/((1<<visual->depth) - 1);
      break;

    case GDK_VISUAL_STATIC_COLOR:
    case GDK_VISUAL_PSEUDO_COLOR:
      result->red = colormap->colors[pixel].red;
      result->green = colormap->colors[pixel].green;
      result->blue = colormap->colors[pixel].blue;
      break;

    default:
      g_assert_not_reached ();
      break;
    }
}

gboolean
gdk_color_change (GdkColormap *colormap,
		  GdkColor    *color)
{
  GdkColormapPrivateWin32 *private;
  PALETTEENTRY pe;

  g_return_val_if_fail (GDK_IS_COLORMAP (colormap), FALSE);
  g_return_val_if_fail (color != NULL, FALSE);

  private = GDK_WIN32_COLORMAP_DATA (colormap);

  if (color->pixel < 0 || color->pixel >= colormap->size)
    return FALSE;

  if (private->use[color->pixel] == GDK_WIN32_PE_STATIC)
    return FALSE;

  pe.peRed = color->red >> 8;
  pe.peGreen = color->green >> 8;
  pe.peBlue = color->blue >> 8;

  GDI_CALL (SetPaletteEntries, (private->hpal, color->pixel, 1, &pe));

  return TRUE;
}

static gint
gdk_colormap_match_color (GdkColormap *cmap,
			  GdkColor    *color,
			  const gchar *available)
{
  GdkColor *colors;
  guint sum, min;
  gint rdiff, gdiff, bdiff;
  gint i, index;

  g_return_val_if_fail (cmap != NULL, 0);
  g_return_val_if_fail (color != NULL, 0);

  colors = cmap->colors;
  min = 3 * (65536);
  index = -1;

  for (i = 0; i < cmap->size; i++)
    {
      if ((!available) || (available && available[i]))
	{
	  rdiff = (color->red - colors[i].red);
	  gdiff = (color->green - colors[i].green);
	  bdiff = (color->blue - colors[i].blue);

	  sum = ABS (rdiff) + ABS (gdiff) + ABS (bdiff);

	  if (sum < min)
	    {
	      index = i;
	      min = sum;
	    }
	}
    }

  return index;
}

GdkScreen*
gdk_colormap_get_screen (GdkColormap *cmap)
{
  g_return_val_if_fail (GDK_IS_COLORMAP (cmap), NULL);

  return _gdk_screen;
}

