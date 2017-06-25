/*
    Copyright (C) 2009-2013 Paul Davis
    Author: Johannes Mueller

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

#include <gtkmm/label.h>

#include "pbd/i18n.h"
#include "button_config_widget.h"

using namespace std;
using namespace Gtk;
using namespace ArdourSurface;

ButtonConfigWidget::ButtonConfigWidget ()
	: HBox ()
	, _choice_jump (_("Jump"))
	, _choice_action (_("Other action"))
	, _jump_distance (ShuttleproControlProtocol::JumpDistance ({ .value = 1, .unit = ShuttleproControlProtocol::BEATS }))
{
	RadioButtonGroup cbg = _choice_jump.get_group ();
	_choice_action.set_group (cbg);

	_choice_jump.signal_toggled().connect (boost::bind (&ButtonConfigWidget::update_choice, this));

	pack_start (_choice_jump);
	pack_start (_jump_distance);
	pack_start (_choice_action);
}

void
ButtonConfigWidget::update_choice ()
{
	_jump_distance.set_sensitive (_choice_jump.get_active ());
}
