/*
    Copyright (C) 2007 Paul Davis 
    Author: Dave Robillard

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

#include "pbd/memento_command.h"
#include "ardour/automation_control.h"
#include "ardour/event_type_map.h"
#include "ardour/session.h"
#include "ardour/source.h"
#include "automation_region_view.h"
#include "public_editor.h"

#include "i18n.h"

AutomationRegionView::AutomationRegionView(ArdourCanvas::Group*                      parent,
                                           AutomationTimeAxisView&                   time_axis,
                                           boost::shared_ptr<ARDOUR::Region>         region,
                                           const Evoral::Parameter&                  param,
                                           boost::shared_ptr<ARDOUR::AutomationList> list,
                                           double                                    spu,
                                           Gdk::Color const &                        basic_color)
	: RegionView(parent, time_axis, region, spu, basic_color)
	, _parameter(param)
{ 
	if (list) {
		assert(list->parameter() == param);
		create_line(list);
	}
	
	group->signal_event().connect (mem_fun (this, &AutomationRegionView::canvas_event), false);
}

void
AutomationRegionView::init (Gdk::Color const & basic_color, bool /*wfd*/)
{
	_enable_display = false;
	
	RegionView::init(basic_color, false);

	compute_colors (basic_color);

	reset_width_dependent_items ((double) _region->length() / samples_per_unit);

	set_height (trackview.current_height());

	_region->StateChanged.connect (mem_fun(*this, &AutomationRegionView::region_changed));

	set_colors ();

	_enable_display = true;
}

void
AutomationRegionView::create_line (boost::shared_ptr<ARDOUR::AutomationList> list)
{
	_line = boost::shared_ptr<AutomationLine>(new AutomationLine(
				ARDOUR::EventTypeMap::instance().to_symbol(list->parameter()),
				trackview, *get_canvas_group(), list, &_time_converter));
	_line->set_colors();
	_line->set_interpolation(list->interpolation());
	_line->show();
	_line->show_all_control_points();
	_line->set_height ((uint32_t)rint(trackview.current_height() - NAME_HIGHLIGHT_SIZE));
}

bool
AutomationRegionView::canvas_event(GdkEvent* ev)
{
	if (ev->type == GDK_BUTTON_RELEASE) {

		const nframes_t when = trackview.editor().pixel_to_frame((nframes_t)ev->button.x)
			- _region->position();
		add_automation_event(ev, when, ev->button.y);
	}

	return false;
}

void
AutomationRegionView::add_automation_event (GdkEvent* /*event*/, nframes_t when, double y)
{
	if (!_line) {
		boost::shared_ptr<Evoral::Control> c = _region->control(_parameter, true);
		boost::shared_ptr<ARDOUR::AutomationControl> ac
				= boost::dynamic_pointer_cast<ARDOUR::AutomationControl>(c);
		assert(ac);
		create_line(ac->alist());
	}
	assert(_line);

	double x = 0;
	AutomationTimeAxisView* const view = automation_view();

	view->canvas_display()->w2i (x, y);

	/* compute vertical fractional position */

	const double h = trackview.current_height() - TimeAxisViewItem::NAME_HIGHLIGHT_SIZE - 2;
	y = 1.0 - (y / h);

	/* map using line */

	_line->view_to_model_coord (x, y);

	view->session().begin_reversible_command (_("add automation event"));
	XMLNode& before = _line->the_list()->get_state();

	_line->the_list()->add (when, y);

	XMLNode& after = _line->the_list()->get_state();
	view->session().commit_reversible_command (new MementoCommand<ARDOUR::AutomationList>(
			*_line->the_list(), &before, &after));

	view->session().set_dirty ();
}

void
AutomationRegionView::set_height (double h)
{
	RegionView::set_height(h);

	if (_line)
		_line->set_height ((uint32_t)rint(h - NAME_HIGHLIGHT_SIZE));
}

bool
AutomationRegionView::set_position (nframes64_t pos, void* src, double* ignored)
{
	return RegionView::set_position(pos, src, ignored);
}


void
AutomationRegionView::reset_width_dependent_items (double pixel_width)
{
	RegionView::reset_width_dependent_items(pixel_width);
	
	if (_line)
		_line->reset();
}


void
AutomationRegionView::region_resized (ARDOUR::Change what_changed)
{
	RegionView::region_resized(what_changed);

	if (_line)
		_line->reset();
}


void
AutomationRegionView::entered()
{
	if (_line)
		_line->track_entered();
}


void
AutomationRegionView::exited()
{
	if (_line)
		_line->track_exited();
}

