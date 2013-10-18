/* 
   Copyright (C) 2007 Paul Davis

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

#ifndef __lib_pbd_functor_command_h__
#define __lib_pbd_functor_command_h__

#include <iostream>
#include <sstream>
#include <string>
#include <map>

#include "pbd/libpbd_visibility.h"
#include "pbd/xml++.h"
#include "pbd/shiva.h"
#include "pbd/command.h"
#include "pbd/failed_constructor.h"

/** This command class is initialized 
 */

namespace PBD {

template <class obj_type, class arg_type>
class LIBPBD_API FunctorCommand : public Command
{
	private:
	typedef void (obj_type::*functor_type)(arg_type);
	typedef std::map< std::string, functor_type > FunctorMap;
	typedef typename FunctorMap::iterator FunctorMapIterator;

	public:
	FunctorCommand(std::string functor, obj_type& object, arg_type b, arg_type a) 
		: functor_name(functor)
		, object(object)
		, before(b)
		, after(a) 
	{
		method = find_functor(functor);

		/* catch destruction of the object */
		new PBD::Shiva< obj_type, FunctorCommand<obj_type, arg_type> > (object, *this);
	}

	~FunctorCommand() {
		GoingAway();
	}

	void operator() () {
		(object.*method) (after);
	}

	void undo() { 
		(object.*method) (before);
	}

	virtual XMLNode &get_state() {
		std::stringstream ss;
		
		XMLNode *node = new XMLNode("FunctorCommand");
		node->add_property("type_name", typeid(obj_type).name());
		node->add_property("functor", functor_name);
		ss << before;
		node->add_property("before", ss.str());
		ss.clear ();
		ss << after;
		node->add_property("after", ss.str());

		return *node;
	}

	static void register_functor(std::string name, functor_type f) {
		functor_map[name] = f;
	}

	private:
	static functor_type find_functor(std::string name) {
		FunctorMapIterator iter;

		if((iter = functor_map.find(name)) == functor_map.end()) {
			throw failed_constructor();
		}

		return iter->second;
	}

	protected:
	std::string functor_name;
	obj_type &object;
	arg_type before;
	arg_type after;
	functor_type method;
	static FunctorMap functor_map;
};

// static initialization of functor_map... 
template <class obj_type, class arg_type>
typename FunctorCommand<obj_type, arg_type>::FunctorMap
FunctorCommand<obj_type, arg_type>::functor_map;

};

#endif // __lib_pbd_functor_command_h__

