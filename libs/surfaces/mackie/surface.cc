#include "surface.h"

#include <sstream>
#include <iomanip>
#include <iostream>

using namespace std;
using namespace Mackie;

Surface::Surface( uint32_t max_strips, uint32_t unit_strips )
: _max_strips( max_strips ), _unit_strips( unit_strips )
{
}

void Surface::init()
{
#ifdef DEBUG
	cout << "Surface::init" << endl;
#endif
	init_controls();
	init_strips( _max_strips, _unit_strips );
#ifdef DEBUG
	cout << "Surface::init finish" << endl;
#endif
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
void Surface::init_strips( uint32_t max_strips, uint32_t unit_strips )
{
	if ( strips.size() < max_strips )
	{
		strips.resize( max_strips );
		for ( uint32_t i = strips.size(); i < max_strips; ++i )
		{
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
			strip->index( i );
			strip->name( name );
			
			// add to data structures
			groups[name] = strip;
			strips[i] = strip;
		}
	}
}
