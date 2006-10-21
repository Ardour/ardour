#ifndef __ardour_region_factory_h__
#define __ardour_region_factory_h__

#include <ardour/types.h>
#include <ardour/region.h>

class XMLNode;

namespace ARDOUR {

class Session;

class RegionFactory {

  public:
	/** This is emitted only when a new id is assigned. Therefore,
	   in a pure Region copy, it will not be emitted.

	   It must be emitted by derived classes, not Region
	   itself, to permit dynamic_cast<> to be used to 
	   infer the type of Region.
	*/
	static sigc::signal<void,boost::shared_ptr<Region> > CheckNewRegion;

	static boost::shared_ptr<Region> create (boost::shared_ptr<Region>, nframes_t start, 
						 nframes_t length, std::string name, 
						 layer_t = 0, Region::Flag flags = Region::DefaultFlags, bool announce = true);
	static boost::shared_ptr<Region> create (boost::shared_ptr<AudioRegion>, nframes_t start, 
						 nframes_t length, std::string name, 
						 layer_t = 0, Region::Flag flags = Region::DefaultFlags, bool announce = true);
	static boost::shared_ptr<Region> create (boost::shared_ptr<Source>, nframes_t start, nframes_t length, const string& name, layer_t = 0, Region::Flag flags = Region::DefaultFlags, bool announce = true);
	static boost::shared_ptr<Region> create (SourceList &, nframes_t start, nframes_t length, const string& name, layer_t = 0, Region::Flag flags = Region::DefaultFlags, bool announce = true);
	static boost::shared_ptr<Region> create (boost::shared_ptr<Region>);
	static boost::shared_ptr<Region> create (Session&, XMLNode&, bool);
	static boost::shared_ptr<Region> create (SourceList &, const XMLNode&);
};

}

#endif /* __ardour_region_factory_h__  */
