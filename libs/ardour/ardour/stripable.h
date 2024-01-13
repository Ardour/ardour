/*
 * Copyright (C) 2016-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2017 Ben Loftis <ben@harrisonconsoles.com>
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

#ifndef __libardour_stripable_h__
#define __libardour_stripable_h__

#include <cstdint>
#include <memory>
#include <string>

#include <boost/utility.hpp>

#include "pbd/signals.h"

#include "ardour/automatable.h"
#include "ardour/presentation_info.h"
#include "ardour/session_object.h"
#include "ardour/libardour_visibility.h"

class StripableColorDialog;

namespace ARDOUR {

class AutomationControl;
class ReadOnlyControl;
class GainControl;
class PeakMeter;
class SoloControl;
class MuteControl;
class PhaseControl;
class SoloIsolateControl;
class SoloSafeControl;
class MonitorControl;
class MonitorProcessor;
class RecordEnableControl;
class RecordSafeControl;

enum WellKnownCtrl : int;
enum WellKnownData : int;

/* This is a virtual base class for any object that needs to be potentially
 * represented by a control-centric user interface using the general model of a
 * mixing console "strip" - a collection of controls that determine the state
 * and behaviour of the object.
 */

class LIBARDOUR_API Stripable : public SessionObject,
                                public Automatable,
                                public std::enable_shared_from_this<Stripable>
{
  public:
	Stripable (Session& session, std::string const & name, PresentationInfo const &);
	virtual ~Stripable ();

	/* XXX
	   midi on/off
	 */

	bool is_auditioner() const { return _presentation_info.flags() & PresentationInfo::Auditioner; }
	bool is_private_route() const { return is_auditioner(); }
	bool is_master() const { return _presentation_info.flags() & PresentationInfo::MasterOut; }
	bool is_monitor() const { return _presentation_info.flags() & PresentationInfo::MonitorOut; }
	bool is_foldbackbus() const { return _presentation_info.flags() & PresentationInfo::FoldbackBus; }
	bool is_surround_master() const { return _presentation_info.flags() & PresentationInfo::SurroundMaster; }
	bool is_main_bus() const { return _presentation_info.flags() & PresentationInfo::MainBus; }
	bool is_singleton () const { return _presentation_info.flags() & PresentationInfo::Singleton; }

	int set_state (XMLNode const&, int);

	bool is_hidden() const { return _presentation_info.flags() & PresentationInfo::Hidden; }
	bool is_selected() const;

	PresentationInfo const & presentation_info () const { return _presentation_info; }
	PresentationInfo& presentation_info () { return _presentation_info; }
	PresentationInfo* presentation_info_ptr () { return &_presentation_info; }

	/* set just the order */

	void  set_presentation_order (PresentationInfo::order_t);

	struct LIBARDOUR_API Sorter
	{
		bool _mixer_order; // master is last
		Sorter (bool mixer_order = false) : _mixer_order (mixer_order) {}
		bool operator() (std::shared_ptr<ARDOUR::Stripable> a, std::shared_ptr<ARDOUR::Stripable> b);
	};

	/* gui's call this for their own purposes. */

	PBD::Signal2<void,std::string,void*> gui_changed;

	/* *************************************************************
	 * Pure interface begins here
	 ***************************************************************/

	virtual std::shared_ptr<PeakMeter>       peak_meter() = 0;
	virtual std::shared_ptr<const PeakMeter> peak_meter() const = 0;

	virtual std::shared_ptr<GainControl> gain_control() const = 0;

	virtual std::shared_ptr<SoloControl> solo_control() const = 0;
	virtual std::shared_ptr<SoloIsolateControl> solo_isolate_control() const = 0;
	virtual std::shared_ptr<SoloSafeControl> solo_safe_control() const = 0;
	virtual std::shared_ptr<MuteControl> mute_control() const = 0;

	virtual std::shared_ptr<PhaseControl> phase_control() const = 0;
	virtual std::shared_ptr<GainControl> trim_control() const = 0;

	virtual std::shared_ptr<MonitorControl> monitoring_control() const = 0;

	virtual std::shared_ptr<AutomationControl> rec_enable_control() const { return std::shared_ptr<AutomationControl>(); }
	virtual std::shared_ptr<AutomationControl> rec_safe_control() const { return std::shared_ptr<AutomationControl>(); }

	virtual bool slaved_to (std::shared_ptr<VCA>) const = 0;
	virtual bool slaved () const = 0;

	/* "well-known" controls for panning. Any or all of these may return
	 * null.
	 */
	virtual std::shared_ptr<AutomationControl> pan_azimuth_control() const = 0;
	virtual std::shared_ptr<AutomationControl> pan_elevation_control() const = 0;
	virtual std::shared_ptr<AutomationControl> pan_width_control() const = 0;
	virtual std::shared_ptr<AutomationControl> pan_frontback_control() const = 0;
	virtual std::shared_ptr<AutomationControl> pan_lfe_control() const = 0;

	/* "well-known" controls. Any or all may NULL. */
	virtual uint32_t eq_band_cnt () const = 0;
	virtual std::string eq_band_name (uint32_t) const = 0;


	virtual std::shared_ptr<AutomationControl> mapped_control (enum WellKnownCtrl, uint32_t band = 0) const = 0;
	virtual std::shared_ptr<ReadOnlyControl>   mapped_output (enum WellKnownData) const = 0;

	/* ACs mapped to any control have changed. API user is to drop references,
	 * and query mapped ctrl again
	 */
	PBD::Signal0<void> MappedControlsChanged;

	virtual std::shared_ptr<AutomationControl> eq_enable_controllable () const = 0;
	virtual std::shared_ptr<AutomationControl> eq_gain_controllable (uint32_t band) const = 0;
	virtual std::shared_ptr<AutomationControl> eq_freq_controllable (uint32_t band) const = 0;
	virtual std::shared_ptr<AutomationControl> eq_q_controllable (uint32_t band) const = 0;
	virtual std::shared_ptr<AutomationControl> eq_shape_controllable (uint32_t band) const = 0;

	virtual std::shared_ptr<AutomationControl> filter_freq_controllable (bool hp /* false for LPF*/) const = 0;
	virtual std::shared_ptr<AutomationControl> filter_slope_controllable (bool hp) const = 0;
	virtual std::shared_ptr<AutomationControl> filter_enable_controllable (bool hp) const = 0;

	virtual std::shared_ptr<AutomationControl> tape_drive_controllable () const = 0;
	virtual std::shared_ptr<AutomationControl> tape_drive_mode_controllable () const = 0;
	virtual std::shared_ptr<ReadOnlyControl> tape_drive_mtr_controllable () const = 0;

	/* "well-known" controls for a compressor in this route. Any or all may
	 * be null.
	 */

	virtual std::shared_ptr<AutomationControl> comp_enable_controllable () const = 0;
	virtual std::shared_ptr<AutomationControl> comp_threshold_controllable () const = 0;
	virtual std::shared_ptr<AutomationControl> comp_speed_controllable () const = 0;
	virtual std::shared_ptr<AutomationControl> comp_mode_controllable () const = 0;
	virtual std::shared_ptr<AutomationControl> comp_makeup_controllable () const = 0;
	virtual std::shared_ptr<AutomationControl> comp_ratio_controllable () const = 0;
	virtual std::shared_ptr<AutomationControl> comp_attack_controllable () const = 0;
	virtual std::shared_ptr<AutomationControl> comp_release_controllable () const = 0;
	virtual std::shared_ptr<AutomationControl> comp_key_filter_freq_controllable () const = 0;
	virtual std::shared_ptr<AutomationControl> comp_lookahead_controllable () const = 0;
	virtual std::shared_ptr<ReadOnlyControl>   comp_meter_controllable () const = 0;
	virtual std::shared_ptr<ReadOnlyControl>   comp_redux_controllable () const = 0;

	virtual std::shared_ptr<AutomationControl> gate_enable_controllable () const = 0;
	virtual std::shared_ptr<AutomationControl> gate_mode_controllable () const = 0;
	virtual std::shared_ptr<AutomationControl> gate_ratio_controllable () const = 0;
	virtual std::shared_ptr<AutomationControl> gate_knee_controllable () const = 0;
	virtual std::shared_ptr<AutomationControl> gate_threshold_controllable () const = 0;
	virtual std::shared_ptr<AutomationControl> gate_depth_controllable () const = 0;
	virtual std::shared_ptr<AutomationControl> gate_hysteresis_controllable () const = 0;
	virtual std::shared_ptr<AutomationControl> gate_hold_controllable () const = 0;
	virtual std::shared_ptr<AutomationControl> gate_attack_controllable () const = 0;
	virtual std::shared_ptr<AutomationControl> gate_release_controllable () const = 0;
	virtual std::shared_ptr<AutomationControl> gate_key_listen_controllable () const = 0;
	virtual std::shared_ptr<AutomationControl> gate_key_filter_enable_controllable () const = 0;
	virtual std::shared_ptr<AutomationControl> gate_key_filter_freq_controllable () const = 0;
	virtual std::shared_ptr<AutomationControl> gate_lookahead_controllable () const = 0;
	virtual std::shared_ptr<ReadOnlyControl>   gate_meter_controllable () const = 0;
	virtual std::shared_ptr<ReadOnlyControl>   gate_redux_controllable () const = 0;

	/* "well-known" controls for sends to well-known busses in this route. Any or all may
	 * be null.
	 *
	 * In Mixbus, these are the sends that connect to the mixbusses.
	 * In Ardour, these are user-created sends that connect to user-created
	 * Aux busses.
	 */
	virtual std::shared_ptr<AutomationControl> send_level_controllable (uint32_t n) const = 0;
	virtual std::shared_ptr<AutomationControl> send_enable_controllable (uint32_t n) const = 0;
	virtual std::shared_ptr<AutomationControl> send_pan_azimuth_controllable (uint32_t n) const = 0;
	virtual std::shared_ptr<AutomationControl> send_pan_azimuth_enable_controllable (uint32_t n) const = 0;

	/* for the same value of @p n, this returns the name of the send
	 * associated with the pair of controllables returned by the above two methods.
	 */
	virtual std::string send_name (uint32_t n) const = 0;

	/* well known control that enables/disables sending to the master bus.
	 *
	 * In Ardour, this returns null.
	 * In Mixbus, it will return a suitable control, or null depending on
	 * the route.
	 */
	virtual std::shared_ptr<AutomationControl> master_send_enable_controllable () const = 0;

	/* well known control for mixbus's correlation meter.
	 *
	 * In Ardour, this returns null.
	 * In Mixbus, it will return a suitable control, or null depending on the route.
	 * @param mm min/max of the correlation range, true for upper value
	 */
	virtual std::shared_ptr<ReadOnlyControl> master_correlation_mtr_controllable (bool mm) const { return std::shared_ptr<ReadOnlyControl>(); }

	/* well known control for mixbus's limiter.
	 *
	 * In Ardour, this returns null.
	 * In Mixbus, it will return a suitable control, or null depending on
	 * the route.
	 */
	virtual std::shared_ptr<AutomationControl> master_limiter_enable_controllable () const { return std::shared_ptr<AutomationControl>(); }
	virtual std::shared_ptr<ReadOnlyControl> master_limiter_mtr_controllable () const { return std::shared_ptr<ReadOnlyControl>(); }

	/* well known control for mixbus's k-meter.
	 *
	 * In Ardour, this returns null.
	 * In Mixbus, it will return a suitable control, or null depending on
	 * the route.
	 */
	virtual std::shared_ptr<ReadOnlyControl> master_k_mtr_controllable () const { return std::shared_ptr<ReadOnlyControl>(); }

	virtual bool muted_by_others_soloing () const = 0;

	virtual std::shared_ptr<MonitorProcessor> monitor_control() const = 0;

	StripableColorDialog* active_color_picker() const { return _active_color_picker; }
	void set_active_color_picker (StripableColorDialog* d) { _active_color_picker = d; }

  protected:
	PresentationInfo _presentation_info;

	private:
	StripableColorDialog* _active_color_picker;
};

}

#endif /* __libardour_stripable_h__ */
