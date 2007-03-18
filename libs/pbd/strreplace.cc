#include <pbd/replace_all.h>

int
replace_all (std::string& str,
	     std::string const& target,
	     std::string const& replacement) 
{
	std::string::size_type start = str.find (target, 0);
	int cnt = 0;

	while (start != std::string::npos) {
		str.replace (start, target.size(), replacement);
		start = str.find (target, start+replacement.size());
		++cnt;
	}

	return cnt;
}

