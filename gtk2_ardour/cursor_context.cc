/*
 * Copyright (C) 2014 David Robillard <d@drobilla.net>
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

#include <pbd/error.h>

#include "editing_context.h"
#include "cursor_context.h"

CursorContext::CursorContext(EditingContext& ec, Gdk::Cursor* cursor)
	: editing_context(ec)
	, _index (editing_context.push_canvas_cursor(cursor))
{}

CursorContext::~CursorContext()
{
	if (_index == editing_context._cursor_stack.size() - 1) {
		editing_context.pop_canvas_cursor();
	} else {
		editing_context._cursor_stack[_index] = NULL;
	}
}

CursorContext::Handle
CursorContext::create(EditingContext& ec, Gdk::Cursor* cursor)
{
	return CursorContext::Handle(new CursorContext(ec, cursor));
}

void
CursorContext::change(Gdk::Cursor* cursor)
{
	editing_context._cursor_stack[_index] = cursor;
	if (_index == editing_context._cursor_stack.size() - 1) {
		editing_context.set_canvas_cursor(cursor);
	}
}

void
CursorContext::set(Handle* handle, EditingContext& ec, Gdk::Cursor* cursor)
{
	if (*handle) {
		(*handle)->change(cursor);
	} else {
		*handle = CursorContext::create(ec, cursor);
	}
}
