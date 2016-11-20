/* FluidSynth - A Software Synthesizer
 *
 * Copyright (C) 2003  Peter Hanappe and others.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 */

#include "fluidsynth_priv.h"
#include "fluid_sys.h"
#include "fluid_hash.h"
#include "fluid_synth.h"
//#include "fluid_cmd.h"
//#include "fluid_adriver.h"
//#include "fluid_mdriver.h"
#include "fluid_settings.h"
#include "fluid_midi.h"

/* Defined in fluid_filerenderer.c */
extern void fluid_file_renderer_settings (fluid_settings_t* settings);

/* maximum allowed components of a settings variable (separated by '.') */
#define MAX_SETTINGS_TOKENS 8	/* currently only a max of 3 are used */
#define MAX_SETTINGS_LABEL 256	/* max length of a settings variable label */

static void fluid_settings_init(fluid_settings_t* settings);
static void fluid_settings_key_destroy_func(void* value);
static void fluid_settings_value_destroy_func(void* value);
static int fluid_settings_tokenize(const char *s, char *buf, char **ptr);

/* Common structure to all settings nodes */
typedef struct {
  int type;             /**< fluid_types_enum */
} fluid_setting_node_t; 

typedef struct {
  fluid_setting_node_t node;
  char* value;
  char* def;
  int hints;
  fluid_list_t* options;
  fluid_str_update_t update;
  void* data;
} fluid_str_setting_t;

typedef struct {
  fluid_setting_node_t node;
  double value;
  double def;
  double min;
  double max;
  int hints;
  fluid_num_update_t update;
  void* data;
} fluid_num_setting_t;

typedef struct {
  fluid_setting_node_t node;
  int value;
  int def;
  int min;
  int max;
  int hints;
  fluid_int_update_t update;
  void* data;
} fluid_int_setting_t;

typedef struct {
  fluid_setting_node_t node;
  fluid_hashtable_t *hashtable;
} fluid_set_setting_t;


static fluid_str_setting_t*
new_fluid_str_setting(const char* value, char* def, int hints, fluid_str_update_t fun, void* data)
{
  fluid_str_setting_t* str;

  str = FLUID_NEW(fluid_str_setting_t);

  if (!str)
  {
    FLUID_LOG(FLUID_ERR, "Out of memory");
    return NULL;
  }

  str->node.type = FLUID_STR_TYPE;
  str->value = value? FLUID_STRDUP(value) : NULL;
  str->def = def? FLUID_STRDUP(def) : NULL;
  str->hints = hints;
  str->options = NULL;
  str->update = fun;
  str->data = data;
  return str;
}

static void
delete_fluid_str_setting(fluid_str_setting_t* str)
{
  if (!str) return;

  if (str->value) FLUID_FREE(str->value);
  if (str->def) FLUID_FREE(str->def);

  if (str->options) {
    fluid_list_t* list = str->options;

    while (list) {
      FLUID_FREE (list->data);
      list = fluid_list_next(list);
    }

    delete_fluid_list(str->options);
  }

  FLUID_FREE(str);
}


static fluid_num_setting_t*
new_fluid_num_setting(double min, double max, double def,
		     int hints, fluid_num_update_t fun, void* data)
{
  fluid_num_setting_t* setting;

  setting = FLUID_NEW(fluid_num_setting_t);

  if (!setting)
  {
    FLUID_LOG(FLUID_ERR, "Out of memory");
    return NULL;
  }

  setting->node.type = FLUID_NUM_TYPE;
  setting->value = def;
  setting->def = def;
  setting->min = min;
  setting->max = max;
  setting->hints = hints;
  setting->update = fun;
  setting->data = data;
  return setting;
}

static void
delete_fluid_num_setting(fluid_num_setting_t* setting)
{
  if (setting) FLUID_FREE(setting);
}

static fluid_int_setting_t*
new_fluid_int_setting(int min, int max, int def,
		     int hints, fluid_int_update_t fun, void* data)
{
  fluid_int_setting_t* setting;

  setting = FLUID_NEW(fluid_int_setting_t);

  if (!setting)
  {
    FLUID_LOG(FLUID_ERR, "Out of memory");
    return NULL;
  }

  setting->node.type = FLUID_INT_TYPE;
  setting->value = def;
  setting->def = def;
  setting->min = min;
  setting->max = max;
  setting->hints = hints;
  setting->update = fun;
  setting->data = data;
  return setting;
}

static void
delete_fluid_int_setting(fluid_int_setting_t* setting)
{
  if (setting) FLUID_FREE(setting);
}

static fluid_set_setting_t*
new_fluid_set_setting(void)
{
  fluid_set_setting_t* setting;

  setting = FLUID_NEW(fluid_set_setting_t);

  if (!setting)
  {
    FLUID_LOG(FLUID_ERR, "Out of memory");
    return NULL;
  }

  setting->node.type = FLUID_SET_TYPE;
  setting->hashtable = new_fluid_hashtable_full(fluid_str_hash, fluid_str_equal,
                                                fluid_settings_key_destroy_func,
                                                fluid_settings_value_destroy_func);
  if (!setting->hashtable)
  {
    FLUID_FREE (setting);
    return NULL;
  }

  return setting;
}

static void
delete_fluid_set_setting(fluid_set_setting_t* setting)
{
  if (setting)
  {
    delete_fluid_hashtable(setting->hashtable);
    FLUID_FREE(setting);
  }
}

/**
 * Create a new settings object
 * @return the pointer to the settings object
 */
fluid_settings_t *
new_fluid_settings(void)
{
  fluid_settings_t* settings;

  settings = new_fluid_hashtable_full(fluid_str_hash, fluid_str_equal,
                                      fluid_settings_key_destroy_func,
                                      fluid_settings_value_destroy_func);
  if (settings == NULL) return NULL;

  fluid_rec_mutex_init (settings->mutex);
  fluid_settings_init(settings);
  return settings;
}

/**
 * Delete the provided settings object
 * @param settings a settings object
 */
