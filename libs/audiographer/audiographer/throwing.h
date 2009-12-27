#ifndef AUDIOGRAPHER_THROWING_H
#define AUDIOGRAPHER_THROWING_H

#ifndef DEFAULT_THROW_LEVEL
#define DEFAULT_THROW_LEVEL ThrowStrict
#endif

namespace AudioGrapher
{

enum ThrowLevel
{
	ThrowNone,     //< Not allowed to throw
	ThrowObject,   //< Object level stuff, ctors, initalizers etc.
	ThrowProcess,  //< Process cycle level stuff
	ThrowStrict,   //< Stricter checks than ThrowProcess, less than ThrowSample
	ThrowSample    //< Sample level stuff
};

/// Class that allows optimizing out error checking during compile time
template<ThrowLevel L = DEFAULT_THROW_LEVEL>
class Throwing
{
  protected:
	Throwing() {}
	bool throw_level (ThrowLevel level) { return L >= level; }
};


} // namespace

#endif // AUDIOGRAPHER_THROWING_H
