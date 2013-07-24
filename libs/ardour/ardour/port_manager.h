/*
    Copyright (C) 2013 Paul Davis

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

#ifndef __libardour_port_manager_h__
#define __libardour_port_manager_h__

#include <vector>
#include <string>
#include <exception>
#include <map>

#include <stdint.h>

#include <boost/shared_ptr.hpp>

#include "pbd/rcu.h"

#include "ardour/chan_count.h"
#include "ardour/port.h"
#include "ardour/port_engine.h"

namespace ARDOUR {

class PortManager 
{
  public:
    typedef std::map<std::string,boost::shared_ptr<Port> > Ports;
    
    PortManager();
    virtual ~PortManager() {}

    PortEngine& port_engine() { return *_impl; }
    
    /* Port registration */
    
    boost::shared_ptr<Port> register_input_port (DataType, const std::string& portname);
    boost::shared_ptr<Port> register_output_port (DataType, const std::string& portname);
    int unregister_port (boost::shared_ptr<Port>);
    
    /* Port connectivity */
    
    int connect (const std::string& source, const std::string& destination);
    int disconnect (const std::string& source, const std::string& destination);
    int disconnect (boost::shared_ptr<Port>);
    bool connected (const std::string&);

    /* other Port management */
    
    bool port_is_physical (const std::string&) const;
    void get_physical_outputs (DataType type, std::vector<std::string>&);
    void get_physical_inputs (DataType type, std::vector<std::string>&);
    boost::shared_ptr<Port> get_port_by_name (const std::string &);
    void port_renamed (const std::string&, const std::string&);
    ChanCount n_physical_outputs () const;
    ChanCount n_physical_inputs () const;
    const char ** get_ports (const std::string& port_name_pattern, const std::string& type_name_pattern, uint32_t flags);
    
    void remove_all_ports ();
    
    /* per-Port monitoring */
    
    bool can_request_input_monitoring () const;
    void request_input_monitoring (const std::string&, bool) const;
    void ensure_input_monitoring (const std::string&, bool) const;

    std::string make_port_name_relative (const std::string&) const;
    std::string make_port_name_non_relative (const std::string&) const;
    bool port_is_mine (const std::string&) const;
    
    class PortRegistrationFailure : public std::exception {
      public:
	PortRegistrationFailure (std::string const & why = "")
		: reason (why) {}
	
	~PortRegistrationFailure () throw () {}
	
	const char *what() const throw () { return reason.c_str(); }
	
      private:
	std::string reason;
    };


  protected:
    PortEngine* _impl;
    SerializedRCUManager<Ports> ports;

    boost::shared_ptr<Port> register_port (DataType type, const std::string& portname, bool input);
    void port_registration_failure (const std::string& portname);
};
	
}

#endif /* __libardour_port_manager_h__ */
