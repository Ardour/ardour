/*
 * Copyright (C) 2010-2018 Fons Adriaensen <fons@linuxaudio.org>
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

#ifndef _ardour_peak_limiter_h_
#define _ardour_peak_limiter_h_

#include <stdint.h>

#include "ardour/automation_control.h"
#include "ardour/libardour_visibility.h"
#include "ardour/processor.h"
#include "ardour/readonly_control.h"

namespace ARDOUR {

class LIBARDOUR_API Limiter : public Processor, public HasReadableCtrl
{
public:
	Limiter (Session&, const std::string& name = "Limiter");
	~Limiter ();

	XMLNode& get_state ();
	int      set_state (const XMLNode&, int version);

	samplecnt_t signal_latency () const;

	bool can_support_io_configuration (const ChanCount& in, ChanCount& out);
	bool configure_io (ChanCount in, ChanCount out);
	void run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nframes, bool result_required);

	void enable (bool yn);
	bool enabled () const;

	boost::shared_ptr<AutomationControl> enable_ctrl () const
	{
		return _enable_ctrl;
	}

	boost::shared_ptr<AutomationControl> threshold_ctrl () const
	{
		return _threshold_ctrl;
	}

	boost::shared_ptr<AutomationControl> release_ctrl () const
	{
		return _release_ctrl;
	}

	boost::shared_ptr<AutomationControl> truepeak_ctrl () const
	{
		return _truepeak_ctrl;
	}

	boost::shared_ptr<ReadOnlyControl> redux_ctrl () const
	{
		return _redux_ctrl;
	}

	float get_parameter (uint32_t port) const;
	std::string describe_parameter (Evoral::Parameter);

private:
	void process (BufferSet&, pframes_t);

	void init (uint32_t nchan);
	void fini ();

	void set_threshold (float);
	void set_release (float);
	void set_truepeak (bool);

	class Histmin
	{
	public:
		Histmin () {}

		void  init (int hlen);
		float write (float v);
		float vmin () { return _vmin; }

	private:
		enum {
			SIZE = 32,
			MASK = SIZE - 1
		};

		int   _hlen;
		int   _hold;
		int   _wind;
		float _vmin;
		float _hist[SIZE];
	};

	float** _dly_buf;
	float** _z;
	float*  _zlf;

	uint32_t  _nchan;
	bool      _processing;
	bool      _truepeak;
	float     _threshold;
	float     _release_time;
	pframes_t _div1, _div2;
	pframes_t _delay;
	pframes_t _dly_mask;
	pframes_t _dly_ridx;
	pframes_t _c1, _c2;
	float     _gt, _m1, _m2;
	float     _w1, _w2, _w3, _wlf;
	float     _z1, _z2, _z3;
	Histmin   _hist1;
	Histmin   _hist2;

	float       _peak;
	float       _redux;
	samplecnt_t _c3, _c4;
	samplecnt_t _div3;

	boost::shared_ptr<AutomationControl> _enable_ctrl;
	boost::shared_ptr<AutomationControl> _threshold_ctrl;
	boost::shared_ptr<AutomationControl> _release_ctrl;
	boost::shared_ptr<AutomationControl> _truepeak_ctrl;
	boost::shared_ptr<ReadOnlyControl>   _redux_ctrl;
};

} // namespace ARDOUR

#endif
