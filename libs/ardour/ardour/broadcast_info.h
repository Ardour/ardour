/*
    Copyright (C) 2008 Paul Davis
    Author: Sakari Bergen

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

#ifndef __ardour_broadcast_info_h__
#define __ardour_broadcast_info_h__

#include <string>
#include <ctime>

#include <sndfile.h>

#include "ardour/types.h"

namespace ARDOUR
{

class Session;

class BroadcastInfo
{
  public:

	/// Construct empty broadcast info
	BroadcastInfo ();
	~BroadcastInfo ();
	
	/// Returns last error sring from libsndfile
	std::string get_error () const { return error; }
	
	/* Convenience functions */
	
	void set_from_session (Session const & session, int64_t time_ref);
	
	/* Reading */
	
	bool load_from_file (std::string const & filename);
	bool load_from_file (SNDFILE* sf);
	
	std::string get_description () const;
	int64_t get_time_reference () const;
	struct tm get_origination_time () const;
	std::string get_originator () const;
	std::string get_originator_ref () const;
	
	/* Writing */
	
	bool write_to_file (std::string const & filename);
	bool write_to_file (SNDFILE* sf);
	
	void set_description (std::string const & desc);
	void set_time_reference (int64_t when);
	void set_origination_time (struct tm * now = 0); // if 0, use time generated at construction
	void set_originator (std::string const & str = "");
	void set_originator_ref (std::string const & str = "");
	
	/* State info */
	
	/// Returns true if a info has been succesfully loaded or anything has been manually set
	bool has_info () const { return _has_info; }

  private:

	SF_BROADCAST_INFO * info;
	struct tm _time;
	
	void update_error ();
	std::string error;
	
	bool _has_info;
};



} // namespace ARDOUR

#endif /* __ardour_broadcast_info_h__ */
