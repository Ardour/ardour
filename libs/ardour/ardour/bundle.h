/*
    Copyright (C) 2002 Paul Davis 

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

#ifndef __ardour_bundle_h__
#define __ardour_bundle_h__

#include <vector>
#include <string>
#include <sigc++/signal.h>
#include <glibmm/thread.h>
#include <pbd/stateful.h> 

using std::vector;
using std::string;

namespace ARDOUR {

/**
 *  A set of `channels', each of which is associated with 0 or more
 *  JACK ports.
 */

class Bundle : public PBD::Stateful, public sigc::trackable {
  public:
	/**
	 *  Bundle constructor.
	 *  @param name Name for this Bundle.
	 *  @param dy true if this Bundle is `dynamic', ie it is created on-the-fly
	 *  and should not be written to the session file.
	 */
	Bundle (string name, bool dy = false) : _name (name), _dynamic(dy) {}
	~Bundle() {}

	/// A vector of JACK port names
	typedef vector<string> PortList;

	void set_name (string name, void *src);

	/**
	 *  @return name of this Bundle.
	 */
	string name() const { return _name; }

	/**
	 *  @return true if this Bundle is marked as `dynamic', meaning
	 *  that it won't be written to the session file.
	 */
	bool dynamic() const { return _dynamic; }

	/**
	 *  @return Number of channels that this Bundle has.
	 */
	uint32_t nchannels () const { return _channels.size(); }
	const PortList& channel_ports (int ch) const;

	void set_nchannels (int n);

	void add_port_to_channel (int ch, string portname);
	void remove_port_from_channel (int ch, string portname);

	/// Our name changed
	sigc::signal<void, void*> NameChanged;
	/// The number of channels changed
	sigc::signal<void> ConfigurationChanged;
	/// The ports associated with one of our channels changed
	sigc::signal<void, int> PortsChanged;

	bool operator==(const Bundle& other) const;

	XMLNode& get_state (void);
	int set_state (const XMLNode&);

  protected:
	Bundle (const XMLNode&);

  private:
	mutable Glib::Mutex channels_lock; ///< mutex for _channels
	vector<PortList> _channels; ///< list of JACK ports associated with each of our channels
	string _name; ///< name
	bool _dynamic; ///< true if `dynamic', ie not to be written to the session file

	int set_channels (const string& str);
	int parse_io_string (const string& str, vector<string>& ports);
};

/**
 *  Bundle in which the JACK ports are inputs.
 */
  
class InputBundle : public Bundle {
  public:
	/**
	 *  InputBundle constructor.
	 *  \param name Name.
	 *  \param dy true if this Bundle is `dynamic'; ie it is created on-the-fly
	 *  and should not be written to the session file.
	 */
	InputBundle (string name, bool dy = false) : Bundle (name, dy) {}
	InputBundle (const XMLNode&);
};

/**
 *  Bundle in which the JACK ports are outputs.
 */
  
class OutputBundle : public Bundle {
  public:
	/**
	 *  OutputBundle constructor.
	 *  \param name Name.
	 *  \param dy true if this Bundle is `dynamic'; ie it is created on-the-fly
	 *  and should not be written to the session file.
	 */  
        OutputBundle (string name, bool dy = false) : Bundle (name, dy) {}
	OutputBundle (const XMLNode&);
};

}

#endif /* __ardour_bundle_h__ */