void
delete_fluid_settings(fluid_settings_t* settings)
{
  fluid_return_if_fail (settings != NULL);

  fluid_rec_mutex_destroy (settings->mutex);
  delete_fluid_hashtable(settings);
}

/* Settings hash key destroy function */
static void
fluid_settings_key_destroy_func(void* value)
{
  FLUID_FREE (value);   /* Free the string key value */
}

/* Settings hash value destroy function */
static void
fluid_settings_value_destroy_func(void* value)
{
  fluid_setting_node_t *node = value;

  switch (node->type) {
  case FLUID_NUM_TYPE:
    delete_fluid_num_setting((fluid_num_setting_t*) value);
    break;
  case FLUID_INT_TYPE:
    delete_fluid_int_setting((fluid_int_setting_t*) value);
    break;
  case FLUID_STR_TYPE:
    delete_fluid_str_setting((fluid_str_setting_t*) value);
    break;
  case FLUID_SET_TYPE:
    delete_fluid_set_setting((fluid_set_setting_t*) value);
    break;
  }
}

void
fluid_settings_init(fluid_settings_t* settings)
{
  fluid_return_if_fail (settings != NULL);

  fluid_synth_settings(settings);
#if 0
  fluid_shell_settings(settings);
  fluid_player_settings(settings);
  fluid_file_renderer_settings(settings);
  fluid_audio_driver_settings(settings);
  fluid_midi_driver_settings(settings);
#endif
}

static int
fluid_settings_tokenize(const char *s, char *buf, char **ptr)
{
  char *tokstr, *tok;
  int n = 0;

  if (strlen (s) > MAX_SETTINGS_LABEL)
  {
    FLUID_LOG(FLUID_ERR, "Setting variable name exceeded max length of %d chars",
	      MAX_SETTINGS_LABEL);
    return 0;
  }

  FLUID_STRCPY(buf, s);	/* copy string to buffer, since it gets modified */
  tokstr = buf;

  while ((tok = fluid_strtok (&tokstr, ".")))
  {
    if (n >= MAX_SETTINGS_TOKENS)
    {
      FLUID_LOG(FLUID_ERR, "Setting variable name exceeded max token count of %d",
		MAX_SETTINGS_TOKENS);
      return 0;
    } else
        ptr[n++] = tok;
  }

  return n;
}

/**
 * Get a setting name, value and type
 *
 * @param settings a settings object
 * @param name Settings name
 * @param value Location to store setting node if found
 * @return 1 if the node exists, 0 otherwise
 */
static int
fluid_settings_get(fluid_settings_t* settings, const char *name,
                   fluid_setting_node_t **value)
{
  fluid_hashtable_t* table = settings;
  fluid_setting_node_t *node = NULL;
  char* tokens[MAX_SETTINGS_TOKENS];
  char buf[MAX_SETTINGS_LABEL+1];
  int ntokens;
  int n;

  ntokens = fluid_settings_tokenize (name, buf, tokens);

  if (table == NULL || ntokens <= 0) return 0;

  for (n = 0; n < ntokens; n++) {

    node = fluid_hashtable_lookup(table, tokens[n]);
    if (!node) return 0;

    table = (node->type == FLUID_SET_TYPE) ? ((fluid_set_setting_t *)node)->hashtable : NULL;
  }

  if (value) *value = node;

  return 1;
}

/**
 * Set a setting name, value and type, replacing it if already exists
 *
 * @param settings a settings object
 * @param name Settings name
 * @param value Node instance to assign (used directly)
 * @return 1 if the value has been set, zero otherwise
 */
static int
fluid_settings_set(fluid_settings_t* settings, const char *name, void* value)
{
  fluid_hashtable_t* table = settings;
  fluid_setting_node_t *node;
  char* tokens[MAX_SETTINGS_TOKENS];
  char buf[MAX_SETTINGS_LABEL+1];
  int n, num;
  char *dupname;

  num = fluid_settings_tokenize (name, buf, tokens) - 1;
  if (num == 0)
    return 0;

  for (n = 0; n < num; n++) {

    node = fluid_hashtable_lookup(table, tokens[n]);

    if (node) {

      if (node->type == FLUID_SET_TYPE) {
	table = ((fluid_set_setting_t *)node)->hashtable;
      } else {
	/* path ends prematurely */
	FLUID_LOG(FLUID_WARN, "'%s' is not a node", name[n]);
	return 0;
      }

    } else {
      /* create a new node */
      fluid_set_setting_t* setnode;

      dupname = FLUID_STRDUP (tokens[n]);
      setnode = new_fluid_set_setting ();

      if (!dupname || !setnode)
      {
        if (dupname) FLUID_FREE (dupname);
        else FLUID_LOG(FLUID_ERR, "Out of memory");

        if (setnode) delete_fluid_set_setting (setnode);

        return 0;
      }

      fluid_hashtable_insert(table, dupname, setnode);
      table = setnode->hashtable;
    }
  }

  dupname = FLUID_STRDUP (tokens[num]);

  if (!dupname)
  {
    FLUID_LOG(FLUID_ERR, "Out of memory");
    return 0;
  }

  fluid_hashtable_insert(table, dupname, value);

  return 1;
}

/** returns 1 if the value has been registered correctly, 0
    otherwise */
int
fluid_settings_register_str(fluid_settings_t* settings, char* name, char* def, int hints,
			    fluid_str_update_t fun, void* data)
{
  fluid_setting_node_t *node;
  fluid_str_setting_t* setting;
  int retval;

  fluid_return_val_if_fail (settings != NULL, 0);
  fluid_return_val_if_fail (name != NULL, 0);
  fluid_return_val_if_fail (name[0] != '\0', 0);

  fluid_rec_mutex_lock (settings->mutex);

  if (!fluid_settings_get(settings, name, &node)) {
    setting = new_fluid_str_setting(def, def, hints, fun, data);
    retval = fluid_settings_set(settings, name, setting);
    if (retval != 1) delete_fluid_str_setting (setting);
  } else {
    /* if variable already exists, don't change its value. */
    if (node->type == FLUID_STR_TYPE) {
      setting = (fluid_str_setting_t*) node;
      setting->update = fun;
      setting->data = data;
      setting->def = def? FLUID_STRDUP(def) : NULL;
      setting->hints = hints;
      retval = 1;
    } else {
      FLUID_LOG(FLUID_WARN, "Type mismatch on setting '%s'", name);
      retval = 0;
    }
  }

  fluid_rec_mutex_unlock (settings->mutex);

  return retval;
}

