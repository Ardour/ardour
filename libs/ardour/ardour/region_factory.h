#ifndef __ardour_region_factory_h__
#define __ardour_region_factory_h__

#include <ardour/types.h>
#include <ardour/region.h>

class XMLNode;

namespace ARDOUR {

class Session;

Region* createRegion (const Region&, jack_nframes_t start, 
		      jack_nframes_t length, std::string name, 
		      layer_t = 0, Region::Flag flags = Region::DefaultFlags);
// Region* createRegion (const Region&, std::string name);
Region* createRegion (const Region&);
Region* createRegion (Session&, XMLNode&, bool);

}

#endif /* __ardour_region_factory_h__  */
