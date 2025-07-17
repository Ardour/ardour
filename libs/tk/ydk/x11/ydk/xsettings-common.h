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
#ifndef XSETTINGS_COMMON_H
#define XSETTINGS_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Renames for GDK inclusion */

#define xsettings_byte_order             _gdk_xsettings_byte_order
#define xsettings_client_destroy         _gdk_xsettings_client_destroy
#define xsettings_client_get_setting     _gdk_xsettings_client_get_setting
#define xsettings_client_new             _gdk_xsettings_client_new
#define xsettings_client_new_with_grab_funcs _gdk_xsettings_client_new_with_grab_funcs
#define xsettings_client_set_grab_func   _gdk_xsettings_client_set_grab_func
#define xsettings_client_set_ungrab_func _gdk_xsettings_client_set_ungrab_func
#define xsettings_client_process_event   _gdk_xsettings_client_process_event
#define xsettings_list_copy              _gdk_xsettings_list_copy
#define xsettings_list_delete            _gdk_xsettings_list_delete
#define xsettings_list_free              _gdk_xsettings_list_free
#define xsettings_list_insert            _gdk_xsettings_list_insert
#define xsettings_list_lookup            _gdk_xsettings_list_lookup
#define xsettings_setting_copy           _gdk_xsettings_setting_copy
#define xsettings_setting_equal          _gdk_xsettings_setting_equal
#define xsettings_setting_free           _gdk_xsettings_setting_free


typedef struct _XSettingsBuffer  XSettingsBuffer;
typedef struct _XSettingsColor   XSettingsColor;
typedef struct _XSettingsList    XSettingsList;
typedef struct _XSettingsSetting XSettingsSetting;

/* Types of settings possible. Enum values correspond to
 * protocol values.
 */
typedef enum 
{
  XSETTINGS_TYPE_INT     = 0,
  XSETTINGS_TYPE_STRING  = 1,
  XSETTINGS_TYPE_COLOR   = 2
} XSettingsType;

typedef enum
{
  XSETTINGS_SUCCESS,
  XSETTINGS_NO_MEM,
  XSETTINGS_ACCESS,
  XSETTINGS_FAILED,
  XSETTINGS_NO_ENTRY,
  XSETTINGS_DUPLICATE_ENTRY
} XSettingsResult;

struct _XSettingsBuffer
{
  char byte_order;
  size_t len;
  unsigned char *data;
  unsigned char *pos;
};

struct _XSettingsColor
{
  unsigned short red, green, blue, alpha;
};

struct _XSettingsList
{
  XSettingsSetting *setting;
  XSettingsList *next;
};

struct _XSettingsSetting
{
  char *name;
  XSettingsType type;
  
  union {
    int v_int;
    char *v_string;
    XSettingsColor v_color;
  } data;

  unsigned long last_change_serial;
};

XSettingsSetting *xsettings_setting_copy  (XSettingsSetting *setting);
void              xsettings_setting_free  (XSettingsSetting *setting);
int               xsettings_setting_equal (XSettingsSetting *setting_a,
					   XSettingsSetting *setting_b);

void              xsettings_list_free   (XSettingsList     *list);
XSettingsList    *xsettings_list_copy   (XSettingsList     *list);
XSettingsResult   xsettings_list_insert (XSettingsList    **list,
					 XSettingsSetting  *setting);
XSettingsSetting *xsettings_list_lookup (XSettingsList     *list,
					 const char        *name);
XSettingsResult   xsettings_list_delete (XSettingsList    **list,
					 const char        *name);

char xsettings_byte_order (void);

#define XSETTINGS_PAD(n,m) ((n + m - 1) & (~(m-1)))

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* XSETTINGS_COMMON_H */
