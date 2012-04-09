#ifndef __ardour_mackie_control_protocol_control_group_h__
#define __ardour_mackie_control_protocol_control_group_h__

#include <vector>

namespace Mackie {

class Control;

/**
	This is a loose group of controls, eg cursor buttons,
	transport buttons, functions buttons etc.
*/
class Group
{
public:
	Group (const std::string & name)
		: _name (name) {}

	virtual ~Group() {}
	
	virtual bool is_strip() const { return false; }
	virtual bool is_master() const { return false; }
	
	virtual void add (Control & control);
	
	const std::string & name() const { return _name; }
	void set_name (const std::string & rhs) { _name = rhs; }
	
	typedef std::vector<Control*> Controls;
	const Controls & controls() const { return _controls; }
	
protected:
	Controls _controls;
	
private:
	std::string _name;
};

}

#endif
