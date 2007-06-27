/*
    Copyright (C) 2003 Paul Davis 

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

#include <ardour/processor.h>
#include <ardour/session.h>
#include <cstdlib>
#include <pbd/memento_command.h>

#include "processor_automation_time_axis.h"
#include "automation_line.h"
#include "canvas_impl.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;

ProcessorAutomationTimeAxisView::ProcessorAutomationTimeAxisView (Session& s, boost::shared_ptr<Route> r, 
								PublicEditor& e, TimeAxisView& parent, Canvas& canvas, std::string n,
								ParamID param, Processor& proc, string state_name)

	: AxisView (s),
	  AutomationTimeAxisView (s, r, e, parent, canvas, n, state_name, proc.name()),
	  _processor(proc),
	  _param (param)
	
{
	char buf[32];
	_xml_node = 0;
	_marked_for_display = false;
	
	ensure_xml_node ();

	XMLNodeList kids;
	XMLNodeConstIterator iter;

	kids = _xml_node->children ();

	snprintf (buf, sizeof(buf), "Port_%" PRIu32, param.id());
		
	for (iter = kids.begin(); iter != kids.end(); ++iter) {
		if ((*iter)->name() == buf) {
		
			XMLProperty *shown = (*iter)->property("shown_editor");
			
			if (shown && shown->value() == "yes") {
				_marked_for_display = true;
			}
			break;
		}
	}
}

ProcessorAutomationTimeAxisView::~ProcessorAutomationTimeAxisView ()
{
}

void
ProcessorAutomationTimeAxisView::add_automation_event (ArdourCanvas::Item* item, GdkEvent* event, nframes_t when, double y)
{
	double x = 0;

	canvas_display->w2i (x, y);

	/* compute vertical fractional position */

	if (y < 0)
		y = 0;
	else if (y > height)
		y = height;
	
	y = 1.0 - (y / height);

	/* map to model space */

	if (!lines.empty()) {
		AutomationList* alist (_processor.automation_list(_param, true));
		string description = _("add automation event to ");
		description += _processor.describe_parameter (_param);

		lines.front()->view_to_model_y (y);
		
		_session.begin_reversible_command (description);
		XMLNode &before = alist->get_state();
		alist->add (when, y);
		XMLNode &after = alist->get_state();
		_session.add_command(new MementoCommand<AutomationList>(*alist, &before, &after));
		_session.commit_reversible_command ();
		_session.set_dirty ();
	}
}

void
ProcessorAutomationTimeAxisView::ensure_xml_node ()
{
	if (_xml_node == 0) {
		if ((_xml_node = _processor.extra_xml ("GUI")) == 0) {
			_xml_node = new XMLNode ("GUI");
			_processor.add_extra_xml (*_xml_node);
		}
	}
}

guint32
ProcessorAutomationTimeAxisView::show_at (double y, int& nth, Gtk::VBox *parent)
{
	ensure_xml_node ();
	update_extra_xml_shown (true);
	
	return TimeAxisView::show_at (y, nth, parent);
}

void
ProcessorAutomationTimeAxisView::hide ()
{
	ensure_xml_node ();
	update_extra_xml_shown (false);

	TimeAxisView::hide ();
}


void
ProcessorAutomationTimeAxisView::update_extra_xml_shown (bool editor_shown)
{
	char buf[32];

	XMLNodeList nlist = _xml_node->children ();
	XMLNodeConstIterator i;
	XMLNode * port_node = 0;

	snprintf (buf, sizeof(buf), "Port_%" PRIu32, _param.id());

	for (i = nlist.begin(); i != nlist.end(); ++i) {
		if ((*i)->name() == buf) {
			port_node = (*i);
			break;
		}
	}

	if (!port_node) {
		port_node = new XMLNode(buf);
		_xml_node->add_child_nocopy(*port_node);
	}
	
	port_node->add_property ("shown_editor", editor_shown ? "yes": "no");
	
}

void
ProcessorAutomationTimeAxisView::set_automation_state (AutoState state)
{
	if (!ignore_state_request) {
		_processor.automation_list (_param, true)->set_automation_state (state);
	}
}

