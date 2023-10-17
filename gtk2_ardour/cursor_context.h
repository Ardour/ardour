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

#ifndef __ardour_gtk_cursor_context_h__
#define __ardour_gtk_cursor_context_h__

#include <memory>

#include <gdkmm/cursor.h>

class EditingContext;

/**
   A scoped handle for changing the editor mouse cursor.

   This is a safe way to change the cursor that ensures it is only modified in
   a strict stack-like fashion.  Whenever this handle goes out of scope, the
   cursor is restored to the previous one.

   This is not quite entirely fool-proof, there is one case to be careful of:
   if a cursor context handle exists, to change it, you must first reset that
   handle (destroying the context) then set it.  Assigning a new context to a
   non-NULL handle will create the new context (pushing a cursor), then destroy
   the old one, which would attempt to pop a non-top context which is an
   error.  To account for this, when replacing a possibly existing context, use
   set() which will automatically do the right thing.
*/
class CursorContext
{
public:
	/** A smart handle for a cursor change context. */
	typedef std::shared_ptr<CursorContext> Handle;

	~CursorContext();

	/** Change the editor cursor and return a cursor context handle.
	 *
	 * When the returned handle goes out of scope, the cursor will be reset to
	 * the previous value.
	 */
	static Handle create(EditingContext&, Gdk::Cursor* cursor);

	/** Change the editor cursor of an existing cursor context. */
	void change(Gdk::Cursor* cursor);

	/** Set a context handle to a new context.
	 *
	 * If the handle points to an existing context, it will first be reset
	 * before the new context is created.
	 */
	static void set(Handle* handle, EditingContext&, Gdk::Cursor* cursor);

private:
	EditingContext& editing_context;
	size_t       _index;

	CursorContext(EditingContext&, Gdk::Cursor* cursor);
};

#endif /* __ardour_gtk_cursor_context_h__ */
