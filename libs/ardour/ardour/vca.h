/*
 * Copyright (C) 2016-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_vca_h__
#define __ardour_vca_h__

#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <glibmm/threads.h>

#include "pbd/controllable.h"
#include "pbd/statefuldestructible.h"

#include "ardour/muteable.h"
#include "ardour/monitorable.h"
#include "ardour/recordable.h"
#include "ardour/soloable.h"
#include "ardour/slavable.h"
#include "ardour/stripable.h"

namespace ARDOUR {

class Route;
class GainControl;
class SoloControl;
class MuteControl;
class MonitorControl;

class LIBARDOUR_API VCA : public Stripable,
                          public Soloable,
                          public Muteable,
                          public Recordable,
                          public Monitorable
{
  public:
	VCA (Session& session,  int32_t num, const std::string& name);
	~VCA();

	int32_t number () const { return _number; }
	std::string full_name() const;

	int init ();
	XMLNode& get_state();
	int set_state (XMLNode const&, int version);

	PBD::Signal0<void> Drop; /* signal to slaves to drop control by this VCA */

	/* Slavable API */

	void assign (boost::shared_ptr<VCA>);

	bool slaved_to (boost::shared_ptr<VCA>) const;
	bool slaved () const;

	/* Soloable API */

	void clear_all_solo_state ();

	bool soloed () const;
	void push_solo_upstream (int32_t) {}
	void push_solo_isolate_upstream (int32_t) {}
	bool can_solo() const { return true; }
	bool can_monitor() const { return false; }
	bool is_safe () const { return false; }

	/* Muteable API */

	bool can_be_muted_by_others () const { return true; }
	bool muted_by_others_soloing() const { return false; }

	/* Recordable API */

	int prep_record_enabled (bool yn) { return 0; }
	bool can_be_record_enabled() { return true; }
	bool can_be_record_safe() { return true; }

	/* Monitorable API */

	MonitorState monitoring_state() const;

	static std::string default_name_template ();
	static int32_t next_vca_number ();
	static std::string xml_node_name;

	/* used by Session to save/restore the atomic counter */
	static int32_t get_next_vca_number ();
	static void set_next_vca_number (int32_t);

	boost::shared_ptr<GainControl> gain_control() const { return _gain_control; }
	boost::shared_ptr<SoloControl> solo_control() const { return _solo_control; }
	boost::shared_ptr<MuteControl> mute_control() const { return _mute_control; }

	/* null Stripable API, because VCAs don't have any of this */

	boost::shared_ptr<SoloIsolateControl> solo_isolate_control() const { return boost::shared_ptr<SoloIsolateControl>(); }
	boost::shared_ptr<SoloSafeControl> solo_safe_control() const { return boost::shared_ptr<SoloSafeControl>(); }
	boost::shared_ptr<PeakMeter>         peak_meter() { return boost::shared_ptr<PeakMeter>(); }
	boost::shared_ptr<const PeakMeter>   peak_meter() const { return boost::shared_ptr<PeakMeter>(); }
	boost::shared_ptr<PhaseControl>      phase_control() const { return boost::shared_ptr<PhaseControl>(); }
	boost::shared_ptr<GainControl>       trim_control() const { return boost::shared_ptr<GainControl>(); }
	boost::shared_ptr<AutomationControl> pan_azimuth_control() const { return boost::shared_ptr<AutomationControl>(); }
	boost::shared_ptr<AutomationControl> pan_elevation_control() const { return boost::shared_ptr<AutomationControl>(); }
	boost::shared_ptr<AutomationControl> pan_width_control() const { return boost::shared_ptr<AutomationControl>(); }
	boost::shared_ptr<AutomationControl> pan_frontback_control() const { return boost::shared_ptr<AutomationControl>(); }
	boost::shared_ptr<AutomationControl> pan_lfe_control() const { return boost::shared_ptr<AutomationControl>(); }
	uint32_t eq_band_cnt () const { return 0; }
	std::string eq_band_name (uint32_t) const { return std::string(); }
	boost::shared_ptr<AutomationControl> eq_enable_controllable () const { return boost::shared_ptr<AutomationControl>(); }
	boost::shared_ptr<AutomationControl> eq_gain_controllable (uint32_t) const { return boost::shared_ptr<AutomationControl>(); }
	boost::shared_ptr<AutomationControl> eq_freq_controllable (uint32_t) const { return boost::shared_ptr<AutomationControl>(); }
	boost::shared_ptr<AutomationControl> eq_q_controllable (uint32_t) const { return boost::shared_ptr<AutomationControl>(); }
	boost::shared_ptr<AutomationControl> eq_shape_controllable (uint32_t) const { return boost::shared_ptr<AutomationControl>(); }
	boost::shared_ptr<AutomationControl> filter_freq_controllable (bool) const { return boost::shared_ptr<AutomationControl>(); }
	boost::shared_ptr<AutomationControl> filter_slope_controllable (bool) const { return boost::shared_ptr<AutomationControl>(); }
	boost::shared_ptr<AutomationControl> filter_enable_controllable (bool) const { return boost::shared_ptr<AutomationControl>(); }
	boost::shared_ptr<AutomationControl> comp_enable_controllable () const { return boost::shared_ptr<AutomationControl>(); }
	boost::shared_ptr<AutomationControl> comp_threshold_controllable () const { return boost::shared_ptr<AutomationControl>(); }
	boost::shared_ptr<AutomationControl> comp_speed_controllable () const { return boost::shared_ptr<AutomationControl>(); }
	boost::shared_ptr<AutomationControl> comp_mode_controllable () const { return boost::shared_ptr<AutomationControl>(); }
	boost::shared_ptr<AutomationControl> comp_makeup_controllable () const { return boost::shared_ptr<AutomationControl>(); }
	boost::shared_ptr<ReadOnlyControl>   comp_redux_controllable () const { return boost::shared_ptr<ReadOnlyControl>(); }
	std::string comp_mode_name (uint32_t mode) const { return std::string(); }
	std::string comp_speed_name (uint32_t mode) const { return std::string(); }
	boost::shared_ptr<AutomationControl> send_level_controllable (uint32_t n) const { return boost::shared_ptr<AutomationControl>(); }
	boost::shared_ptr<AutomationControl> send_enable_controllable (uint32_t n) const { return boost::shared_ptr<AutomationControl>(); }
	boost::shared_ptr<AutomationControl> send_pan_azimuth_controllable (uint32_t n) const { return boost::shared_ptr<AutomationControl>(); }
	boost::shared_ptr<AutomationControl> send_pan_azimuth_enable_controllable (uint32_t n) const { return boost::shared_ptr<AutomationControl>(); }
	std::string send_name (uint32_t n) const { return std::string(); }
	boost::shared_ptr<AutomationControl> master_send_enable_controllable () const { return boost::shared_ptr<AutomationControl>(); }
	boost::shared_ptr<MonitorProcessor> monitor_control() const { return boost::shared_ptr<MonitorProcessor>(); }
	boost::shared_ptr<MonitorControl> monitoring_control() const { return boost::shared_ptr<MonitorControl>(); }

	//additional filter params (currently 32C only )
	boost::shared_ptr<AutomationControl> eq_lpf_controllable () const { return boost::shared_ptr<AutomationControl>(); }
	boost::shared_ptr<AutomationControl> filter_enable_controllable () const { return boost::shared_ptr<AutomationControl>(); }

	protected:
	SlavableControlList slavables () const;

  private:
	int32_t _number;

	boost::shared_ptr<GainControl> _gain_control;
	boost::shared_ptr<SoloControl> _solo_control;
	boost::shared_ptr<MuteControl> _mute_control;


	static int32_t next_number;
	static Glib::Threads::Mutex number_lock;

	void solo_target_going_away (boost::weak_ptr<Route>);
	void mute_target_going_away (boost::weak_ptr<Route>);
	bool soloed_locked () const;
	bool muted_locked () const;
};

} /* namespace */

#endif /* __ardour_vca_h__ */
