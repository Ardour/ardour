#ifndef __libpbd_stacktrace_h__
#define __libpbd_stacktrace_h__

#include <ostream>

namespace PBD {
	void stacktrace (std::ostream& out, int levels = 0);
}

#endif /* __libpbd_stacktrace_h__ */
