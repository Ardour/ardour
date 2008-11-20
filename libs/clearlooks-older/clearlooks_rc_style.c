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
 */

#include "clearlooks_style.h"
#include "clearlooks_rc_style.h"

static void      clearlooks_rc_style_init         (ClearlooksRcStyle      *style);
static void      clearlooks_rc_style_class_init   (ClearlooksRcStyleClass *klass);
static GtkStyle *clearlooks_rc_style_create_style (GtkRcStyle             *rc_style);
static guint     clearlooks_rc_style_parse        (GtkRcStyle             *rc_style,
						   GtkSettings            *settings,
						   GScanner               *scanner);
static void      clearlooks_rc_style_merge        (GtkRcStyle             *dest,
						   GtkRcStyle             *src);


static GtkRcStyleClass *parent_class;

GType clearlooks_type_rc_style = 0;

enum
{
  TOKEN_SPOTCOLOR = G_TOKEN_LAST + 1,
  TOKEN_CONTRAST,
  TOKEN_SUNKENMENU,
  TOKEN_PROGRESSBARSTYLE,
  TOKEN_MENUBARSTYLE,
  TOKEN_MENUITEMSTYLE,
  TOKEN_LISTVIEWITEMSTYLE
};

static struct
  {
    const gchar        *name;
    guint               token;
  }
theme_symbols[] =
{
  { "spotcolor", 		TOKEN_SPOTCOLOR  },
  { "contrast", 		TOKEN_CONTRAST  },
  { "sunkenmenubar",    TOKEN_SUNKENMENU },
  { "progressbarstyle",  TOKEN_PROGRESSBARSTYLE },
  { "menubarstyle",     TOKEN_MENUBARSTYLE },
  { "menuitemstyle",    TOKEN_MENUITEMSTYLE },
  { "listviewitemstyle", TOKEN_LISTVIEWITEMSTYLE }
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
  clearlooks_rc->has_spot_color = FALSE;
  clearlooks_rc->contrast = 1.0;
  clearlooks_rc->sunkenmenubar = 1;
  clearlooks_rc->progressbarstyle = 0;
  clearlooks_rc->menubarstyle = 0;
  clearlooks_rc->menuitemstyle = 1;
  clearlooks_rc->listviewitemstyle = 1;
}

static void
clearlooks_rc_style_class_init (ClearlooksRcStyleClass *klass)
{
  GtkRcStyleClass *rc_style_class = GTK_RC_STYLE_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  rc_style_class->parse = clearlooks_rc_style_parse;
  rc_style_class->create_style = clearlooks_rc_style_create_style;
  rc_style_class->merge = clearlooks_rc_style_merge;
}

static guint
theme_parse_color(GtkSettings  *settings,
		  GScanner     *scanner,
		  GdkColor     *color)
{
  guint token;

  /* Skip 'blah_color' */
  token = g_scanner_get_next_token(scanner);

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  return gtk_rc_parse_color (scanner, color);
}

static guint
theme_parse_contrast(GtkSettings  *settings,
		     GScanner     *scanner,
		     double       *contrast)
{
  guint token;

  /* Skip 'contrast' */
  token = g_scanner_get_next_token(scanner);

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_FLOAT)
    return G_TOKEN_FLOAT;

  *contrast = scanner->value.v_float;
  
  return G_TOKEN_NONE;
}

static guint
theme_parse_sunkenmenubar(GtkSettings  *settings,
		     GScanner     *scanner,
		     guint8       *sunken)
{
  guint token;

  /* Skip 'sunkenmenubar' */
  token = g_scanner_get_next_token(scanner);

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_INT)
    return G_TOKEN_INT;

  *sunken = scanner->value.v_int;
  
  return G_TOKEN_NONE;
}

static guint
theme_parse_progressbarstyle(GtkSettings  *settings,
		     GScanner     *scanner,
		     guint8       *progressbarstyle)
{
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
theme_parse_menubarstyle(GtkSettings  *settings,
		     GScanner     *scanner,
		     guint8       *menubarstyle)
{
  guint token;

  /* Skip 'menubarstyle' */
  token = g_scanner_get_next_token(scanner);

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_INT)
    return G_TOKEN_INT;

  *menubarstyle = scanner->value.v_int;
  
  return G_TOKEN_NONE;
}

