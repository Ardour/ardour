#include "ardour/debug.h"
#include "surface.h"

#include <sstream>
#include <iomanip>
#include <iostream>

using namespace std;
using namespace PBD;
using namespace Mackie;

Surface::Surface( uint32_t max_strips, uint32_t unit_strips )
: _max_strips( max_strips ), _unit_strips( unit_strips )
{
}

void Surface::init()
{
	DEBUG_TRACE (DEBUG::MackieControl, "Surface::init\n");
	init_controls();
	init_strips( _max_strips, _unit_strips );
	DEBUG_TRACE (DEBUG::MackieControl, "Surface::init finish\n");
}

Surface::~Surface()
{
	// delete groups
	for( Groups::iterator it = groups.begin(); it != groups.end(); ++it )
	{
		delete it->second;
	}
	
	// delete controls
	for( Controls::iterator it = controls.begin(); it != controls.end(); ++it )
	{
		delete *it;
	}
}

// Mackie-specific, because of multiple devices on separate ports
// add the strips from 9..max_strips
// unit_strips is the number of strips for additional units.
void Surface::init_strips (uint32_t max_strips, uint32_t unit_strips)
{
	if ( strips.size() < max_strips ) {

		uint32_t const old_size = strips.size();
		strips.resize (max_strips);
		
		for (uint32_t i = old_size; i < max_strips; ++i) {
			// because I can't find itoa
			ostringstream os;
			os << "strip_" << i + 1;
			string name = os.str();
			
			// shallow copy existing strip
			// which works because the controls
			// have the same ids across units
			// TODO this needs to be a deep copy because
			// controls hold state now - in_use
			Strip * strip = new Strip( *strips[i % unit_strips] );
			
			// update the relevant values
			strip->index (i);
			strip->name (name);
			
			// add to data structures
			groups[name] = strip;
			strips[i] = strip;
		}
	}
}