/** returns 1 if the value has been register correctly, zero
    otherwise */
int
fluid_settings_register_num(fluid_settings_t* settings, char* name, double def,
			    double min, double max, int hints,
			    fluid_num_update_t fun, void* data)
{
  fluid_setting_node_t *node;
  int retval;

  fluid_return_val_if_fail (settings != NULL, 0);
  fluid_return_val_if_fail (name != NULL, 0);
  fluid_return_val_if_fail (name[0] != '\0', 0);

  /* For now, all floating point settings are bounded below and above */
  hints |= FLUID_HINT_BOUNDED_BELOW | FLUID_HINT_BOUNDED_ABOVE;

  fluid_rec_mutex_lock (settings->mutex);

  if (!fluid_settings_get(settings, name, &node)) {
    /* insert a new setting */
    fluid_num_setting_t* setting;
    setting = new_fluid_num_setting(min, max, def, hints, fun, data);
    retval = fluid_settings_set(settings, name, setting);
    if (retval != 1) delete_fluid_num_setting (setting);
  } else {
    if (node->type == FLUID_NUM_TYPE) {
      /* update the existing setting but don't change its value */
      fluid_num_setting_t* setting = (fluid_num_setting_t*) node;
      setting->update = fun;
      setting->data = data;
      setting->min = min;
      setting->max = max;
      setting->def = def;
      setting->hints = hints;
      retval = 1;
    } else {
      /* type mismatch */
      FLUID_LOG(FLUID_WARN, "Type mismatch on setting '%s'", name);
      retval = 0;
    }
  }

  fluid_rec_mutex_unlock (settings->mutex);

  return retval;
}

/** returns 1 if the value has been register correctly, zero
    otherwise. */
int
fluid_settings_register_int(fluid_settings_t* settings, char* name, int def,
			    int min, int max, int hints,
			    fluid_int_update_t fun, void* data)
{
  fluid_setting_node_t *node;
  int retval;

  fluid_return_val_if_fail (settings != NULL, 0);
  fluid_return_val_if_fail (name != NULL, 0);
  fluid_return_val_if_fail (name[0] != '\0', 0);

  /* For now, all integer settings are bounded below and above */
  hints |= FLUID_HINT_BOUNDED_BELOW | FLUID_HINT_BOUNDED_ABOVE;

  fluid_rec_mutex_lock (settings->mutex);

  if (!fluid_settings_get(settings, name, &node)) {
    /* insert a new setting */
    fluid_int_setting_t* setting;
    setting = new_fluid_int_setting(min, max, def, hints, fun, data);
    retval = fluid_settings_set(settings, name, setting);
    if (retval != 1) delete_fluid_int_setting (setting);
  } else {
    if (node->type == FLUID_INT_TYPE) {
      /* update the existing setting but don't change its value */
      fluid_int_setting_t* setting = (fluid_int_setting_t*) node;
      setting->update = fun;
      setting->data = data;
      setting->min = min;
      setting->max = max;
      setting->def = def;
      setting->hints = hints;
      retval = 1;
    } else {
      /* type mismatch */
      FLUID_LOG(FLUID_WARN, "Type mismatch on setting '%s'", name);
      retval = 0;
    }
  }

  fluid_rec_mutex_unlock (settings->mutex);

  return retval;
}

/**
 * Get the type of the setting with the given name
 *
 * @param settings a settings object
 * @param name a setting's name
 * @return the type for the named setting, or #FLUID_NO_TYPE when it does not exist
 */
int
fluid_settings_get_type(fluid_settings_t* settings, const char *name)
{
  fluid_setting_node_t *node;
  int type;

  fluid_return_val_if_fail (settings != NULL, FLUID_NO_TYPE);
  fluid_return_val_if_fail (name != NULL, FLUID_NO_TYPE);
  fluid_return_val_if_fail (name[0] != '\0', FLUID_NO_TYPE);

  fluid_rec_mutex_lock (settings->mutex);
  type = fluid_settings_get (settings, name, &node) ? node->type : FLUID_NO_TYPE;
  fluid_rec_mutex_unlock (settings->mutex);

  return (type);
}

/**
 * Get the hints for the named setting as an integer bitmap
 *
 * @param settings a settings object
 * @param name a setting's name
 * @return the hints associated to the named setting if it exists, zero otherwise
 */
int
fluid_settings_get_hints(fluid_settings_t* settings, const char *name)
{
  fluid_setting_node_t *node;
  int hints = 0;

  fluid_return_val_if_fail (settings != NULL, 0);
  fluid_return_val_if_fail (name != NULL, 0);
  fluid_return_val_if_fail (name[0] != '\0', 0);

  fluid_rec_mutex_lock (settings->mutex);

  if (fluid_settings_get(settings, name, &node)) {
    if (node->type == FLUID_NUM_TYPE) {
      fluid_num_setting_t* setting = (fluid_num_setting_t*) node;
      hints = setting->hints;
    } else if (node->type == FLUID_STR_TYPE) {
      fluid_str_setting_t* setting = (fluid_str_setting_t*) node;
      hints = setting->hints;
    } else if (node->type == FLUID_INT_TYPE) {
      fluid_int_setting_t* setting = (fluid_int_setting_t*) node;
      hints = setting->hints;
    }
  }

  fluid_rec_mutex_unlock (settings->mutex);

  return hints;
}

/**
 * Ask whether the setting is changeable in real-time.
 *
 * @param settings a settings object
 * @param name a setting's name
 * @return non zero if the setting is changeable in real-time
 */
