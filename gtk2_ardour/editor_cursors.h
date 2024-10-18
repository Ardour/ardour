/*
 * Copyright (C) 2010-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2012-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2012-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
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

#pragma once

#include "pbd/signals.h"
#include "ardour/types.h"

#include "canvas/arrow.h"
#include "canvas/line.h"
#include "canvas/types.h"

class EditingContext;

class EditorCursor
{
public:
	EditorCursor (EditingContext&, bool (EditingContext::*)(GdkEvent*,ArdourCanvas::Item*), std::string const &);
	EditorCursor (EditingContext&, std::string const &);
	~EditorCursor ();

	void set_position (samplepos_t);


	void show ();
	void hide ();
	void set_color (Gtkmm2ext::Color);
	void set_sensitive (bool);

	samplepos_t current_sample () const {
		return _current_sample;
	}

	ArdourCanvas::Arrow& canvas_item () {
		return *_canvas_item;
	}

	PBD::Signal<void(samplepos_t)> PositionChanged;

private:
	EditingContext&       _editor;
	ArdourCanvas::Arrow*  _canvas_item;
	samplepos_t           _current_sample;
};

