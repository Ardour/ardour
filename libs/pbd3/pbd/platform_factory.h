#ifndef __platform_factory__
#define __platform_factory__

#include <pbd/platform.h>

class PlatformFactory {
public:
	static Platform* create_platform ();
};


#endif // __platform_factory__
