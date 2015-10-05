/*
 *   Copyright (C) 2006 Paul Davis
 *   Copyright (C) 2007 Michael Taht
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *   */

/* The Bling class theoretically knows nothing about the device it's blinging
   and depends on the overlying implementation to tell it about the format of the
   device. Maybe this will become a template or people will inherit from it */

/* Bling is where all the bad, bad, marketing driven ideas go */

class bling {
public:
	enum BlingMode {
	        BlingOff = 0,
	        BlingOn = 1,
		BlingEnter = 2,
		BlingExit = 4,
		// Light Specific Stuff
	        BlingKit = 8,
	        BlingRotating = 16,
	        BlingPairs = 32,
	        BlingRows = 64,
		BlingColumns = 128,
	        BlingFlashAllLights = 256,
		// Screen Specific Stuff
		// Slider Specific Stuff
		BlingSliderMax,
		BlingSliderMid,
		BlingSliderMin,
		// Random stuff
		BlingRandomLight,
		BlingRandomSlider,
		BlingRandomScreen,
		BlingAllSliders
	};
	bling();
	~bling();
	set(BlingMode);
	unset(BlingMode);
	run();
	next();
	prev();
	msg(string&);
	scrollmsg(string&);

protected:
// The as yet undefined "advanced_ui" class provides methods to find out at run time
// what the heck is what
	BlingMode blingmode;
	advancedUI *intf;
	int last_light;
// I don't think these actually need to be part of the public definition of the class
	enter();
	exit();
	rotate();
// etc
};

// make absolutely sure we have the pointer to the interface
// something like this

#define BLING_INTFA(a) (intf)? 0:intf->a
#define BLING_INTF(a) { if (intf) { intf->a; } else { return 0; } }

// Should any of these bother to return a status code?

bling::rotate() {
	BLING_INTF(light(last_light,off));
	last_light = BLING_INTFA(next_light(last_light));
	BLING_INTF(light(last_light,on));
}

bling::enter() {
}

bling::exit() {
}

bling::flashall() {
}

bling::rows() {
}

bling::columns() {
}

bling::msg() {
}

bling::scrollmsg() {
}

// Based on the current bling mode, do whatever it is you are going to do
bling::run() {

}

// etc
