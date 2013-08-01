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
    
    PortManager ();
    virtual ~PortManager() {}

    void set_port_engine (PortEngine& pe);
    PortEngine& port_engine() { return *_impl; }

    uint32_t port_name_size() const;
    std::string my_name() const;

    /* Port registration */
    
    boost::shared_ptr<Port> register_input_port (DataType, const std::string& portname);
    boost::shared_ptr<Port> register_output_port (DataType, const std::string& portname);
    int unregister_port (boost::shared_ptr<Port>);
    
    /* Port connectivity */
    
    int  connect (const std::string& source, const std::string& destination);
    int  disconnect (const std::string& source, const std::string& destination);
    int  disconnect (boost::shared_ptr<Port>);
    int  reestablish_ports ();
    int  reconnect_ports ();

    bool  connected (const std::string&);
    bool  connected_to (const std::string&, const std::string&);
    bool  physically_connected (const std::string&);
    int   get_connections (const std::string&, std::vector<std::string>&);

    /* Naming */

    boost::shared_ptr<Port> get_port_by_name (const std::string &);
    void                    port_renamed (const std::string&, const std::string&);
    std::string             make_port_name_relative (const std::string& name) const;
    std::string             make_port_name_non_relative (const std::string& name) const;
    bool                    port_is_mine (const std::string& fullname) const;

    /* other Port management */
    
    bool      port_is_physical (const std::string&) const;
    void      get_physical_outputs (DataType type, std::vector<std::string>&);
    void      get_physical_inputs (DataType type, std::vector<std::string>&);
    ChanCount n_physical_outputs () const;
    ChanCount n_physical_inputs () const;

    int get_ports (const std::string& port_name_pattern, DataType type, PortFlags flags, std::vector<std::string>&);
    
    void remove_all_ports ();
    
    /* per-Port monitoring */
    
    bool can_request_input_monitoring () const;
    void request_input_monitoring (const std::string&, bool) const;
    void ensure_input_monitoring (const std::string&, bool) const;
    
    class PortRegistrationFailure : public std::exception {
      public:
	PortRegistrationFailure (std::string const & why = "")
		: reason (why) {}
	
	~PortRegistrationFailure () throw () {}
	
	const char *what() const throw () { return reason.c_str(); }
	
      private:
	std::string reason;
    };

    /* the port engine will invoke these callbacks when the time is right */
    
    void registration_callback ();
    int graph_order_callback ();
    void connect_callback (const std::string&, const std::string&, bool connection);

    bool port_remove_in_progress() const { return _port_remove_in_progress; }

    /** Emitted if the backend notifies us of a graph order event */
    PBD::Signal0<void> GraphReordered;

    /** Emitted if a Port is registered or unregistered */
    PBD::Signal0<void> PortRegisteredOrUnregistered;
    
    /** Emitted if a Port is connected or disconnected.
     *  The Port parameters are the ports being connected / disconnected, or 0 if they are not known to Ardour.
     *  The std::string parameters are the (long) port names.
     *  The bool parameter is true if ports were connected, or false for disconnected.
     */
    PBD::Signal5<void, boost::weak_ptr<Port>, std::string, boost::weak_ptr<Port>, std::string, bool> PortConnectedOrDisconnected;

  protected:
    boost::shared_ptr<PortEngine> _impl;
    SerializedRCUManager<Ports> ports;
    bool _port_remove_in_progress;

    boost::shared_ptr<Port> register_port (DataType type, const std::string& portname, bool input);
    void port_registration_failure (const std::string& portname);
};
	
} // namespace

#endif /* __libardour_port_manager_h__ */
