/*
 * Copyright (C) 2025 Paul Davis <paul@linuxaudiosystems.com>
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

#include "pbd/compose.h"

#include "pianoroll.h"
#include "pianoroll_window.h"

using namespace ARDOUR;

PianorollWindow::PianorollWindow (std::string const & name)
	: ArdourWindow (string_compose ("%1 - %2", PROGRAM_NAME, name))
	, pianoroll (new Pianoroll (name))
{
	pianoroll->viewport().set_size_request (600, 120);

	add (pianoroll->toolbox());
	pianoroll->toolbox().show ();
}

PianorollWindow::~PianorollWindow ()
{
	delete pianoroll;
}
