#ifndef __irix_platform__
#define __irix_platform__

#include <pbd/platform.h>

class IrixPlatform : public Platform {
  public:
	IrixPlatform () : Platform () {};
	virtual ~IrixPlatform ();

	virtual int pre_config ();
	virtual int post_config ();
	virtual int pre_ui ();
	virtual int post_ui ();
	
	virtual int dsp_startup();
};


#endif // __irix_platform__
