/* Clearlooks theme engine
 * Copyright (C) 2005 Richard Stellingwerff.
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
 *
 * Written by Owen Taylor <otaylor@redhat.com>
 * and by Alexander Larsson <alexl@redhat.com>
 * Modified by Richard Stellingwerff <remenic@gmail.com>
 * Modified by Kulyk Nazar <schamane@myeburg.net>
 */

#include "clearlooks_style.h"
#include "clearlooks_rc_style.h"

#include "animation.h"

static void      clearlooks_rc_style_init         (ClearlooksRcStyle      *style);
#ifdef HAVE_ANIMATION
static void      clearlooks_rc_style_finalize     (GObject                *object);
#endif
static void      clearlooks_rc_style_class_init   (ClearlooksRcStyleClass *klass);
static GtkStyle *clearlooks_rc_style_create_style (GtkRcStyle             *rc_style);
static guint     clearlooks_rc_style_parse        (GtkRcStyle             *rc_style,
						   GtkSettings            *settings,
						   GScanner               *scanner);
static void      clearlooks_rc_style_merge        (GtkRcStyle             *dest,
						   GtkRcStyle             *src);


static GtkRcStyleClass *clearlooks_parent_rc_class;

GType clearlooks_type_rc_style = 0;

enum
{
	TOKEN_SCROLLBARCOLOR = G_TOKEN_LAST + 1,
	TOKEN_COLORIZESCROLLBAR,
	TOKEN_CONTRAST,
	TOKEN_SUNKENMENU,
	TOKEN_PROGRESSBARSTYLE,
	TOKEN_MENUBARSTYLE,
	TOKEN_TOOLBARSTYLE,
	TOKEN_MENUITEMSTYLE,
	TOKEN_LISTVIEWITEMSTYLE,
	TOKEN_ANIMATION,
	TOKEN_STYLE,
	TOKEN_RADIUS,

	TOKEN_CLASSIC,
	TOKEN_GLOSSY,
	TOKEN_INVERTED,
	TOKEN_GUMMY,

	TOKEN_TRUE,
	TOKEN_FALSE
};

static struct
{
	const gchar        *name;
	guint               token;
}
clearlooks_gtk2_rc_symbols[] =
{
	{ "scrollbar_color",    TOKEN_SCROLLBARCOLOR  },
	{ "colorize_scrollbar", TOKEN_COLORIZESCROLLBAR },
	{ "contrast",           TOKEN_CONTRAST  },
	{ "sunkenmenubar",      TOKEN_SUNKENMENU },
	{ "progressbarstyle",   TOKEN_PROGRESSBARSTYLE },
	{ "menubarstyle",       TOKEN_MENUBARSTYLE }, 
	{ "toolbarstyle",       TOKEN_TOOLBARSTYLE },
	{ "menuitemstyle",      TOKEN_MENUITEMSTYLE },
	{ "listviewitemstyle",  TOKEN_LISTVIEWITEMSTYLE },
	{ "animation",          TOKEN_ANIMATION },
	{ "style",              TOKEN_STYLE },
	{ "radius",             TOKEN_RADIUS },

	{ "CLASSIC",            TOKEN_CLASSIC },
	{ "GLOSSY",             TOKEN_GLOSSY },
	{ "INVERTED",           TOKEN_INVERTED },
	{ "GUMMY",              TOKEN_GUMMY },

	{ "TRUE",               TOKEN_TRUE },
	{ "FALSE",              TOKEN_FALSE }
};


void
clearlooks_rc_style_register_type (GTypeModule *module)
{
	static const GTypeInfo object_info =
	{
		sizeof (ClearlooksRcStyleClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) clearlooks_rc_style_class_init,
		NULL,           /* class_finalize */
		NULL,           /* class_data */
		sizeof (ClearlooksRcStyle),
		0,              /* n_preallocs */
		(GInstanceInitFunc) clearlooks_rc_style_init,
		NULL
	};

	clearlooks_type_rc_style = g_type_module_register_type (module,
						     GTK_TYPE_RC_STYLE,
						     "ClearlooksRcStyle",
						     &object_info, 0);
}

