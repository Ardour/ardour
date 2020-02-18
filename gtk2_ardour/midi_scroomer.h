/*
 * Copyright (C) 2008-2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_midi_scroomer_h__
#define __ardour_midi_scroomer_h__

#include "widgets/scroomer.h"

class MidiScroomer : public ArdourWidgets::Scroomer
{
public:
	MidiScroomer(Gtk::Adjustment&);
	~MidiScroomer();

	bool on_expose_event(GdkEventExpose*);
	void on_size_request(Gtk::Requisition*);

	void get_colors(double color[], Component comp);
};

#endif /* __ardour_midi_scroomer_h__ */
