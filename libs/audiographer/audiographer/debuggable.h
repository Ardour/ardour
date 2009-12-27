#ifndef AUDIOGRAPHER_DEBUGGABLE_H
#define AUDIOGRAPHER_DEBUGGABLE_H

#ifndef DEFAULT_DEBUG_LEVEL
#define DEFAULT_DEBUG_LEVEL DebugNone
#endif

#include <iostream>

namespace AudioGrapher
{

enum DebugLevel
{
	DebugNone,     //< Disabled
	DebugObject,   //< Object level stuff, ctors, initalizers etc.
	DebugProcess,  //< Process cycle level stuff
	DebugVerbose,  //< Lots of output, not on sample level
	DebugSample    //< Sample level stuff
};

/// Class that allows optimizing out debugging code during compile time
template<DebugLevel L = DEFAULT_DEBUG_LEVEL>
class Debuggable
{
  protected:
	Debuggable(std::ostream & debug_stream = std::cerr)
		: stream (debug_stream) {}

	bool debug_level (DebugLevel level) {
		#ifdef NDEBUG
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
