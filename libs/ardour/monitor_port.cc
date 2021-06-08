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

#include <cassert>

#include "pbd/malign.h"

#include "ardour/audioengine.h"
#include "ardour/audio_buffer.h"
#include "ardour/monitor_port.h"
#include "ardour/port.h"
#include "ardour/rc_configuration.h"
#include "ardour/runtime_functions.h"
#include "ardour/session.h"

#define GAIN_COEFF_DELTA (1e-5)

using namespace ARDOUR;
using namespace std;

MonitorPort::MonitorPort ()
	: _monitor_ports (new MonitorPorts)
	, _buffer (new AudioBuffer (0))
	, _input (0)
	, _data (0)
	, _insize (0)
	, _silent (false)
{
	_src.setup (Port::resampler_quality ());
	_src.set_rrfilt (10);
}

MonitorPort::~MonitorPort ()
{
	if (_input) {
		cache_aligned_free (_input);
	}
	if (_data) {
		cache_aligned_free (_data);
	}
	delete _buffer;
}

void
MonitorPort::set_buffer_size (pframes_t n_samples)
{
	if (_input) {
		cache_aligned_free (_input);
	}
	if (_data) {
		cache_aligned_free (_data);
	}
	cache_aligned_malloc ((void**) &_input, sizeof (Sample) * n_samples);
	cache_aligned_malloc ((void**) &_data, sizeof (Sample) * lrint (floor (n_samples * Config->get_max_transport_speed ())));
	_insize = n_samples;
	_silent = false;
}

bool
MonitorPort::silent () const
{
	return _silent;
}

void
MonitorPort::monitor (PortEngine& e, pframes_t n_samples)
{
	if (!_silent) {
		memset (_input, 0, sizeof (Sample) * _insize);
		_silent = true;
	}
	boost::shared_ptr<MonitorPorts> cycle_ports = _monitor_ports.reader ();

	for (MonitorPorts::iterator i = cycle_ports->begin (); i != cycle_ports->end(); ++i) {
		if (i->second->remove && i->second->gain == 0) {
			continue;
		}

		PortEngine::PortHandle ph = e.get_port_by_name (i->first);
		if (!ph) {
			continue;
		}
		Sample* buf = (Sample*) e.get_buffer (ph, n_samples);
		if (!buf) {
			continue;
		}
		collect (i->second, buf, n_samples, i->first);
	}
	finalize (n_samples);
}

void
MonitorPort::collect (boost::shared_ptr<MonitorInfo> mi, Sample* buf, pframes_t n_samples, std::string const& pn)
{
	gain_t target_gain = mi->remove ? 0.0 : 1.0;
	gain_t current_gain = mi->gain;

	if (target_gain == current_gain && target_gain == 0) {
		return;
	}
	if (target_gain == current_gain) {
		if (_silent) {
			copy_vector (_input, buf, n_samples);
		} else {
			mix_buffers_no_gain (_input, buf, n_samples);
		}
	} else {
		/* fade in/out */
		Session* s = AudioEngine::instance()->session ();
		assert (s);
		const float a = 800.f / (gain_t)s->nominal_sample_rate() ; // ~ 1/50Hz to fade by 40dB
		const int   max_nproc = 4;
		uint32_t    remain = n_samples;
		uint32_t    offset = 0;

		while (remain > 0) {
			uint32_t n_proc = remain > max_nproc ? max_nproc : remain;
			for (uint32_t i = 0; i < n_proc; ++i) {
				_input[i + offset] += current_gain * buf[i + offset];
			}
			current_gain += a * (target_gain - current_gain);
			remain -= n_proc;
			offset += n_proc;
		}
		if (fabsf (current_gain - target_gain) < GAIN_COEFF_DELTA) {
			mi->gain = target_gain;
#if 1 // not strictly needed
			if (target_gain == 0) {
				/* remove port from list, uses RCUWriter */
				remove_port (pn, true);
			}
#endif
		} else {
			mi->gain = current_gain;
		}
	}
	_silent = false;
}

void
MonitorPort::finalize (pframes_t n_samples)
{
	_src.inp_data  = (float*)_input;
	_src.inp_count = n_samples;
	_src.out_count = Port::cycle_nframes ();
	_src.set_rratio (Port::cycle_nframes () / (double)n_samples);
	_src.out_data  = _data;
	_src.process ();

	while (_src.out_count > 0) {
		*_src.out_data =  _src.out_data[-1];
		++_src.out_data;
		--_src.out_count;
	}
}

ARDOUR::AudioBuffer&
MonitorPort::get_audio_buffer (pframes_t n_samples)
{
	/* caller must hold process lock */

	/* _data was read and resampled as necessary in ::cycle_start */
	Sample* addr = &_data[Port::port_offset ()];
	_buffer->set_data (addr, n_samples);
	return *_buffer;
}

