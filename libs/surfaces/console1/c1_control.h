#ifndef ardour_surface_console1_button_h
#define ardour_surface_console1_button_h

#include "ardour/debug.h"
#include "console1.h"

namespace Console1
{

using ControllerID = Console1::ControllerID;

class Controller
{
public:

	Controller (Console1* console1, ControllerID id)
	        : console1 (console1)
	        , _id (id)
	{
	}

	Controller (Console1*                      console1,
	            ControllerID                   id,
	            std::function<void (uint32_t)> action,
	            std::function<void (uint32_t)> shift_action        = 0,
	            std::function<void (uint32_t)> plugin_action       = 0,
	            std::function<void (uint32_t)> plugin_shift_action = 0)
	        : console1 (console1)
	        , _id (id)
	        , action (action)
	        , shift_action (shift_action)
	        , plugin_action (plugin_action)
	        , plugin_shift_action (plugin_shift_action)
	{
	}

	virtual ~Controller ()
	{
	}

	Console1*    console1;
	ControllerID id () const
	{
		return _id;
	}

    virtual void clear_value() {}

	virtual ControllerType get_type ()
	{
		return CONTROLLER;
	}

	void set_action (std::function<void (uint32_t)> new_action)
	{
		action = new_action;
	}

	void set_plugin_action (std::function<void (uint32_t)> new_action)
	{
		plugin_action = new_action;
	}

	void set_plugin_shift_action (std::function<void (uint32_t)> new_action)
	{
		plugin_shift_action = new_action;
	}
	std::function<void (uint32_t)> get_action (){
		return action;
	}
	std::function<void (uint32_t)> get_shift_action ()
	{
		return shift_action;
	}

	std::function<void (uint32_t)> get_plugin_action ()
	{
		return plugin_action;
	}

	std::function<void (uint32_t)> get_plugin_shift_action ()
	{
		return plugin_shift_action;
	}

protected:
	ControllerID                   _id;
	std::function<void (uint32_t)> action;
	std::function<void (uint32_t)> shift_action;
	std::function<void (uint32_t)> plugin_action;
	std::function<void (uint32_t)> plugin_shift_action;
};

class Encoder : public Controller
{
public:
	Encoder (Console1*                      console1,
	         ControllerID                   id,
	         std::function<void (uint32_t)> action,
	         std::function<void (uint32_t)> shift_action        = 0,
	         std::function<void (uint32_t)> plugin_action       = 0,
	         std::function<void (uint32_t)> plugin_shift_action = 0)
	        : Controller (console1, id, action, shift_action, plugin_action, plugin_shift_action)
	{
		console1->controllerMap.insert (std::make_pair (id, this));
	}

	ControllerType get_type ()
	{
		return ENCODER;
	}

	virtual void set_value (uint32_t value)
	{
		MIDI::byte buf[3];
		buf[0] = 0xB0;
		buf[1] = _id;
		buf[2] = value;

		console1->write (buf, 3);
	}

	PBD::Signal<void (uint32_t)>* plugin_signal;
};

class ControllerButton : public Controller
{
public:
	ControllerButton (Console1*                      console1,
	                  ControllerID                   id,
	                  std::function<void (uint32_t)> action,
	                  std::function<void (uint32_t)> shift_action        = 0,
	                  std::function<void (uint32_t)> plugin_action       = 0,
	                  std::function<void (uint32_t)> plugin_shift_action = 0)
	        : Controller (console1, id, action, shift_action, plugin_action, plugin_shift_action)
	{
		console1->controllerMap.insert (std::make_pair (id, this));
	}

	ControllerType get_type ()
	{
		return CONTROLLER_BUTTON;
	}

	virtual void set_led_state (bool onoff)
	{
		MIDI::byte buf[3];
		buf[0] = 0xB0;
		buf[1] = _id;
		buf[2] = onoff ? 127 : 0;

		console1->write (buf, 3);
	}

	virtual void set_led_value (uint32_t val)
	{
		MIDI::byte buf[3];
		buf[0] = 0xB0;
		buf[1] = _id;
		buf[2] = val;

		console1->write (buf, 3);
	}
};

class MultiStateButton : public Controller
{
public:
	MultiStateButton (Console1*                      console1,
	                  ControllerID                   id,
	                  std::vector<uint32_t>          state_values,
	                  std::function<void (uint32_t)> action,
	                  std::function<void (uint32_t)> shift_action        = 0,
	                  std::function<void (uint32_t)> plugin_action       = 0,
	                  std::function<void (uint32_t)> plugin_shift_action = 0)
	        : Controller (console1, id, action, shift_action, plugin_action, plugin_shift_action)
	        , state_values (state_values)
	{
		console1->controllerMap.insert (std::make_pair (id, this));
	}

	ControllerType get_type ()
	{
		return MULTISTATE_BUTTON;
	}

	virtual void set_led_state (uint32_t state)
	{
		if (state >= state_values.size ())
			return;
		MIDI::byte buf[3];
		buf[0] = 0xB0;
		buf[1] = _id;
		buf[2] = state_values[state];

		console1->write (buf, 3);
	}

	uint32_t state_count ()
	{
		return state_values.size ();
	}

private:
	std::vector<uint32_t> state_values;
};

class Meter : public Controller
{
public:
	Meter (Console1*              console1,
	       ControllerID           id,
	       std::function<void ()> action,
	       std::function<void ()> shift_action = 0)
	        : Controller (console1, id)
	        , action (action)
	        , shift_action (shift_action)
	{
		console1->meters.insert (std::make_pair (id, this));
	}

	ControllerType get_type ()
	{
		return METER;
	}

	virtual void set_value (uint32_t value)
	{
		MIDI::byte buf[3];
		buf[0] = 0xB0;
		buf[1] = _id;
		buf[2] = value;

		console1->write (buf, 3);
	}
	std::function<void ()> action;
	std::function<void ()> shift_action;
};

} // namespace Console1

#endif // ardour_surface_console1_button_h
