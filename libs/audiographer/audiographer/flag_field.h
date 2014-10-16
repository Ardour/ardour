#ifndef AUDIOGRAPHER_FLAG_FIELD_H
#define AUDIOGRAPHER_FLAG_FIELD_H

#include <stdint.h>
#include <iterator>
#include <climits>

#include <boost/operators.hpp>

#include "audiographer/visibility.h"

namespace AudioGrapher {

/** Flag field capable of holding 32 flags.
  * Easily grown in size to 64 flags by changing storage_type.
  */
class LIBAUDIOGRAPHER_API FlagField
  : public boost::less_than_comparable<FlagField>
  , boost::equivalent<FlagField>
  , boost::equality_comparable<FlagField>
{
  public:
	
	typedef uint8_t  Flag;
	typedef uint32_t storage_type;
	
	/// Bi-directional iterator for flag set. Iterates over flags that are set in this field.
	class iterator
	  : public std::iterator<std::bidirectional_iterator_tag, Flag>
	  , public boost::less_than_comparable<iterator>
	  , boost::equivalent<iterator>
	  , boost::equality_comparable<iterator>
	{
	  public:
		iterator (FlagField const & parent, Flag position) : parent (parent), position (position) {}
		iterator (iterator const & other) : parent (other.parent), position (other.position) {}
		
		value_type operator*() const { return position; }
		value_type const * operator->() const { return &position; }
		
		iterator & operator++()
		{
			do {
				++position;
			} while (!parent.has (position) && position != max());
			return *this;
		}
		iterator operator++(int) { iterator copy (*this); ++(*this); return copy; }
		
		iterator & operator--()
		{
			do {
				--position;
			} while (!parent.has (position) && position != max());
			return *this;
		}
		iterator   operator--(int) { iterator copy (*this); --(*this); return copy; }
		
		bool operator< (iterator const & other) const { return position < other.position; }
		
	  private:
		FlagField const & parent;
		Flag              position;
	};
	
  public:
	
	FlagField() : _flags (0) {}
	FlagField(FlagField const & other) : _flags (other._flags) {}
	
	inline bool has (Flag flag)    const { return _flags & (1 << flag); }
	inline storage_type flags ()   const { return _flags; }
	inline operator bool()         const { return _flags; }
	inline void set (Flag flag)          { _flags |= (1 << flag); }
	inline void remove (Flag flag)       { _flags &= ~(1 << flag); }
	inline void reset ()                 { _flags = 0; }

	/// Returns the flags in \a other that are not set in this field
	inline FlagField unsupported_flags_of (FlagField const & other) const { return ~(_flags | ~other._flags); }
	
	/// Set all flags that are set in \a other
	inline FlagField & operator+= (FlagField const & other) { _flags |= other._flags; return *this; }
	
	/** Checks whether this field has all the flags set that are set in \a other
	  * NOTE: Can NOT be used for strict weak ordering!
	  * \return \a true if \a other has flags set that this field does not
	  */
	inline bool operator< (FlagField const & other) const { return unsupported_flags_of (other); }

	iterator begin() const
	{
		iterator it (*this, 0);
		if (!*this) { return end(); }
		if (!has (0)) { ++it; }
		return it;
	}
	iterator end() const { iterator it (*this, max()); return it; }

  private:
	FlagField(storage_type flags) : _flags (flags) {}
	static Flag max() { return CHAR_BIT * sizeof (storage_type) + 1; }
	
	storage_type _flags;
};

} // namespace

#endif // AUDIOGRAPHER_FLAG_FIELD_H
