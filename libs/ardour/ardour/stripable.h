/*
  Copyright (C) 2016 Paul Davis

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

#ifndef __libardour_stripable_h__
#define __libardour_stripable_h__

#include <stdint.h>

#include <string>
#include <boost/utility.hpp>
#include <boost/shared_ptr.hpp>

#include "ardour/session_object.h"

namespace ARDOUR {

class AutomationControl;
class GainControl;
class PeakMeter;
class SoloControl;
class MuteControl;
class PhaseControl;
class SoloIsolateControl;
class SoloSafeControl;
class MonitorControl;

/* This is a virtual base class for any object that needs to be potentially
 * represented by a control-centric user interface using the general model of a
 * mixing console "strip" - a collection of controls that determine the state
 * and behaviour of the object.
 */

class Stripable : public SessionObject {
   public:
	Stripable (Session& session, const std::string& name)
		: SessionObject (session, name) {}

	/* XXX
	   midi on/off
	   selected status
	   visible/hidden
	 */

	virtual uint32_t remote_control_id () const = 0;

	virtual boost::shared_ptr<PeakMeter>       peak_meter() = 0;
	virtual boost::shared_ptr<const PeakMeter> peak_meter() const = 0;

	virtual boost::shared_ptr<GainControl> gain_control() const = 0;

	virtual boost::shared_ptr<SoloControl> solo_control() const = 0;
	virtual boost::shared_ptr<MuteControl> mute_control() const = 0;

	virtual boost::shared_ptr<PhaseControl> phase_control() const = 0;
	virtual boost::shared_ptr<GainControl> trim_control() const = 0;

	virtual boost::shared_ptr<MonitorControl> monitoring_control() const = 0;
	virtual boost::shared_ptr<AutomationControl> recenable_control() const { return boost::shared_ptr<AutomationControl>(); }

	/* "well-known" controls for panning. Any or all of these may return
         * null.
         */

	virtual boost::shared_ptr<AutomationControl> pan_azimuth_control() const = 0;
	virtual boost::shared_ptr<AutomationControl> pan_elevation_control() const = 0;
	virtual boost::shared_ptr<AutomationControl> pan_width_control() const = 0;
	virtual boost::shared_ptr<AutomationControl> pan_frontback_control() const = 0;
	virtual boost::shared_ptr<AutomationControl> pan_lfe_control() const = 0;

        /* "well-known" controls for an EQ in this route. Any or all may
         * be null. eq_band_cnt() must return 0 if there is no EQ present.
         * Passing an @param band value >= eq_band_cnt() will guarantee the
         * return of a null ptr (or an empty string for eq_band_name()).
         */
	virtual uint32_t eq_band_cnt () const = 0;
	virtual std::string eq_band_name (uint32_t) const = 0;
	virtual boost::shared_ptr<AutomationControl> eq_gain_controllable (uint32_t band) const = 0;
	virtual boost::shared_ptr<AutomationControl> eq_freq_controllable (uint32_t band) const = 0;
	virtual boost::shared_ptr<AutomationControl> eq_q_controllable (uint32_t band) const = 0;
	virtual boost::shared_ptr<AutomationControl> eq_shape_controllable (uint32_t band) const = 0;
	virtual boost::shared_ptr<AutomationControl> eq_enable_controllable () const = 0;
	virtual boost::shared_ptr<AutomationControl> eq_hpf_controllable () const = 0;

        /* "well-known" controls for a compressor in this route. Any or all may
         * be null.
         */
	virtual boost::shared_ptr<AutomationControl> comp_enable_controllable () const = 0;
	virtual boost::shared_ptr<AutomationControl> comp_threshold_controllable () const = 0;
	virtual boost::shared_ptr<AutomationControl> comp_speed_controllable () const = 0;
	virtual boost::shared_ptr<AutomationControl> comp_mode_controllable () const = 0;
	virtual boost::shared_ptr<AutomationControl> comp_makeup_controllable () const = 0;
	virtual boost::shared_ptr<AutomationControl> comp_redux_controllable () const = 0;

        /* @param mode must be supplied by the comp_mode_controllable(). All other values
         * result in undefined behaviour
         */
	virtual std::string comp_mode_name (uint32_t mode) const = 0;
        /* @param mode - as for comp mode name. This returns the name for the
         * parameter/control accessed via comp_speed_controllable(), which can
         * be mode dependent.
         */
	virtual std::string comp_speed_name (uint32_t mode) const = 0;

        /* "well-known" controls for sends to well-known busses in this route. Any or all may
         * be null.
         *
         * In Mixbus, these are the sends that connect to the mixbusses.
         * In Ardour, these are user-created sends that connect to user-created
         * Aux busses.
         */
	virtual boost::shared_ptr<AutomationControl> send_level_controllable (uint32_t n) const = 0;
	virtual boost::shared_ptr<AutomationControl> send_enable_controllable (uint32_t n) const = 0;
        /* for the same value of @param n, this returns the name of the send
         * associated with the pair of controllables returned by the above two methods.
         */
	virtual std::string send_name (uint32_t n) const = 0;

        /* well known control that enables/disables sending to the master bus.
         *
         * In Ardour, this returns null.
         * In Mixbus, it will return a suitable control, or null depending on
         * the route.
         */
	virtual boost::shared_ptr<AutomationControl> master_send_enable_controllable () const = 0;

	virtual bool muted_by_others_soloing () const = 0;
	virtual bool muted_by_others () const = 0;
};


}

#endif /* __libardour_stripable_h__ */