static void
clearlooks_rc_style_init (ClearlooksRcStyle *clearlooks_rc)
{
	clearlooks_rc->style = CL_STYLE_CLASSIC;

	clearlooks_rc->flags = 0;

	clearlooks_rc->contrast = 1.0;
	clearlooks_rc->menubarstyle = 0;
	clearlooks_rc->toolbarstyle = 0;
	clearlooks_rc->animation = FALSE;
	clearlooks_rc->colorize_scrollbar = FALSE;
	clearlooks_rc->radius = 3.0;
}

#ifdef HAVE_ANIMATION
static void
clearlooks_rc_style_finalize (GObject *object)
{
	/* cleanup all the animation stuff */
	clearlooks_animation_cleanup ();

	if (G_OBJECT_CLASS (clearlooks_parent_rc_class)->finalize != NULL)
		G_OBJECT_CLASS (clearlooks_parent_rc_class)->finalize(object);
}
#endif


static void
clearlooks_rc_style_class_init (ClearlooksRcStyleClass *klass)
{
	GtkRcStyleClass *rc_style_class = GTK_RC_STYLE_CLASS (klass);
#ifdef HAVE_ANIMATION
	GObjectClass    *g_object_class = G_OBJECT_CLASS (klass);
#endif

	clearlooks_parent_rc_class = g_type_class_peek_parent (klass);

	rc_style_class->parse = clearlooks_rc_style_parse;
	rc_style_class->create_style = clearlooks_rc_style_create_style;
	rc_style_class->merge = clearlooks_rc_style_merge;

#ifdef HAVE_ANIMATION
	g_object_class->finalize = clearlooks_rc_style_finalize;
#endif
}

static guint
clearlooks_gtk2_rc_parse_boolean (GtkSettings *settings,
                     GScanner     *scanner,
                     gboolean *retval)
{
	(void) settings;
	
	guint token;
	token = g_scanner_get_next_token(scanner);

	token = g_scanner_get_next_token(scanner);
	if (token != G_TOKEN_EQUAL_SIGN)
	   return G_TOKEN_EQUAL_SIGN;

	token = g_scanner_get_next_token(scanner);
	if (token == TOKEN_TRUE)
	   *retval = TRUE;
	else if (token == TOKEN_FALSE)
	   *retval = FALSE;
	else
	   return TOKEN_TRUE;

	return G_TOKEN_NONE;
}

static guint
clearlooks_gtk2_rc_parse_color(GtkSettings  *settings,
		  GScanner     *scanner,
		  GdkColor     *color)
{
	(void) settings;
	
	guint token;

	/* Skip 'blah_color' */
	token = g_scanner_get_next_token(scanner);

	token = g_scanner_get_next_token(scanner);
	if (token != G_TOKEN_EQUAL_SIGN)
	   return G_TOKEN_EQUAL_SIGN;

	return gtk_rc_parse_color (scanner, color);
}

static guint
clearlooks_gtk2_rc_parse_double (GtkSettings  *settings,
                                 GScanner     *scanner,
                                 gdouble      *val)
{
	(void) settings;
	
	guint token;

	/* Skip 'blah' */
	token = g_scanner_get_next_token(scanner);

	token = g_scanner_get_next_token(scanner);
	if (token != G_TOKEN_EQUAL_SIGN)
	   return G_TOKEN_EQUAL_SIGN;

	token = g_scanner_get_next_token(scanner);
	if (token != G_TOKEN_FLOAT)
	   return G_TOKEN_FLOAT;

	*val = scanner->value.v_float;

	return G_TOKEN_NONE;
}

static guint
clearlooks_gtk2_rc_parse_int (GtkSettings  *settings,
		         GScanner     *scanner,
		         guint8       *progressbarstyle)
{
	(void) settings;

	guint token;

	/* Skip 'sunkenmenubar' */
	token = g_scanner_get_next_token(scanner);

	token = g_scanner_get_next_token(scanner);
	if (token != G_TOKEN_EQUAL_SIGN)
	   return G_TOKEN_EQUAL_SIGN;

	token = g_scanner_get_next_token(scanner);
	if (token != G_TOKEN_INT)
	   return G_TOKEN_INT;

	*progressbarstyle = scanner->value.v_int;

	return G_TOKEN_NONE;
}

