/*
 * Copyright (C) 2009-2014 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014 John Emmas <john@creativepost.co.uk>
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

#ifndef __libpbd_configuration_h__
#define __libpbd_configuration_h__

#include <map>
#include <string>
#include <vector>

#include <boost/function.hpp>
#include "pbd/signals.h"
#include "pbd/stateful.h"
#include "pbd/configuration_variable.h"

class XMLNode;

namespace PBD {

class LIBPBD_API Configuration : public PBD::Stateful
{
  public:
	Configuration() {}
	virtual ~Configuration() {}

	virtual void map_parameters (boost::function<void (std::string)>&) = 0;
	virtual int set_state (XMLNode const &, int) = 0;
	virtual XMLNode & get_state () const = 0;
	virtual XMLNode & get_variables (std::string const & nodename) const = 0;
	virtual void set_variables (XMLNode const &) = 0;

	PBD::Signal1<void,std::string> ParameterChanged;

	typedef std::vector<std::string> Metadata;

	static Metadata const * get_metadata (std::string const &);
	static std::map<std::string,Metadata> all_metadata;
};

} // namespace PBD

#endif /* __libpbd_configuration_h__ */