int
fluid_settings_is_realtime(fluid_settings_t* settings, const char *name)
{
  fluid_setting_node_t *node;
  int isrealtime = FALSE;

  fluid_return_val_if_fail (settings != NULL, 0);
  fluid_return_val_if_fail (name != NULL, 0);
  fluid_return_val_if_fail (name[0] != '\0', 0);

  fluid_rec_mutex_lock (settings->mutex);

  if (fluid_settings_get(settings, name, &node)) {
    if (node->type == FLUID_NUM_TYPE) {
      fluid_num_setting_t* setting = (fluid_num_setting_t*) node;
      isrealtime = setting->update != NULL;
    } else if (node->type == FLUID_STR_TYPE) {
      fluid_str_setting_t* setting = (fluid_str_setting_t*) node;
      isrealtime = setting->update != NULL;
    } else if (node->type == FLUID_INT_TYPE) {
      fluid_int_setting_t* setting = (fluid_int_setting_t*) node;
      isrealtime = setting->update != NULL;
    }
  }

  fluid_rec_mutex_unlock (settings->mutex);

  return isrealtime;
}

/**
 * Set a string value for a named setting
 *
 * @param settings a settings object
 * @param name a setting's name
 * @param str new string value
 * @return 1 if the value has been set, 0 otherwise
 */
int
fluid_settings_setstr(fluid_settings_t* settings, const char *name, const char *str)
{
  fluid_setting_node_t *node;
  int retval = 0;

  fluid_return_val_if_fail (settings != NULL, 0);
  fluid_return_val_if_fail (name != NULL, 0);
  fluid_return_val_if_fail (name[0] != '\0', 0);

  fluid_rec_mutex_lock (settings->mutex);

  if (fluid_settings_get (settings, name, &node)) {
    if (node->type == FLUID_STR_TYPE) {
      fluid_str_setting_t *setting = (fluid_str_setting_t *)node;

      if (setting->value) FLUID_FREE (setting->value);
      setting->value = str ? FLUID_STRDUP (str) : NULL;

      /* Call under lock to keep update() synchronized with the current value */
      if (setting->update) (*setting->update)(setting->data, name, str);
      retval = 1;
    }
    else if (node->type == FLUID_INT_TYPE)      /* Handle yes/no for boolean values for backwards compatibility */
    {
      fluid_int_setting_t *setting = (fluid_int_setting_t *)node;

      if (setting->hints & FLUID_HINT_TOGGLED)
      {
        if (FLUID_STRCMP (str, "yes") == 0)
        {
          setting->value = TRUE;
          if (setting->update) (*setting->update)(setting->data, name, TRUE);
        }
        else if (FLUID_STRCMP (str, "no") == 0)
        {
          setting->value = FALSE;
          if (setting->update) (*setting->update)(setting->data, name, FALSE);
        }
      }
    }
  } else {
    /* insert a new setting */
    fluid_str_setting_t* setting;
    setting = new_fluid_str_setting(str, NULL, 0, NULL, NULL);
    retval = fluid_settings_set(settings, name, setting);
    if (retval != 1) delete_fluid_str_setting (setting);
  }

  fluid_rec_mutex_unlock (settings->mutex);

  return retval;
}

/**
 * Copy the value of a string setting
 * @param settings a settings object
 * @param name a setting's name
 * @param str Caller supplied buffer to copy string value to
 * @param len Size of 'str' buffer (no more than len bytes will be written, which
 *   will always include a zero terminator)
 * @return 1 if the value exists, 0 otherwise
 * @since 1.1.0
 *
 * Like fluid_settings_getstr() but is thread safe.  A size of 256 should be
 * more than sufficient for the string buffer.
 */
int
fluid_settings_copystr(fluid_settings_t* settings, const char *name,
                       char *str, int len)
{
  fluid_setting_node_t *node;
  int retval = 0;

  fluid_return_val_if_fail (settings != NULL, 0);
  fluid_return_val_if_fail (name != NULL, 0);
  fluid_return_val_if_fail (name[0] != '\0', 0);
  fluid_return_val_if_fail (str != NULL, 0);
  fluid_return_val_if_fail (len > 0, 0);

  str[0] = 0;

  fluid_rec_mutex_lock (settings->mutex);

  if (fluid_settings_get (settings, name, &node))
  {
    if (node->type == FLUID_STR_TYPE)
    {
      fluid_str_setting_t *setting = (fluid_str_setting_t *)node;

      if (setting->value)
      {
        FLUID_STRNCPY (str, setting->value, len);
        str[len - 1] = 0;   /* Force terminate, in case of truncation */
      }

      retval = 1;
    }
    else if (node->type == FLUID_INT_TYPE)      /* Handle boolean integers for backwards compatibility */
    {
      fluid_int_setting_t *setting = (fluid_int_setting_t *)node;

      if (setting->hints & FLUID_HINT_TOGGLED)
      {
        FLUID_STRNCPY (str, setting->value ? "yes" : "no", len);
        str[len - 1] = 0;   /* Force terminate, in case of truncation */

        retval = 1;
      }
    }
  }

  fluid_rec_mutex_unlock (settings->mutex);

  return retval;
}

/**
 * Duplicate the value of a string setting
 * @param settings a settings object
 * @param name a setting's name
 * @param str Location to store pointer to allocated duplicate string
 * @return 1 if the value exists and was successfully duplicated, 0 otherwise
 * @since 1.1.0
 *
 * Like fluid_settings_copystr() but allocates a new copy of the string.  Caller
 * owns the string and should free it with free() when done using it.
 */
