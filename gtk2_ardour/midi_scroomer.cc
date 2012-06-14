/*
    Copyright (C) 2008 Paul Davis

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

#include "midi_scroomer.h"

#include <cairomm/context.h>

#include <iostream>

using namespace Gtkmm2ext;
using namespace Gtk;
using namespace std;

//std::map<int, Glib::RefPtr<Gdk::Pixmap> > MidiScroomer::piano_pixmaps;

MidiScroomer::MidiScroomer(Adjustment& adj)
	: Gtkmm2ext::Scroomer(adj)
{

	adj.set_lower(0);
	adj.set_upper(127);

	/* set minimum view range to one octave */
	set_min_page_size(12);
}

MidiScroomer::~MidiScroomer()
{
}

bool
MidiScroomer::on_expose_event(GdkEventExpose* ev)
{
	Cairo::RefPtr<Cairo::Context> cc = get_window()->create_cairo_context();
	GdkRectangle comp_rect, clip_rect;
	Component first_comp = point_in(ev->area.y);
	Component last_comp = point_in(ev->area.y + ev->area.height);
	int height = get_height();
	int lnote, hnote;
	double y2note = (double) 127 / height;
	double note2y = (double) height / 127;
	double note_width = 0.8 * get_width();
	double note_height = 1.4 * note2y;
	double black_shift = 0.1 * note2y;
	double colors[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

	//cerr << ev->area.y << " " << ev->area.height << endl;

	comp_rect.x = 0;
	comp_rect.width = get_width();

	for (int i = first_comp; i <= last_comp; ++i) {
		Component comp = (Component) i;
		set_comp_rect(comp_rect, comp);

		if (gdk_rectangle_intersect(&comp_rect, &ev->area, &clip_rect)) {
			get_colors(colors, comp);

			cc->rectangle(clip_rect.x, clip_rect.y, clip_rect.width, clip_rect.height);
			cc->set_source_rgb (colors[3], colors[4], colors[5]);
			cc->fill_preserve();
			cc->clip();

			cc->set_source_rgb(colors[0], colors[1], colors[2]);
			cc->set_line_width(note_height);

			lnote = 127 - (int) floor((double) (clip_rect.y + clip_rect.height) * y2note) - 1;
			hnote = 127 - (int) floor((double) clip_rect.y * y2note) + 1;

			for (int note = lnote; note < hnote + 1; ++note) {
				double y = height - note * note2y;
				bool draw = false;

				switch (note % 12) {
				case 1:
				case 6:
					y -= black_shift;
					draw = true;
					break;
				case 3:
				case 10:
					y += black_shift;
					draw = true;
					break;
				case 8:
					draw = true;
					break;
				default:
					break;
				}

				if(draw) {
					cc->set_line_width(1.4 * note2y);
					cc->move_to(0, y);
					cc->line_to(note_width, y);
					cc->stroke();
				}
			}

			if (i == Handle1 || i == Handle2) {
				cc->rectangle(comp_rect.x + 0.5f, comp_rect.y + 0.5f, comp_rect.width - 1.0f, comp_rect.height - 1.0f);
				cc->set_line_width(1.0f);
				cc->set_source_rgb (1.0f, 1.0f, 1.0f);
				cc->stroke();
			}

			cc->reset_clip();
		}
	}

	return true;
}

void
MidiScroomer::get_colors(double color[], Component comp)
{
	switch (comp) {
	case TopBase:
	case BottomBase:
		color[0] = 0.24f;
		color[1] = 0.24f;
		color[2] = 0.24f;
		color[3] = 0.33f;
		color[4] = 0.33f;
		color[5] = 0.33f;
		break;
	case Handle1:
	case Handle2:
		color[0] = 0.91f;
		color[1] = 0.91f;
		color[2] = 0.91f;
		color[3] = 0.0f;
		color[4] = 0.0f;
		color[5] = 0.0f;
		break;
	case Slider:
		color[0] = 0.38f;
		color[1] = 0.38f;
		color[2] = 0.38f;
		color[3] = 0.77f;
		color[4] = 0.77f;
		color[5] = 0.77f;
		break;
	default:
		break;
	}
}

void
MidiScroomer::on_size_request(Gtk::Requisition* r)
{
	r->width = 12;
}
