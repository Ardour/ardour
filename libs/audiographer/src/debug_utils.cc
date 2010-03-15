#include "audiographer/debug_utils.h"

#include "audiographer/process_context.h"

#include <sstream>

namespace AudioGrapher {

std::string
DebugUtils::process_context_flag_name (FlagField::Flag flag)
{
	std::ostringstream ret;
	
	switch (flag) {
		case ProcessContext<>::EndOfInput:
			ret << "EndOfInput";
			break;
		default:
			ret << flag;
			break;
	}
	
	return ret.str();
}

} // namespace