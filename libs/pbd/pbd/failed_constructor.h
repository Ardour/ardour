#ifndef __pbd_failed_constructor_h__
#define __pbd_failed_constructor_h__

#include <exception>

class failed_constructor : public std::exception {
  public:
	virtual const char *what() const throw() { return "failed constructor"; }
};

#endif /* __pbd_failed_constructor_h__ */
