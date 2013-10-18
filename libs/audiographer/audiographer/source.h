#ifndef AUDIOGRAPHER_SOURCE_H
#define AUDIOGRAPHER_SOURCE_H

#include "types.h"
#include "sink.h"

#include <boost/shared_ptr.hpp>

#include "audiographer/visibility.h"

namespace AudioGrapher
{

/** A source for data
  * This is a pure virtual interface for all data sources in AudioGrapher
  */
template<typename T>
class LIBAUDIOGRAPHER_API Source
{
  public:
	virtual ~Source () { }
	
	typedef boost::shared_ptr<Sink<T> > SinkPtr;
	
	/// Adds an output to this source. All data generated is forwarded to \a output
	virtual void add_output (SinkPtr output) = 0;
	
	/// Removes all outputs added
	virtual void clear_outputs () = 0;
	
	/// Removes a specific output from this source
	virtual void remove_output (SinkPtr output) = 0;
};

} // namespace

#endif //AUDIOGRAPHER_SOURCE_H

