#include <glibmm/thread.h>
#include <iostream>

class PBDMutex : public Glib::Mutex 
{
  public:
	PBDMutex() : Glib::Mutex() {}
	~PBDMutex() {
		if (trylock()) {
			unlock ();
		} else {
			std::cerr << "Mutex @ " << this << " locked during destructor\n";
			unlock ();
		}
	}
};
