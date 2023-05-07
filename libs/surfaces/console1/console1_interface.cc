/*
 * Copyright (C) 2023 Holger Dehnhardt <holger@dehnhardt.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <pbd/failed_constructor.h>

#include "control_protocol/control_protocol.h"
#include "console1.h"

using namespace ARDOUR;
using namespace ArdourSurface;

static ControlProtocol*
new_console1 (Session* s)
{
	Console1* console1 = 0;

	try {
		console1 =  new Console1 (*s);
	} catch (failed_constructor& err) {
		delete console1;
		console1 = 0;
		return 0;
	}

    return console1;
}

static void
delete_console1 (ControlProtocol* cp)
{
	delete cp;
}

static ControlProtocolDescriptor console1_descriptor = {
	/* name :              */   "Softube Console1",
	/* id :                */   "uri://ardour.org/surfaces/console1:0",
	/* module :            */   0,
	/* available           */   0,
	/* probe port :        */   0,
	/* match usb           */   0,
	/* initialize :        */   new_console1,
	/* destroy :           */   delete_console1,
};

extern "C" ARDOURSURFACE_API ControlProtocolDescriptor* protocol_descriptor () { return &console1_descriptor; }

