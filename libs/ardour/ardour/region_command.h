#ifndef __libardour_region_command_h__
#define __libardour_region_command_h__

#include <sstream>
#include <string>
#include <vector>

#include "pbd/command.h"

namespace ARDOUR  {

class Region;

class RegionCommand : public Command {
  public:
	enum Property {
		Name,
		PositionLockStyle,
		Length,
		Start,
		Position,
		PositionOnTop,
		Layer,
		SyncPosition,
		Hidden,
		Muted,
		Opaque,
		Locked,
		PositionLocked,

		/* audio */
		ScaleAmplitude,
		FadeInActive,
		FadeInShape,
		FadeInLength,
		FadeIn,
		FadeOutActive,
		FadeOutShape,
		FadeOutLength,
		FadeOut,
		EnvelopActive,
		DefaultEnvelope
	};
	
	RegionCommand (boost::shared_ptr<Region>);
	RegionCommand (boost::shared_ptr<Region>, const XMLNode&);
	RegionCommand (boost::shared_ptr<Region>, Property, const std::string& target_value);

	
	/* this is mildly type-unsafe, in that we could pass in the wrong types for before&after
	   given the value of `property'. however, its just as safe as a variant that accepts
	   strings, and makes this whole class much easier to use.
	*/

	template<typename T> void add_property_change (Property property, const T& before, const T& after) {
		std::stringstream sb, sa;

		/* in case T is a floating point value ... 
		 */

		sb.precision (15);
		sa.precision (15);

		/* format */

		sb << before;
		sa << after;

		/* and stash it away */

		_add_property_change (property, sb.str(), sa.str());
	}
	
	void set_name (const std::string& str) { _name = str; }
	const std::string& name() const { return _name; }
	
	void operator() ();
	void undo();
	void redo() { (*this)(); }
	
	XMLNode &get_state();
	int set_state (const XMLNode&, int /*version*/);

  private:
	struct PropertyTriple {
	    Property property;
	    std::string before;
	    std::string after;

	    PropertyTriple (Property p, const std::string& b, const std::string& a)
	         : property (p), before (b), after (a) {}
	};

	boost::shared_ptr<Region> region;
	typedef std::vector<PropertyTriple> PropertyTriples;
	PropertyTriples property_changes;

	void do_property_change (Property prop, const std::string& value);
	void _add_property_change (Property, const std::string& before_value, const std::string& after_value);
};

}

#endif /* __libardour_region_command_h__ */
