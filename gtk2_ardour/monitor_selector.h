/*
    Copyright (C) 2002-2007 Paul Davis

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

#ifndef __gtkardour_monitor_output_selector_h__
#define __gtkardour_monitor_output_selector_h__

#include "port_matrix.h"
#include "ardour_window.h"

class MonitorSelector : public PortMatrix
{
  public:
	MonitorSelector (Gtk::Window*, ARDOUR::Session *, boost::shared_ptr<ARDOUR::IO>);

	void set_state (ARDOUR::BundleChannel c[2], bool);
	PortMatrixNode::State get_state (ARDOUR::BundleChannel c[2]) const;

	std::string disassociation_verb () const;
	std::string channel_noun () const;

        ARDOUR::Session* session() const { return _session; }

	uint32_t n_io_ports () const;
	boost::shared_ptr<ARDOUR::IO> const io () { return _io; }
	void setup_ports (int);
	bool list_is_global (int) const;

	bool find_inputs_for_io_outputs () const {
		return _find_inputs_for_io_outputs;
	}

	int ours () const {
		return _ours;
	}

	int other () const {
		return _other;
	}

	bool can_add_channels (boost::shared_ptr<ARDOUR::Bundle>) const { return false; }
	bool can_remove_channels (boost::shared_ptr<ARDOUR::Bundle>) const { return false; }
	bool can_rename_channels (boost::shared_ptr<ARDOUR::Bundle>) const { return false; }

  private:

	void io_changed ();
	void io_changed_proxy ();

	int _other;
	int _ours;
	boost::shared_ptr<ARDOUR::IO> _io;
	boost::shared_ptr<PortGroup> _port_group;
	bool _find_inputs_for_io_outputs;
	PBD::ScopedConnection _io_connection;
};

class MonitorSelectorWindow : public ArdourWindow
{
  public:
	MonitorSelectorWindow (ARDOUR::Session *, boost::shared_ptr<ARDOUR::IO>, bool can_cancel = false);

	MonitorSelector& selector() { return _selector; }

  protected:
	void on_map ();
	void on_show ();

  private:
	MonitorSelector _selector;

	void io_name_changed (void *src);
	bool wm_delete (GdkEventAny*);
};

#endif /* __gtkardour_monitor_output_selector_h__ */
