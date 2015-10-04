/*
 *   Copyright (C) 2006 Paul Davis
 *   Copyright (C) 2007 Michael Taht
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *   */

/* The most common header files that the tranzport uses */

#ifndef ardour_tranzport_common
#define ardour_tranzport_common
#include <iostream>
#include <algorithm>
#include <cmath>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <float.h>
#include <sys/time.h>
#include <errno.h>

#include "pbd/pthread_utils.h"

#include "ardour/route.h"
#include "ardour/audio_track.h"
#include "ardour/rc_configuration.h"
#include "ardour/tempo.h"
#include "ardour/location.h"
#include "ardour/dB.h"

#endif /* ardour_tranzport_common */