static guint
clearlooks_gtk2_rc_parse_style (GtkSettings      *settings,
                                GScanner         *scanner,
                                ClearlooksStyles *style)
{
	(void) settings;

	guint token;

	g_assert (CL_NUM_STYLES == CL_STYLE_GUMMY + 1); /* so that people don't forget ;-) */

	/* Skip 'style' */
	token = g_scanner_get_next_token (scanner);

	token = g_scanner_get_next_token (scanner);
	if (token != G_TOKEN_EQUAL_SIGN)
	   return G_TOKEN_EQUAL_SIGN;

	token = g_scanner_get_next_token (scanner);
  
	switch (token)
	{
		case TOKEN_CLASSIC:
		   *style = CL_STYLE_CLASSIC;
		   break;
		case TOKEN_GLOSSY:
		   *style = CL_STYLE_GLOSSY;
		   break;
		case TOKEN_INVERTED:
		   *style = CL_STYLE_INVERTED;
		   break;
		case TOKEN_GUMMY:
		   *style = CL_STYLE_GUMMY;
		   break;
		default:
		   return TOKEN_CLASSIC;
	}

	return G_TOKEN_NONE;
}

static guint
clearlooks_gtk2_rc_parse_dummy (GtkSettings      *settings,
                                GScanner         *scanner,
                                const gchar      *name)
{
	(void) settings;

	guint token;

	/* Skip option */
	token = g_scanner_get_next_token (scanner);

	/* print a warning. Isn't there a way to get the string from the scanner? */
	g_scanner_warn (scanner, "Clearlooks configuration option \"%s\" is not supported and will be ignored.", name);

	/* equal sign */
	token = g_scanner_get_next_token (scanner);
	if (token != G_TOKEN_EQUAL_SIGN)
	   return G_TOKEN_EQUAL_SIGN;

	/* eat whatever comes next */
	token = g_scanner_get_next_token (scanner);

	return G_TOKEN_NONE;
}

static guint
clearlooks_rc_style_parse (GtkRcStyle *rc_style,
			   GtkSettings  *settings,
			   GScanner   *scanner)
		     
