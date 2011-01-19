/*
    Copyright (C) 2011 Paul Davis

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

#include "ardour/pannable.h"
#include "ardour/panner.h"
#include "ardour/pan_controllable.h"

using namespace ARDOUR;

double
PanControllable::lower () const
{
        switch (parameter().type()) {
        case PanWidthAutomation:
                return -1.0;
        default:
                return 0.0;
        }
}

void
PanControllable::set_value (double v)
{
        boost::shared_ptr<Panner> p = owner->panner();

        if (!p) {
                /* no panner: just do it */
                AutomationControl::set_value (v);
                return;
        }

        bool can_set = false;

        switch (parameter().type()) {
        case PanWidthAutomation:
                can_set = p->clamp_width (v);
                break;
        case PanAzimuthAutomation:
                can_set = p->clamp_position (v);
                break;
        case PanElevationAutomation:
                can_set = p->clamp_elevation (v);
                break;
        default:
                break;
        }

        if (can_set) {
                AutomationControl::set_value (v);
        }
}

