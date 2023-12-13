/* Copyright (C) 1991, 1992, 1993 Free Software Foundation, Inc.
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

/*
 * Stripped down, converted to UTF-8 and test cases added
 *
 *                    Owen Taylor, 13 December 2002;
 */

#include "config.h"
#include <string.h>

#include <glib.h>

/* We need to make sure that all constants are defined
 * to properly compile this file
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

static gunichar
get_char (const char **str)
{
  gunichar c = g_utf8_get_char (*str);
  *str = g_utf8_next_char (*str);

#ifdef G_PLATFORM_WIN32
  c = g_unichar_tolower (c);
#endif

  return c;
}

#if defined(G_OS_WIN32) || defined(G_WITH_CYGWIN)
#define DO_ESCAPE 0
#else  
#define DO_ESCAPE 1
#endif  

static gunichar
get_unescaped_char (const char **str,
		    gboolean    *was_escaped)
{
  gunichar c = get_char (str);

  *was_escaped = DO_ESCAPE && c == '\\';
  if (*was_escaped)
    c = get_char (str);
  
  return c;
}

/* Match STRING against the filename pattern PATTERN, returning zero if
   it matches, nonzero if not.  */

static gboolean
gtk_fnmatch_intern (const char *pattern,
		    const char *string,
		    gboolean    component_start,
		    gboolean    no_leading_period)
{
  const char *p = pattern, *n = string;
  
  while (*p)
    {
      const char *last_n = n;
      
      gunichar c = get_char (&p);
      gunichar nc = get_char (&n);
      
      switch (c)
	{
   	case '?':
	  if (nc == '\0')
	    return FALSE;
	  else if (nc == G_DIR_SEPARATOR)
	    return FALSE;
	  else if (nc == '.' && component_start && no_leading_period)
	    return FALSE;
	  break;
	case '\\':
	  if (DO_ESCAPE)
	    c = get_char (&p);
	  if (nc != c)
	    return FALSE;
	  break;
	case '*':
	  if (nc == '.' && component_start && no_leading_period)
	    return FALSE;

	  {
	    const char *last_p = p;

	    for (last_p = p, c = get_char (&p);
		 c == '?' || c == '*';
		 last_p = p, c = get_char (&p))
	      {
		if (c == '?')
		  {
		    if (nc == '\0')
		      return FALSE;
		    else if (nc == G_DIR_SEPARATOR)
		      return FALSE;
		    else
		      {
			last_n = n; nc = get_char (&n);
		      }
		  }
	      }

	    /* If the pattern ends with wildcards, we have a
	     * guaranteed match unless there is a dir separator
	     * in the remainder of the string.
	     */
	    if (c == '\0')
	      {
		if (strchr (last_n, G_DIR_SEPARATOR) != NULL)
		  return FALSE;
		else
		  return TRUE;
	      }

	    if (DO_ESCAPE && c == '\\')
	      c = get_char (&p);

	    for (p = last_p; nc != '\0';)
	      {
		if ((c == '[' || nc == c) &&
		    gtk_fnmatch_intern (p, last_n, component_start, no_leading_period))
		  return TRUE;
		
		component_start = (nc == G_DIR_SEPARATOR);
		last_n = n;
		nc = get_char (&n);
	      }
		  
	    return FALSE;
	  }

	case '[':
	  {
	    /* Nonzero if the sense of the character class is inverted.  */
	    gboolean not;
	    gboolean was_escaped;

	    if (nc == '\0' || nc == G_DIR_SEPARATOR)
	      return FALSE;

	    if (nc == '.' && component_start && no_leading_period)
	      return FALSE;

	    not = (*p == '!' || *p == '^');
	    if (not)
	      ++p;

	    c = get_unescaped_char (&p, &was_escaped);
	    for (;;)
	      {
		register gunichar cstart = c, cend = c;
		if (c == '\0')
		  /* [ (unterminated) loses.  */
		  return FALSE;

		c = get_unescaped_char (&p, &was_escaped);
		
		if (!was_escaped && c == '-' && *p != ']')
		  {
		    cend = get_unescaped_char (&p, &was_escaped);
		    if (cend == '\0')
		      return FALSE;

		    c = get_char (&p);
		  }

		if (nc >= cstart && nc <= cend)
		  goto matched;

		if (!was_escaped && c == ']')
		  break;
	      }
	    if (!not)
	      return FALSE;
	    break;

	  matched:;
	    /* Skip the rest of the [...] that already matched.  */
	    /* XXX 1003.2d11 is unclear if was_escaped is right.  */
	    while (was_escaped || c != ']')
	      {
		if (c == '\0')
		  /* [... (unterminated) loses.  */
		  return FALSE;

		c = get_unescaped_char (&p, &was_escaped);
	      }
	    if (not)
	      return FALSE;
	  }
	  break;

	default:
	  if (c != nc)
	    return FALSE;
	}

      component_start = (nc == G_DIR_SEPARATOR);
    }

  if (*n == '\0')
    return TRUE;

  return FALSE;
}

/* Match STRING against the filename pattern PATTERN, returning zero if
 *  it matches, nonzero if not.
 *
 * GTK+ used to use a old version of GNU fnmatch() that was buggy
 * in various ways and didn't handle UTF-8. The following is
 * converted to UTF-8. To simplify the process of making it
 * correct, this is special-cased to the combinations of flags
 * that gtkfilesel.c uses.
 *
 *   FNM_FILE_NAME   - always set
 *   FNM_LEADING_DIR - never set
 *   FNM_NOESCAPE    - set only on windows
 *   FNM_CASEFOLD    - set only on windows
 */
