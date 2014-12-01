/*
    Copyright (C) 2010 Paul Davis

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

#include "ardour/midi_automation_list_binder.h"
#include "ardour/automation_list.h"
#include "ardour/automation_control.h"
#include "ardour/midi_source.h"
#include "ardour/midi_model.h"

using namespace ARDOUR;

MidiAutomationListBinder::MidiAutomationListBinder (boost::shared_ptr<MidiSource> s, Evoral::Parameter p)
	: _source (s)
	, _parameter (p)
{

}

MidiAutomationListBinder::MidiAutomationListBinder (XMLNode* node, Session::SourceMap const & sources)
	: _parameter (0, 0, 0)
{
	XMLProperty* source = node->property ("source-id");
	assert (source);

	XMLProperty* parameter = node->property ("parameter");
	assert (parameter);

	Session::SourceMap::const_iterator i = sources.find (PBD::ID (source->value()));
	assert (i != sources.end());
	_source = boost::dynamic_pointer_cast<MidiSource> (i->second);

	_parameter = EventTypeMap::instance().from_symbol (parameter->value());
}

AutomationList*
MidiAutomationListBinder::get () const
{
	boost::shared_ptr<MidiModel> model = _source->model ();
	assert (model);

	boost::shared_ptr<AutomationControl> control = model->automation_control (_parameter);
	assert (control);

	return control->alist().get();
}

void
MidiAutomationListBinder::add_state (XMLNode* node)
{
	node->add_property ("source-id", _source->id().to_s());
	node->add_property ("parameter", EventTypeMap::instance().to_symbol (_parameter));
}
