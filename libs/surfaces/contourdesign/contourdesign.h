/*
 * Copyright (C) 2019 Johannes Mueller <github@johannes-mueller.org>
 * Copyright (C) 2019 Robin Gareus <robin@gareus.org>
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

#ifndef ardour_contourdesign_control_protocol_h
#define ardour_contourdesign_control_protocol_h

#include <string>
#include <vector>
#include <boost/shared_ptr.hpp>
#include <glibmm/main.h>

#define ABSTRACT_UI_EXPORTS
#include "pbd/abstract_ui.h"
#include "ardour/types.h"
#include "control_protocol/control_protocol.h"

struct libusb_device_handle;
struct libusb_transfer;

class ContourDesignGUI;

namespace ArdourSurface {

struct ContourDesignControlUIRequest : public BaseUI::BaseRequestObject {
public:
	ContourDesignControlUIRequest () {}
	~ContourDesignControlUIRequest () {}
};

enum JumpUnit {
	SECONDS = 0,
	BEATS = 1,
	BARS = 2
};

struct JumpDistance {
	JumpDistance () : value (1.0), unit (BEATS) {}
	JumpDistance (double v, JumpUnit u) : value (v), unit (u) {}
	JumpDistance (const JumpDistance& o) : value (o.value), unit (o.unit) {}
	JumpDistance& operator= (const JumpDistance& o) {
		value = o.value;
		unit = o.unit;
		return *this;
	}

	double value;
	JumpUnit unit;
};

class ButtonBase;


class ContourDesignControlProtocol
	: public ARDOUR::ControlProtocol
	, public AbstractUI<ContourDesignControlUIRequest>
{
public:
	ContourDesignControlProtocol (ARDOUR::Session &);
	virtual ~ContourDesignControlProtocol ();

	enum DeviceType {
		None = 0,
		ShuttlePRO,
		ShuttlePRO_v2,
		ShuttleXpress
	};

	DeviceType device_type() const { return _device_type; }

	static bool probe ();
	static void* request_factory (uint32_t);

	int set_active (bool yn);

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

	void stripable_selection_changed () {}

	void handle_event ();

	static const int num_shuttle_speeds = 7;

	void prev_marker_keep_rolling ();
	void next_marker_keep_rolling ();

	void jump_forward (JumpDistance dist);
	void jump_backward (JumpDistance dist);

	boost::shared_ptr<ButtonBase> make_button_action (std::string action_string);

	int usb_errorcode () const { return _error; }

	bool keep_rolling () const { return _keep_rolling; }
	void set_keep_rolling (bool kr) { _keep_rolling = kr; }

	bool test_mode () const { return _test_mode; }
	void set_test_mode (bool tm) { _test_mode = tm; }

	int get_button_count() const { return _button_actions.size(); }
	const boost::shared_ptr<ButtonBase> get_button_action (unsigned int index) const;
	void set_button_action (unsigned int index, const boost::shared_ptr<ButtonBase> btn_act);

	JumpDistance jog_distance () const { return _jog_distance; }
	void set_jog_distance (JumpDistance jd) { _jog_distance = jd; }

	void set_shuttle_speed (unsigned int index, double speed);
	double shuttle_speed (unsigned int index) const {
		return _shuttle_speeds[index];
	}

	PBD::Signal1<void, unsigned short> ButtonPress;
	PBD::Signal1<void, unsigned short> ButtonRelease;

private:
	void do_request (ContourDesignControlUIRequest*);
	void start ();
	void stop ();

	bool has_editor () const { return true; }
	void* get_gui () const;
	void  tear_down_gui ();

	void thread_init ();

	int acquire_device ();
	void release_device ();

	void setup_default_button_actions ();
	void handle_button_press (unsigned short btn);
	void handle_button_release (unsigned short btn);

	void jog_event_backward ();
	void jog_event_forward ();

	void shuttle_event (int position);

	bool wait_for_event ();
	GSource* _io_source;
	libusb_device_handle* _dev_handle;
	libusb_transfer* _usb_transfer;
	bool _supposed_to_quit;

	unsigned char _buf[5];

	DeviceType _device_type;

	bool _shuttle_was_zero, _was_rolling_before_shuttle;

	struct State {
		int8_t shuttle;
		uint8_t jog;
		uint16_t buttons;
	};
	State _state;

	bool _test_mode;

	// Config stuff

	bool _keep_rolling;
	std::vector<double> _shuttle_speeds;
	JumpDistance _jog_distance;

	std::vector<boost::shared_ptr<ButtonBase> > _button_actions;

	mutable ContourDesignGUI* _gui;
	void build_gui ();

	int _error;
	bool _needs_reattach;
};



class ButtonBase
{
public:
	ButtonBase (ContourDesignControlProtocol& spc) : _spc (spc) {}
	virtual ~ButtonBase () {}
	virtual void execute () = 0;

	virtual XMLNode& get_state (XMLNode& node) const = 0;

protected:
	ContourDesignControlProtocol& _spc;
};


class ButtonJump : public ButtonBase
{
public:
	ButtonJump (JumpDistance dist, ContourDesignControlProtocol& ccp)
		: ButtonBase (ccp)
		, _dist (dist) {}
	~ButtonJump () {}

	void execute ();
	JumpDistance get_jump_distance () const { return _dist; };

	XMLNode& get_state (XMLNode& node) const;

private:
	JumpDistance _dist;
};

class ButtonAction : public ButtonBase
{
public:
	ButtonAction (const std::string as, ContourDesignControlProtocol& ccp)
		: ButtonBase (ccp)
		, _action_string (as) {}
	~ButtonAction () {}

	void execute ();
	std::string get_path () const { return _action_string; }

	XMLNode& get_state (XMLNode& node) const;

private:
	const std::string _action_string;
};



} // namespace

#endif  /* ardour_contourdesign_control_protocol_h */
