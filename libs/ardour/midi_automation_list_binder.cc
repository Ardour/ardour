/*
 * Copyright (C) 2010-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2011-2014 David Robillard <d@drobilla.net>
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
	std::string id_str;
	std::string parameter_str;
	if (!node->get_property ("source-id", id_str) ||
	    !node->get_property ("parameter", parameter_str)) {
		assert (false);
	}

	Session::SourceMap::const_iterator i = sources.find (PBD::ID (id_str));
	assert (i != sources.end());
	_source = boost::dynamic_pointer_cast<MidiSource> (i->second);

	_parameter = EventTypeMap::instance().from_symbol (parameter_str);
}

void
MidiAutomationListBinder::set_state (XMLNode const & node, int version) const
{
	boost::shared_ptr<MidiModel> model = _source->model ();
	assert (model);

	boost::shared_ptr<AutomationControl> control = model->automation_control (_parameter);
	assert (control);

	control->alist().get()->set_state (node, version);
}

XMLNode&
MidiAutomationListBinder::get_state () const
{
	boost::shared_ptr<MidiModel> model = _source->model ();
	assert (model);

	boost::shared_ptr<AutomationControl> control = model->automation_control (_parameter);
	assert (control);

	return control->alist().get()->get_state ();
}

std::string
MidiAutomationListBinder::type_name() const
{
	boost::shared_ptr<MidiModel> model = _source->model ();
	assert (model);

	boost::shared_ptr<AutomationControl> control = model->automation_control (_parameter);
	assert (control);

	return PBD::demangled_name (*control->alist().get());
}


void
MidiAutomationListBinder::add_state (XMLNode* node)
{
	node->set_property ("source-id", _source->id().to_s());
	node->set_property ("parameter", EventTypeMap::instance().to_symbol (_parameter));
}
