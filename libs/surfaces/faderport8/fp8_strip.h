/*
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

#ifndef _ardour_surfaces_fp8strip_h_
#define _ardour_surfaces_fp8strip_h_

#include <stdint.h>
#include <boost/shared_ptr.hpp>

#include "pbd/signals.h"
#include "pbd/controllable.h"

#include "fp8_base.h"
#include "fp8_button.h"

#ifdef FADERPORT16
# define N_STRIPS 16
#elif defined FADERPORT2
# define N_STRIPS 1
#else
# define N_STRIPS 8
#endif

namespace ARDOUR {
	class Stripable;
	class AutomationControl;
	class PeakMeter;
	class ReadOnlyControl;
}

namespace ArdourSurface { namespace FP_NAMESPACE {

class FP8Strip
{
public:
	FP8Strip (FP8Base& b, uint8_t id);
	~FP8Strip ();

	enum CtrlElement {
		BtnSolo,
		BtnMute,
		BtnSelect,
		Fader,
		Meter,
		Redux,
		BarVal,
		BarMode
	};

	static uint8_t midi_ctrl_id (CtrlElement type, uint8_t id);

	FP8ButtonInterface& solo_button () { return _solo; }
	FP8ButtonInterface& mute_button () { return _mute; }
	FP8ButtonInterface& selrec_button () { return _selrec; }
	FP8ButtonInterface& recarm_button () { return *_selrec.button_shift(); }
	FP8ButtonInterface& select_button () { return *_selrec.button(); }

	void set_select_button_color (uint32_t color) {
		if ((color & 0xffffff00) == 0) {
			select_button ().set_color (0xffffffff);
		} else {
			select_button ().set_color (color);
		}
	}

	bool midi_touch (bool t);
	bool midi_fader (float val);

	void initialize (); // call only when connected, sends midi

	void set_select_cb (boost::function<void ()>&);

	enum DisplayMode {
		Stripables,
		PluginSelect, // no clock display
		PluginParam, // param value
		SendDisplay, // param value + select-bar
	};

	void set_periodic_display_mode (DisplayMode m);

	// convenience function to call all set_XXX_controllable
	void set_stripable (boost::shared_ptr<ARDOUR::Stripable>, bool panmode);
	void set_text_line (uint8_t, std::string const&, bool inv = false);

	enum CtrlMask {
		CTRL_FADER  = 0x001,
		CTRL_MUTE   = 0x002,
		CTRL_SOLO   = 0x004,
		CTRL_REC    = 0x004,
		CTRL_PAN    = 0x008,
		CTRL_SELECT = 0x010,
		CTRL_TEXT0  = 0x100,
		CTRL_TEXT1  = 0x200,
		CTRL_TEXT2  = 0x400,
		CTRL_TEXT3  = 0x800,

		CTRL_TEXT01 = 0x300,
		CTRL_TEXT   = 0xf00,
		CTRL_ALL    = 0xfff,
	};

	void unset_controllables (int which = CTRL_ALL);

	void set_fader_controllable  (boost::shared_ptr<ARDOUR::AutomationControl>);
	void set_mute_controllable   (boost::shared_ptr<ARDOUR::AutomationControl>);
	void set_solo_controllable   (boost::shared_ptr<ARDOUR::AutomationControl>);
	void set_rec_controllable    (boost::shared_ptr<ARDOUR::AutomationControl>);
	void set_pan_controllable    (boost::shared_ptr<ARDOUR::AutomationControl>);
	void set_select_controllable (boost::shared_ptr<ARDOUR::AutomationControl>);

private:
	FP8Base&  _base;
	uint8_t   _id;
	FP8MomentaryButton _solo;
	FP8MomentaryButton _mute;
	FP8ARMSensitiveButton _selrec;

	bool _touching;

	PBD::ScopedConnection _base_connection; // periodic
	PBD::ScopedConnectionList _button_connections;

	std::string _stripable_name;

	boost::shared_ptr<ARDOUR::AutomationControl> _fader_ctrl;
	boost::shared_ptr<ARDOUR::AutomationControl> _mute_ctrl;
	boost::shared_ptr<ARDOUR::AutomationControl> _solo_ctrl;
	boost::shared_ptr<ARDOUR::AutomationControl> _rec_ctrl;
	boost::shared_ptr<ARDOUR::AutomationControl> _pan_ctrl;
	boost::shared_ptr<ARDOUR::AutomationControl> _x_select_ctrl;

	PBD::ScopedConnection _fader_connection;
	PBD::ScopedConnection _mute_connection;
	PBD::ScopedConnection _solo_connection;
	PBD::ScopedConnection _rec_connection;
	PBD::ScopedConnection _pan_connection;
	PBD::ScopedConnection _x_select_connection;

	boost::shared_ptr<ARDOUR::PeakMeter> _peak_meter;
	boost::shared_ptr<ARDOUR::ReadOnlyControl> _redux_ctrl;

	void set_x_select_controllable (boost::shared_ptr<ARDOUR::AutomationControl>);
	boost::function<void ()> _select_plugin_functor;

	void drop_automation_controls ();

	PBD::Controllable::GroupControlDisposition group_mode () const;

	/* notifications, update view */
	void notify_fader_changed ();
	void notify_solo_changed ();
	void notify_mute_changed ();
	void notify_rec_changed ();
	void notify_pan_changed ();
	void notify_x_select_changed ();

	/* actions, update model */
	void set_mute (bool);
	void set_solo (bool);
	void set_select ();
	void set_recarm ();

	/* periodic poll, update view */
	void set_strip_name ();
	void periodic_update_fader ();
	void periodic_update_meter ();
	void periodic_update_timecode (uint32_t);
	void periodic ();

	/* cache */
	unsigned short _last_fader;
	uint8_t _last_meter;
	uint8_t _last_redux;
	uint8_t _last_barpos;

	/* display */
	void set_strip_mode (uint8_t, bool clear = false);
	void set_bar_mode (uint8_t, bool force = false);

	uint8_t _strip_mode;
	uint8_t _bar_mode;
	DisplayMode _displaymode;
	std::string _last_line[4];
};

} } /* namespace */
#endif /* _ardour_surfaces_fp8strip_h_ */
