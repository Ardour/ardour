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

#ifndef _ardour_surfaces_fp8button_h_
#define _ardour_surfaces_fp8button_h_

#include <stdint.h>

#include "pbd/base_ui.h"
#include "pbd/signals.h"

#include "fp8_base.h"

namespace ArdourSurface { namespace FP_NAMESPACE {

/* virtual base-class and interface */
class FP8ButtonInterface
{
public:
	FP8ButtonInterface () {}
	virtual ~FP8ButtonInterface () {}

	/* user API */
	PBD::Signal0<void> pressed;
	PBD::Signal0<void> released;

	virtual bool is_pressed () const { return false; }
	virtual bool is_active () const { return false; }

	virtual void ignore_release () {}

	/* internal API - called from midi thread,
	 * user pressed/released button the device
	 */
	virtual bool midi_event (bool) = 0;

	/* internal API - called from surface thread
	 * set Light on the button
	 */
	virtual void set_active (bool a) = 0;
	virtual void set_color (uint32_t rgba) {}
	virtual void set_blinking (bool) {}

	static bool force_change; // used during init
};

/* ****************************************************************************
 * Implementations
 */

class FP8DummyButton : public FP8ButtonInterface
{
public:
	virtual void set_active (bool a) {}
	virtual bool midi_event (bool) { return false; }
};


/* common implementation */
class FP8ButtonBase : public FP8ButtonInterface
{
public:
	FP8ButtonBase (FP8Base& b)
		: _base (b)
		, _pressed (false)
		, _active (false)
		, _ignore_release (false)
		, _rgba (0)
		, _blinking (false)
	{ }

	bool is_pressed () const { return _pressed; }
	bool is_active () const { return _active; }

	virtual bool midi_event (bool a)
	{
		if (a == _pressed) {
			return false;
		}
		_pressed = a;
		if (a) {
			pressed (); /* EMIT SIGNAL */
		} else {
			if (_ignore_release) {
				_ignore_release = false;
			} else {
				released (); /* EMIT SIGNAL */
			}
		}
		return true;
	}

	virtual void ignore_release () {
		if (_pressed) {
			_ignore_release = true;
		}
	}

	bool blinking () const { return _blinking; }

	void set_blinking (bool yes) {
		if (yes && !_blinking) {
			_blinking = true;
			_base.BlinkIt.connect_same_thread (_blink_connection, boost::bind (&FP8ButtonBase::blink, this, _1));
		} else if (!yes && _blinking) {
			_blink_connection.disconnect ();
			_blinking = false;
			blink (true);
		}
	}

protected:
	FP8Base&              _base;
	bool                  _pressed;
	bool                  _active;
	bool                  _ignore_release;
	uint32_t              _rgba;
	virtual void blink (bool onoff) = 0;

private:
	PBD::ScopedConnection _blink_connection;
	bool _blinking;
};

/* A basic LED or RGB button, not shift sensitive */
class FP8Button : public FP8ButtonBase
{
public:
	FP8Button (FP8Base& b, uint8_t id, bool color = false)
		: FP8ButtonBase (b)
		, _midi_id (id)
		, _has_color (color)
	{ }

	virtual void set_active (bool a)
	{
		if (_active == a && !force_change) {
			return;
		}
		_active = a;
		_base.tx_midi3 (0x90, _midi_id, a ? 0x7f : 0x00);
	}

	void set_color (uint32_t rgba)
	{
		if (!_has_color || _rgba == rgba) {
			return;
		}
		_rgba = rgba;
		_base.tx_midi3 (0x91, _midi_id, (_rgba >> 25) & 0x7f);
		_base.tx_midi3 (0x92, _midi_id, (_rgba >> 17) & 0x7f);
		_base.tx_midi3 (0x93, _midi_id, (_rgba >>  9) & 0x7f);
	}

protected:
	void blink (bool onoff)
	{
		if (!_active) { return; }
		_base.tx_midi3 (0x90, _midi_id, onoff ? 0x7f : 0x00);
	}

	uint8_t  _midi_id; // MIDI-note
	bool     _has_color;
};

/* footswitch and encoder-press buttons */
class FP8ReadOnlyButton : public FP8Button
{
public:
	FP8ReadOnlyButton (FP8Base& b, uint8_t id, bool color = false)
		: FP8Button (b, id, color)
	{}

