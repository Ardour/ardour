#ifndef __libardour_jack_connection_h__
#define __libardour_jack_connection_h__

#include <string>
#include "weak_libjack.h"

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

    PBD::Signal<void()> Connected;
    PBD::Signal<void(const char*)> Disconnected;

    void halted_callback ();
    void halted_info_callback (jack_status_t, const char*);

    static bool in_control() { return _in_control; }

    uint32_t probed_buffer_size () const { assert (!connected ()); return _probed_buffer_size; }
    uint32_t probed_sample_rate () const { assert (!connected ()); return _probed_sample_rate; }

  private:
    jack_client_t* volatile _jack;
    std::string _client_name;
    std::string session_uuid;
    static bool _in_control;
    uint32_t _probed_buffer_size; // when not in control
    uint32_t _probed_sample_rate; // when not in control
};

} // namespace

#endif /* __libardour_jack_connection_h__ */