int
fluid_settings_dupstr(fluid_settings_t* settings, const char *name, char** str)
{
  fluid_setting_node_t *node;
  int retval = 0;

  fluid_return_val_if_fail (settings != NULL, 0);
  fluid_return_val_if_fail (name != NULL, 0);
  fluid_return_val_if_fail (name[0] != '\0', 0);
  fluid_return_val_if_fail (str != NULL, 0);

  fluid_rec_mutex_lock (settings->mutex);

  if (fluid_settings_get(settings, name, &node))
  {
    if (node->type == FLUID_STR_TYPE)
    {
      fluid_str_setting_t *setting = (fluid_str_setting_t *)node;

      if (setting->value)
      {
        *str = FLUID_STRDUP (setting->value);
        if (!*str) FLUID_LOG (FLUID_ERR, "Out of memory");
      }

      if (!setting->value || *str) retval = 1;    /* Don't set to 1 if out of memory */
    }
    else if (node->type == FLUID_INT_TYPE)      /* Handle boolean integers for backwards compatibility */
    {
      fluid_int_setting_t *setting = (fluid_int_setting_t *)node;

      if (setting->hints & FLUID_HINT_TOGGLED)
      {
        *str = FLUID_STRDUP (setting->value ? "yes" : "no");
        if (!*str) FLUID_LOG (FLUID_ERR, "Out of memory");

        if (!setting->value || *str) retval = 1;    /* Don't set to 1 if out of memory */
      }
    }
  }

  fluid_rec_mutex_unlock (settings->mutex);

  return retval;
}

/**
 * Get the value of a string setting
 * @param settings a settings object
 * @param name a setting's name
 * @param str Location to store pointer to the settings string value
 * @return 1 if the value exists, 0 otherwise
 * @deprecated
 *
 * If the value does not exists, 'str' is set to NULL. Otherwise, 'str' will
 * point to the value. The application does not own the returned value and it
 * is valid only until a new value is assigned to the setting of the given name.
 *
 * NOTE: In a multi-threaded environment, caller must ensure that the setting
 * being read by fluid_settings_getstr() is not assigned during the
 * duration of callers use of the setting's value.  Use fluid_settings_copystr()
 * or fluid_settings_dupstr() which does not have this restriction.
 */
int
fluid_settings_getstr(fluid_settings_t* settings, const char *name, char** str)
{
  fluid_setting_node_t *node;
  int retval = 0;

  fluid_return_val_if_fail (settings != NULL, 0);
  fluid_return_val_if_fail (name != NULL, 0);
  fluid_return_val_if_fail (name[0] != '\0', 0);
  fluid_return_val_if_fail (str != NULL, 0);

  fluid_rec_mutex_lock (settings->mutex);

  if (fluid_settings_get(settings, name, &node))
  {
    if (node->type == FLUID_STR_TYPE)
    {
      fluid_str_setting_t *setting = (fluid_str_setting_t *)node;
      *str = setting->value;
      retval = 1;
    }
    else if (node->type == FLUID_INT_TYPE)      /* Handle boolean integers for backwards compatibility */
    {
      fluid_int_setting_t *setting = (fluid_int_setting_t *)node;

      if (setting->hints & FLUID_HINT_TOGGLED)
      {
        *str = setting->value ? "yes" : "no";
        retval = 1;
      }
    }
  }
  else *str = NULL;

  fluid_rec_mutex_unlock (settings->mutex);

  return retval;
}

/**
 * Test a string setting for some value.
 *
 * @param settings a settings object
 * @param name a setting's name
 * @param s a string to be tested
 * @return 1 if the value exists and is equal to 's', 0 otherwise
 */
int
fluid_settings_str_equal (fluid_settings_t* settings, const char *name, const char *s)
{
  fluid_setting_node_t *node;
  int retval = 0;

  fluid_return_val_if_fail (settings != NULL, 0);
  fluid_return_val_if_fail (name != NULL, 0);
  fluid_return_val_if_fail (name[0] != '\0', 0);
  fluid_return_val_if_fail (s != NULL, 0);

  fluid_rec_mutex_lock (settings->mutex);

  if (fluid_settings_get (settings, name, &node))
  {
    if (node->type == FLUID_STR_TYPE)
    {
      fluid_str_setting_t *setting = (fluid_str_setting_t *)node;
      if (setting->value) retval = FLUID_STRCMP (setting->value, s) == 0;
    }
    else if (node->type == FLUID_INT_TYPE)      /* Handle boolean integers for backwards compatibility */
    {
      fluid_int_setting_t *setting = (fluid_int_setting_t *)node;

      if (setting->hints & FLUID_HINT_TOGGLED)
        retval = FLUID_STRCMP (setting->value ? "yes" : "no", s) == 0;
    }
  }

  fluid_rec_mutex_unlock (settings->mutex);

  return retval;
}

/**
 * Get the default value of a string setting.  Note that the returned string is
 * not owned by the caller and should not be modified or freed.
 *
 * @param settings a settings object
 * @param name a setting's name
 * @return the default string value of the setting if it exists, NULL otherwise
 */
char*
fluid_settings_getstr_default(fluid_settings_t* settings, const char *name)
{
  fluid_setting_node_t *node;
  char *retval = NULL;

  fluid_return_val_if_fail (settings != NULL, NULL);
  fluid_return_val_if_fail (name != NULL, NULL);
  fluid_return_val_if_fail (name[0] != '\0', NULL);

  fluid_rec_mutex_lock (settings->mutex);

  if (fluid_settings_get (settings, name, &node))
  {
    if (node->type == FLUID_STR_TYPE)
    {
      fluid_str_setting_t* setting = (fluid_str_setting_t*) node;
      retval = setting->def;
    }
    else if (node->type == FLUID_INT_TYPE)      /* Handle boolean integers for backwards compatibility */
    {
      fluid_int_setting_t *setting = (fluid_int_setting_t *)node;

      if (setting->hints & FLUID_HINT_TOGGLED)
        retval = setting->def ? "yes" : "no";
    }
  }

  fluid_rec_mutex_unlock (settings->mutex);

  return retval;
}

/**
 * Add an option to a string setting (like an enumeration value).
 * @param settings a settings object
 * @param name a setting's name
 * @param s option string to add
 * @return 1 if the setting exists and option was added, 0 otherwise
 *
 * Causes the setting's #FLUID_HINT_OPTIONLIST hint to be set.
 */
