#ifndef AUDIOGRAPHER_IDENTITY_VERTEX_H
#define AUDIOGRAPHER_IDENTITY_VERTEX_H

#include "audiographer/visibility.h"
#include "audiographer/types.h"
#include "audiographer/utils/listed_source.h"
#include "audiographer/sink.h"

namespace AudioGrapher
{

/// Outputs its input directly to a number of Sinks
template<typename T = DefaultSampleType>
class LIBAUDIOGRAPHER_API IdentityVertex : public ListedSource<T>, Sink<T>
{
  public:
	void process (ProcessContext<T> const & c) { ListedSource<T>::output(c); }
	void process (ProcessContext<T> & c) { ListedSource<T>::output(c); }
};


} // namespace

#endif // AUDIOGRAPHER_IDENTITY_VERTEX_H
