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
