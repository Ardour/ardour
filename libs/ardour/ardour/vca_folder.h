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
#include "ardour/vca.h"

namespace ARDOUR {

class LIBARDOUR_API VCAFolder : public Folder
{
  public:
	VCAFolder (std::shared_ptr<VCA>, Session &, std::string const &, PresentationInfo const &);

	int set_state (XMLNode const&, int);
	XMLNode& get_state() const;

	std::shared_ptr<PeakMeter>       peak_meter() { return _vca->peak_meter (); }
	std::shared_ptr<const PeakMeter> peak_meter() const { return _vca->peak_meter (); }
	std::shared_ptr<GainControl> gain_control() const { return _vca->gain_control (); }
	std::shared_ptr<SoloControl> solo_control() const { return _vca->solo_control (); }
	std::shared_ptr<SoloIsolateControl> solo_isolate_control() const { return _vca->solo_isolate_control (); }
	std::shared_ptr<SoloSafeControl> solo_safe_control() const { return _vca->solo_safe_control (); }
	std::shared_ptr<MuteControl> mute_control() const { return _vca->mute_control (); }
	std::shared_ptr<PhaseControl> phase_control() const { return _vca->phase_control (); }
	std::shared_ptr<GainControl> trim_control() const { return _vca->trim_control (); }
	std::shared_ptr<MonitorControl> monitoring_control() const { return _vca->monitoring_control (); }
	std::shared_ptr<AutomationControl> rec_enable_control() const {  return _vca->rec_enable_control (); }
	std::shared_ptr<AutomationControl> rec_safe_control() const { return _vca->rec_safe_control (); }
	bool slaved_to (std::shared_ptr<VCA> vca) const { return _vca->slaved_to (vca); }
	bool slaved () const { return _vca->slaved (); }
	std::shared_ptr<AutomationControl> pan_azimuth_control() const { return _vca->pan_azimuth_control (); }
	std::shared_ptr<AutomationControl> pan_elevation_control() const { return _vca->pan_elevation_control (); }
	std::shared_ptr<AutomationControl> pan_width_control() const { return _vca->pan_width_control (); }
	std::shared_ptr<AutomationControl> pan_frontback_control() const { return _vca->pan_frontback_control (); }
	std::shared_ptr<AutomationControl> pan_lfe_control() const { return _vca->pan_lfe_control (); }
	uint32_t eq_band_cnt () const { return _vca->eq_band_cnt (); }
	std::string eq_band_name (uint32_t n) const { return _vca->eq_band_name (n); }
	std::shared_ptr<AutomationControl> mapped_control (enum WellKnownCtrl wkc, uint32_t band) const { return _vca->mapped_control (wkc, band); }
	std::shared_ptr<ReadOnlyControl>   mapped_output (enum WellKnownData wkd) const { return _vca->mapped_output (wkd); }
	std::shared_ptr<AutomationControl> send_level_controllable (uint32_t n, bool locked = false) const { return _vca->send_level_controllable (n); }
	std::shared_ptr<AutomationControl> send_enable_controllable (uint32_t n) const { return _vca->send_enable_controllable (n); }
	std::shared_ptr<AutomationControl> send_pan_azimuth_controllable (uint32_t n) const { return _vca->send_pan_azimuth_controllable (n); }
	std::shared_ptr<AutomationControl> send_pan_azimuth_enable_controllable (uint32_t n) const { return _vca->send_pan_azimuth_enable_controllable (n); }
	std::string send_name (uint32_t n) const { return _vca->send_name (n); }
	std::shared_ptr<AutomationControl> master_send_enable_controllable () const { return _vca->master_send_enable_controllable (); }
	bool muted_by_others_soloing () const { return _vca->muted_by_others_soloing (); }
	std::shared_ptr<MonitorProcessor> monitor_control() const { return _vca->monitor_control(); }

  protected:
	std::shared_ptr<VCA> _vca;
};
}