	void set_active (bool) { }
};

/* virtual button. used for shift toggle. */
class ShadowButton : public FP8ButtonBase
{
public:
	ShadowButton (FP8Base& b)
		: FP8ButtonBase (b)
	{}

	PBD::Signal1<void, bool> ActiveChanged;
	PBD::Signal0<void> ColourChanged;

	uint32_t color () const { return _rgba; }

	bool midi_event (bool a)
	{
		assert (0);
		return false;
	}

	bool set_pressed (bool a)
	{
		return FP8ButtonBase::midi_event (a);
	}

	void set_active (bool a)
	{
		if (_active == a && !force_change) {
			return;
		}
		_active = a;
		ActiveChanged (a); /* EMIT SIGNAL */
	}

	void set_color (uint32_t rgba)
	{
		if (_rgba == rgba) {
			return;
		}
		_rgba = rgba;
		ColourChanged ();
	}

protected:
	void blink (bool onoff) {
		if (!_active) { return; }
		ActiveChanged (onoff);
	}
};

/* Wraps 2 buttons with the same physical MIDI ID */
class FP8DualButton : public FP8ButtonInterface
{
public:
	FP8DualButton (FP8Base& b, uint8_t id, bool color = false)
		: _base (b)
		, _b0 (b)
		, _b1 (b)
		, _midi_id (id)
		, _has_color (color)
		, _rgba (0)
		, _shift (false)
	{
		_b0.ActiveChanged.connect_same_thread (_button_connections, boost::bind (&FP8DualButton::active_changed, this, false, _1));
		_b1.ActiveChanged.connect_same_thread (_button_connections, boost::bind (&FP8DualButton::active_changed, this, true, _1));
		if (_has_color) {
			_b0.ColourChanged.connect_same_thread (_button_connections, boost::bind (&FP8DualButton::colour_changed, this, false));
			_b1.ColourChanged.connect_same_thread (_button_connections, boost::bind (&FP8DualButton::colour_changed, this, true));
		}
	}

	bool midi_event (bool a) {
		return (_shift ? _b1 : _b0).set_pressed (a);
	}

	void set_active (bool a) {
		/* This button is never directly used
		 * by the libardour side API.
		 */
		assert (0);
	}

	void active_changed (bool s, bool a) {
		if (s != _shift) {
			return;
		}
		_base.tx_midi3 (0x90, _midi_id, a ? 0x7f : 0x00);
	}

	void colour_changed (bool s) {
		if (s != _shift || !_has_color) {
			return;
		}
		uint32_t rgba = (_shift ? _b1 : _b0).color ();
		if (rgba == _rgba) {
			return;
		}
		_rgba = rgba;
		_base.tx_midi3 (0x91, _midi_id, (rgba >> 25) & 0x7f);
		_base.tx_midi3 (0x92, _midi_id, (rgba >> 17) & 0x7f);
		_base.tx_midi3 (0x93, _midi_id, (rgba >>  9) & 0x7f);
	}

	FP8ButtonInterface* button () { return &_b0; }
	FP8ButtonInterface* button_shift () { return &_b1; }

protected:
	FP8Base&     _base;

	virtual void connect_toggle () = 0;

	void shift_changed (bool shift) {
		if (_shift == shift) {
			return;
		}
		(_shift ? _b1 : _b0).set_pressed (false);
		_shift = shift;
		active_changed (_shift, (_shift ? _b1 : _b0).is_active());
		colour_changed (_shift);
	}

private:
	ShadowButton _b0;
	ShadowButton _b1;
	uint8_t      _midi_id; // MIDI-note
	bool         _has_color;
	uint32_t     _rgba;
	bool         _shift;
	PBD::ScopedConnectionList _button_connections;
};

class FP8ShiftSensitiveButton : public FP8DualButton
{
public:
	FP8ShiftSensitiveButton (FP8Base& b, uint8_t id, bool color = false)
		:FP8DualButton (b, id, color)
	{
		connect_toggle ();
	}

protected:
	void connect_toggle ()
	{
		_base.ShiftButtonChange.connect_same_thread (_shift_connection, boost::bind (&FP8ShiftSensitiveButton::shift_changed, this, _1));
	}

private:
	PBD::ScopedConnection _shift_connection;
};

class FP8ARMSensitiveButton : public FP8DualButton
{
public:
	FP8ARMSensitiveButton (FP8Base& b, uint8_t id, bool color = false)
		:FP8DualButton (b, id, color)
	{
		connect_toggle ();
	}

protected:
	void connect_toggle ()
	{
		_base.ARMButtonChange.connect_same_thread (_arm_connection, boost::bind (&FP8ARMSensitiveButton::shift_changed, this, _1));
	}

private:
	PBD::ScopedConnection _arm_connection;
};


// short press: activate in press, deactivate on release,
// long press + hold, activate on press, de-activate directly on release
// e.g. mute/solo  press + hold => changed()
class FP8MomentaryButton : public FP8ButtonBase
{
public:
	FP8MomentaryButton (FP8Base& b, uint8_t id)
		: FP8ButtonBase (b)
		, _midi_id (id)
	{}

