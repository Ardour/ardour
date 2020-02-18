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

#ifndef _WIDGETS_BINDING_PROXY_
#define _WIDGETS_BINDING_PROXY_

#include <string>
#include <boost/shared_ptr.hpp>

#include "pbd/signals.h"

#include "widgets/visibility.h"

namespace PBD {
	class Controllable;
}

namespace ArdourWidgets {
	class PopUp;
}

namespace ArdourWidgets {

class LIBWIDGETS_API BindingProxy : public sigc::trackable
{
public:
	BindingProxy (boost::shared_ptr<PBD::Controllable>);
	BindingProxy ();
	virtual ~BindingProxy();

	void set_bind_button_state (guint button, guint statemask);

	static bool is_bind_action (GdkEventButton *);
	bool button_press_handler (GdkEventButton *);

	boost::shared_ptr<PBD::Controllable> get_controllable() const { return controllable; }
	void set_controllable (boost::shared_ptr<PBD::Controllable>);

protected:
	ArdourWidgets::PopUp* prompter;
	boost::shared_ptr<PBD::Controllable> controllable;

	static guint bind_button;
	static guint bind_statemask;

	PBD::ScopedConnection learning_connection;
	PBD::ScopedConnection _controllable_going_away_connection;
	void learning_finished ();
	bool prompter_hiding (GdkEventAny *);
};

}

#endif
