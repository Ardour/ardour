/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
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

#ifndef _ardour_surfaces_m2button_h_
#define _ardour_surfaces_m2button_h_

#include <stdint.h>
#include "gtkmm2ext/colors.h"
#include "pbd/signals.h"

namespace ArdourSurface {

class M2ButtonInterface
{
	public:
		M2ButtonInterface () {}
		virtual ~M2ButtonInterface () {}

		/* user API */
		PBD::Signal1<void, bool> changed;
		PBD::Signal0<void> pressed;
		PBD::Signal0<void> released;

		virtual void set_blinking (bool) {}
		virtual void set_color (uint32_t rgba) {}

		virtual bool is_pressed () const { return false; }
		virtual bool active () const { return is_pressed (); }

		virtual void ignore_release () {}

		// TODO allow to suspend *next* release signal
		// e.g. press + hold "grid", move encoder -> release "grid" -> noop

		/* internal API - called from device thread */
		virtual bool set_active (bool a) { return false; }
		virtual uint8_t lightness (float) const { return 0; }
		virtual uint32_t color (float) const { return 0; }
};

class M2Button : public M2ButtonInterface
{
	public:
		M2Button ()
			: M2ButtonInterface ()
			, _pressed (false)
			, _blink (false)
			, _ignore_release (false)
			, _lightness (0)
			, _rgba (0)
		{}

		/* user API */
		void set_blinking (bool en) {
			_blink = en;
		}

		virtual void set_color (uint32_t rgba) {
			_rgba = rgba;
			/* 7 bit color */
			const uint8_t r = ((rgba >> 24) & 0xff) >> 1;
			const uint8_t g = ((rgba >> 16) & 0xff) >> 1;
			const uint8_t b = ((rgba >>  8) & 0xff) >> 1;
			_lightness = std::max (r, std::max (g, b));
		}

		bool is_pressed () const { return _pressed; }

		void ignore_release () {
			if (_pressed) {
				_ignore_release = true;
			}
		}
		
		/* internal API - called from device thread */
		virtual bool set_active (bool a) {
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
			changed (a); /* EMIT SIGNAL */
			return true;
		}

		uint8_t lightness (float blink) const {
			if (_blink && blink >= 0.f && blink <= 1.f) {
				return (uint8_t) floorf(blink * _lightness);
			}
			return _lightness;
		}

		uint32_t color (float blink) const {
			if (_blink && blink >= 0.f && blink <= 1.f) {
				Gtkmm2ext::HSV hsv (_rgba);
				Gtkmm2ext::HSV s (hsv.shade (blink));
				return s.color();
			}
			return _rgba;
		}

	protected:
		bool     _pressed;
		bool     _blink;
		bool     _ignore_release;
		uint8_t  _lightness;
		uint32_t _rgba;
};

class M2StatelessButton : public M2Button
{
	public:
		M2StatelessButton () : M2Button () {}

		bool set_active (bool a) {
			if (a == _pressed) {
				return false;
			}
			if (a) {
				set_color (0xffffffff);
			} else {
				set_color (0x000000ff);
			}
			return M2Button::set_active (a);
		}
};

class M2ToggleButton : public M2Button
{
	public:
		M2ToggleButton ()
		: M2Button ()
		, _active (false)
		{
			changed.connect_same_thread (changed_connection, boost::bind (&M2ToggleButton::change_event, this, _1));
		}

		PBD::Signal1<void, bool> toggled;
		bool active () const { return _active; }

	protected:
		void change_event (bool down) {
			if (down) { return; }
			_active = !_active;
			set_color (_active ? 0xffffffff : 0x000000ff);
			toggled (_active);
		}

		PBD::ScopedConnection changed_connection;
		bool _active;
};

class M2ToggleHoldButton : public M2Button
{
	public:
		M2ToggleHoldButton ()
		: M2Button ()
		, _active (false)
		, _active_on_release (false)
		{
			changed.connect_same_thread (changed_connection, boost::bind (&M2ToggleHoldButton::change_event, this, _1));
		}

		PBD::Signal1<void, bool> toggled;
		bool active () const { return _active; }
		void unset_active_on_release () { if (is_pressed ()) { _active_on_release = false; } }

	protected:
		void change_event (bool down) {
			if (down) {
				if (_active) {
					_active_on_release = false;
					return;
				}
				_active = true;
				_active_on_release = true;
			} else {
				if (_active == _active_on_release) {
					return;
				}
				_active = _active_on_release;
			}

			set_color (_active ? 0xffffffff : 0x000000ff);
			toggled (_active);
		}

		PBD::ScopedConnection changed_connection;
		bool _active;
		bool _active_on_release;
};


} /* namespace */
#endif /* _ardour_surfaces_m2button_h_ */