static guint
theme_parse_menuitemstyle(GtkSettings  *settings,
		     GScanner     *scanner,
		     guint8       *menuitemstyle)
{
  guint token;

  /* Skip 'sunkenmenubar' */
  token = g_scanner_get_next_token(scanner);

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_INT)
    return G_TOKEN_INT;

  *menuitemstyle = scanner->value.v_int;
  
  return G_TOKEN_NONE;
}

static guint
theme_parse_listviewitemstyle(GtkSettings  *settings,
                          GScanner     *scanner,
                          guint8       *listviewitemstyle)
{         
  guint token;
	            
  token = g_scanner_get_next_token(scanner);
	              
  token = g_scanner_get_next_token(scanner);

  if (token != G_TOKEN_EQUAL_SIGN)
     return G_TOKEN_EQUAL_SIGN;
		          
  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_INT)
    return G_TOKEN_INT;
		            
  *listviewitemstyle = scanner->value.v_int;
	                
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

  if (!g_scanner_lookup_symbol(scanner, theme_symbols[0].name))
    {
      g_scanner_freeze_symbol_table(scanner);
      for (i = 0; i < G_N_ELEMENTS (theme_symbols); i++)
	g_scanner_scope_add_symbol(scanner, scope_id,
				   theme_symbols[i].name,
				   GINT_TO_POINTER(theme_symbols[i].token));
      g_scanner_thaw_symbol_table(scanner);
    }

  /* We're ready to go, now parse the top level */

  token = g_scanner_peek_next_token(scanner);
  while (token != G_TOKEN_RIGHT_CURLY)
    {
      switch (token)
	{
	case TOKEN_SPOTCOLOR:
	  token = theme_parse_color(settings, scanner, &clearlooks_style->spot_color);
	  clearlooks_style->has_spot_color = TRUE;
	  break;
	case TOKEN_CONTRAST:
	  token = theme_parse_contrast(settings, scanner, &clearlooks_style->contrast);
	  break;
	case TOKEN_SUNKENMENU:
	  token = theme_parse_sunkenmenubar(settings, scanner, &clearlooks_style->sunkenmenubar);
	  break;
	case TOKEN_PROGRESSBARSTYLE:
	  token = theme_parse_progressbarstyle(settings, scanner, &clearlooks_style->progressbarstyle);
	  break;
	case TOKEN_MENUBARSTYLE:
	  token = theme_parse_menubarstyle(settings, scanner, &clearlooks_style->menubarstyle);
	  break;
	case TOKEN_MENUITEMSTYLE:
	  token = theme_parse_menuitemstyle(settings, scanner, &clearlooks_style->menuitemstyle);
	  break;
	case TOKEN_LISTVIEWITEMSTYLE:
	  token = theme_parse_listviewitemstyle(settings, scanner, &clearlooks_style->listviewitemstyle);
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
	
	parent_class->merge (dest, src);
	
	if (!CLEARLOOKS_IS_RC_STYLE (src))
		return;
	
	src_w = CLEARLOOKS_RC_STYLE (src);
	dest_w = CLEARLOOKS_RC_STYLE (dest);
	
	dest_w->contrast = src_w->contrast;
	dest_w->sunkenmenubar = src_w->sunkenmenubar;
	dest_w->progressbarstyle = src_w->progressbarstyle;
	dest_w->menubarstyle = src_w->menubarstyle;
	dest_w->menuitemstyle = src_w->menuitemstyle;
	dest_w->listviewitemstyle = src_w->listviewitemstyle;

	if (src_w->has_spot_color)
	{
		dest_w->has_spot_color = TRUE;
		dest_w->spot_color = src_w->spot_color;
	}
}


/* Create an empty style suitable to this RC style
 */
static GtkStyle *
clearlooks_rc_style_create_style (GtkRcStyle *rc_style)
{
  return GTK_STYLE (g_object_new (CLEARLOOKS_TYPE_STYLE, NULL));
}
