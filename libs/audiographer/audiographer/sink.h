#ifndef AUDIOGRAPHER_SINK_H
#define AUDIOGRAPHER_SINK_H

#include <boost/shared_ptr.hpp>

#include "process_context.h"

namespace AudioGrapher
{

template <typename T>
class Sink  {
  public:
	virtual ~Sink () {}
	
	/** Process given data.
	  * The data can not be modified, so in-place processing is not allowed.
	  * At least this function must be implemented by deriving classes
	  */
	virtual void process (ProcessContext<T> const & context) = 0;
	
	/** Process given data
	  * Data may be modified, so in place processing is allowed.
	  * The default implementation calls the non-modifying version,
	  * so this function does not need to be overriden.
	  * However, if the sink can do in-place processing,
	  * overriding this is highly recommended.
	  *
	  * If this is not overridden adding "using Sink<T>::process;"
	  * to the deriving class declaration is suggested to avoid
	  * warnings about hidden virtual functions.
	  */
	inline virtual void process (ProcessContext<T> & context)
	{
		this->process (static_cast<ProcessContext<T> const &> (context));
	}
};

} // namespace

#endif // AUDIOGRAPHER_SINK_H

