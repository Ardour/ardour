/*
 * Copyright (C) 2021 Robin Gareus <robin@gareus.org>
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

#ifndef _ardour_monitor_port_h_
#define _ardour_monitor_port_h_

#include <boost/shared_ptr.hpp>
#include <set>

#include "zita-resampler/vmresampler.h"

#include "pbd/rcu.h"

#include "ardour/audio_buffer.h"
#include "ardour/port_engine.h"

namespace ARDOUR {

class LIBARDOUR_API MonitorPort : public boost::noncopyable
{
public:
	~MonitorPort ();

	void set_buffer_size (pframes_t);
	bool silent () const;
	AudioBuffer& get_audio_buffer (pframes_t);

	void add_port (std::string const&);
	void remove_port (std::string const&, bool instantly = false);
	bool monitoring (std::string const& = "") const;
	void active_monitors (std::list <std::string>&) const;
	void set_active_monitors (std::list <std::string> const&);
	void clear_ports (bool instantly);

	PBD::Signal2<void, std::string, bool> MonitorInputChanged;

protected:
	friend class PortManager;
	MonitorPort ();
	void monitor (PortEngine&, pframes_t);

private:
	struct MonitorInfo {
		MonitorInfo ()
		{
			gain = 0;
			remove = false;
		}

		float gain;
		bool  remove;
	};

	void collect (boost::shared_ptr<MonitorInfo>, Sample*, pframes_t, std::string const&);
	void finalize (pframes_t);

	typedef std::map<std::string, boost::shared_ptr<MonitorInfo> > MonitorPorts;
	SerializedRCUManager<MonitorPorts> _monitor_ports;

	AudioBuffer*            _buffer;
	ArdourZita::VMResampler _src;
	Sample*                 _input;
	Sample*                 _data;
	pframes_t               _insize;
	bool                    _silent;
};

}

#endif
