#ifndef __ardour_mackie_control_protocol_strip_h__
#define __ardour_mackie_control_protocol_strip_h__

#include <string>
#include <iostream>

#include "control_group.h"

namespace Mackie {

class Control;
class Surface;
class Button;
class Pot;
class Fader;
class Meter;

struct StripControlDefinition {
    const char* name;
    uint32_t base_id;
    Control* (*factory)(Surface&, int index, int ordinal, const char* name, Group&);
};

struct GlobalControlDefinition {
    const char* name;
    uint32_t id;
    Control* (*factory)(Surface&, int index, int ordinal, const char* name, Group&);
    const char* group_name;
};

/**
	This is the set of controls that make up a strip.
*/
class Strip : public Group
{
public:
	Strip (const std::string& name, int index); /* master strip only */
	Strip (Surface&, const std::string & name, int surface_number, int index, int unit_index, StripControlDefinition* ctls);

	virtual bool is_strip() const { return true; }
	virtual void add (Control & control);
	int index() const { return _index; } // zero based
	
	Button & solo();
	Button & recenable();
	Button & mute();
	Button & select();
	Button & vselect();
	Button & fader_touch();
	Pot & vpot();
	Fader & gain();
	Meter& meter ();

	bool has_solo() const { return _solo != 0; }
	bool has_recenable() const { return _recenable != 0; }
	bool has_mute() const { return _mute != 0; }
	bool has_select() const { return _select != 0; }
	bool has_vselect() const { return _vselect != 0; }
	bool has_fader_touch() const { return _fader_touch != 0; }
	bool has_vpot() const { return _vpot != 0; }
	bool has_gain() const { return _gain != 0; }
	bool has_meter() const { return _meter != 0; }
private:
	Button* _solo;
	Button* _recenable;
	Button* _mute;
	Button* _select;
	Button* _vselect;
	Button* _fader_touch;
	Pot*    _vpot;
	Fader*  _gain;
	Meter*  _meter;
	int     _index;
};

std::ostream & operator <<  (std::ostream &, const Strip &);

class MasterStrip : public Strip
{
public:
	MasterStrip (const std::string & name, int index)
		: Strip (name, index) {}
	
	virtual bool is_master() const  { return true; }
};

}

#endif /* __ardour_mackie_control_protocol_strip_h__ */
