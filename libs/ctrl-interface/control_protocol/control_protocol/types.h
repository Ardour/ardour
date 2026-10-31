/*
 * Copyright (C) 2012-2016 Paul Davis <paul@linuxaudiosystems.com>
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

#pragma once

#include <string>
#include <memory>
#include <vector>

#include "pbd/stateful.h"

#include "control_protocol/visibility.h"

class XMLNode;

namespace ARDOUR {

class Route;
class Stripable;
class ControlProtocolInfo;
class ControlProtocolManager;

class LIBCONTROLCP_API CPDeviceIdentifier
{
  public:

  CPDeviceIdentifier (std::string const & p, std::string const & m, std::string const & d)
  : protocol_name (p)
  , manufacturer (m)
  , device_name (d)
  {}
  virtual ~CPDeviceIdentifier() {}

  std::string protocol_name;
  std::string manufacturer;
  std::string device_name;

  bool operator< (CPDeviceIdentifier const & other) const {
	  if (protocol_name < other.protocol_name) {
		  return true;
	  } else if (protocol_name > other.protocol_name) {
		  return false;
	  } else if (manufacturer < other.manufacturer) {
		  return true;
	  } else if (manufacturer > other.manufacturer) {
		  return false;
	  } else if (device_name < other.device_name) {
		  return true;
	  }
	  return false;
  }
  bool operator== (CPDeviceIdentifier const & other) const { return protocol_name == other.protocol_name && manufacturer == other.manufacturer && device_name == other.device_name; }
  bool operator!= (CPDeviceIdentifier const & other) const { return protocol_name != other.protocol_name && manufacturer != other.manufacturer && device_name != other.device_name; }

  std::string to_string() const;

  XMLNode& get_state () const;
  int set_state (XMLNode const &, int);
};

enum CPRequestedStatus {
	Unrequested,
	RequestedOff,
	RequestedOn,
};

typedef std::vector<std::weak_ptr<ARDOUR::Route> >    RouteNotificationList;
typedef std::shared_ptr<RouteNotificationList>        RouteNotificationListPtr;
typedef std::vector<std::shared_ptr<ARDOUR::Route> >  StrongRouteNotificationList;

typedef std::vector<std::weak_ptr<ARDOUR::Stripable> >    StripableNotificationList;
typedef std::shared_ptr<StripableNotificationList>        StripableNotificationListPtr;
typedef std::vector<std::shared_ptr<ARDOUR::Stripable> >  StrongStripableNotificationList;

} /* namespace */

std::ostream & operator<< (std::ostream & o, ARDOUR::CPDeviceIdentifier const &);
