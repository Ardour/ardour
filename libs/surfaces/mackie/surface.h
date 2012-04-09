#ifndef mackie_surface_h
#define mackie_surface_h

#include "controls.h"
#include "types.h"
#include <stdint.h>

namespace Mackie
{

class MackieButtonHandler;
class SurfacePort;
class MackieMidiBuilder;
class Button;
class Meter;
class Fader;
class Jog;
class Pot;
class Led;
class LedRing;

/**
	This represents an entire control surface, made up of Groups,
	Strips and Controls. There are several collections for
	ease of addressing in different ways, but only one collection
	has definitive ownership.

	It handles mapping button ids to press_ and release_ calls.

	There are various emulations of the Mackie around, so specific
	emulations will inherit from this to change button mapping, or 
	have 7 fader channels instead of 8, or whatever.

	Currently there are BcfSurface and MackieSurface.

	TODO maybe make Group inherit from Control, for ease of ownership.
*/
class Surface
{
public:
	/**
		A Surface can be made up of multiple units. eg one Mackie MCU plus
		one or more Mackie MCU extenders.
		
		\param max_strips is the number of strips for the entire surface.
		\param unit_strips is the number of strips per unit.
	*/

	Surface (uint32_t max_strips, uint32_t unit_strips);
	virtual ~Surface();

	/// Calls the virtual initialisation methods. This *must* be called after
	/// construction, because c++ is too dumb to call virtual methods from
	/// inside a constructor
	void init();

	typedef std::vector<Control*> Controls;
	
	/// This collection has ownership of all the controls
	Controls controls;

	/**
		These are alternative addressing schemes
		They use maps because the indices aren't always
		0-based.
		
		Indexed by raw_id not by id. @see Control for the distinction.
	*/
	std::map<int,Fader*> faders;
	std::map<int,Pot*> pots;
	std::map<int,Button*> buttons;
	std::map<int,Led*> leds;
	std::map<int,Meter*> meters;

	/// no strip controls in here because they usually
	/// have the same names.
	std::map<std::string,Control*> controls_by_name;

	/// The collection of all numbered strips. No master
	/// strip in here.
	typedef std::vector<Strip*> Strips;
	Strips strips;

	/// This collection owns the groups
	typedef std::map<std::string,Group*> Groups;
	Groups groups;

	uint32_t max_strips() const { return _max_strips; }
	
	/// map button ids to calls to press_ and release_ in mbh
	virtual void handle_button (MackieButtonHandler & mbh, ButtonState bs, Button & button);

public:
	/// display an indicator of the first switched-in Route. Do nothing by default.
	virtual void display_bank_start( SurfacePort &, MackieMidiBuilder &, uint32_t /*current_bank*/ ) {};
		
	/// called from MackieControlPRotocol::zero_all to turn things off
	virtual void zero_all( SurfacePort &, MackieMidiBuilder & ) {};

	/// turn off leds around the jog wheel. This is for surfaces that use a pot
	/// pretending to be a jog wheel.
	virtual void blank_jog_ring( SurfacePort &, MackieMidiBuilder & ) {};

	virtual bool has_timecode_display() const = 0;
	virtual void display_timecode( SurfacePort &, MackieMidiBuilder &, const std::string & /*timecode*/, const std::string & /*timecode_last*/) {};
	
public:
	/**
		This is used to calculate the clicks per second that define
		a transport speed of 1.0 for the jog wheel. 100.0 is 10 clicks
		per second, 50.5 is 5 clicks per second.
	*/
	virtual float scrub_scaling_factor() = 0;

	/**
		The scaling factor function for speed increase and decrease. At
		low transport speeds this should return a small value, for high transport
		speeds, this should return an exponentially larger value. This provides
		high definition control at low speeds and quick speed changes to/from
		higher speeds.
	*/
	virtual float scaled_delta( const ControlState & state, float current_speed ) = 0;

protected:
	virtual void init_controls();
	virtual void init_strips ();

	const uint32_t _max_strips;
	const uint32_t _unit_strips;
};

}

#endif
