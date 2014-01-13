#ifndef AUDIOGRAPHER_THROWING_H
#define AUDIOGRAPHER_THROWING_H

#ifndef DEFAULT_THROW_LEVEL
#define DEFAULT_THROW_LEVEL ThrowStrict
#endif

#include "audiographer/visibility.h"

namespace AudioGrapher
{

/** Compile time defined throw level.
  * Throw levels less than ThrowStrict should be used with caution.
  * Not throwing could mean getting a segfault.
  * However, if you want ultra-optimized code and/or don't care about handling
  * error situations, feel free to use whatever you want.
  */
enum /*LIBAUDIOGRAPHER_API*/ ThrowLevel
{
	ThrowNone,     ///< Not allowed to throw
	ThrowObject,   ///< Object level stuff, ctors, initalizers etc.
	ThrowProcess,  ///< Process cycle level stuff
	ThrowStrict,   ///< Stricter checks than ThrowProcess, less than ThrowSample
	ThrowSample    ///< Sample level stuff
};

/** Class that allows optimizing out error checking during compile time.
  * Usage: to take all advantage of this class you should wrap all 
  * throwing statemets like this:
  * \code
  * if (throw_level (SomeThrowLevel) && other_optional_conditionals) {
  * 	throw Exception (...);
  * }
  * \endcode
  *
  * The order of the conditionals in the if-clause is important.
  * The checks specified in \a other_optional_conditionals are only
  * optimized out if \a throw_level() is placed before it with a
  * logical and (short-circuiting).
  */
template<ThrowLevel L = DEFAULT_THROW_LEVEL>
class /*LIBAUDIOGRAPHER_API*/ Throwing
{
  protected:
	Throwing() {}
	bool throw_level (ThrowLevel level) { return L >= level; }
};


} // namespace

#endif // AUDIOGRAPHER_THROWING_H
