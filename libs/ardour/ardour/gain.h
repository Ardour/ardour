/*
    Copyright (C) 2000 Paul Davis 

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

#ifndef __ardour_gain_h__
#define __ardour_gain_h__

#include "ardour.h"
#include "curve.h"

namespace ARDOUR {

struct Gain : public Curve {

    Gain();
    Gain (const Gain&);
    Gain& operator= (const Gain&);

    static void fill_linear_fade_in (Gain& curve, nframes_t frames);
    static void fill_linear_volume_fade_in (Gain& curve, nframes_t frames);
    static void fill_linear_fade_out (Gain& curve, nframes_t frames);
    static void fill_linear_volume_fade_out (Gain& curve, nframes_t frames);

};

} /* namespace ARDOUR */

#endif /* __ardour_gain_h__ */
