/*
    Copyright (C) 1998-99 Paul Barton-Davis 

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

    $Id$
*/

#ifndef __pbd_abstract_ui_h__
#define __pbd_abstract_ui_h__

#include <pbd/receiver.h>
#include <sigc++/sigc++.h>

class Touchable;

class AbstractUI : public Receiver
{
  public:
	enum RequestType {
		ErrorMessage,
		Quit,
		CallSlot,
		CallSlotLocked,
		TouchDisplay,
		StateChange,
		SetTip,
		AddIdle,
		AddTimeout,
	};

	bool ok() { return _ok; }

	AbstractUI () {}
	virtual ~AbstractUI() {}

	virtual void run (Receiver &old_receiver) = 0;
	virtual void quit    () = 0;
	virtual bool running () = 0;
	virtual void request (RequestType) = 0;
	virtual void touch_display (Touchable *) = 0;
	virtual void call_slot (sigc::slot<void>) = 0;
	virtual bool caller_is_gui_thread() = 0;

	/* needed to be a receiver ... */

	virtual void receive (Transmitter::Channel, const char *) = 0;
	
  protected:
	bool _ok;
};

#endif // __pbd_abstract_ui_h__


