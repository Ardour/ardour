/*
    Copyright (C) 2017 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __libpbd_position_types_h__
#define __libpbd_position_types_h__

#include <stdint.h>

namespace Temporal {

/* Any position measured in audio samples.
   Assumed to be non-negative but not enforced.
*/
typedef int64_t samplepos_t;

/* Any distance from a given samplepos_t.
   Maybe positive or negative.
*/
typedef int64_t sampleoffset_t;

/* Any count of audio samples.
   Assumed to be positive but not enforced.
*/
typedef int64_t samplecnt_t;

static const samplepos_t max_samplepos = INT64_MAX;
static const samplecnt_t max_samplecnt = INT64_MAX;

}

#endif /* __libpbd_position_types_h__ */
