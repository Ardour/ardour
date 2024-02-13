/*
 * Copyright (C) 2005-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
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

#include "public_editor.h"

#include "pbd/i18n.h"

PublicEditor* PublicEditor::_instance = 0;

const int PublicEditor::window_border_width = 12;
const int PublicEditor::container_border_width = 12;
const int PublicEditor::vertical_spacing = 6;
const int PublicEditor::horizontal_spacing = 6;

ARDOUR::DataType PublicEditor::pbdid_dragged_dt = ARDOUR::DataType::NIL;

PublicEditor::PublicEditor (Gtk::Widget& content)
	: Tabbable (content, _("Editor"), X_("editor"))
	, EditingContext (X_("Editor"))
{
	_suspend_route_redisplay_counter.store (0);
}

PublicEditor::~PublicEditor()
{
}

