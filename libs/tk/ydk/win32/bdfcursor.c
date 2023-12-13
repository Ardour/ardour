/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2001 Stefan Ondrejicka
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
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <stdlib.h>

#include <glib.h>

typedef struct {
	gchar	*name;
	gint	id;
	gchar	*bitmap;
	gint	hotx;
	gint	hoty;
} font_info_t;

typedef struct {
	gchar	*name;
	gint	id;
	gint	width;
	gint	height;
	gint	hotx;
	gint	hoty;
	gchar	*data;
} cursor_info_t;

static GSList *fonts = NULL;
static GSList *cursors = NULL;

static gint dw,dh;

static gboolean debug = FALSE;

#define HEX(c) (((c) >= '0' && (c) <= '9') ? \
	((c) - '0') : (toupper(c) - 'A' + 10))

static void print_font(fi)
font_info_t *fi;
{
	int x,y;

	for (y = 0; y < dh; y++)
	{
		for (x = 0; x < dw; x++)
		{
			printf(fi->bitmap[y*dw+x]? "X" : " ");
		}
		printf("\n");
	}
}

static void print_cursor(ci)
cursor_info_t *ci;
{
	int x,y;

	for (y = 0; y < ci->height; y++)
	{
		printf("/* ");
		for (x = 0; x < ci->width; x++)
		{
			if (ci->hotx == x && ci->hoty == y)
				printf("o");
			else
				switch (ci->data[y*ci->width+x])
				{
					case 0:
						printf(" ");
					break;
					case 1:
						printf(".");
					break;
					case 2:
						printf("X");
					break;
				}
		}
		printf(" */\n");
	}
}

static gint read_bdf_font(fname)
gchar *fname;
{
	FILE *f;
	gchar line[2048];
	gint rv = 0;
	gboolean startchar = FALSE, startbitmap = FALSE;
	gchar *charname,*p,*bitmap;
	gint dx = 0,dy = 0;
	gint w,h,x,y,py;
	gint id,tmp;

	dw = 0;
	dh = 0;

	if (!(f = fopen(fname, "r")))
	{
		perror(fname);
		return -1;
	}

	if (fgets(line, sizeof(line), f) && strncasecmp("STARTFONT ", line, 10))
	{
		printf("!BDF font file\n");
		fclose(f);
		return -1;
	}
	
	p = line;
	while (fgets(line, sizeof(line), f))
	{
		if (!startchar)
		{
			if (!strncasecmp("STARTCHAR ", line, 10))
			{
				startchar = TRUE;
				charname = g_strndup(p + 10,
					strcspn(p+10, "\r\n"));
			}
			else if (!strncasecmp("FONTBOUNDINGBOX ", line, 16))
				sscanf(p+16, "%d %d %d %d", &dw, &dh, &dx, &dy);
		}
		else
		{
			if (!strncasecmp("ENDCHAR", line, 7))
			{
				font_info_t *nfi;

				if (debug)
					printf(" %*s*/\n", dw, "");
				startchar = FALSE;
				startbitmap = FALSE;

				nfi = g_malloc(sizeof(font_info_t));
				memset(nfi, '\0', sizeof(font_info_t));

				nfi->name = charname;
				nfi->id = id;
				nfi->bitmap = bitmap;
				nfi->hotx = 0 - dx;
				nfi->hoty = 0 - dy;

				fonts = g_slist_append(fonts, nfi);
			}
			else if (startbitmap)
			{
				int px,cx;
				guchar mask;

				px = x - dx + py * dw;
				for (cx = 0; cx < w; cx++)
				{
					mask = 1 << (3 - (cx % 4));

					bitmap[px+cx] =
						(mask & HEX(line[cx/4])) != 0;

					if (debug)
						printf(bitmap[px+cx] ? "X" : " ");
				}
				py++;
				if (debug)
					printf(" %*s*/\n/* %*s", dw-w, "", dw+dx, "");
			}
			else if (!strncasecmp("BBX ", line, 4))
			{
				sscanf(p+4, "%d %d %d %d", &w, &h, &x, &y);
				if (debug)
					printf("/* %s: */\n/* %*s", charname, dw+dx, "");
			}
			else if (!strncasecmp("ENCODING ", line, 9))
			{
				if (sscanf(p+9, "%d %d", &tmp, &id) != 2)
					id = tmp;
			}
			else if (!strncasecmp("BITMAP", line, 6))
			{
				py = y - dy;
				startbitmap = TRUE;
				bitmap = g_malloc(dw*dh);
				memset(bitmap, '\0', dw*dh);
			}
		}
	}

	if (strncasecmp("ENDFONT", line, 7))
		rv = -1;

	fclose(f);

	return rv;
}