bool
MonitorPort::monitoring (std::string const& pn) const
{
	boost::shared_ptr<MonitorPorts> mp = _monitor_ports.reader ();
	if (pn.empty ()) {
		for (MonitorPorts::iterator i = mp->begin (); i != mp->end(); ++i) {
			if (!i->second->remove) {
				return true;
			}
		}
		return false;
	}
	MonitorPorts::iterator i = mp->find (pn);
	if (i == mp->end ()) {
		return false;
	}
	return !i->second->remove;
}

void
MonitorPort::active_monitors (std::list<std::string>& portlist) const
{
	boost::shared_ptr<MonitorPorts> mp = _monitor_ports.reader ();
	for (MonitorPorts::iterator i = mp->begin (); i != mp->end(); ++i) {
		if (i->second->remove) {
			continue;
		}
		portlist.push_back (i->first);
	}
}

void
MonitorPort::set_active_monitors (std::list<std::string> const& pl)
{
	if (pl.empty () && !monitoring ()) {
		return;
	}

	std::list<std::string> removals;
	std::list<std::string> additions;

	{
		RCUWriter<MonitorPorts> mp_rcu (_monitor_ports);
		boost::shared_ptr<MonitorPorts> mp = mp_rcu.get_copy ();
		/* clear ports not present in portlist */
		for (MonitorPorts::iterator i = mp->begin (); i != mp->end (); ++i) {
			if (std::find (pl.begin (), pl.end (), i->first) != pl.end ()) {
				continue;
			}
			if (i->second->remove) {
				continue;
			}
			i->second->remove = true;
			removals.push_back (i->first);
		}
		/* add ports */
		for (std::list<std::string>::const_iterator i = pl.begin (); i != pl.end (); ++i) {
			std::pair<MonitorPorts::iterator, bool> it = mp->insert (make_pair (*i, boost::shared_ptr<MonitorInfo> (new MonitorInfo ())));
			if (!it.second && !it.first->second->remove) {
				/* already present */
				continue;
			}
			it.first->second->remove = false;
			additions.push_back (*i);
		}
	}

	for (std::list<std::string>::const_iterator i = removals.begin (); i != removals.end (); ++i) {
		MonitorInputChanged (*i, false); /* EMIT SIGNAL */
	}
	for (std::list<std::string>::const_iterator i = additions.begin (); i != additions.end (); ++i) {
		MonitorInputChanged (*i, true); /* EMIT SIGNAL */
	}
	if (!removals.empty () || !additions.empty ()) {
		AudioEngine::instance()->session ()->SoloChanged (); /* EMIT SIGNAL */
	}
}

void
MonitorPort::add_port (std::string const& pn)
{
	Session* s = AudioEngine::instance()->session ();
	if (!s) {
		return;
	}
	assert (s->monitor_out ());
	assert (!AudioEngine::instance()->port_is_mine (pn));

	{
		RCUWriter<MonitorPorts> mp_rcu (_monitor_ports);
		boost::shared_ptr<MonitorPorts> mp = mp_rcu.get_copy ();
		std::pair<MonitorPorts::iterator, bool> it = mp->insert (make_pair (pn, boost::shared_ptr<MonitorInfo> (new MonitorInfo ())));
		if (!it.second) {
			if (!it.first->second->remove) {
				/* already present */
				return;
			}
			/* in case it was recently removed and still fades */
			it.first->second->remove = false;
		}
	}

	MonitorInputChanged (pn, true); /* EMIT SIGNAL */
	s->SoloChanged (); /* EMIT SIGNAL */
}

void
MonitorPort::remove_port (std::string const& pn, bool instantly)
{
	Session* s = AudioEngine::instance()->session ();
	if (!s) {
		return;
	}

	{
		RCUWriter<MonitorPorts> mp_rcu (_monitor_ports);
		boost::shared_ptr<MonitorPorts> mp = mp_rcu.get_copy ();
		MonitorPorts::iterator i = mp->find (pn);
		if (i == mp->end ()) {
			return;
		}
		if (instantly) {
			mp->erase (i);
		} else {
			i->second->remove = true; // queue fade out
		}
	}

	MonitorInputChanged (pn, false); /* EMIT SIGNAL */
	s->SoloChanged (); /* EMIT SIGNAL */
}

void
MonitorPort::clear_ports (bool instantly)
{
	Session* s = AudioEngine::instance()->session ();
	if (!s) {
		instantly = true;
	}
	MonitorPorts copy;

	if (instantly) {
		RCUWriter<MonitorPorts> mp_rcu (_monitor_ports);
		boost::shared_ptr<MonitorPorts> mp = mp_rcu.get_copy ();
		mp->swap (copy);
		assert (mp->empty ());
	} else {
		boost::shared_ptr<MonitorPorts> mp = _monitor_ports.reader ();
		copy = *mp;
		for (MonitorPorts::iterator i = copy.begin (); i != copy.end(); ++i) {
			i->second->remove = true;
		}
	}

	for (MonitorPorts::iterator i = copy.begin (); i != copy.end(); ++i) {
		MonitorInputChanged (i->first, false); /* EMIT SIGNAL */
	}

	if (!s) {
		return;
	}

	if (!copy.empty ()) {
		s->SoloChanged (); /* EMIT SIGNAL */
	}
}
