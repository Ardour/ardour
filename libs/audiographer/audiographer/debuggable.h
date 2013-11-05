#ifndef AUDIOGRAPHER_DEBUGGABLE_H
#define AUDIOGRAPHER_DEBUGGABLE_H

#ifndef DEFAULT_DEBUG_LEVEL
#define DEFAULT_DEBUG_LEVEL DebugNone
#endif

#include <iostream>

namespace AudioGrapher
{

/// Compile time defined debug level
enum DebugLevel
{
	DebugNone,     ///< Disabled
	DebugObject,   ///< Object level stuff, ctors, initalizers etc.
	DebugFlags,    ///< Debug ProcessContext flags only on process cycle level
	DebugProcess,  ///< Process cycle level stuff
	DebugVerbose,  ///< Lots of output, not on sample level
	DebugSample    ///< Sample level stuff
};

/** Class that allows optimizing out debugging code during compile time.
  * Usage: to take all advantage of this class you should wrap all 
  * debugging statemets like this:
  * \code
  * if (debug_level (SomeDebugLevel) && other_optional_conditionals) {
  * 	debug_stream() << "Debug output" << std::endl;
  * }
  * \endcode
  *
  * The order of the conditionals in the if-clause is important.
  * The checks specified in \a other_optional_conditionals are only
  * optimized out if \a debug_level() is placed before it with a
  * logical and (short-circuiting).
  */
template<DebugLevel L = DEFAULT_DEBUG_LEVEL>
class Debuggable
{
  protected:
	Debuggable(std::ostream & debug_stream = std::cerr)
		: stream (debug_stream) {}

	bool debug_level (DebugLevel level) {
#ifndef NDEBUG
		(void) level; /* stop pedantic gcc complaints about unused parameter */
		return false;
#else
		return L >= level;
#endif
	}
	std::ostream & debug_stream() { return stream; }

  private:
	  std::ostream & stream;
};


} // namespace

#endif // AUDIOGRAPHER_DEBUGGABLE_H