int
fluid_settings_add_option(fluid_settings_t* settings, const char *name, const char *s)
{
  fluid_setting_node_t *node;
  int retval = 0;

  fluid_return_val_if_fail (settings != NULL, 0);
  fluid_return_val_if_fail (name != NULL, 0);
  fluid_return_val_if_fail (name[0] != '\0', 0);
  fluid_return_val_if_fail (s != NULL, 0);

  fluid_rec_mutex_lock (settings->mutex);

  if (fluid_settings_get(settings, name, &node)
      && (node->type == FLUID_STR_TYPE)) {
    fluid_str_setting_t* setting = (fluid_str_setting_t*) node;
    char* copy = FLUID_STRDUP(s);
    setting->options = fluid_list_append(setting->options, copy);
    setting->hints |= FLUID_HINT_OPTIONLIST;
    retval = 1;
  }

  fluid_rec_mutex_unlock (settings->mutex);

  return retval;
}

/**
 * Remove an option previously assigned by fluid_settings_add_option().
 * @param settings a settings object
 * @param name a setting's name
 * @param s option string to remove
 * @return 1 if the setting exists and option was removed, 0 otherwise
 */
int
fluid_settings_remove_option(fluid_settings_t* settings, const char *name, const char* s)
{
  fluid_setting_node_t *node;
  int retval = 0;

  fluid_return_val_if_fail (settings != NULL, 0);
  fluid_return_val_if_fail (name != NULL, 0);
  fluid_return_val_if_fail (name[0] != '\0', 0);
  fluid_return_val_if_fail (s != NULL, 0);

  fluid_rec_mutex_lock (settings->mutex);

  if (fluid_settings_get(settings, name, &node)
      && (node->type == FLUID_STR_TYPE)) {

    fluid_str_setting_t* setting = (fluid_str_setting_t*) node;
    fluid_list_t* list = setting->options;

    while (list) {
      char* option = (char*) fluid_list_get(list);
      if (FLUID_STRCMP(s, option) == 0) {
	FLUID_FREE (option);
	setting->options = fluid_list_remove_link(setting->options, list);
	retval = 1;
        break;
      }
      list = fluid_list_next(list);
    }
  }

  fluid_rec_mutex_unlock (settings->mutex);

  return retval;
}

/**
 * Set a numeric value for a named setting.
 *
 * @param settings a settings object
 * @param name a setting's name
 * @param val new setting's value
 * @return 1 if the value has been set, 0 otherwise
 */
int
fluid_settings_setnum(fluid_settings_t* settings, const char *name, double val)
{
  fluid_setting_node_t *node;
  fluid_num_setting_t* setting;
  int retval = 0;

  fluid_return_val_if_fail (settings != NULL, 0);
  fluid_return_val_if_fail (name != NULL, 0);
  fluid_return_val_if_fail (name[0] != '\0', 0);

  fluid_rec_mutex_lock (settings->mutex);

  if (fluid_settings_get(settings, name, &node)) {
    if (node->type == FLUID_NUM_TYPE) {
      setting = (fluid_num_setting_t*) node;

      if (val < setting->min) val = setting->min;
      else if (val > setting->max) val = setting->max;

      setting->value = val;

      /* Call under lock to keep update() synchronized with the current value */
      if (setting->update) (*setting->update)(setting->data, name, val);
      retval = 1;
    }
  } else {
    /* insert a new setting */
    fluid_num_setting_t* setting;
    setting = new_fluid_num_setting(-1e10, 1e10, 0.0f, 0, NULL, NULL);
    setting->value = val;
    retval = fluid_settings_set(settings, name, setting);
    if (retval != 1) delete_fluid_num_setting (setting);
  }

  fluid_rec_mutex_unlock (settings->mutex);

  return retval;
}

/**
 * Get the numeric value of a named setting
 *
 * @param settings a settings object
 * @param name a setting's name
 * @param val variable pointer to receive the setting's numeric value
 * @return 1 if the value exists, 0 otherwise
 */
int
fluid_settings_getnum(fluid_settings_t* settings, const char *name, double* val)
{
  fluid_setting_node_t *node;
  int retval = 0;

  fluid_return_val_if_fail (settings != NULL, 0);
  fluid_return_val_if_fail (name != NULL, 0);
  fluid_return_val_if_fail (name[0] != '\0', 0);
  fluid_return_val_if_fail (val != NULL, 0);

  fluid_rec_mutex_lock (settings->mutex);

  if (fluid_settings_get(settings, name, &node)
      && (node->type == FLUID_NUM_TYPE)) {
    fluid_num_setting_t* setting = (fluid_num_setting_t*) node;
    *val = setting->value;
    retval = 1;
  }

  fluid_rec_mutex_unlock (settings->mutex);

  return retval;
}

/**
 * Get the range of values of a numeric setting
 *
 * @param settings a settings object
 * @param name a setting's name
 * @param min setting's range lower limit
 * @param max setting's range upper limit
 */
void
fluid_settings_getnum_range(fluid_settings_t* settings, const char *name,
                            double* min, double* max)
{
  fluid_setting_node_t *node;

  fluid_return_if_fail (settings != NULL);
  fluid_return_if_fail (name != NULL);
  fluid_return_if_fail (name[0] != '\0');
  fluid_return_if_fail (min != NULL);
  fluid_return_if_fail (max != NULL);

  fluid_rec_mutex_lock (settings->mutex);

  if (fluid_settings_get(settings, name, &node)
      && (node->type == FLUID_NUM_TYPE)) {
    fluid_num_setting_t* setting = (fluid_num_setting_t*) node;
    *min = setting->min;
    *max = setting->max;
  }

  fluid_rec_mutex_unlock (settings->mutex);
}

/**
 * Get the default value of a named numeric (double) setting
 *
 * @param settings a settings object
 * @param name a setting's name
 * @return the default value if the named setting exists, 0.0f otherwise
 */
double
fluid_settings_getnum_default(fluid_settings_t* settings, const char *name)
{
  fluid_setting_node_t *node;
  double retval = 0.0;

  fluid_return_val_if_fail (settings != NULL, 0.0);
  fluid_return_val_if_fail (name != NULL, 0.0);
  fluid_return_val_if_fail (name[0] != '\0', 0.0);

  fluid_rec_mutex_lock (settings->mutex);

  if (fluid_settings_get(settings, name, &node)
      && (node->type == FLUID_NUM_TYPE)) {
    fluid_num_setting_t* setting = (fluid_num_setting_t*) node;
    retval = setting->def;
  }

  fluid_rec_mutex_unlock (settings->mutex);

  return retval;
}

