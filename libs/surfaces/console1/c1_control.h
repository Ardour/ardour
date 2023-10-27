#ifndef ardour_surface_console1_button_h
#define ardour_surface_console1_button_h

#include "ardour/debug.h"
#include "console1.h"

namespace ArdourSurface {

using ControllerID = Console1::ControllerID;

class Controller
{
  public:
	enum ControllerType
	{
		CONTROLLER,
		CONTROLLER_BUTTON,
		MULTISTATE_BUTTON,
		ENCODER,
		METER
	};

	Controller (Console1* console1, ControllerID id)
	  : console1 (console1)
	  , _id (id)
	{
	}

	virtual ~Controller () {}

	Console1* console1;
	ControllerID id () const { return _id; }

	virtual ControllerType get_type () { return CONTROLLER; }

  protected:
	ControllerID _id;
};

class ControllerButton : public Controller
{
  public:
	ControllerButton (Console1* console1,
	                  ControllerID id,
	                  boost::function<void (uint32_t)> action,
	                  boost::function<void (uint32_t)> shift_action = 0,
	                  boost::function<void (uint32_t)> plugin_action = 0,
                      boost::function<void (uint32_t)> plugin_shift_action = 0 )
	  : Controller (console1, id)
	  , action (action)
	  , shift_action (shift_action)
	  , plugin_action (plugin_action)
      , plugin_shift_action (plugin_shift_action)
	{
		console1->buttons.insert (std::make_pair (id, this));
	}

	ControllerType get_type () { return CONTROLLER_BUTTON; }

	void set_plugin_action (boost::function<void (uint32_t)> action) { plugin_action = action; }
	void set_plugin_shift_action (boost::function<void (uint32_t)> action) { plugin_shift_action = action; }

	virtual void set_led_state (bool onoff)
	{
		// DEBUG_TRACE(DEBUG::Console1, "ControllerButton::set_led_state ...\n");
		MIDI::byte buf[3];
		buf[0] = 0xB0;
		buf[1] = _id;
		buf[2] = onoff ? 127 : 0;

		console1->write (buf, 3);
	}

	virtual void set_led_value (uint32_t val)
	{
		// DEBUG_TRACE(DEBUG::Console1, "ControllerButton::set_led_state ...\n");
		MIDI::byte buf[3];
		buf[0] = 0xB0;
		buf[1] = _id;
		buf[2] = val;

		console1->write (buf, 3);
	}
	boost::function<void (uint32_t)> action;
	boost::function<void (uint32_t)> shift_action;
	boost::function<void (uint32_t)> plugin_action;
	boost::function<void (uint32_t)> plugin_shift_action;
};

class MultiStateButton : public Controller
{
  public:
	MultiStateButton (Console1* console1,
	                  ControllerID id,
	                  std::vector<uint32_t> state_values,
	                  boost::function<void (uint32_t)> action,
	                  boost::function<void (uint32_t)> shift_action = 0,
	                  boost::function<void (uint32_t)> plugin_action = 0,
	                  boost::function<void (uint32_t)> plugin_shift_action = 0
                      )
	  : Controller (console1, id)
	  , action (action)
	  , shift_action (shift_action)
	  , plugin_action (action)
	  , plugin_shift_action (shift_action)
	  , state_values (state_values)
	{
		console1->multi_buttons.insert (std::make_pair (id, this));
	}

	ControllerType get_type () { return MULTISTATE_BUTTON; }

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

	void set_plugin_action (boost::function<void (uint32_t)> action) { plugin_action = action; }
	void set_plugin_shift_action (boost::function<void (uint32_t)> action) { plugin_shift_action = action; }

	uint32_t state_count () { return state_values.size (); }

	boost::function<void (uint32_t)> action;
	boost::function<void (uint32_t)> shift_action;
	boost::function<void (uint32_t)> plugin_action;
	boost::function<void (uint32_t)> plugin_shift_action;

  private:
	std::vector<uint32_t> state_values;
};

class Meter : public Controller
{
  public:
	Meter (Console1* console1,
	       ControllerID id,
	       boost::function<void ()> action,
	       boost::function<void ()> shift_action = 0)
	  : Controller (console1, id)
	  , action (action)
	  , shift_action (shift_action)
	{
		console1->meters.insert (std::make_pair (id, this));
	}

	ControllerType get_type () { return METER; }

	virtual void set_value (uint32_t value)
	{
		MIDI::byte buf[3];
		buf[0] = 0xB0;
		buf[1] = _id;
		buf[2] = value;

		console1->write (buf, 3);
	}
	boost::function<void ()> action;
	boost::function<void ()> shift_action;
};

class Encoder : public Controller
{
  public:
	Encoder (Console1* console1,
	         ControllerID id,
	         boost::function<void (uint32_t)> action,
	         boost::function<void (uint32_t)> shift_action = 0,
	         boost::function<void (uint32_t)> plugin_action = 0,
             boost::function<void (uint32_t)> plugin_shift_action = 0)
	  : Controller (console1, id)
	  , action (action)
	  , shift_action (shift_action)
	  , plugin_action (plugin_action)
	  , plugin_shift_action (plugin_action)
	{
		console1->encoders.insert (std::make_pair (id, this));
	}

	ControllerType get_type () { return ENCODER; }

	void set_plugin_action (boost::function<void (uint32_t)> action) { plugin_action = action; }
	void set_plugin_shift_action (boost::function<void (uint32_t)> action) { plugin_shift_action = action; }

	virtual void set_value (uint32_t value)
	{
		MIDI::byte buf[3];
		buf[0] = 0xB0;
		buf[1] = _id;
		buf[2] = value;

		console1->write (buf, 3);
	}
	boost::function<void (uint32_t)> action;
	boost::function<void (uint32_t val)> shift_action;
	boost::function<void (uint32_t val)> plugin_action;
	boost::function<void (uint32_t val)> plugin_shift_action;

	PBD::Signal1<void, uint32_t>* plugin_signal;
};

}
#endif // ardour_surface_console1_button_h
