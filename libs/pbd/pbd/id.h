#ifndef __pbd_id_h__
#define __pbd_id_h__

#include <uuid/uuid.h>
#include <string>

namespace PBD {

class ID {
  public:
	ID ();
	ID (std::string);
	
	bool operator== (const ID& other) const;
	bool operator!= (const ID& other) const {
		return !operator== (other);
	}
	ID& operator= (std::string); 

	bool operator< (const ID& other) const {
		return memcmp (id, other.id, sizeof (id)) < 0;
	}

	void print (char* buf) const;
	
  private:
	uuid_t id;
	int string_assign (std::string);
};

}
std::ostream& operator<< (std::ostream& ostr, const PBD::ID&);

#endif /* __pbd_id_h__ */
