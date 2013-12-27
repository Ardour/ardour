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

*/

#ifndef __qui_popup_h__
#define __qui_popup_h__

#include <string>
#include <gtkmm.h>

#include <pbd/touchable.h>

#include "gtkmm2ext/visibility.h"

namespace Gtkmm2ext {

class LIBGTKMM2EXT_API PopUp : public Gtk::Window, public Touchable
{
  public:
	PopUp (Gtk::WindowPosition pos, unsigned int show_for_msecs = 0,
	       bool delete_on_hide = false);
	virtual ~PopUp ();
	void touch ();
	void remove ();
	void set_text (std::string);
	void set_name (std::string);
	gint button_click (GdkEventButton *);

	bool on_delete_event (GdkEventAny* );

  protected:
	void on_realize ();

  private:
	Gtk::Label label;
	std::string my_text;
	gint timeout;
	static gint remove_prompt_timeout (void *);
	bool delete_on_hide;
	unsigned int popdown_time;

};

} /* namespace */

#endif  // __qui_popup_h__
