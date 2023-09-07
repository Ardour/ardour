/*
 * Copyright (C) 2023 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2023 Ben Loftis <ben@harrisonaudio.com>
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

#ifndef __gtk_ardour_section_box_h__
#define __gtk_ardour_section_box_h__

#include "ardour/types.h"

#include "canvas/rectangle.h"
#include "canvas/types.h"

class Editor;

class SectionBox : public ArdourCanvas::Rectangle
{
public:
	SectionBox (Editor&, ArdourCanvas::Item*);

	void set_position (samplepos_t, samplepos_t);

private:
	Editor& _editor;
};

#endif // __gtk_ardour_editor_cursors_h__
