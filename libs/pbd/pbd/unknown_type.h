#ifndef __pbd_unknown_type_h__
#define __pbd_unknown_type_h__

#include <exception>

class unknown_type : public std::exception {
  public:
	virtual const char *what() const throw() { return "unknown type"; }
};

#endif /* __pbd_unknown_type_h__ */
