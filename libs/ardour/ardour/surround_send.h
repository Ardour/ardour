/*
 * Copyright (C) 2023 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2023 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_surround_send_h__
#define __ardour_surround_send_h__

#include "ardour/processor.h"
#include "ardour/send.h"

namespace ARDOUR
{
class Amp;
class SurroundPannable;
class MuteMaster;
class GainControl;

class LIBARDOUR_API SurroundSend : public Processor, public LatentSend
{
public:
	SurroundSend (Session&, std::shared_ptr<MuteMaster>);
	virtual ~SurroundSend ();

	/* methods for the UI to access SurroundSend controls */
	std::shared_ptr<GainControl>      gain_control () const { return _gain_control; }
	std::shared_ptr<SurroundPannable> pannable (size_t chn = 0) const;

	uint32_t n_pannables () const;

	PBD::Signal0<void> NPannablesChanged;
	PBD::Signal0<void> PanChanged;

	/* Route/processor interface */
	bool can_support_io_configuration (const ChanCount& in, ChanCount& out) { return in == out; }
	bool configure_io (ChanCount in, ChanCount out);
	int  set_block_size (pframes_t);
	void run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nframes, bool);
	bool display_to_user() const;
	bool does_routing() const { return true; }

	std::string describe_parameter(Evoral::Parameter param);

	/* Latent Send */
	void set_delay_in (samplecnt_t);
	void set_delay_out (samplecnt_t, size_t bus = 0);
	void update_delaylines (bool rt_ok);
	samplecnt_t get_delay_in () const { return _delay_in; }
	samplecnt_t get_delay_out () const { return _delay_out; }
	samplecnt_t signal_latency () const;

	/* These may only be called by a SurroundReturn (to which we are attached) from within its ::run() * method */
	BufferSet const& bufs () const { return _mixbufs; }

	std::shared_ptr<SurroundPannable> const& pan_param (size_t chn, timepos_t& s, timepos_t& e) const;
	std::shared_ptr<AutomationControl> send_enable_control () const { return _send_enable_control; }

protected:
	int set_state (const XMLNode&, int version);
	XMLNode& state () const;

private:
	void   ensure_mixbufs ();
	gain_t target_gain () const;
	void   cycle_start (pframes_t);
	void   add_pannable ();

	void send_enable_changed ();
	void proc_active_changed ();

	BufferSet _mixbufs;
	int32_t   _surround_id;
	timepos_t _cycle_start;
	timepos_t _cycle_end;
	gain_t    _current_gain;
	bool      _has_state;
	bool      _ignore_enable_change;

	std::vector<std::shared_ptr<SurroundPannable>> _pannable;

	std::shared_ptr<AutomationControl> _send_enable_control;
	std::shared_ptr<GainControl>       _gain_control;
	std::shared_ptr<Amp>               _amp;
	std::shared_ptr<MuteMaster>        _mute_master;
	std::shared_ptr<DelayLine>         _send_delay;
	std::shared_ptr<DelayLine>         _thru_delay;

	PBD::ScopedConnectionList _change_connections;
};

} // namespace ARDOUR

#endif /* __ardour_surround_send_h__ */
