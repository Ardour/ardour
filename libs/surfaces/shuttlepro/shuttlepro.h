/*
    Copyright (C) 2009-2017 Paul Davis
    Author: Johannes Mueller

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

#ifndef ardour_shuttlepro_control_protocol_h
#define ardour_shuttlepro_control_protocol_h

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

class ShuttleproGUI;

namespace ArdourSurface {

struct ShuttleproControlUIRequest : public BaseUI::BaseRequestObject {
public:
	ShuttleproControlUIRequest () {}
	~ShuttleproControlUIRequest () {}
};

enum JumpUnit {
	SECONDS = 0,
	BEATS = 1,
	BARS = 2
};

struct JumpDistance {
	JumpDistance (double v, JumpUnit u) : value (v), unit (u) {}
	JumpDistance (const JumpDistance& o) : value (o.value), unit (o.unit) {}
	double value;
	JumpUnit unit;
};

class ButtonBase;


class ShuttleproControlProtocol
	: public ARDOUR::ControlProtocol
	, public AbstractUI<ShuttleproControlUIRequest>
{
	friend ShuttleproGUI;
public:
	ShuttleproControlProtocol (ARDOUR::Session &);
	virtual ~ShuttleproControlProtocol ();

	static bool probe ();

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

private:
	void do_request (ShuttleproControlUIRequest*);
	int start ();
	int stop ();

	bool has_editor () const { return true; }
	void* get_gui () const;
	void  tear_down_gui ();

	void thread_init ();

	int acquire_device ();
	void release_device ();

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

	bool _shuttle_was_zero, _was_rolling_before_shuttle;

	struct State {
		int8_t shuttle;
		uint8_t jog;
		uint16_t buttons;
	};
	State _state;

	bool _test_mode;
	PBD::Signal1<void, unsigned short> ButtonPress;
	PBD::Signal1<void, unsigned short> ButtonRelease;

	// Config stuff

	bool _keep_rolling;
	std::vector<double> _shuttle_speeds;
	JumpDistance _jog_distance;

	std::vector<boost::shared_ptr<ButtonBase> > _button_actions;
	void setup_default_button_actions ();

	mutable void* _gui;
	void build_gui ();

	int _error;
	bool _needs_reattach;
};



class ButtonBase
{
public:
	ButtonBase (ShuttleproControlProtocol& spc) : _spc (spc) {}
	virtual ~ButtonBase () {}
	virtual void execute () = 0;

	virtual XMLNode& get_state (XMLNode& node) const = 0;

protected:
	ShuttleproControlProtocol& _spc;
};


class ButtonJump : public ButtonBase
{
public:
	ButtonJump (JumpDistance dist, ShuttleproControlProtocol& scp)
		: ButtonBase (scp)
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
	ButtonAction (const std::string as, ShuttleproControlProtocol& scp)
		: ButtonBase (scp)
		, _action_string (as) {}
	~ButtonAction () {}

	void execute ();
	std::string get_path () const { return _action_string; }

	XMLNode& get_state (XMLNode& node) const;

private:
	const std::string _action_string;
};



} // namespace

#endif  /* ardour_shuttlepro_control_protocol_h */