static gint font_info_compare(fi, name)
font_info_t *fi;
char *name;
{
	return strcmp(name, fi->name);
}

static cursor_info_t *gen_cursor(bmap, mask)
font_info_t *bmap;
font_info_t *mask;
{
	cursor_info_t *ci;
	int bx = dw,by = dh,ex = 0,ey = 0;
	int i,j;

	for (j = 0; j < dh; j++)
	{
		gboolean havep = FALSE;

		for (i = 0; i < dw; i++)
		{
			if (bmap->bitmap[j*dw+i] || mask->bitmap[j*dw+i])
			{
				havep = TRUE;
				bx = MIN(bx, i);
				ex = MAX(i+1, ex);
			}
		}

		if (havep)
		{
			by = MIN(by, j);
			ey = MAX(ey, j+1);
		}
	}

	ci = g_malloc(sizeof(cursor_info_t));
	ci->name = g_strdup(bmap->name);
	ci->id = bmap->id;

	ci->width = ex - bx;
	ci->height = ey - by;

	ci->hotx = bmap->hotx - bx;
	ci->hoty = ci->height - (bmap->hoty - by);

	ci->data = g_malloc(ci->width * ci->height);
	memset(ci->data, '\0', ci->width * ci->height);

	for (j = 0; j < ci->height; j++)
	{
		for (i = 0; i < ci->width; i++)
		{
			int ofs = (by + j) * dw + bx + i;

			ci->data[j*ci->width + i] = mask->bitmap[ofs] *
				(1 + bmap->bitmap[ofs]);
		}
	}

	return ci;
}

static void compose_cursors_from_fonts()
{
	GSList *l;

	for (l = g_slist_copy (fonts); l; l = g_slist_delete_link (l,l))
	{
		font_info_t *fi = l->data;
		gchar *name;
		GSList *ml;

		name = g_strconcat(fi->name, "_mask", NULL);

		if ((ml = g_slist_find_custom(fonts, name,
			(GCompareFunc) font_info_compare)))
		{
			cursors = g_slist_append(cursors, gen_cursor(l->data, ml->data));
			fonts = g_slist_remove(fonts, l->data);
			fonts = g_slist_remove(fonts, ml->data);
		}

		g_free(name);
	}
}

static char *dump_cursor(ci, id)
cursor_info_t *ci;
int id;
{
	static gchar cdata[8192];
	gchar *p;
	gint i;
	gint c;
	gboolean flushed;

	sprintf(cdata, "  { \"%s\", %d, %d, %d, %d, %d, \n    \"",
		ci->name, ci->id, ci->width, ci->height, ci->hotx, ci->hoty);
	p = cdata + strlen(cdata);

	for (i = 0; i < ci->width * ci->height; i++)
	{
		flushed = FALSE;

		if (!(i%4))
			c = 0;

		c = c << 2;

		c += ci->data[i];

		if ((i % 4) == 3)
		{
			flushed = TRUE;
			sprintf(p, "\\%03o", c);
			p += strlen(p);
		}

		if (i > 0 && !(i % 64))
		{
			strcpy(p ,"\"\n    \"");
			p += strlen(p);
		}
	}
	if (!flushed)
	{
		sprintf(p, "\\%03o", c);
		p += strlen(p);
	}

	strcpy(p, "\" }");

	return cdata;
}

static int dump_cursors()
{
	GSList *ptr;
	FILE *f = stdout;

	fprintf(f, "static const struct { const gchar *name; gint type; guchar width; guchar height; guchar hotx; guchar hoty; guchar *data; } cursors[] = {\n");

	for (ptr = cursors; ptr; ptr = ptr->next)
	{
		if (debug)
			print_cursor(ptr->data);
		fprintf(f, "%s, \n", dump_cursor(ptr->data));
	}

	fprintf(f, "  { NULL, 0, 0, 0, 0, 0, NULL },\n};\n");

	return 0;
}

gint main(argc, argv)
gint argc;
gchar **argv;
{
	if (argc != 2)
	{
		printf("missing parameters !\n");
		printf("Usage: %s [BDF cursor file]\n", argv[0]);
		return -1;
	}

	if (g_getenv ("BDFCURSOR_DEBUG") != NULL)
	  debug = TRUE;

	if (read_bdf_font(argv[1]) || !fonts)
	{
		printf("Error reading font\n");
		return 1;
	}

	compose_cursors_from_fonts();

	if (!cursors)
	{
		printf("failed to generate cursors from font!\n");
		return 1;
	}

	dump_cursors();

	if (fonts)
	{
		printf("some fonts remained unconverted!\n");
		return 1;
	}

	return 0;
}

