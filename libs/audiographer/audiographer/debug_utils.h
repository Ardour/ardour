#ifndef AUDIOGRAPHER_DEBUG_UTILS_H
#define AUDIOGRAPHER_DEBUG_UTILS_H

#include <string>

#ifdef __GNUC__
#include <cxxabi.h>
#endif

namespace AudioGrapher
{

struct DebugUtils
{
	template<typename T>
	static std::string demangled_name (T const & obj)
	{
#ifdef __GNUC__
		int status;
		char * res = abi::__cxa_demangle (typeid(obj).name(), 0, 0, &status);
		if (status == 0) {
			std::string s(res);
			free (res);
			return s;
		}
#endif
		return typeid(obj).name();
	}
	
};

} // namespace

#endif // AUDIOGRAPHER_DEBUG_UTILS_H
