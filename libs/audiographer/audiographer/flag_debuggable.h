#ifndef AUDIOGRAPHER_FLAG_DEBUGGABLE_H
#define AUDIOGRAPHER_FLAG_DEBUGGABLE_H

#include "audiographer/visibility.h"
#include "debuggable.h"
#include "debug_utils.h"
#include "process_context.h"
#include "types.h"

#include <boost/format.hpp>

namespace AudioGrapher
{

/// A debugging class for nodes that support a certain set of flags.
template<DebugLevel L = DEFAULT_DEBUG_LEVEL>
class LIBAUDIOGRAPHER_API FlagDebuggable : public Debuggable<L>
{
  public:
	typedef FlagField::Flag Flag;

  protected:

	/// Adds a flag to the set of flags supported
	void add_supported_flag (Flag flag)
	{
		flags.set (flag);
	}
	
	/// Prints debug output if \a context contains flags that are not supported by this class
	template<typename SelfType, typename ContextType>
	void check_flags (SelfType & self, ProcessContext<ContextType> context)
	{
		if (!Debuggable<L>::debug_level (DebugFlags)) { return; }
		FlagField unsupported = flags.unsupported_flags_of (context.flags());
		
		for (FlagField::iterator it = unsupported.begin(); it != unsupported.end(); ++it) {
			Debuggable<L>::debug_stream() << boost::str (boost::format
				("%1% does not support flag %2%")
				% DebugUtils::demangled_name (self) % DebugUtils::process_context_flag_name (*it)
				) << std::endl;
		}
	}

  private:
	FlagField flags;
};


} // namespace

#endif // AUDIOGRAPHER_FLAG_DEBUGGABLE_H
