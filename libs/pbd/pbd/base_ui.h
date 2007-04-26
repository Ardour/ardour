/*
    Copyright (C) 2000-2007 Paul Davis 

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

#ifndef __pbd_base_ui_h__
#define __pbd_base_ui_h__

#include <string>
#include <stdint.h>

#include <sigc++/slot.h>
#include <sigc++/trackable.h>

class BaseUI : virtual public sigc::trackable {
  public:
	BaseUI (std::string name, bool with_signal_pipes);
	virtual ~BaseUI();

	BaseUI* base_instance() { return base_ui_instance; }

	std::string name() const { return _name; }

	bool ok() const { return _ok; }

	enum RequestType {
		range_guarantee = ~0
	};

	struct BaseRequestObject {
	    RequestType type;
	    sigc::slot<void> the_slot;
	};

	static RequestType new_request_type();
	static RequestType CallSlot;

  protected:
	int signal_pipe[2];
	bool _ok; 

  private:
	std::string _name; 
	BaseUI* base_ui_instance;

	static uint32_t rt_bit;

	int setup_signal_pipe ();
};

#endif /* __pbd_base_ui_h__ */
