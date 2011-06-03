/*
    Copyright (C) 2011 Paul Davis

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

#include "pbd/compose.h"

#include "gtkmm2ext/cairocell.h"
#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/utils.h"

#include "ardour/location.h"
#include "ardour/session.h"

#include "time_info_box.h"
#include "audio_clock.h"
#include "editor.h"

#include "i18n.h"

using namespace Gtk;
using namespace ARDOUR;

TimeInfoBox::TimeInfoBox ()
	: Table (4, 4)
{
	selection_start = new AudioClock ("selection-start", false, "SelectionClockDisplay", false, false, false, false);
	selection_end = new AudioClock ("selection-end", false, "SelectionClockDisplay", false, false, false, false);
	selection_length = new AudioClock ("selection-length", false, "SelectionClockDisplay", false, false, true, false);

	punch_start = new AudioClock ("punch-start", false, "PunchClockDisplay", false, false, false, false);
	punch_end = new AudioClock ("punch-end", false, "PunchClockDisplay", false, false, false, false);

	CairoEditableText& ss (selection_start->main_display());
	ss.set_ypad (1);
	ss.set_xpad (1);
	ss.set_corner_radius (0);
	ss.set_draw_background (false);

	CairoEditableText& se (selection_end->main_display());
	se.set_ypad (1);
	se.set_xpad (1);
	se.set_corner_radius (0);
	se.set_draw_background (false);

	CairoEditableText& sl (selection_length->main_display());
	sl.set_ypad (1);
	sl.set_xpad (2);
	sl.set_corner_radius (0);
	sl.set_draw_background (false);

	CairoEditableText& ps (punch_start->main_display());
	ps.set_ypad (1);
	ps.set_xpad (2);
	ps.set_corner_radius (0);
	ps.set_draw_background (false);

	CairoEditableText& pe (punch_end->main_display());
	pe.set_ypad (1);
	pe.set_xpad (2);
	pe.set_corner_radius (0);
	pe.set_draw_background (false);

	selection_title.set_markup (string_compose ("<span size=\"x-small\">%1</span>", _("Selection")));
	punch_title.set_markup (string_compose ("<span size=\"x-small\">%1</span>", _("Punch")));

	set_homogeneous (false);
	set_spacings (0);

	Gtk::Label* l;

	attach (selection_title, 0, 2, 0, 1);
	l = manage (new Label);
	l->set_markup (string_compose ("<span size=\"x-small\">%1</span>", _("Start")));
        attach (*l, 0, 1, 1, 2);
        attach (*selection_start, 1, 2, 1, 2);
	l = manage (new Label);
	l->set_markup (string_compose ("<span size=\"x-small\">%1</span>", _("End")));
        attach (*l, 0, 1, 2, 3);
        attach (*selection_end, 1, 2, 2, 3);
	l = manage (new Label);
	l->set_markup (string_compose ("<span size=\"x-small\">%1</span>", _("Length")));
        attach (*l, 0, 1, 3, 4);
        attach (*selection_length, 1, 2, 3, 4);

	attach (punch_title, 2, 4, 0, 1);
	l = manage (new Label);
	l->set_markup (string_compose ("<span size=\"x-small\">%1</span>", _("Start")));
        attach (*l, 2, 3, 1, 2);
        attach (*punch_start, 3, 4, 1, 2);
	l = manage (new Label);
	l->set_markup (string_compose ("<span size=\"x-small\">%1</span>", _("End")));
        attach (*l, 2, 3, 2, 3);
        attach (*punch_end, 3, 4, 2, 3);

        show_all ();

	Editor::instance().get_selection().TimeChanged.connect (sigc::mem_fun (*this, &TimeInfoBox::selection_changed));
}

TimeInfoBox::~TimeInfoBox ()
{
        delete selection_length;
        delete selection_end;
        delete selection_start;
        
        delete punch_start;
        delete punch_end;
}

void
TimeInfoBox::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);

	selection_start->set_session (s);
	selection_end->set_session (s);
	selection_length->set_session (s);

	punch_start->set_session (s);
	punch_end->set_session (s);

	if (s) {
		Location* punch = s->locations()->auto_punch_location ();
		
		if (punch) {
			watch_punch (punch);
		}
		
		_session->auto_punch_location_changed.connect (_session_connections, MISSING_INVALIDATOR, 
							       boost::bind (&TimeInfoBox::punch_location_changed, this, _1), gui_context());
	}
}

void
TimeInfoBox::selection_changed ()
{
	selection_start->set (Editor::instance().get_selection().time.start());
	selection_end->set (Editor::instance().get_selection().time.end_frame());
	selection_length->set (Editor::instance().get_selection().time.length());
}

void
TimeInfoBox::punch_location_changed (Location* loc)
{
	if (loc) {
		watch_punch (loc);
	} 
}

void
TimeInfoBox::watch_punch (Location* punch)
{
	punch_connections.drop_connections ();

	punch->start_changed.connect (punch_connections, MISSING_INVALIDATOR, boost::bind (&TimeInfoBox::punch_changed, this, _1), gui_context());
	punch->end_changed.connect (punch_connections, MISSING_INVALIDATOR, boost::bind (&TimeInfoBox::punch_changed, this, _1), gui_context());

	punch_changed (punch);
}

void
TimeInfoBox::punch_changed (Location* loc)
{
	if (!loc) {
		punch_start->set (99999999);
		punch_end->set (999999999);
		return;
	}

	punch_start->set (loc->start());
	punch_end->set (loc->end());
}	

bool
TimeInfoBox::on_expose_event (GdkEventExpose* ev)
{
	Table::on_expose_event (ev);

	{
		Cairo::RefPtr<Cairo::Context> context = get_window()->create_cairo_context();
		
		context->rectangle (ev->area.x, ev->area.y, ev->area.width, ev->area.height);
		context->clip ();
		
		context->set_source_rgba (0.01, 0.02, 0.21, 1.0);
		Gtkmm2ext::rounded_rectangle (context, 0, 0, get_allocation().get_width(), get_allocation().get_height(), 5);
		context->fill ();
	}

	return false;
}
