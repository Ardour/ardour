/*
 * Copyright © 2001 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Red Hat not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Red Hat makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * RED HAT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL RED HAT
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Owen Taylor, Red Hat, Inc.
 */
#include "config.h"
#include "string.h"
#include "stdlib.h"

#include <X11/Xlib.h>
#include <X11/Xmd.h>		/* For CARD32 */

#include "xsettings-common.h"

XSettingsSetting *
xsettings_setting_copy (XSettingsSetting *setting)
{
  XSettingsSetting *result;
  size_t str_len;
  
  result = malloc (sizeof *result);
  if (!result)
    return NULL;

  str_len = strlen (setting->name);
  result->name = malloc (str_len + 1);
  if (!result->name)
    goto err;

  memcpy (result->name, setting->name, str_len + 1);

  result->type = setting->type;

  switch (setting->type)
    {
    case XSETTINGS_TYPE_INT:
      result->data.v_int = setting->data.v_int;
      break;
    case XSETTINGS_TYPE_COLOR:
      result->data.v_color = setting->data.v_color;
      break;
    case XSETTINGS_TYPE_STRING:
      str_len = strlen (setting->data.v_string);
      result->data.v_string = malloc (str_len + 1);
      if (!result->data.v_string)
	goto err;

      memcpy (result->data.v_string, setting->data.v_string, str_len + 1);
      break;
    }

  result->last_change_serial = setting->last_change_serial;

  return result;

 err:
  if (result->name)
    free (result->name);
  free (result);
  
  return NULL;
}

XSettingsList *
xsettings_list_copy (XSettingsList *list)
{
  XSettingsList *new = NULL;
  XSettingsList *old_iter = list;
  XSettingsList *new_iter = NULL;

  while (old_iter)
    {
      XSettingsList *new_node;

      new_node = malloc (sizeof *new_node);
      if (!new_node)
	goto error;

      new_node->setting = xsettings_setting_copy (old_iter->setting);
      if (!new_node->setting)
	{
	  free (new_node);
	  goto error;
	}

      if (new_iter)
	new_iter->next = new_node;
      else
	new = new_node;

      new_iter = new_node;
      
      old_iter = old_iter->next;
    }

  return new;

 error:
  xsettings_list_free (new);
  return NULL;
}

int
xsettings_setting_equal (XSettingsSetting *setting_a,
			 XSettingsSetting *setting_b)
{
  if (setting_a->type != setting_b->type)
    return 0;

  if (strcmp (setting_a->name, setting_b->name) != 0)
    return 0;

  switch (setting_a->type)
    {
    case XSETTINGS_TYPE_INT:
      return setting_a->data.v_int == setting_b->data.v_int;
    case XSETTINGS_TYPE_COLOR:
      return (setting_a->data.v_color.red == setting_b->data.v_color.red &&
	      setting_a->data.v_color.green == setting_b->data.v_color.green &&
	      setting_a->data.v_color.blue == setting_b->data.v_color.blue &&
	      setting_a->data.v_color.alpha == setting_b->data.v_color.alpha);
    case XSETTINGS_TYPE_STRING:
      return strcmp (setting_a->data.v_string, setting_b->data.v_string) == 0;
    }

  return 0;
}

void
xsettings_setting_free (XSettingsSetting *setting)
{
  if (setting->type == XSETTINGS_TYPE_STRING)
    free (setting->data.v_string);

  if (setting->name)
    free (setting->name);
  
  free (setting);
}

void
xsettings_list_free (XSettingsList *list)
{
  while (list)
    {
      XSettingsList *next = list->next;

      xsettings_setting_free (list->setting);
      free (list);

      list = next;
    }
}

XSettingsResult
xsettings_list_insert (XSettingsList    **list,
		       XSettingsSetting  *setting)
{
  XSettingsList *node;
  XSettingsList *iter;
  XSettingsList *last = NULL;

  node = malloc (sizeof *node);
  if (!node)
    return XSETTINGS_NO_MEM;
  node->setting = setting;

  iter = *list;
  while (iter)
    {
      int cmp = strcmp (setting->name, iter->setting->name);

      if (cmp < 0)
	break;
      else if (cmp == 0)
	{
	  free (node);
	  return XSETTINGS_DUPLICATE_ENTRY;
	}

      last = iter;
      iter = iter->next;
    }
  
  if (last)
    last->next = node;
  else
    *list = node;
  
  node->next = iter;
  
  return XSETTINGS_SUCCESS;
}

XSettingsResult
xsettings_list_delete (XSettingsList **list,
		       const char     *name)
{
  XSettingsList *iter;
  XSettingsList *last = NULL;

  iter = *list;
  while (iter)
    {
      if (strcmp (name, iter->setting->name) == 0)
	{
	  if (last)
	    last->next = iter->next;
	  else
	    *list = iter->next;
  
	  xsettings_setting_free (iter->setting);
	  free (iter);

	  return XSETTINGS_SUCCESS;
	}

      last = iter;
      iter = iter->next;
    }

  return XSETTINGS_FAILED;
}

XSettingsSetting *
xsettings_list_lookup (XSettingsList *list,
		       const char    *name)
{
  XSettingsList *iter;

  iter = list;
  while (iter)
    {
      if (strcmp (name, iter->setting->name) == 0)
	return iter->setting;

      iter = iter->next;
    }

  return NULL;
}

char
xsettings_byte_order (void)
{
  CARD32 myint = 0x01020304;
  return (*(char *)&myint == 1) ? MSBFirst : LSBFirst;
}
