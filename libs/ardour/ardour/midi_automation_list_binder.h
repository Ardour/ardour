/*
 * Copyright (C) 2010-2011 Carl Hetherington <carl@carlh.net>
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

#ifndef __ardour_midi_automation_list_binder_h__
#define __ardour_midi_automation_list_binder_h__

#include "pbd/memento_command.h"
#include "evoral/Parameter.h"
#include "ardour/session.h"

namespace ARDOUR {

class MidiSource;
class AutomationList;

/** A class for late-binding a MidiSource and a Parameter to an AutomationList */
class LIBARDOUR_API MidiAutomationListBinder : public MementoCommandBinder<ARDOUR::AutomationList>
{
public:
	MidiAutomationListBinder (ARDOUR::MidiSource&, Evoral::Parameter);
	MidiAutomationListBinder (XMLNode *, ARDOUR::Session::SourceMap const &);

	void set_state (XMLNode const & node , int version) const;
	XMLNode& get_state () const;
	std::string type_name() const;

	void add_state (XMLNode *);

	void source_died () {
		std::cerr << "Source died, drop binder\n";
		/* The source we are binding died, so drop references to ourselves */
		this->drop_references ();
	}

private:
	ARDOUR::MidiSource*  _source;
	Evoral::Parameter _parameter;
	PBD::ScopedConnection source_death_connection;
};

} // namespace ARDOUR

#endif // __ardour_midi_automation_list_binder_h__
