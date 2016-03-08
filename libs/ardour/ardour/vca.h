/*
    Copyright (C) 2016 Paul Davis

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __ardour_vca_h__
#define __ardour_vca_h__

#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include "pbd/controllable.h"
#include "pbd/statefuldestructible.h"

#include "ardour/automatable.h"
#include "ardour/stripable.h"

namespace ARDOUR {

class GainControl;
class Route;

class LIBARDOUR_API VCA : public Stripable, public Automatable, public boost::enable_shared_from_this<VCA> {
  public:
	VCA (Session& session,  uint32_t num, const std::string& name);
	~VCA();

	uint32_t number () const { return _number; }
	uint32_t remote_control_id() const;

	int init ();
	XMLNode& get_state();
	int set_state (XMLNode const&, int version);

	void add_solo_target (boost::shared_ptr<Route>);
	void remove_solo_target (boost::shared_ptr<Route>);
	void add_mute_target (boost::shared_ptr<Route>);
	void remove_mute_target (boost::shared_ptr<Route>);

	bool soloed () const;
	bool muted () const;

	static std::string default_name_template ();
	static int next_vca_number ();
	static std::string xml_node_name;

	/* used by Session to save/restore the atomic counter */
	static uint32_t get_next_vca_number ();
	static void set_next_vca_number (uint32_t);

	virtual boost::shared_ptr<GainControl> gain_control() const { return _gain_control; }
	virtual boost::shared_ptr<AutomationControl> solo_control() const { return _solo_control; }
	virtual boost::shared_ptr<AutomationControl> mute_control() const { return _mute_control; }

	/* null Stripable API, because VCAs don't have any of this */

	virtual boost::shared_ptr<PeakMeter>       peak_meter() { return boost::shared_ptr<PeakMeter>(); }
	virtual boost::shared_ptr<const PeakMeter> peak_meter() const { return boost::shared_ptr<PeakMeter>(); }
	virtual boost::shared_ptr<AutomationControl> phase_control() const { return boost::shared_ptr<AutomationControl>(); }
	virtual boost::shared_ptr<AutomationControl> trim_control() const { return boost::shared_ptr<AutomationControl>(); }
	virtual boost::shared_ptr<AutomationControl> monitoring_control() const { return boost::shared_ptr<AutomationControl>(); }
	virtual boost::shared_ptr<AutomationControl> recenable_control() const { return boost::shared_ptr<AutomationControl>(); }
	virtual boost::shared_ptr<AutomationControl> pan_azimuth_control() const { return boost::shared_ptr<AutomationControl>(); }
	virtual boost::shared_ptr<AutomationControl> pan_elevation_control() const { return boost::shared_ptr<AutomationControl>(); }
	virtual boost::shared_ptr<AutomationControl> pan_width_control() const { return boost::shared_ptr<AutomationControl>(); }
	virtual boost::shared_ptr<AutomationControl> pan_frontback_control() const { return boost::shared_ptr<AutomationControl>(); }
	virtual boost::shared_ptr<AutomationControl> pan_lfe_control() const { return boost::shared_ptr<AutomationControl>(); }
	virtual uint32_t eq_band_cnt () const { return 0; }
	virtual std::string eq_band_name (uint32_t) const { return std::string(); }
	virtual boost::shared_ptr<AutomationControl> eq_gain_controllable (uint32_t band) const { return boost::shared_ptr<AutomationControl>(); }
	virtual boost::shared_ptr<AutomationControl> eq_freq_controllable (uint32_t band) const { return boost::shared_ptr<AutomationControl>(); }
	virtual boost::shared_ptr<AutomationControl> eq_q_controllable (uint32_t band) const { return boost::shared_ptr<AutomationControl>(); }
	virtual boost::shared_ptr<AutomationControl> eq_shape_controllable (uint32_t band) const { return boost::shared_ptr<AutomationControl>(); }
	virtual boost::shared_ptr<AutomationControl> eq_enable_controllable () const { return boost::shared_ptr<AutomationControl>(); }
	virtual boost::shared_ptr<AutomationControl> eq_hpf_controllable () const { return boost::shared_ptr<AutomationControl>(); }
	virtual boost::shared_ptr<AutomationControl> comp_enable_controllable () const { return boost::shared_ptr<AutomationControl>(); }
	virtual boost::shared_ptr<AutomationControl> comp_threshold_controllable () const { return boost::shared_ptr<AutomationControl>(); }
	virtual boost::shared_ptr<AutomationControl> comp_speed_controllable () const { return boost::shared_ptr<AutomationControl>(); }
	virtual boost::shared_ptr<AutomationControl> comp_mode_controllable () const { return boost::shared_ptr<AutomationControl>(); }
	virtual boost::shared_ptr<AutomationControl> comp_makeup_controllable () const { return boost::shared_ptr<AutomationControl>(); }
	virtual boost::shared_ptr<AutomationControl> comp_redux_controllable () const { return boost::shared_ptr<AutomationControl>(); }
	virtual std::string comp_mode_name (uint32_t mode) const { return std::string(); }
	virtual std::string comp_speed_name (uint32_t mode) const { return std::string(); }
	virtual boost::shared_ptr<AutomationControl> send_level_controllable (uint32_t n) const { return boost::shared_ptr<AutomationControl>(); }
	virtual boost::shared_ptr<AutomationControl> send_enable_controllable (uint32_t n) const { return boost::shared_ptr<AutomationControl>(); }
	virtual std::string send_name (uint32_t n) const { return std::string(); }
	virtual boost::shared_ptr<AutomationControl> master_send_enable_controllable () const { return boost::shared_ptr<AutomationControl>(); }

  private:
	class VCASoloControllable : public AutomationControl {
          public:
		VCASoloControllable (std::string const & name, boost::shared_ptr<VCA> vca);
		void set_value (double, PBD::Controllable::GroupControlDisposition group_override);
		void set_value_unchecked (double);
		double get_value () const;
	  private:
		void _set_value (double, PBD::Controllable::GroupControlDisposition group_override);
		boost::weak_ptr<VCA> _vca;
	};

	class VCAMuteControllable : public AutomationControl {
          public:
		VCAMuteControllable (std::string const & name, boost::shared_ptr<VCA> vca);
		void set_value (double, PBD::Controllable::GroupControlDisposition group_override);
		void set_value_unchecked (double);
		double get_value () const;
	  private:
		void _set_value (double, PBD::Controllable::GroupControlDisposition group_override);
		boost::weak_ptr<VCA> _vca;
	};

	friend class VCASoloControllable;
	friend class VCAMuteControllable;

	uint32_t    _number;

	RouteList solo_targets;
	PBD::ScopedConnectionList solo_connections;
	mutable Glib::Threads::RWLock solo_lock;

	RouteList mute_targets;
	PBD::ScopedConnectionList mute_connections;
	mutable Glib::Threads::RWLock mute_lock;

	boost::shared_ptr<GainControl> _gain_control;
	boost::shared_ptr<VCASoloControllable> _solo_control;
	boost::shared_ptr<VCAMuteControllable> _mute_control;
	bool _solo_requested;
	bool _mute_requested;

	static gint next_number;

	void solo_target_going_away (boost::weak_ptr<Route>);
	void mute_target_going_away (boost::weak_ptr<Route>);
	bool soloed_locked () const;
	bool muted_locked () const;

	void set_solo (bool yn);
	void set_mute (bool yn);

};

} /* namespace */

#endif /* __ardour_vca_h__ */
