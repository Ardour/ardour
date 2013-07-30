JACKPortEngine::init ()
{
        jack_set_port_registration_callback (_priv_jack, _registration_callback, this);
        jack_set_port_connect_callback (_priv_jack, _connect_callback, this);
        jack_set_graph_order_callback (_priv_jack, _graph_order_callback, this);
}

void
JACKPortEngine::_registration_callback (jack_port_id_t /*id*/, int /*reg*/, void* arg)
{
	JACKPortEngine* pm = static_cast<JACKAudioBackend*> (arg);

	if (!pm->port_remove_in_progress) {
		pm->engine.PortRegisteredOrUnregistered (); /* EMIT SIGNAL */
	}
}

void
JACKPortEngine::_connect_callback (jack_port_id_t id_a, jack_port_id_t id_b, int conn, void* arg)
{
	JACKPortEngine* pm = static_cast<JACKAudioBackend*> (arg);
	pm->connect_callback (id_a, id_b, conn);
}

void
JACKPortEngine::connect_callback (jack_port_id_t id_a, jack_port_id_t id_b, int conn)
{
	if (port_remove_in_progress) {
		return;
	}

	GET_PRIVATE_JACK_POINTER (_priv_jack);

	jack_port_t* jack_port_a = jack_port_by_id (_priv_jack, id_a);
	jack_port_t* jack_port_b = jack_port_by_id (_priv_jack, id_b);

	boost::shared_ptr<Port> port_a;
	boost::shared_ptr<Port> port_b;
	Ports::iterator x;
	boost::shared_ptr<Ports> pr = ports.reader ();

	x = pr->find (make_port_name_relative (jack_port_name (jack_port_a)));
	if (x != pr->end()) {
		port_a = x->second;
	}

	x = pr->find (make_port_name_relative (jack_port_name (jack_port_b)));
	if (x != pr->end()) {
		port_b = x->second;
	}

	PortConnectedOrDisconnected (
		port_a, jack_port_name (jack_port_a),
		port_b, jack_port_name (jack_port_b),
		conn == 0 ? false : true
		); /* EMIT SIGNAL */
}

int
JACKPortEngine::_graph_order_callback (void *arg)
{
	JACKPortEngine* pm = static_cast<JACKAudioBackend*> (arg);

	if (pm->connected() && !pm->port_remove_in_progress) {
		pm->engine.GraphReordered (); /* EMIT SIGNAL */
	}
	
	return 0;
}


JACKPortEngine::physically_connected (PortHandle p)
{
	jack_port_t* _jack_port = (jack_port_t*) p;

	const char** jc = jack_port_get_connections (_jack_port);

	if (jc) {
		for (int i = 0; jc[i]; ++i) {

			jack_port_t* port = jack_port_by_name (_engine->jack(), jc[i]);

			if (port && (jack_port_flags (port) & JackPortIsPhysical)) {
                                if (jack_free) {
                                        jack_free (jc);
                                } else {
                                        free (jc);
                                }
				return true;
			}
		}
                if (jack_free) {
                        jack_free (jc);
                } else {
                        free (jc);
                }
	}

	return false;
}
