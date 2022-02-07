/*
 * Copyright (C) 2006-2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2018 Len Ovens <len@ovenwerks.net>
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

#ifndef __ardour_send_h__
#define __ardour_send_h__

#include <string>

#include "pbd/stateful.h"

#include "ardour/ardour.h"
#include "ardour/delivery.h"

namespace ARDOUR {

class PeakMeter;
class Amp;
class GainControl;
class DelayLine;

/** Internal Abstraction for Sends (and MixbusSends) */
class LIBARDOUR_API LatentSend
{
public:
	LatentSend ();
	virtual ~LatentSend() {}

	samplecnt_t get_delay_in () const { return _delay_in; }
	samplecnt_t get_delay_out () const { return _delay_out; }

	/* should only be called by Route::update_signal_latency */
	virtual void set_delay_in (samplecnt_t) = 0;

	/* should only be called by InternalReturn::set_playback_offset
	 * (via Route::update_signal_latency)
	 */
	virtual void set_delay_out (samplecnt_t, size_t bus = 0) = 0;
	virtual void update_delaylines (bool rt_ok) = 0;

	static PBD::Signal0<void> ChangedLatency;
	static PBD::Signal0<void> QueueUpdate;

protected:
	samplecnt_t _delay_in;
	samplecnt_t _delay_out;
};

class LIBARDOUR_API Send : public Delivery, public LatentSend
{
public:
	Send (Session&, boost::shared_ptr<Pannable> pannable, boost::shared_ptr<MuteMaster>, Delivery::Role r = Delivery::Send, bool ignore_bitslot = false);
	virtual ~Send ();

	bool display_to_user() const;
	bool is_foldback () const { return _role == Foldback; }

	boost::shared_ptr<Amp> amp() const { return _amp; }
	boost::shared_ptr<PeakMeter> meter() const { return _meter; }
	boost::shared_ptr<GainControl> gain_control() const { return _gain_control; }

	bool metering() const { return _metering; }
	void set_metering (bool yn) { _metering = yn; }

	int set_state(const XMLNode&, int version);

	PBD::Signal0<void> SelfDestruct;
	void set_remove_on_disconnect (bool b) { _remove_on_disconnect = b; }
	bool remove_on_disconnect () const { return _remove_on_disconnect; }

	bool has_panner () const;
	bool panner_linked_to_route () const;
	void set_panner_linked_to_route (bool);

	uint32_t pans_required() const { return _configured_input.n_audio(); }

	void run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nframes, bool);

	bool can_support_io_configuration (const ChanCount& in, ChanCount& out);
	bool configure_io (ChanCount in, ChanCount out);

	/* latency compensation */
	void set_delay_in (samplecnt_t);
	void set_delay_out (samplecnt_t, size_t bus = 0);
	samplecnt_t get_delay_in () const { return _delay_in; }
	samplecnt_t get_delay_out () const { return _delay_out; }
	samplecnt_t signal_latency () const;

	void activate ();
	void deactivate ();

	void update_delaylines (bool rt_ok);

	bool set_name (const std::string& str);

	static std::string name_and_id_new_send (Session&, Delivery::Role r, uint32_t&, bool);

protected:
	XMLNode& state ();

	bool _metering;
	boost::shared_ptr<GainControl> _gain_control;
	boost::shared_ptr<Amp> _amp;
	boost::shared_ptr<PeakMeter> _meter;
	boost::shared_ptr<DelayLine> _send_delay;
	boost::shared_ptr<DelayLine> _thru_delay;

private:
	/* disallow copy construction */
	Send (const Send&);

	void panshell_changed ();
	void pannable_changed ();
	void snd_output_changed (IOChange, void*);

	int set_state_2X (XMLNode const &, int);

	bool _remove_on_disconnect;
};

} // namespace ARDOUR

#endif /* __ardour_send_h__ */
