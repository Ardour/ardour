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

#ifndef __ardour_export_channel_configuration_h__
#define __ardour_export_channel_configuration_h__

#include <list>
#include <string>
#include <algorithm>

#include <boost/enable_shared_from_this.hpp>

#include "ardour/export_channel.h"
#include "ardour/export_pointers.h"

#include "pbd/xml++.h"

namespace ARDOUR
{

class Session;

class LIBARDOUR_API ExportChannelConfiguration : public boost::enable_shared_from_this<ExportChannelConfiguration>
{

  private:
	friend class ExportElementFactory;
	ExportChannelConfiguration (Session & session);

  public:
	bool operator== (ExportChannelConfiguration const & other) const { return channels == other.channels; }
	bool operator!= (ExportChannelConfiguration const & other) const { return channels != other.channels; }

	XMLNode & get_state ();
	int set_state (const XMLNode &);

	typedef std::list<ExportChannelPtr> ChannelList;

	ChannelList const & get_channels () const { return channels; }
	bool all_channels_have_ports () const;

	std::string name () const { return _name; }
	void set_name (std::string name) { _name = name; }
	void set_split (bool value) { split = value; }

	RegionExportChannelFactory::Type region_processing_type() const { return region_type; }
	void set_region_processing_type(RegionExportChannelFactory::Type type) { region_type = type; }

	bool get_split () const { return split; }
	uint32_t get_n_chans () const { return channels.size(); }

	void register_channel (ExportChannelPtr channel) { channels.push_back (channel); }
	void register_channels (ChannelList const & new_channels) {
		std::copy (new_channels.begin(), new_channels.end(), std::back_inserter(channels));
	}
	void clear_channels () { channels.clear (); }

	/** Returns a list of channel configurations that match the files created.
	  * I.e. many configurations if splitting is enabled, one if not. */
	void configurations_for_files (std::list<boost::shared_ptr<ExportChannelConfiguration> > & configs);

  private:

	Session & session;

	ChannelList     channels;
	bool            split; // Split to mono files
	std::string  _name;
	RegionExportChannelFactory::Type region_type;
};

} // namespace ARDOUR

#endif /* __ardour_export_channel_configuration_h__ */
