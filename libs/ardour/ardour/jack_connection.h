#ifndef __libardour_jack_connection_h__
#define __libardour_jack_connection_h__

#include <string>
#include <jack/jack.h>

#include "pbd/signals.h"

namespace ARDOUR {

class JackConnection {
  public:
    JackConnection (const std::string& client_name, const std::string& session_uuid);
    ~JackConnection ();

    const std::string& client_name() const { return _client_name; }

    int open ();
    int close ();
    bool connected () const { return _jack != 0; }

    jack_client_t* jack() const { return _jack; }

    PBD::Signal1<void,const char*> Disconnected;

    void halted_callback ();
    void halted_info_callback (jack_status_t, const char*);

  private:
    jack_client_t* volatile _jack;
    std::string _client_name;
    std::string session_uuid;
};

} // namespace 

#endif /* __libardour_jack_connection_h__ */
