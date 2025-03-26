/*
 * Copyright (C) 2025 Robin Gareus <robin@gareus.org>
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

#include "pbd/ringbuffer.h"

#include "ardour/ardour.h"
#include "ardour/dsp_filter.h"
#include "ardour/session_handle.h"
#include "ardour/types.h"

namespace ARDOUR {
	class Delivery;
}

class RTAManager
	: public ARDOUR::SessionHandlePtr
	, public PBD::ScopedConnectionList
	, public sigc::trackable
{
public:
	static RTAManager* instance ();
	~RTAManager ();

	class RTA : public sigc::trackable
	{
	public:
		RTA (std::shared_ptr<ARDOUR::Route>);
		~RTA ();

		RTA (RTA const&) = delete;

		bool init ();
		void reset ();
		bool run ();

		void set_rta_speed (ARDOUR::DSP::PerceptualAnalyzer::Speed);
		void set_rta_warp (ARDOUR::DSP::PerceptualAnalyzer::Warp);

		using PerceptualAnalyzer = ARDOUR::DSP::PerceptualAnalyzer;

		std::shared_ptr<ARDOUR::Route>          route () const;
		std::shared_ptr<ARDOUR::Delivery>       delivery () const;
		std::vector<PerceptualAnalyzer*> const& analyzers () const;

	private:
		using RTARingBuffer    = PBD::RingBuffer<ARDOUR::Sample>;
		using RTARingBufferPtr = std::shared_ptr<RTARingBuffer>;
		using RTABufferList    = std::vector<RTARingBufferPtr>;
		using RTABufferListPtr = std::shared_ptr<RTABufferList>;

		void route_io_changed ();

		std::shared_ptr<ARDOUR::Route>   _route;
		std::vector<PerceptualAnalyzer*> _analyzers;
		ARDOUR::samplecnt_t              _rate;
		size_t                           _blocksize;
		size_t                           _stepsize;
		size_t                           _offset;
		RTABufferListPtr                 _ringbuffers;
		PerceptualAnalyzer::Speed        _speed;
		PerceptualAnalyzer::Warp         _warp;
		PBD::ScopedConnectionList        _route_connections;
	};

	void set_session (ARDOUR::Session*);

	XMLNode& get_state () const;

	void attach (std::shared_ptr<ARDOUR::Route>);
	void remove (std::shared_ptr<ARDOUR::Route>);
	bool attached (std::shared_ptr<ARDOUR::Route>) const;

	std::list<RTA> const& rta () const
	{
		return _rta;
	}

	void set_active (bool);
	void set_rta_speed (ARDOUR::DSP::PerceptualAnalyzer::Speed);
	void set_rta_warp (ARDOUR::DSP::PerceptualAnalyzer::Warp);

	ARDOUR::DSP::PerceptualAnalyzer::Speed rta_speed () const
	{
		return _speed;
	}
	ARDOUR::DSP::PerceptualAnalyzer::Warp rta_warp () const
	{
		return _warp;
	}

	PBD::Signal<void ()> SignalReady;
	PBD::Signal<void ()> SettingsChanged;

private:
	RTAManager ();
	static RTAManager* _instance;

	void run_rta ();
	void session_going_away ();
	void route_removed (std::weak_ptr<ARDOUR::Route>);

	std::list<RTA>                         _rta;
	bool                                   _active;
	ARDOUR::DSP::PerceptualAnalyzer::Speed _speed;
	ARDOUR::DSP::PerceptualAnalyzer::Warp  _warp;

	sigc::connection _update_connection;
};
