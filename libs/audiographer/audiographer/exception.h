#ifndef AUDIOGRAPHER_EXCEPTION_H
#define AUDIOGRAPHER_EXCEPTION_H

#include <exception>
#include <string>

#include "pbd/compose.h"

#include "audiographer/visibility.h"
#include "audiographer/debug_utils.h"

namespace AudioGrapher
{

/** AudioGrapher Exception class.
  * Automatically tells which class an exception was thrown from.
  */
class LIBAUDIOGRAPHER_API Exception : public std::exception
{
  public:
	template<typename T>
	Exception (T const & thrower, std::string const & reason)
		: explanation(string_compose ("Exception thrown by %1: %2",  DebugUtils::demangled_name (thrower), reason))
	{}

	virtual ~Exception () throw() { }

	const char* what() const throw()
	{
		return explanation.c_str();
	}

  private:
	std::string const explanation;

};

} // namespace AudioGrapher

#endif // AUDIOGRAPHER_EXCEPTION_H
