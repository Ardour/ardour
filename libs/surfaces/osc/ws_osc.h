/*
 * Copyright (C) 2017 Paul Davis
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef ardour_wsosc_h
#define ardour_wsosc_h

#include <string>
#include <vector>
#include <stdint.h>
#include <libwebsockets.h>

#include <glibmm.h>

namespace ArdourSurface {

class OSC;

class WSOSCServer
{
  public:
	WSOSCServer (OSC*, int32_t port, std::string const & cert_path, std::string const & key_path);
	~WSOSCServer ();

  private:
	OSC* osc;
	int port;
	std::string cert_path;
	std::string key_path;
	struct lws_context* context;

	typedef std::vector<Glib::RefPtr<Glib::IOSource> > Sources;
	Sources sources;

	void add_fd (int fd);
	bool event_handler (Glib::IOCondition, int);
};


} // namespace

#endif // ardour_wsosc_h
