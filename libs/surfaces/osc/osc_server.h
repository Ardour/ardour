#ifndef ardour_osc_control_protocol_h
#define ardour_osc_control_protocol_h

#include <string>

#include <sys/time.h>
#include <pthread.h>

#include <lo/lo.h>

#include <pbd/lockmonitor.h>
#include <ardour/control_protocol.h>
#include <ardour/types.h>

#include <pbd/abstract_ui.h>

struct OSCRequest : public BaseUI::BaseRequestObject {
    /* nothing yet */
};

class ControlOSC : public ARDOUR::ControlProtocol, public AbstractUI<OSCRequest> 
{
  public:
	ControlOSC (ARDOUR::Session&, uint32_t port);
	virtual ~ControlOSC();

	int set_active (bool yn);

	bool caller_is_ui_thread();

  private:
	uint32_t _port;
	volatile bool _ok;
	volatile bool _shutdown;
	lo_server _osc_server;
	lo_server _osc_unix_server;
	std::string _osc_unix_socket_path;
	pthread_t _osc_thread;
	int _request_pipe[2];

	static void * _osc_receiver(void * arg);
	void osc_receiver();

	bool init_osc_thread ();
	void terminate_osc_thread ();
	void poke_osc_thread ();

	void register_callbacks ();

	void on_session_load ();
	void on_session_unload ();

	std::string get_server_url ();
	std::string get_unix_server_url ();

	void do_request (OSCRequest* req);

#define PATH_CALLBACK(name) \
        static int _ ## name (const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data) { \
		return static_cast<ControlOSC*>(user_data)->cb_ ## name (path, types, argv, argc, data); \
        } \
        int cb_ ## name (const char *path, const char *types, lo_arg **argv, int argc, void *data) { \
		name (); \
		return 0; \
	}


	PATH_CALLBACK(add_marker);
	PATH_CALLBACK(loop_toggle);
	PATH_CALLBACK(goto_start);
	PATH_CALLBACK(goto_end);
	PATH_CALLBACK(rewind);
	PATH_CALLBACK(ffwd);
	PATH_CALLBACK(transport_stop);
	PATH_CALLBACK(transport_play);
	PATH_CALLBACK(save_state);
	PATH_CALLBACK(prev_marker);
	PATH_CALLBACK(next_marker);
	PATH_CALLBACK(undo);
	PATH_CALLBACK(redo);
	PATH_CALLBACK(toggle_punch_in);
	PATH_CALLBACK(toggle_punch_out);
	PATH_CALLBACK(rec_enable_toggle);
	PATH_CALLBACK(toggle_all_rec_enables);

#define PATH_CALLBACK1(name,type) \
        static int _ ## name (const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data) { \
                cerr << "callback from OSC\n"; \
		return static_cast<ControlOSC*>(user_data)->cb_ ## name (path, types, argv, argc, data); \
        } \
        int cb_ ## name (const char *path, const char *types, lo_arg **argv, int argc, void *data) { \
                if (argc > 0) { \
			name (argv[0]->type); \
                }\
		return 0; \
	}


	PATH_CALLBACK1(set_transport_speed,f);
};


#endif // ardour_osc_control_protocol_h
