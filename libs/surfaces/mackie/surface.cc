#include "ardour/debug.h"
#include "surface.h"

#include <sstream>
#include <iomanip>
#include <iostream>

using namespace std;
using namespace PBD;
using namespace Mackie;

Surface::Surface( uint32_t max_strips, uint32_t unit_strips )
	: _max_strips (max_strips)
	, _unit_strips( unit_strips )
{
}

void Surface::init()
{
	DEBUG_TRACE (DEBUG::MackieControl, "Surface::init\n");
	init_controls ();
	init_strips ();
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

