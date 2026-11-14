/*
 * Copyright (C) 2026 Paul Davis <paul@linuxaudiosystems.com>
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

#include "ardour/folder.h"
#include "ardour/route_group.h"

namespace ARDOUR {

class LIBARDOUR_API GroupFolder : public Folder
{
  public:
	GroupFolder (std::shared_ptr<RouteGroup>, Session &, std::string const &, PresentationInfo const &);

	int set_state (XMLNode const&, int);
	XMLNode& get_state() const;

/* FINE => First member of group If Not Empty */
#define GF_FINE(ctrl) if (_group->empty()) { return nullptr; } return _group->stripable_list()->front()->ctrl
#define GF_FIRST(empty_return) if (_group->empty()) { return empty_return; } return _group->stripable_list()->front()

	std::shared_ptr<PeakMeter>       peak_meter() { GF_FINE(peak_meter ()); }
	std::shared_ptr<const PeakMeter> peak_meter() const { GF_FINE(peak_meter ()); }
	std::shared_ptr<GainControl> gain_control() const { GF_FINE(gain_control ()); }
	std::shared_ptr<SoloControl> solo_control() const { GF_FINE(solo_control ()); }
	std::shared_ptr<SoloIsolateControl> solo_isolate_control() const { GF_FINE(solo_isolate_control ()); }
	std::shared_ptr<SoloSafeControl> solo_safe_control() const { GF_FINE(solo_safe_control ()); }
	std::shared_ptr<MuteControl> mute_control() const { GF_FINE(mute_control ()); }
	std::shared_ptr<PhaseControl> phase_control() const { GF_FINE(phase_control ()); }
	std::shared_ptr<GainControl> trim_control() const { GF_FINE(trim_control ()); }
	std::shared_ptr<MonitorControl> monitoring_control() const { GF_FINE(monitoring_control ()); }
	std::shared_ptr<AutomationControl> rec_enable_control() const { GF_FINE(rec_enable_control ()); }
	std::shared_ptr<AutomationControl> rec_safe_control() const { GF_FINE(rec_safe_control ()); }
	bool slaved_to (std::shared_ptr<VCA> vca) const { GF_FIRST(false)->slaved_to (vca); }
	bool slaved () const { GF_FIRST(false)->slaved(); }
	std::shared_ptr<AutomationControl> pan_azimuth_control() const { GF_FINE(pan_azimuth_control ()); }
	std::shared_ptr<AutomationControl> pan_elevation_control() const { GF_FINE(pan_elevation_control ()); }
	std::shared_ptr<AutomationControl> pan_width_control() const { GF_FINE(pan_width_control ()); }
	std::shared_ptr<AutomationControl> pan_frontback_control() const { GF_FINE(pan_frontback_control ()); }
	std::shared_ptr<AutomationControl> pan_lfe_control() const { GF_FINE(pan_lfe_control ()); }
	uint32_t eq_band_cnt () const { GF_FIRST(0)->eq_band_cnt (); }
	std::string eq_band_name (uint32_t n) const { GF_FIRST("")->eq_band_name (n); }
	std::shared_ptr<AutomationControl> mapped_control (enum WellKnownCtrl wkc, uint32_t band) const { GF_FINE(mapped_control (wkc, band)); }
	std::shared_ptr<ReadOnlyControl>   mapped_output (enum WellKnownData wkd) const { GF_FINE(mapped_output (wkd)); }
	std::shared_ptr<AutomationControl> send_level_controllable (uint32_t n, bool locked = false) const { GF_FINE(send_level_controllable (n)); }
	std::shared_ptr<AutomationControl> send_enable_controllable (uint32_t n) const { GF_FINE(send_enable_controllable (n)); }
	std::shared_ptr<AutomationControl> send_pan_azimuth_controllable (uint32_t n) const { GF_FINE(send_pan_azimuth_controllable (n)); }
	std::shared_ptr<AutomationControl> send_pan_azimuth_enable_controllable (uint32_t n) const { GF_FINE(send_pan_azimuth_enable_controllable (n)); }
	std::string send_name (uint32_t n) const { GF_FIRST("")->send_name (n); }
	std::shared_ptr<AutomationControl> master_send_enable_controllable () const { GF_FINE(master_send_enable_controllable ()); }
	bool muted_by_others_soloing () const { GF_FIRST(false)->muted_by_others_soloing (); }
	std::shared_ptr<MonitorProcessor> monitor_control() const { GF_FINE(monitor_control()); }

	MonitorState monitoring_state () const { GF_FIRST(MonitoringSilence)->monitoring_state(); }

	/* Manipulate the Group, not this folder */

	void add (std::shared_ptr<Stripable>) {}
	void add (std::vector<std::shared_ptr<Stripable>>) {}
	void remove (std::shared_ptr<Stripable>) {}
	void remove (std::vector<std::shared_ptr<Stripable>>) {}
	void clear () {}

  protected:
	std::shared_ptr<RouteGroup> _group;
};
}
