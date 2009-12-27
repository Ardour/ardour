#ifndef AUDIOGRAPHER_EXCEPTION_H
#define AUDIOGRAPHER_EXCEPTION_H

#include <exception>
#include <string>
#include <cxxabi.h>

#include <boost/format.hpp>

namespace AudioGrapher
{

class Exception : public std::exception
{
  public:
	template<typename T>
	Exception (T const & thrower, std::string const & reason)
	  : reason (boost::str (boost::format (
			"Exception thrown by %1%: %2%") % name (thrower) % reason))
	{}

	virtual ~Exception () throw() { }

	const char* what() const throw()
	{
		return reason.c_str();
	}
	
  protected:
	template<typename T>
	std::string name (T const & obj)
	{
#ifdef __GNUC__
		int status;
		char * res = abi::__cxa_demangle (typeid(obj).name(), 0, 0, &status);
		if (status == 0) {
			std::string s(res);
			free (res);
			return s;
		}
#endif
		return typeid(obj).name();
	}

  private:
	std::string const reason;

};

} // namespace AudioGrapher

#endif // AUDIOGRAPHER_EXCEPTION_H