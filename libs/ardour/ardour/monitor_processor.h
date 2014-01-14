/*
    Copyright (C) 2010 Paul Davis

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

#ifndef __ardour_monitor_processor_h__
#define __ardour_monitor_processor_h__

#include <algorithm>
#include <iostream>
#include <vector>

#include "pbd/signals.h"
#include "pbd/compose.h"
#include "pbd/controllable.h"

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"
#include "ardour/processor.h"

class XMLNode;

namespace ARDOUR {

class Session;

template<typename T>
class /*LIBARDOUR_API*/ MPControl : public PBD::Controllable {
public:
	MPControl (T initial, const std::string& name, PBD::Controllable::Flag flag,
	           float lower = 0.0f, float upper = 1.0f)
		: PBD::Controllable (name, flag)
		, _value (initial)
		, _lower (lower)
		, _upper (upper)
	{}

	/* Controllable API */

	void set_value (double v) {
		T newval = (T) v;
		if (newval != _value) {
			_value = std::max (_lower, std::min (_upper, newval));
			Changed(); /* EMIT SIGNAL */
		}
	}

	double get_value () const {
		return (float) _value;
	}

	double lower () const { return _lower; }
	double upper () const { return _upper; }

	/* "access as T" API */

	MPControl& operator=(const T& v) {
		if (v != _value) {
			_value = std::max (_lower, std::min (_upper, v));
			Changed (); /* EMIT SIGNAL */
		}
		return *this;
	}

	bool operator==(const T& v) const {
		return _value == v;
	}

	bool operator<(const T& v) const {
		return _value < v;
	}

	bool operator<=(const T& v) const {
		return _value <= v;
	}

	bool operator>(const T& v) const {
		return _value > v;
	}

	bool operator>=(const T& v) const {
		return _value >= v;
	}

	operator T() const { return _value; }
	T val() const { return _value; }

protected:
	T _value;
	T _lower;
	T _upper;
};

class LIBARDOUR_API MonitorProcessor : public Processor
{
public:
	MonitorProcessor (Session&);
	~MonitorProcessor ();

	bool display_to_user() const;

	void run (BufferSet& /*bufs*/, framepos_t /*start_frame*/, framepos_t /*end_frame*/, pframes_t /*nframes*/, bool /*result_required*/);

	XMLNode& state (bool full);
	int set_state (const XMLNode&, int /* version */);

	bool configure_io (ChanCount in, ChanCount out);
	bool can_support_io_configuration (const ChanCount& in, ChanCount& out);

	void set_cut_all (bool);
	void set_dim_all (bool);
	void set_polarity (uint32_t, bool invert);
	void set_cut (uint32_t, bool cut);
	void set_dim (uint32_t, bool dim);
	void set_solo (uint32_t, bool);
	void set_mono (bool);

	gain_t dim_level() const { return _dim_level; }
	gain_t solo_boost_level() const { return _solo_boost_level; }

	bool dimmed (uint32_t chn) const;
	bool soloed (uint32_t chn) const;
	bool inverted (uint32_t chn) const;
	bool cut (uint32_t chn) const;
	bool cut_all () const;
	bool dim_all () const;
	bool mono () const;

	PBD::Signal0<void> Changed;

	boost::shared_ptr<PBD::Controllable> channel_cut_control (uint32_t) const;
	boost::shared_ptr<PBD::Controllable> channel_dim_control (uint32_t) const;
	boost::shared_ptr<PBD::Controllable> channel_polarity_control (uint32_t) const;
	boost::shared_ptr<PBD::Controllable> channel_solo_control (uint32_t) const;

	boost::shared_ptr<PBD::Controllable> dim_control () const { return _dim_all_control; }
	boost::shared_ptr<PBD::Controllable> cut_control () const { return _cut_all_control; }
	boost::shared_ptr<PBD::Controllable> mono_control () const { return _mono_control; }
	boost::shared_ptr<PBD::Controllable> dim_level_control () const { return _dim_level_control; }
	boost::shared_ptr<PBD::Controllable> solo_boost_control () const { return _solo_boost_level_control; }

private:
	struct ChannelRecord {
		gain_t current_gain;

		/* pointers - created first, but managed by boost::shared_ptr<> */

		MPControl<gain_t>* cut_ptr;
		MPControl<bool>*   dim_ptr;
		MPControl<gain_t>* polarity_ptr;
		MPControl<bool>*   soloed_ptr;

		/* shared ptr access and lifetime management, for external users */

		boost::shared_ptr<PBD::Controllable> cut_control;
		boost::shared_ptr<PBD::Controllable> dim_control;
		boost::shared_ptr<PBD::Controllable> polarity_control;
		boost::shared_ptr<PBD::Controllable> soloed_control;

		/* typed controllables for internal use */

		MPControl<gain_t>& cut;
		MPControl<bool>&   dim;
		MPControl<gain_t>& polarity;
		MPControl<bool>&   soloed;

		ChannelRecord (uint32_t);
	};

	std::vector<ChannelRecord*> _channels;

	uint32_t             solo_cnt;

	/* pointers - created first, but managed by boost::shared_ptr<> */

	MPControl<bool>*            _dim_all_ptr;
	MPControl<bool>*            _cut_all_ptr;
	MPControl<bool>*            _mono_ptr;
	MPControl<volatile gain_t>* _dim_level_ptr;
	MPControl<volatile gain_t>* _solo_boost_level_ptr;

	/* shared ptr access and lifetime management, for external users */

	boost::shared_ptr<PBD::Controllable> _dim_all_control;
	boost::shared_ptr<PBD::Controllable> _cut_all_control;
	boost::shared_ptr<PBD::Controllable> _mono_control;
	boost::shared_ptr<PBD::Controllable> _dim_level_control;
	boost::shared_ptr<PBD::Controllable> _solo_boost_level_control;

	/* typed controllables for internal use */

	MPControl<bool>&            _dim_all;
	MPControl<bool>&            _cut_all;
	MPControl<bool>&            _mono;
	MPControl<volatile gain_t>& _dim_level;
	MPControl<volatile gain_t>& _solo_boost_level;

	void allocate_channels (uint32_t);
};

} /* namespace */

#endif /* __ardour_monitor_processor_h__ */
