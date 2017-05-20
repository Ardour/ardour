#include <glibmm.h>

#include "osc.h"
#include "ws_osc.h"

using namespace std;
using namespace ArdourSurface;
using namespace Glib;

// 0 for unlimited
#define MAX_BUFFER_SIZE 0

static int callback_main(   struct lws *wsi,
                            enum lws_callback_reasons reason,
                            void *user,
                            void *in,
                            size_t len )
{
#if 0
	int fd;
	unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + 512 + LWS_SEND_BUFFER_POST_PADDING];
	unsigned char *p = &buf[LWS_SEND_BUFFER_PRE_PADDING];

	switch( reason ) {
	case LWS_CALLBACK_ESTABLISHED:
		self->onConnectWrapper( lws_get_socket_fd( wsi ) );
		lws_callback_on_writable( wsi );
		break;

	case LWS_CALLBACK_SERVER_WRITEABLE:
		fd = lws_get_socket_fd( wsi );
		while( !self->connections[fd]->buffer.empty( ) )
		{
			string message = self->connections[fd]->buffer.front( );
			int charsSent = lws_write( wsi, (unsigned char*)message.c_str( ), message.length( ), LWS_WRITE_TEXT );
			if( charsSent < message.length( ) )
				self->onErrorWrapper( fd, string( "Error writing to socket" ) );
			else
				// Only pop the message if it was sent successfully.
				self->connections[fd]->buffer.pop_front( );
		}
		lws_callback_on_writable( wsi );
		break;

	case LWS_CALLBACK_RECEIVE:
		self->onMessage( lws_get_socket_fd( wsi ), string( (const char *)in ) );
		break;

	case LWS_CALLBACK_CLOSED:
		self->onDisconnectWrapper( lws_get_socket_fd( wsi ) );
		break;

	default:
		break;
	}
#endif
	return 0;
}

static struct lws_protocols protocols[] = {
	{
		"/",
		callback_main,
		0, // user data struct not used
		MAX_BUFFER_SIZE,
	},{ NULL, NULL, 0, 0 } // terminator
};

WSOSCServer::WSOSCServer (OSC* o, int32_t p, string const & cpath, string const & kpath)
	: osc (o)
	, port (p)
	, cert_path (cpath)
	, key_path (kpath)
	, context (0)
{
	lws_set_log_level( 0, lwsl_emit_syslog ); // We'll do our own logging, thank you.

	struct lws_context_creation_info info;

	memset( &info, 0, sizeof info );
	info.port = port;
	info.iface = NULL;
	info.protocols = protocols;

	if (cert_path.empty( ) && !key_path.empty( ) ) {
		info.ssl_cert_filepath        = cert_path.c_str( );
		info.ssl_private_key_filepath = key_path.c_str( );
	} else {
		info.ssl_cert_filepath        = 0;
		info.ssl_private_key_filepath = 0;
	}
	info.gid = -1;
	info.uid = -1;
	info.options = 0;

	// keep alive
	info.ka_time = 60; // 60 seconds until connection is suspicious
	info.ka_probes = 10; // 10 probes after ^ time
	info.ka_interval = 10; // 10s interval for sending probes
	context = lws_create_context( &info );

	if (!context) {
	}
}

WSOSCServer::~WSOSCServer ()
{
}

bool
WSOSCServer::event_handler (IOCondition, int)
{
	return true;
}

void
WSOSCServer::add_fd (int fd)
{
	Glib::RefPtr<IOSource> src = IOSource::create (fd, IO_IN|IO_OUT|IO_HUP|IO_ERR);
	src->connect (sigc::bind (sigc::mem_fun (*this, &WSOSCServer::event_handler), fd));
	osc->attach (src);
	GSource* s = src->gobj();
	g_source_ref (s);
	sources.push_back (src);
}
