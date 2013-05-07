/*
    Copyright (C) 2000-2007 Paul Davis

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

#include "public_editor.h"

PublicEditor* PublicEditor::_instance = 0;

const int PublicEditor::window_border_width = 12;
const int PublicEditor::container_border_width = 12;
const int PublicEditor::vertical_spacing = 6;
const int PublicEditor::horizontal_spacing = 6;

sigc::signal<void> PublicEditor::DropDownKeys;

PublicEditor::PublicEditor ()
  	: Window (Gtk::WINDOW_TOPLEVEL)
	, VisibilityTracker (*((Gtk::Window*)this))
{
}

PublicEditor::~PublicEditor()
{
}

