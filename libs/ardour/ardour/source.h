/*
    Copyright (C) 2000 Paul Davis 

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

    $Id$
*/

#ifndef __ardour_source_h__
#define __ardour_source_h__

#include <string>

#include <sigc++/signal.h>

#include <ardour/ardour.h>
#include <ardour/stateful.h>

namespace ARDOUR {

class Source : public Stateful, public sigc::trackable
{
  public:
	Source (std::string name);
	Source (const XMLNode&);
	virtual ~Source ();

	std::string name() const { return _name; }
	int set_name (std::string str, bool destructive);

	ARDOUR::id_t  id() const   { return _id; }

	uint32_t use_cnt() const { return _use_cnt; }
	void use ();
	void release ();

	time_t timestamp() const { return _timestamp; }
	void stamp (time_t when) { _timestamp = when; }

	XMLNode& get_state ();
	int set_state (const XMLNode&);

	sigc::signal<void,Source *> GoingAway;

  protected:
	string            _name;
	uint32_t          _use_cnt;
	time_t            _timestamp;

  private:
	ARDOUR::id_t _id;
};

}

#endif /* __ardour_source_h__ */