gboolean
_gtk_fnmatch (const char *pattern,
	      const char *string,
	      gboolean no_leading_period)
{
  return gtk_fnmatch_intern (pattern, string, TRUE, no_leading_period);
}

#undef FNMATCH_TEST_CASES
#ifdef FNMATCH_TEST_CASES

#define TEST(pat, str, no_leading_period, result) \
  g_assert (_gtk_fnmatch ((pat), (str), (no_leading_period)) == result)

int main (int argc, char **argv)
{
  TEST ("[a-]", "-", TRUE, TRUE);
  
  TEST ("a", "a", TRUE, TRUE);
  TEST ("a", "b", TRUE, FALSE);

  /* Test what ? matches */
  TEST ("?", "a", TRUE, TRUE);
  TEST ("?", ".", TRUE, FALSE);
  TEST ("a?", "a.", TRUE, TRUE);
  TEST ("a/?", "a/b", TRUE, TRUE);
  TEST ("a/?", "a/.", TRUE, FALSE);
  TEST ("?", "/", TRUE, FALSE);

  /* Test what * matches */
  TEST ("*", "a", TRUE, TRUE);
  TEST ("*", ".", TRUE, FALSE);
  TEST ("a*", "a.", TRUE, TRUE);
  TEST ("a/*", "a/b", TRUE, TRUE);
  TEST ("a/*", "a/.", TRUE, FALSE);
  TEST ("*", "/", TRUE, FALSE);

  /* Range tests */
  TEST ("[ab]", "a", TRUE, TRUE);
  TEST ("[ab]", "c", TRUE, FALSE);
  TEST ("[^ab]", "a", TRUE, FALSE);
  TEST ("[!ab]", "a", TRUE, FALSE);
  TEST ("[^ab]", "c", TRUE, TRUE);
  TEST ("[!ab]", "c", TRUE, TRUE);
  TEST ("[a-c]", "b", TRUE, TRUE);
  TEST ("[a-c]", "d", TRUE, FALSE);
  TEST ("[a-]", "-", TRUE, TRUE);
  TEST ("[]]", "]", TRUE, TRUE);
  TEST ("[^]]", "a", TRUE, TRUE);
  TEST ("[!]]", "a", TRUE, TRUE);

  /* Various unclosed ranges */
  TEST ("[ab", "a", TRUE, FALSE);
  TEST ("[a-", "a", TRUE, FALSE);
  TEST ("[ab", "c", TRUE, FALSE);
  TEST ("[a-", "c", TRUE, FALSE);
  TEST ("[^]", "a", TRUE, FALSE);

  /* Ranges and special no-wildcard matches */
  TEST ("[.]", ".", TRUE, FALSE);
  TEST ("a[.]", "a.", TRUE, TRUE);
  TEST ("a/[.]", "a/.", TRUE, FALSE);
  TEST ("[/]", "/", TRUE, FALSE);
  TEST ("[^/]", "a", TRUE, TRUE);
  
  /* Basic tests of * (and combinations of * and ?) */
  TEST ("a*b", "ab", TRUE, TRUE);
  TEST ("a*b", "axb", TRUE, TRUE);
  TEST ("a*b", "axxb", TRUE, TRUE);
  TEST ("a**b", "ab", TRUE, TRUE);
  TEST ("a**b", "axb", TRUE, TRUE);
  TEST ("a**b", "axxb", TRUE, TRUE);
  TEST ("a*?*b", "ab", TRUE, FALSE);
  TEST ("a*?*b", "axb", TRUE, TRUE);
  TEST ("a*?*b", "axxb", TRUE, TRUE);

  /* Test of  *[range] */
  TEST ("a*[cd]", "ac", TRUE, TRUE);
  TEST ("a*[cd]", "axc", TRUE, TRUE);
  TEST ("a*[cd]", "axx", TRUE, FALSE);

  TEST ("a/[.]", "a/.", TRUE, FALSE);
  TEST ("a*[.]", "a/.", TRUE, FALSE);

  /* Test of UTF-8 */

  TEST ("ä", "ä", TRUE, TRUE);      /* TEST ("ä", "ä", TRUE); */
  TEST ("?", "ä", TRUE, TRUE);       /* TEST ("?", "ä", TRUE); */
  TEST ("*ö", "äö", TRUE, TRUE);   /* TEST ("*ö", "äö", TRUE); */
  TEST ("*ö", "ääö", TRUE, TRUE); /* TEST ("*ö", "ääö", TRUE); */
  TEST ("[ä]", "ä", TRUE, TRUE);    /* TEST ("[ä]", "ä", TRUE); */
  TEST ("[ä-ö]", "é", TRUE, TRUE); /* TEST ("[ä-ö]", "é", TRUE); */
  TEST ("[ä-ö]", "a", TRUE, FALSE); /* TEST ("[ä-ö]", "a", FALSE); */

#ifdef DO_ESCAPE
  /* Tests of escaping */
  TEST ("\\\\", "\\", TRUE, TRUE);
  TEST ("\\?", "?", TRUE, TRUE);
  TEST ("\\?", "a", TRUE, FALSE);
  TEST ("\\*", "*", TRUE, TRUE);
  TEST ("\\*", "a", TRUE, FALSE);
  TEST ("\\[a-b]", "[a-b]", TRUE, TRUE);
  TEST ("[\\\\]", "\\", TRUE, TRUE);
  TEST ("[\\^a]", "a", TRUE, TRUE);
  TEST ("[a\\-c]", "b", TRUE, FALSE);
  TEST ("[a\\-c]", "-", TRUE, TRUE);
  TEST ("[a\\]", "a", TRUE, FALSE);
#endif /* DO_ESCAPE */
  
  return 0;
}

#endif /* FNMATCH_TEST_CASES */
