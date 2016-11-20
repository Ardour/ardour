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


#ifndef _FLUID_SETTINGS_H
#define _FLUID_SETTINGS_H



/** returns 1 if the option was added, 0 otherwise */
int fluid_settings_add_option(fluid_settings_t* settings, const char* name, const char* s);

/** returns 1 if the option was added, 0 otherwise */
int fluid_settings_remove_option(fluid_settings_t* settings, const char* name, const char* s);


typedef int (*fluid_num_update_t)(void* data, const char* name, double value);
typedef int (*fluid_str_update_t)(void* data, const char* name, const char* value);
typedef int (*fluid_int_update_t)(void* data, const char* name, int value);

/** returns 0 if the value has been registered correctly, non-zero
    otherwise */
int fluid_settings_register_str(fluid_settings_t* settings, char* name, char* def, int hints,
			       fluid_str_update_t fun, void* data);

/** returns 0 if the value has been registered correctly, non-zero
    otherwise */
int fluid_settings_register_num(fluid_settings_t* settings, char* name, double def,
                                double min, double max, int hints,
                                fluid_num_update_t fun, void* data);

/** returns 0 if the value has been registered correctly, non-zero
    otherwise */
int fluid_settings_register_int(fluid_settings_t* settings, char* name, int def,
                                int min, int max, int hints,
                                fluid_int_update_t fun, void* data);


#endif /* _FLUID_SETTINGS_H */
