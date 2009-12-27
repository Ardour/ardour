#ifndef AUDIOGRAPHER_TYPES_H
#define AUDIOGRAPHER_TYPES_H

#include <stdint.h>

namespace AudioGrapher {

typedef int64_t nframes_t;
typedef uint8_t ChannelCount;

/** Flag field capable of holding 32 flags.
    Easily grown in size to 64 flags by changing storage_type */
class FlagField {
  public:
	typedef uint8_t  Flag;
	typedef uint32_t storage_type;
	
	FlagField() : _flags (0) {}
	FlagField(FlagField const & other) : _flags (other._flags) {}
	
	inline bool has (Flag flag)    const { return _flags & (1 << flag); }
	inline storage_type flags ()   const { return _flags; }
	inline operator bool()         const { return _flags; }
	inline void set (Flag flag)          { _flags |= (1 << flag); }
	inline void remove (Flag flag)       { _flags &= ~(1 << flag); }
	inline void reset ()                 { _flags = 0; }
	
	inline FlagField & operator+= (FlagField const & other) { _flags |= other._flags; return *this; }
	inline bool operator== (FlagField const & other) const { return _flags == other._flags; }

  private:
	storage_type _flags;
};

} // namespace

#endif // __audiographer_types_h__