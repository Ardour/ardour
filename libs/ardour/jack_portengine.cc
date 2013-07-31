#define GET_PRIVATE_JACK_POINTER(localvar)  jack_client_t* localvar = _jack_connection->jack(); if (!(localvar)) { return; }
#define GET_PRIVATE_JACK_POINTER_RET(localvar,r) jack_client_t* localvar = _jack_connection->jack(); if (!(localvar)) { return r; }

static uint32_t
ardour_port_flags_to_jack_flags (PortFlags flags)
{
	uint32_t jack_flags = 0;
	
	if (flags & PortIsInput) {
		jack_flags |= JackPortIsInput;
	}
	if (flags & IsInput) {
		jack_flags |= JackPortIsOutput;
	}
	if (flags & IsOutput) {
		jack_flags |= JackPortIsTerminal;
	}
	if (flags & IsPhysical) {
		jack_flags |= JackPortIsPhysical;
	}
	if (flags & CanMonitor) {
		jack_flags |= JackPortCanMonitor;
	}

	return jack_flags;
}

static DataType
jack_port_type_to_ardour_data_type (const char* jack_type)
{
	if (strcmp (jack_type, JACK_DEFAULT_AUDIO_TYPE) == 0) {
		return DataType::AUDIO;
	} else if (strcmp (jack_type, JACK_DEFAULT_MIDI_TYPE) == 0) {
		return DataType::MIDI;
	}
	return DataType::NIL;
}

static const char*
ardour_data_type_to_jack_port_type (DataType d)
{
	switch (d) {
	case DataType::AUDIO:
		return JACK_DEFAULT_AUDIO_TYPE;
	case DataType::MIDI:
		return JACK_DEFAULT_MIDI_TYPE;
	}

	return "";
}

JACKPortEngine::JACKPortEngine (PortManager& pm, boost::shared_ptr<JackConnection> jc)
	: PortEngine (pm)
	, _jack_connection (jc)
{
	jack_client_t* client = _jack_connection->

        jack_set_port_registration_callback (_priv_jack, _registration_callback, this);
        jack_set_port_connect_callback (_priv_jack, _connect_callback, this);
        jack_set_graph_order_callback (_priv_jack, _graph_order_callback, this);
}

void
JACKPortEngine::_registration_callback (jack_port_id_t /*id*/, int /*reg*/, void* arg)
{
	static_cast<JACKPortEngine*> (arg)->_manager->registration_callback ();
}

void
JACKPortEngine::_connect_callback (jack_port_id_t id_a, jack_port_id_t id_b, int conn, void* arg)
{
	static_cast<JACKPortEngine*> (arg)->connect_callback (id_a, id_b, conn);
}

void
JACKPortEngine::connect_callback (jack_port_id_t id_a, jack_port_id_t id_b, int conn)
{
	if (_manager->port_remove_in_progress()) {
		return;
	}

	GET_PRIVATE_JACK_POINTER (_priv_jack);

	jack_port_t* a = jack_port_by_id (_priv_jack, id_a);
	jack_port_t* b = jack_port_by_id (_priv_jack, id_b);

	_manager->connect_callback (jack_port_name (a), jack_port_name (b), conn == 0 ? false : true);
}

int
JACKPortEngine::_graph_order_callback (void *arg)
{
	return static_cast<JACKPortEngine*> (arg)->graph_order_callback ();
}

int
JACKPortEngine::graph_order_callback ()
{
	if (_jack_connection->connected()) {
		_manager->graph_order_callback ();
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

DataType
JACKPortEngine::port_data_type (PortHandle p)
{
	return jack_port_type_to_ardour_data_type (jack_port_type (p));
}

const string&
JACKPortEngine::my_name() const
{
	return _client_name;
}

bool
JACKPortEngine::port_is_physical (PortHandle* ph) const
{
	if (!ph) {
                return false;
        }

        return jack_port_flags (ph) & JackPortIsPhysical;
}

int
JACKPortEngine::get_ports (const string& port_name_pattern, DataType type, PortFlags flags, vector<string>& s)
{

	GET_PRIVATE_JACK_POINTER_RET (_priv_jack,0);

	const char** ports =  jack_get_ports (_priv_jack, port_name_pattern.c_str(), 
					      ardour_data_type_to_jack_port_type (type), 
					      ardour_port_flags_to_jack_flags (flags));

	if (ports == 0) {
		return s;
	}

	for (uint32_t i = 0; ports[i]; ++i) {
		s.push_back (ports[i]);
		jack_free (ports[i]);
	}

	jack_free (ports);
	
	return s.size();
}

ChanCount
JACKPortEngine::n_physical (unsigned long flags) const
{
	ChanCount c;

	GET_PRIVATE_JACK_POINTER_RET (_jack, c);

	const char ** ports = jack_get_ports (_priv_jack, NULL, NULL, JackPortIsPhysical | flags);

	if (ports) {
		for (uint32_t i = 0; ports[i]; ++i) {
			if (!strstr (ports[i], "Midi-Through")) {
				DataType t (jack_port_type (jack_port_by_name (_jack, ports[i])));
				c.set (t, c.get (t) + 1);
				jack_free (ports[i]);
			}
		}
		
		jack_free (ports);
	}

	return c;
}

ChanCount
JACKPortEngine::n_physical_inputs () const
{
	return n_physical (JackPortIsInput);
}

ChanCount
JACKPortEngine::n_physical_outputs () const
{
	return n_physical (JackPortIsOutput);
}

void
JACKPortEngine::get_physical (DataType type, unsigned long flags, vector<string>& phy)
{
	GET_PRIVATE_JACK_POINTER (_priv_jack);
	const char ** ports;

	if ((ports = jack_get_ports (_priv_jack, NULL, type.to_jack_type(), JackPortIsPhysical | flags)) == 0) {
		return;
	}

	if (ports) {
		for (uint32_t i = 0; ports[i]; ++i) {
                        if (strstr (ports[i], "Midi-Through")) {
                                continue;
                        }
			phy.push_back (ports[i]);
			jack_free (ports[i]);
		}
		jack_free (ports);
	}
}

/** Get physical ports for which JackPortIsOutput is set; ie those that correspond to
 *  a physical input connector.
 */
void
JACKPortEngine::get_physical_inputs (DataType type, vector<string>& ins)
{
	get_physical (type, JackPortIsOutput, ins);
}

/** Get physical ports for which JackPortIsInput is set; ie those that correspond to
 *  a physical output connector.
 */
void
JACKPortEngine::get_physical_outputs (DataType type, vector<string>& outs)
{
	get_physical (type, JackPortIsInput, outs);
}


bool
JACKPortEngine::can_request_hardware_monitoring ()
{
	GET_PRIVATE_JACK_POINTER_RET (_priv_jack,false);
	const char ** ports;

	if ((ports = jack_get_ports (_priv_jack, NULL, JACK_DEFAULT_AUDIO_TYPE, JackPortCanMonitor)) == 0) {
		return false;
	}

	for (uint32_t i = 0; ports[i]; ++i) {
		jack_free (ports[i]);
	}

	jack_free (ports);

	return true;
}

framecnt_t
JACKPortEngine::last_frame_time () const
{
	GET_PRIVATE_JACK_POINTER_RET (_priv_jack, 0);
	return jack_last_frame_time (_priv_jack);
}