	~FP8MomentaryButton () {
		_hold_connection.disconnect ();
	}

	PBD::Signal1<void, bool> StateChange;

	void set_active (bool a)
	{
		if (_active == a && !force_change) {
			return;
		}
		_active = a;
		_base.tx_midi3 (0x90, _midi_id, a ? 0x7f : 0x00);
	}

	void reset ()
	{
		_was_active_on_press = false;
		_hold_connection.disconnect ();
	}

	void ignore_release () { }

	bool midi_event (bool a)
	{
		if (a == _pressed) {
			return false;
		}

		_pressed = a;

		if (a) {
			_was_active_on_press = _active;
		}

		if (a && !_active) {
			_momentaty = false;
			StateChange (true); /* EMIT SIGNAL */
			Glib::RefPtr<Glib::TimeoutSource> hold_timer =
				Glib::TimeoutSource::create (500);
			hold_timer->attach (fp8_loop()->get_context());
			_hold_connection = hold_timer->connect (sigc::mem_fun (*this, &FP8MomentaryButton::hold_timeout));
		} else if (!a && _was_active_on_press) {
			_hold_connection.disconnect ();
			_momentaty = false;
			StateChange (false); /* EMIT SIGNAL */
		} else if (!a && _momentaty) {
			_hold_connection.disconnect ();
			_momentaty = false;
			StateChange (false); /* EMIT SIGNAL */
		}
		return true;
	}

protected:
	void blink (bool onoff)
	{
		if (!blinking ()) {
			_base.tx_midi3 (0x90, _midi_id, _active ? 0x7f : 0x00);
			return;
		}
		_base.tx_midi3 (0x90, _midi_id, onoff ? 0x7f : 0x00);
	}

	uint8_t  _midi_id; // MIDI-note
	bool     _momentaty;
	bool     _was_active_on_press;

private:
	bool hold_timeout ()
	{
		_momentaty = true;
		return false;
	}
	sigc::connection _hold_connection;
};

/* an auto-repeat button.
 * press + hold emits continuous "press" events.
 */
class FP8RepeatButton : public FP8Button
{
public:
	FP8RepeatButton (FP8Base& b, uint8_t id, bool color = false)
		: FP8Button (b, id, color)
		, _skip (0)
	{}

	~FP8RepeatButton ()
	{
		stop_repeat ();
	}

	bool midi_event (bool a)
	{
		bool rv = FP8Button::midi_event (a);
		if (rv && a) {
			start_repeat ();
		}
		return rv;
	}

	void stop_repeat ()
	{
		_press_timeout_connection.disconnect ();
	}

private:
	void start_repeat ()
	{
		stop_repeat ();
		_skip = 5;
		Glib::RefPtr<Glib::TimeoutSource> press_timer =
			Glib::TimeoutSource::create (100);
		press_timer->attach (fp8_loop()->get_context());
		_press_timeout_connection = press_timer->connect (sigc::mem_fun (*this, &FP8RepeatButton::repeat_press));
	}

	bool repeat_press ()
	{
		if (!_pressed) {
			return false;
		}
		if (_skip > 0) {
			--_skip;
			return true;
		}
		pressed ();
		return true;
	}

	int _skip;
	sigc::connection _press_timeout_connection;
};

} } /* namespace */
#endif /* _ardour_surfaces_fp8button_h_ */