/**
 * Set an integer value for a setting
 *
 * @param settings a settings object
 * @param name a setting's name
 * @param val new setting's integer value
 * @return 1 if the value has been set, 0 otherwise
 */
int
fluid_settings_setint(fluid_settings_t* settings, const char *name, int val)
{
  fluid_setting_node_t *node;
  fluid_int_setting_t* setting;
  int retval = 0;

  fluid_return_val_if_fail (settings != NULL, 0);
  fluid_return_val_if_fail (name != NULL, 0);
  fluid_return_val_if_fail (name[0] != '\0', 0);

  fluid_rec_mutex_lock (settings->mutex);

  if (fluid_settings_get(settings, name, &node)) {
    if (node->type == FLUID_INT_TYPE) {
      setting = (fluid_int_setting_t*) node;

      if (val < setting->min) val = setting->min;
      else if (val > setting->max) val = setting->max;

      setting->value = val;

      /* Call under lock to keep update() synchronized with the current value */
      if (setting->update) (*setting->update)(setting->data, name, val);
      retval = 1;
    }
  } else {
    /* insert a new setting */
    fluid_int_setting_t* setting;
    setting = new_fluid_int_setting(INT_MIN, INT_MAX, 0, 0, NULL, NULL);
    setting->value = val;
    retval = fluid_settings_set(settings, name, setting);
    if (retval != 1) delete_fluid_int_setting (setting);
  }

  fluid_rec_mutex_unlock (settings->mutex);

  return retval;
}

/**
 * Get an integer value setting.
 *
 * @param settings a settings object
 * @param name a setting's name
 * @param val pointer to a variable to receive the setting's integer value
 * @return 1 if the value exists, 0 otherwise
 */
int
fluid_settings_getint(fluid_settings_t* settings, const char *name, int* val)
{
  fluid_setting_node_t *node;
  int retval = 0;

  fluid_return_val_if_fail (settings != NULL, 0);
  fluid_return_val_if_fail (name != NULL, 0);
  fluid_return_val_if_fail (name[0] != '\0', 0);
  fluid_return_val_if_fail (val != NULL, 0);

  fluid_rec_mutex_lock (settings->mutex);

  if (fluid_settings_get(settings, name, &node)
      && (node->type == FLUID_INT_TYPE)) {
    fluid_int_setting_t* setting = (fluid_int_setting_t*) node;
    *val = setting->value;
    retval = 1;
  }

  fluid_rec_mutex_unlock (settings->mutex);

  return retval;
}

/**
 * Get the range of values of an integer setting
 * @param settings a settings object
 * @param name a setting's name
 * @param min setting's range lower limit
 * @param max setting's range upper limit
 */
void
fluid_settings_getint_range(fluid_settings_t* settings, const char *name,
                            int* min, int* max)
{
  fluid_setting_node_t *node;

  fluid_return_if_fail (settings != NULL);
  fluid_return_if_fail (name != NULL);
  fluid_return_if_fail (name[0] != '\0');
  fluid_return_if_fail (min != NULL);
  fluid_return_if_fail (max != NULL);

  fluid_rec_mutex_lock (settings->mutex);

  if (fluid_settings_get(settings, name, &node)
      && (node->type == FLUID_INT_TYPE)) {
    fluid_int_setting_t* setting = (fluid_int_setting_t*) node;
    *min = setting->min;
    *max = setting->max;
  }

  fluid_rec_mutex_unlock (settings->mutex);
}

/**
 * Get the default value of an integer setting.
 *
 * @param settings a settings object
 * @param name a setting's name
 * @return the setting's default integer value it it exists, zero otherwise
 */
int
fluid_settings_getint_default(fluid_settings_t* settings, const char *name)
{
  fluid_setting_node_t *node;
  int retval = 0;

  fluid_return_val_if_fail (settings != NULL, 0);
  fluid_return_val_if_fail (name != NULL, 0);
  fluid_return_val_if_fail (name[0] != '\0', 0);

  fluid_rec_mutex_lock (settings->mutex);

  if (fluid_settings_get(settings, name, &node)
      && (node->type == FLUID_INT_TYPE)) {
    fluid_int_setting_t* setting = (fluid_int_setting_t*) node;
    retval = setting->def;
  }

  fluid_rec_mutex_unlock (settings->mutex);

  return retval;
}

/**
 * Iterate the available options for a named string setting, calling the provided
 * callback function for each existing option.
 *
 * @param settings a settings object
 * @param name a setting's name
 * @param data any user provided pointer
 * @param func callback function to be called on each iteration
 *
 * NOTE: Starting with FluidSynth 1.1.0 the \a func callback is called for each
 * option in alphabetical order.  Sort order was undefined in previous versions.
 */
void
fluid_settings_foreach_option (fluid_settings_t* settings, const char *name,
                               void* data, fluid_settings_foreach_option_t func)
{
  fluid_setting_node_t *node;
  fluid_str_setting_t *setting;
  fluid_list_t *p, *newlist = NULL;

  fluid_return_if_fail (settings != NULL);
  fluid_return_if_fail (name != NULL);
  fluid_return_if_fail (name[0] != '\0');
  fluid_return_if_fail (func != NULL);

  fluid_rec_mutex_lock (settings->mutex);       /* ++ lock */

  if (!fluid_settings_get (settings, name, &node) || node->type != FLUID_STR_TYPE)
  {
    fluid_rec_mutex_unlock (settings->mutex);   /* -- unlock */
    return;
  }

  setting = (fluid_str_setting_t*)node;

  /* Duplicate option list */
  for (p = setting->options; p; p = p->next)
    newlist = fluid_list_append (newlist, fluid_list_get (p));

  /* Sort by name */
  newlist = fluid_list_sort (newlist, fluid_list_str_compare_func);

  for (p = newlist; p; p = p->next)
    (*func)(data, (char *)name, (char *)fluid_list_get (p));

  fluid_rec_mutex_unlock (settings->mutex);   /* -- unlock */

  delete_fluid_list (newlist);
}

