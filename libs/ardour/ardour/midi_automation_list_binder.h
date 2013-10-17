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

#include "pbd/memento_command.h"
#include "evoral/Parameter.hpp"
#include "ardour/session.h"

namespace ARDOUR {

class MidiSource;
class AutomationList;

/** A class for late-binding a MidiSource and a Parameter to an AutomationList */
class LIBARDOUR_API MidiAutomationListBinder : public MementoCommandBinder<ARDOUR::AutomationList>
{
public:
	MidiAutomationListBinder (boost::shared_ptr<ARDOUR::MidiSource>, Evoral::Parameter);
	MidiAutomationListBinder (XMLNode *, ARDOUR::Session::SourceMap const &);

	ARDOUR::AutomationList* get () const;
	void add_state (XMLNode *);

private:
	boost::shared_ptr<ARDOUR::MidiSource> _source;
	Evoral::Parameter _parameter;
};

}

