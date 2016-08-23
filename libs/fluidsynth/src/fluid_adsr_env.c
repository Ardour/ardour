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

#include "fluid_adsr_env.h"

void 
fluid_adsr_env_set_data(fluid_adsr_env_t* env,
                        fluid_adsr_env_section_t section,
                        unsigned int count,
                        fluid_real_t coeff,
                        fluid_real_t increment,
                        fluid_real_t min,
                        fluid_real_t max)
{
  env->data[section].count = count;
  env->data[section].coeff = coeff;
  env->data[section].increment = increment;
  env->data[section].min = min;
  env->data[section].max = max;
}

