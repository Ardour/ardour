/*
 * Copyright (C) 2006 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017-2019 Robin Gareus <robin@gareus.org>
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
#include <climits>
#include <iostream>

#include "pbd/controllable.h"
#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/keyboard.h"
#include "widgets/binding_proxy.h"
#include "widgets/popup.h"

#include "pbd/i18n.h"

using namespace std;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace ArdourWidgets;

guint BindingProxy::bind_button = 2;
guint BindingProxy::bind_statemask = Gdk::CONTROL_MASK;

BindingProxy::BindingProxy (boost::shared_ptr<Controllable> c)
	: prompter (0),
	  controllable (c)
{
	if (c) {
		c->DropReferences.connect (
				_controllable_going_away_connection, invalidator (*this),
				boost::bind (&BindingProxy::set_controllable, this, boost::shared_ptr<Controllable> ()),
				gui_context());
	}
}

BindingProxy::BindingProxy ()
	: prompter (0)
{
}

BindingProxy::~BindingProxy ()
{
	if (prompter) {
		delete prompter;
	}
}

void
BindingProxy::set_controllable (boost::shared_ptr<Controllable> c)
{
	learning_finished ();
	controllable = c;

	_controllable_going_away_connection.disconnect ();
	if (c) {
		c->DropReferences.connect (
				_controllable_going_away_connection, invalidator (*this),
				boost::bind (&BindingProxy::set_controllable, this, boost::shared_ptr<Controllable> ()),
				gui_context());
	}
}

void
BindingProxy::set_bind_button_state (guint button, guint statemask)
{
	bind_button = button;
	bind_statemask = statemask;
}

bool
BindingProxy::is_bind_action (GdkEventButton *ev)
{
	return (Keyboard::modifier_state_equals (ev->state, bind_statemask) && ev->button == bind_button );
}

bool
BindingProxy::button_press_handler (GdkEventButton *ev)
{
	if ( controllable && is_bind_action(ev) ) {
		if (Controllable::StartLearning (controllable)) {
			string prompt = _("operate controller now");
			if (prompter == 0) {
				prompter = new PopUp (Gtk::WIN_POS_MOUSE, 30000, false);
				prompter->signal_unmap_event().connect (mem_fun (*this, &BindingProxy::prompter_hiding));
			}
			prompter->set_text (prompt);
			prompter->touch (); // shows popup
			controllable->LearningFinished.connect_same_thread (learning_connection, boost::bind (&BindingProxy::learning_finished, this));
		}
		return true;
	}

	return false;
}

void
BindingProxy::learning_finished ()
{
	learning_connection.disconnect ();
	if (prompter) {
		prompter->touch (); // hides popup
	}
}

bool
BindingProxy::prompter_hiding (GdkEventAny* /*ev*/)
{
	learning_connection.disconnect ();
	if (controllable) {
		Controllable::StopLearning (controllable);
	}
	return false;
}
