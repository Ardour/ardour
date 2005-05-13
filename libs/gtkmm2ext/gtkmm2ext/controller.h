/*
    Copyright (C) 1998-99 Paul Davis 
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

#ifndef __gtkmm2ext_controller_h__
#define __gtkmm2ext_controller_h__

#include <gtkmm.h>
#include <gtkmm2ext/popup.h>
#include <midi++/controllable.h>

namespace Gtkmm2ext {

class Controller : public MIDI::Controllable

{
  public:
	Controller (Gtk::Adjustment *, MIDI::Port *);
        virtual ~Controller () {}
	
	void set_value (float);
	float lower () { return adjustment->get_lower(); }
	float upper () { return adjustment->get_upper(); }
	float range () { return upper() - lower() /* XXX +1 ??? */ ; }

	void midicontrol_prompt ();
	void midicontrol_unprompt ();

  protected:
	Gtk::Adjustment *adjustment;

  private:
	Gtkmm2ext::PopUp prompter;
	gfloat new_value;
	bool new_value_pending;

	static gint update_controller_value (void *);
};

}; /* namespace */

#endif // __gtkmm2ext_controller_h__		


