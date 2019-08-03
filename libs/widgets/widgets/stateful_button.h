/*
 * Copyright (C) 2005 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#ifndef _WIDGETS_STATEFUL_BUTTON_H_
#define _WIDGETS_STATEFUL_BUTTON_H_

#include <vector>

#include <gtkmm/togglebutton.h>

#include "widgets/visibility.h"

namespace ArdourWidgets {

class LIBWIDGETS_API StateButton
{
public:
	StateButton();
	virtual ~StateButton() {}

	void set_visual_state (int);
	int  get_visual_state () { return visual_state; }
	void set_self_managed (bool yn) { _self_managed = yn; }
	virtual void set_widget_name (const std::string& name) = 0;

protected:
	int  visual_state;
	bool _self_managed;
	bool _is_realized;
	bool style_changing;
	Gtk::StateType state_before_prelight;
	bool is_toggle;

	virtual std::string  get_widget_name() const = 0;
	virtual Gtk::Widget* get_child_widget () = 0;

	void avoid_prelight_on_style_changed (const Glib::RefPtr<Gtk::Style>& style, GtkWidget* widget);
	void avoid_prelight_on_state_changed (Gtk::StateType old_state, GtkWidget* widget);
};


class LIBWIDGETS_API StatefulToggleButton : public StateButton, public Gtk::ToggleButton
{
public:
	StatefulToggleButton();
	explicit StatefulToggleButton(const std::string &label);
	~StatefulToggleButton() {}
	void set_widget_name (const std::string& name);

protected:
	void on_realize ();
	void on_toggled ();
	void on_style_changed (const Glib::RefPtr<Gtk::Style>& style);
	void on_state_changed (Gtk::StateType old_state);

	Gtk::Widget* get_child_widget ();
	std::string get_widget_name() const { return get_name(); }
};

class LIBWIDGETS_API StatefulButton : public StateButton, public Gtk::Button
{
public:
	StatefulButton();
	explicit StatefulButton(const std::string &label);
	virtual ~StatefulButton() {}
	void set_widget_name (const std::string& name);

protected:
	void on_realize ();
	void on_style_changed (const Glib::RefPtr<Gtk::Style>& style);
	void on_state_changed (Gtk::StateType old_state);

	Gtk::Widget* get_child_widget ();
	std::string get_widget_name() const { return get_name(); }
};

} /* end namespace */

#endif
