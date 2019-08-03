/*
 * Copyright (C) 2010-2007 Paul Davis <paul@linuxaudiosystems.com>
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

#include <string>
#include <iostream>


#include <gtkmm/main.h>

#include "widgets/stateful_button.h"

using namespace Gtk;
using namespace Glib;
using namespace ArdourWidgets;
using namespace std;

StateButton::StateButton ()
        : visual_state (0)
        , _self_managed (false)
        , _is_realized (false)
        , style_changing (false)
        , state_before_prelight (Gtk::STATE_NORMAL)
        , is_toggle (false)
{
}

void
StateButton::set_visual_state (int n)
{
	if (!_is_realized) {
		/* not yet realized */
		visual_state = n;
		return;
	}

	if (n == visual_state) {
		return;
	}

	string name = get_widget_name ();
	name = name.substr (0, name.find_last_of ('-'));

	switch (n) {
	case 0:
		/* relax */
		break;
	case 1:
                name += "-active";
		break;

	case 2:
		name += "-alternate";
		break;

        case 3:
                name += "-alternate2";
                break;
	}

	set_widget_name (name);
	visual_state = n;
}

void
StateButton::avoid_prelight_on_style_changed (const Glib::RefPtr<Gtk::Style>& /* old_style */,  GtkWidget* widget)
{
        /* don't go into an endless recursive loop if we're changing
           the style in response to an existing style change.
        */

        if (style_changing) {
                return;
        }

        if (gtk_widget_get_state (widget) == GTK_STATE_PRELIGHT) {

                /* avoid PRELIGHT: make sure that the prelight colors in this new style match
                   the colors of the new style in whatever state we were in
                   before we switched to prelight.
                */

                GtkRcStyle* rcstyle = gtk_widget_get_modifier_style (widget);
                GtkStyle* style = gtk_widget_get_style (widget);

                rcstyle->fg[GTK_STATE_PRELIGHT] = style->fg[state_before_prelight];
                rcstyle->bg[GTK_STATE_PRELIGHT] = style->bg[state_before_prelight];
                rcstyle->color_flags[GTK_STATE_PRELIGHT] = (GtkRcFlags) (GTK_RC_FG|GTK_RC_BG);

                style_changing = true;
                g_object_ref (rcstyle);
                gtk_widget_modify_style (widget, rcstyle);

                Widget* child = get_child_widget();
                if (child) {
                        gtk_widget_modify_style (GTK_WIDGET(child->gobj()), rcstyle);
                }


                g_object_unref (rcstyle);
                style_changing = false;
        }
}

void
StateButton::avoid_prelight_on_state_changed (Gtk::StateType old_state, GtkWidget* widget)
{
        GtkStateType state = gtk_widget_get_state (widget);

        if (state == GTK_STATE_PRELIGHT) {

                state_before_prelight = old_state;


                /* avoid PRELIGHT when currently ACTIVE:
                   if we just went into PRELIGHT, make sure that the colors
                   match those of whatever state we were in before.
                */

                GtkRcStyle* rcstyle = gtk_widget_get_modifier_style (widget);
                GtkStyle* style = gtk_widget_get_style (widget);

                rcstyle->fg[GTK_STATE_PRELIGHT] = style->fg[old_state];
                rcstyle->bg[GTK_STATE_PRELIGHT] = style->bg[old_state];
                rcstyle->color_flags[GTK_STATE_PRELIGHT] = (GtkRcFlags) (GTK_RC_FG|GTK_RC_BG);

                g_object_ref (rcstyle);
                gtk_widget_modify_style (widget, rcstyle);

                Widget* child = get_child_widget ();

                if (child) {
                        gtk_widget_modify_style (GTK_WIDGET(child->gobj()), rcstyle);
                }

                g_object_unref (rcstyle);

        }
}

/* ----------------------------------------------------------------- */

StatefulToggleButton::StatefulToggleButton ()
{
        is_toggle = true;
}

StatefulToggleButton::StatefulToggleButton (const std::string& label)
        : ToggleButton (label)
{
        is_toggle = true;
}

void
StatefulToggleButton::on_realize ()
{
	ToggleButton::on_realize ();

	_is_realized = true;
	visual_state++; // to force transition
	set_visual_state (visual_state - 1);
}

void
StatefulButton::on_realize ()
{
	Button::on_realize ();

	_is_realized = true;
	visual_state++; // to force transition
	set_visual_state (visual_state - 1);
}

void
StatefulToggleButton::on_toggled ()
{
	if (!_self_managed) {
		if (get_active()) {
                        set_state (Gtk::STATE_ACTIVE);
		} else {
                        set_state (Gtk::STATE_NORMAL);
		}
	}
}


void
StatefulToggleButton::on_style_changed (const Glib::RefPtr<Gtk::Style>& style)
{
        avoid_prelight_on_style_changed (style, GTK_WIDGET(gobj()));
        Button::on_style_changed (style);
}

void
StatefulToggleButton::on_state_changed (Gtk::StateType old_state)
{
        avoid_prelight_on_state_changed (old_state, GTK_WIDGET(gobj()));
        Button::on_state_changed (old_state);
}

Widget*
StatefulToggleButton::get_child_widget ()
{
        return get_child();
}

void
StatefulToggleButton::set_widget_name (const std::string& name)
{
	set_name (name);
	Widget* w = get_child();

	if (w) {
		w->set_name (name);
	}
}

/*--------------------------------------------- */

StatefulButton::StatefulButton ()
{
}

StatefulButton::StatefulButton (const std::string& label)
        : Button (label)
{
}

void
StatefulButton::on_style_changed (const Glib::RefPtr<Gtk::Style>& style)
{
        avoid_prelight_on_style_changed (style, GTK_WIDGET(gobj()));
        Button::on_style_changed (style);
}

void
StatefulButton::on_state_changed (Gtk::StateType old_state)
{
        avoid_prelight_on_state_changed (old_state, GTK_WIDGET(gobj()));
        Button::on_state_changed (old_state);
}

Widget*
StatefulButton::get_child_widget ()
{
        return get_child();
}

void
StatefulButton::set_widget_name (const std::string& name)
{
	set_name (name);
	Widget* w = get_child();

	if (w) {
		w->set_name (name);
	}
}
