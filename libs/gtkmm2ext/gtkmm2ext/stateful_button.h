/*
    Copyright (C) 2005 Paul Davis

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

#ifndef __pbd_gtkmm_abutton_h__
#define __pbd_gtkmm_abutton_h__

#include <vector>

#include <gtkmm/togglebutton.h>

namespace Gtkmm2ext {

class StateButton 
{
   public:
	StateButton();
	virtual ~StateButton() {}

	void set_visual_state (int);
	int  get_visual_state () { return visual_state; }
	void set_self_managed (bool yn) { _self_managed = yn; }

  protected:
	int  visual_state;
	bool _self_managed;
	bool _is_realized;

	virtual std::string get_widget_name() const = 0;
	virtual void set_widget_name (std::string) = 0;
};


class StatefulToggleButton : public StateButton, public Gtk::ToggleButton
{
   public:
	StatefulToggleButton() {}
	explicit StatefulToggleButton(const std::string &label) : Gtk::ToggleButton (label) {}
	~StatefulToggleButton() {}

  protected:
	void on_realize ();
	void on_toggled ();

	std::string get_widget_name() const { return get_name(); }
	void set_widget_name (std::string name) { set_name (name); get_child()->set_name (name); }
};

class StatefulButton : public StateButton, public Gtk::Button
{
   public:
	StatefulButton() {}
	explicit StatefulButton(const std::string &label) : Gtk::Button (label) {}
	virtual ~StatefulButton() {}

  protected:
	void on_realize ();

	std::string get_widget_name() const { return get_name(); }
	void set_widget_name (std::string name) { set_name (name); get_child()->set_name (name); }
};

};

#endif