{
	static GQuark scope_id = 0;
	ClearlooksRcStyle *clearlooks_style = CLEARLOOKS_RC_STYLE (rc_style);

	guint old_scope;
	guint token;
	guint i;

	/* Set up a new scope in this scanner. */

	if (!scope_id)
	   scope_id = g_quark_from_string("clearlooks_theme_engine");

	/* If we bail out due to errors, we *don't* reset the scope, so the
	* error messaging code can make sense of our tokens.
	*/
	old_scope = g_scanner_set_scope(scanner, scope_id);

	/* Now check if we already added our symbols to this scope
	* (in some previous call to clearlooks_rc_style_parse for the
	* same scanner.
	*/

	if (!g_scanner_lookup_symbol(scanner, clearlooks_gtk2_rc_symbols[0].name))
	{
		for (i = 0; i < G_N_ELEMENTS (clearlooks_gtk2_rc_symbols); i++)
			g_scanner_scope_add_symbol(scanner, scope_id,
				   	clearlooks_gtk2_rc_symbols[i].name,
				   	GINT_TO_POINTER(clearlooks_gtk2_rc_symbols[i].token));
	}

	/* We're ready to go, now parse the top level */

	token = g_scanner_peek_next_token(scanner);
	while (token != G_TOKEN_RIGHT_CURLY)
	{
		switch (token)
		{
			case TOKEN_SCROLLBARCOLOR:
				token = clearlooks_gtk2_rc_parse_color (settings, scanner, &clearlooks_style->scrollbar_color);
				clearlooks_style->flags |= CL_FLAG_SCROLLBAR_COLOR;
				break;
			case TOKEN_COLORIZESCROLLBAR:
				token = clearlooks_gtk2_rc_parse_boolean (settings, scanner, &clearlooks_style->colorize_scrollbar);
				clearlooks_style->flags |= CL_FLAG_COLORIZE_SCROLLBAR;
				break;
			case TOKEN_CONTRAST:
				token = clearlooks_gtk2_rc_parse_double (settings, scanner, &clearlooks_style->contrast);
				clearlooks_style->flags |= CL_FLAG_CONTRAST;
				break;
			case TOKEN_MENUBARSTYLE:
				token = clearlooks_gtk2_rc_parse_int (settings, scanner, &clearlooks_style->menubarstyle);
				clearlooks_style->flags |= CL_FLAG_MENUBARSTYLE;
				break;
			case TOKEN_TOOLBARSTYLE:
				token = clearlooks_gtk2_rc_parse_int (settings, scanner, &clearlooks_style->toolbarstyle);
				clearlooks_style->flags |= CL_FLAG_TOOLBARSTYLE;
				break;
			case TOKEN_ANIMATION:
				token = clearlooks_gtk2_rc_parse_boolean (settings, scanner, &clearlooks_style->animation);
				clearlooks_style->flags |= CL_FLAG_ANIMATION;
				break;
			case TOKEN_STYLE:
				token = clearlooks_gtk2_rc_parse_style (settings, scanner, &clearlooks_style->style);
				clearlooks_style->flags |= CL_FLAG_STYLE;
				break;
			case TOKEN_RADIUS:
				token = clearlooks_gtk2_rc_parse_double (settings, scanner, &clearlooks_style->radius);
				clearlooks_style->flags |= CL_FLAG_RADIUS;
				break;

			/* stuff to ignore */
			case TOKEN_SUNKENMENU:
				token = clearlooks_gtk2_rc_parse_dummy (settings, scanner, "sunkenmenu");
				break;
			case TOKEN_PROGRESSBARSTYLE:
				token = clearlooks_gtk2_rc_parse_dummy (settings, scanner, "progressbarstyle");
				break;
			case TOKEN_MENUITEMSTYLE:
				token = clearlooks_gtk2_rc_parse_dummy (settings, scanner, "menuitemstyle");
				break;
			case TOKEN_LISTVIEWITEMSTYLE:
				token = clearlooks_gtk2_rc_parse_dummy (settings, scanner, "listviewitemstyle");
				break;

			default:
				g_scanner_get_next_token(scanner);
				token = G_TOKEN_RIGHT_CURLY;
				break;
		}

		if (token != G_TOKEN_NONE)
			return token;

		token = g_scanner_peek_next_token(scanner);
	}

	g_scanner_get_next_token(scanner);

	g_scanner_set_scope(scanner, old_scope);

	return G_TOKEN_NONE;
}

static void
clearlooks_rc_style_merge (GtkRcStyle *dest,
			   GtkRcStyle *src)
{
	ClearlooksRcStyle *dest_w, *src_w;
	ClearlooksRcFlags flags;

	clearlooks_parent_rc_class->merge (dest, src);

	if (!CLEARLOOKS_IS_RC_STYLE (src))
		return;

	src_w = CLEARLOOKS_RC_STYLE (src);
	dest_w = CLEARLOOKS_RC_STYLE (dest);

	flags = (~dest_w->flags) & src_w->flags;

	if (flags & CL_FLAG_STYLE)
		dest_w->style = src_w->style;
	if (flags & CL_FLAG_CONTRAST)
		dest_w->contrast = src_w->contrast;
	if (flags & CL_FLAG_MENUBARSTYLE)
		dest_w->menubarstyle = src_w->menubarstyle;
	if (flags & CL_FLAG_TOOLBARSTYLE)
		dest_w->toolbarstyle = src_w->toolbarstyle;
	if (flags & CL_FLAG_SCROLLBAR_COLOR)
		dest_w->scrollbar_color = src_w->scrollbar_color;
	if (flags & CL_FLAG_COLORIZE_SCROLLBAR)
		dest_w->colorize_scrollbar = src_w->colorize_scrollbar;
	if (flags & CL_FLAG_ANIMATION)
		dest_w->animation = src_w->animation;
	if (flags & CL_FLAG_RADIUS)
		dest_w->radius = src_w->radius;

	dest_w->flags |= src_w->flags;
}


/* Create an empty style suitable to this RC style
 */
static GtkStyle *
clearlooks_rc_style_create_style (GtkRcStyle *rc_style)
{
	(void) rc_style;
	
	return GTK_STYLE (g_object_new (CLEARLOOKS_TYPE_STYLE, NULL));
}
