/*
    Copyright (C) 2006 Paul Davis
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

#include <string>
#include <climits>
#include <iostream>

#include <pbd/controllable.h>

#include <gtkmm2ext/binding_proxy.h>

#include "i18n.h"

using namespace Gtkmm2ext;
using namespace std;
using namespace PBD;

BindingProxy::BindingProxy (boost::shared_ptr<Controllable> c)
	: prompter (0),
	  controllable (c),
	  bind_button (2),
	  bind_statemask (Gdk::CONTROL_MASK)

{			  
}

BindingProxy::BindingProxy ()
	: prompter (0),
	  bind_button (2),
	  bind_statemask (Gdk::CONTROL_MASK)

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
}

void
BindingProxy::set_bind_button_state (guint button, guint statemask)
{
	bind_button = button;
	bind_statemask = statemask;
}

void
BindingProxy::get_bind_button_state (guint &button, guint &statemask)
{
	button = bind_button;
	statemask = bind_statemask;
}

bool
BindingProxy::button_press_handler (GdkEventButton *ev)
{
	if (controllable && (ev->state & bind_statemask) && ev->button == bind_button) { 
		if (Controllable::StartLearning (controllable.get())) {
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
		Controllable::StopLearning (controllable.get());
	}
	return false;
}

