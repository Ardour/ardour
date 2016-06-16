/*
    Copyright (C) 2016 Paul Davis

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

#ifndef __ardour_push2_h__
#define __ardour_push2_h__

#include <vector>
#include <map>
#include <list>
#include <set>

#include <libusb.h>

#define ABSTRACT_UI_EXPORTS
#include "pbd/abstract_ui.h"
#include "midi++/types.h"
#include "ardour/types.h"
#include "control_protocol/control_protocol.h"

#define ABLETON 0x2982
#define PUSH2   0x1967

namespace ArdourSurface {

struct Push2Request : public BaseUI::BaseRequestObject {
public:
	Push2Request () {}
	~Push2Request () {}
};

class Push2 : public ARDOUR::ControlProtocol
            , public AbstractUI<Push2Request>
{
   public:
	Push2 (ARDOUR::Session&);
	~Push2 ();

	static bool probe ();
	static void* request_factory (uint32_t);

   private:
	libusb_device_handle *handle;
	void do_request (Push2Request*);
	int stop ();
};


} /* namespace */

#endif /* __ardour_push2_h__ */
