/*
    Copyright (C) 2014 Paul Davis
    Author: David Robillard

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

#include <pbd/error.h>

#include "editor.h"
#include "cursor_context.h"

CursorContext::CursorContext(Editor& editor, Gdk::Cursor* cursor)
	: _editor(editor)
	, _index(editor.push_canvas_cursor(cursor))
{}

CursorContext::~CursorContext()
{
	if (_index == _editor._cursor_stack.size() - 1) {
		_editor.pop_canvas_cursor();
	} else {
		_editor._cursor_stack[_index] = NULL;
	}
}

CursorContext::Handle
CursorContext::create(Editor& editor, Gdk::Cursor* cursor)
{
	return CursorContext::Handle(new CursorContext(editor, cursor));
}

void
CursorContext::change(Gdk::Cursor* cursor)
{
	_editor._cursor_stack[_index] = cursor;
	if (_index == _editor._cursor_stack.size() - 1) {
		_editor.set_canvas_cursor(cursor);
	}
}

void
CursorContext::set(Handle* handle, Editor& editor, Gdk::Cursor* cursor)
{
	if (*handle) {
		(*handle)->change(cursor);
	} else {
		*handle = CursorContext::create(editor, cursor);
	}
}
