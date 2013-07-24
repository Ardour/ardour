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

namespace ARDOUR {

class Port;

class PortManager 
{
  public:
    typedef std::map<std::string,boost::shared_ptr<Port> > Ports;
    
    PortManager();
    virtual ~PortManager() {}
    
    /* Port registration */
    
    virtual boost::shared_ptr<Port> register_input_port (DataType, const std::string& portname) = 0;
    virtual boost::shared_ptr<Port> register_output_port (DataType, const std::string& portname) = 0;
    virtual int unregister_port (boost::shared_ptr<Port>) = 0;
    
    /* Port connectivity */
    
    virtual int connect (const std::string& source, const std::string& destination) = 0;
    virtual int disconnect (const std::string& source, const std::string& destination) = 0;
    virtual int disconnect (boost::shared_ptr<Port>) = 0;
    
    /* other Port management */
    
    virtual bool port_is_physical (const std::string&) const = 0;
    virtual void get_physical_outputs (DataType type, std::vector<std::string>&) = 0;
    virtual void get_physical_inputs (DataType type, std::vector<std::string>&) = 0;
    virtual boost::shared_ptr<Port> get_port_by_name (const std::string &) = 0;
    virtual void port_renamed (const std::string&, const std::string&) = 0;
    virtual ChanCount n_physical_outputs () const = 0;
    virtual ChanCount n_physical_inputs () const = 0;
    virtual const char ** get_ports (const std::string& port_name_pattern, const std::string& type_name_pattern, uint32_t flags) = 0;
    
    void remove_all_ports ();
    
    /* per-Port monitoring */
    
    virtual bool can_request_input_monitoring () const = 0;
    virtual void request_input_monitoring (const std::string&, bool) const = 0;
    
    class PortRegistrationFailure : public std::exception {
      public:
	PortRegistrationFailure (std::string const & why = "")
		: reason (why) {}
	
	~PortRegistrationFailure () throw () {}
	
	virtual const char *what() const throw () { return reason.c_str(); }
	
      private:
	std::string reason;
    };

  protected:
    typedef void* PortHandle;
    PortHandle register (const std::string&, DataType type, Port::Flags);
    void  unregister (PortHandle);
    bool  connected (PortHandle);
    int   disconnect_all (PortHandle);
    bool  connected_to (PortHandle, const std::string);
    int   get_connections (PortHandle, std::vector<std::string>&);

  private:
    SerializedRCUManager<Ports> ports;
    boost::shared_ptr<Port> register_port (DataType type, const std::string& portname, bool input);
    void port_registration_failure (const std::string& portname);
};
	
}

#endif /* __libardour_port_manager_h__ */