/**
 * Count option string values for a string setting.
 * @param settings a settings object
 * @param name Name of setting
 * @return Count of options for this string setting (0 if none, -1 if not found
 *   or not a string setting)
 * @since 1.1.0
 */
int
fluid_settings_option_count (fluid_settings_t *settings, const char *name)
{
  fluid_setting_node_t *node;
  int count = -1;

  fluid_return_val_if_fail (settings != NULL, -1);
  fluid_return_val_if_fail (name != NULL, -1);
  fluid_return_val_if_fail (name[0] != '\0', -1);

  fluid_rec_mutex_lock (settings->mutex);
  if (fluid_settings_get(settings, name, &node) && node->type == FLUID_STR_TYPE)
    count = fluid_list_size (((fluid_str_setting_t *)node)->options);
  fluid_rec_mutex_unlock (settings->mutex);

  return (count);
}

/**
 * Concatenate options for a string setting together with a separator between.
 * @param settings Settings object
 * @param name Settings name
 * @param separator String to use between options (NULL to use ", ")
 * @return Newly allocated string or NULL on error (out of memory, not a valid
 *   setting \a name or not a string setting).  Free the string when finished with it.
 * @since 1.1.0
 */
char *
fluid_settings_option_concat (fluid_settings_t *settings, const char *name,
                              const char *separator)
{
  fluid_setting_node_t *node;
  fluid_str_setting_t *setting;
  fluid_list_t *p, *newlist = NULL;
  int count, len;
  char *str, *option;

  fluid_return_val_if_fail (settings != NULL, NULL);
  fluid_return_val_if_fail (name != NULL, NULL);
  fluid_return_val_if_fail (name[0] != '\0', NULL);

  if (!separator) separator = ", ";

  fluid_rec_mutex_lock (settings->mutex);       /* ++ lock */

  if (!fluid_settings_get (settings, name, &node) || node->type != FLUID_STR_TYPE)
  {
    fluid_rec_mutex_unlock (settings->mutex);   /* -- unlock */
    return (NULL);
  }

  setting = (fluid_str_setting_t*)node;

  /* Duplicate option list, count options and get total string length */
  for (p = setting->options, count = 0, len = 0; p; p = p->next, count++)
  {
    option = fluid_list_get (p);

    if (option)
    {
      newlist = fluid_list_append (newlist, option);
      len += strlen (option);
    }
  }

  if (count > 1) len += (count - 1) * strlen (separator);
  len++;        /* For terminator */

  /* Sort by name */
  newlist = fluid_list_sort (newlist, fluid_list_str_compare_func);

  str = FLUID_MALLOC (len);

  if (str)
  {
    str[0] = 0;
    for (p = newlist; p; p = p->next)
    {
      option = fluid_list_get (p);
      strcat (str, option);
      if (p->next) strcat (str, separator);
    }
  }

  fluid_rec_mutex_unlock (settings->mutex);   /* -- unlock */

  delete_fluid_list (newlist);

  if (!str) FLUID_LOG (FLUID_ERR, "Out of memory");

  return (str);
}

/* Structure passed to fluid_settings_foreach_iter recursive function */
typedef struct
{
  char path[MAX_SETTINGS_LABEL+1];      /* Maximum settings label length */
  fluid_list_t *names;                  /* For fluid_settings_foreach() */
} fluid_settings_foreach_bag_t;

static int
fluid_settings_foreach_iter (void* key, void* value, void* data)
{
  fluid_settings_foreach_bag_t *bag = data;
  char *keystr = key;
  fluid_setting_node_t *node = value;
  int pathlen;
  char *s;

  pathlen = strlen (bag->path);

  if (pathlen > 0)
  {
    bag->path[pathlen] = '.';
    bag->path[pathlen + 1] = 0;
  }

  strcat (bag->path, keystr);

  switch (node->type) {
  case FLUID_NUM_TYPE:
  case FLUID_INT_TYPE:
  case FLUID_STR_TYPE:
    s = FLUID_STRDUP (bag->path);
    if (s) bag->names = fluid_list_append (bag->names, s);
    break;
  case FLUID_SET_TYPE:
    fluid_hashtable_foreach(((fluid_set_setting_t *)value)->hashtable,
                            fluid_settings_foreach_iter, bag);
    break;
  }

  bag->path[pathlen] = 0;

  return 0;
}

/**
 * Iterate the existing settings defined in a settings object, calling the
 * provided callback function for each setting.
 *
 * @param settings a settings object
 * @param data any user provided pointer
 * @param func callback function to be called on each iteration
 *
 * NOTE: Starting with FluidSynth 1.1.0 the \a func callback is called for each
 * setting in alphabetical order.  Sort order was undefined in previous versions.
 */
void
fluid_settings_foreach (fluid_settings_t* settings, void* data,
                        fluid_settings_foreach_t func)
{
  fluid_settings_foreach_bag_t bag;
  fluid_setting_node_t *node;
  fluid_list_t *p;
  int r;

  fluid_return_if_fail (settings != NULL);
  fluid_return_if_fail (func != NULL);

  bag.path[0] = 0;
  bag.names = NULL;

  fluid_rec_mutex_lock (settings->mutex);

  /* Add all node names to the bag.names list */
  fluid_hashtable_foreach (settings, fluid_settings_foreach_iter, &bag);

  /* Sort names */
  bag.names = fluid_list_sort (bag.names, fluid_list_str_compare_func);

  /* Loop over names and call the callback */
  for (p = bag.names; p; p = p->next)
  {
    r = fluid_settings_get (settings, (char *)(p->data), &node);
    if (r && node) (*func) (data, (char *)(p->data), node->type);
    FLUID_FREE (p->data);       /* -- Free name */
  }

  fluid_rec_mutex_unlock (settings->mutex);

  delete_fluid_list (bag.names);        /* -- Free names list */
}
