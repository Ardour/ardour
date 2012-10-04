/*
  Copyright (C) 2004 Paul Davis 

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

  $Id$
*/

#include <fcntl.h>
#include <cerrno>

#include <pbd/failed_constructor.h>
#include <pbd/error.h>
#include <pbd/xml++.h>

#include <midi++/types.h>
#include <midi++/alsa_sequencer.h>
#include <midi++/manager.h>

#include "i18n.h"

//#define DOTRACE 1

#ifdef DOTRACE
#define TR_FN() (cerr << __FUNCTION__ << endl)
#define TR_VAL(v) (cerr << __FILE__  " " << __LINE__ << " " #v  "=" << v << endl)
#else
#define TR_FN()
#define TR_VAL(v)
#endif

using namespace std;
using namespace MIDI;
using namespace PBD;

snd_seq_t* ALSA_SequencerMidiPort::seq = 0;
ALSA_SequencerMidiPort::AllPorts ALSA_SequencerMidiPort::_all_ports;
bool ALSA_SequencerMidiPort::_read_done = false;
bool ALSA_SequencerMidiPort::_read_signal_connected = false;

ALSA_SequencerMidiPort::ALSA_SequencerMidiPort (const XMLNode& node)
	: Port (node)
	, decoder (0) 
	, encoder (0) 
	, port_id (-1)
{
	TR_FN();
	int err;
	Descriptor desc (node);

	if (!seq && init_client (desc.device) < 0) {
		_ok = false; 

	} else {
		
		if (0 <= (err = create_ports (desc)) &&
		    0 <= (err = snd_midi_event_new (1024, &decoder)) &&	// Length taken from ARDOUR::Session::midi_read ()
		    0 <= (err = snd_midi_event_new (64, &encoder))) {	// Length taken from ARDOUR::Session::mmc_buffer
			snd_midi_event_init (decoder);
			snd_midi_event_init (encoder);
			_ok = true;

                        if (!_read_signal_connected) {
                                /* we need to do some magic just before a read is initiated
                                 */
                                Manager::PreRead.connect (sigc::ptr_fun (ALSA_SequencerMidiPort::prepare_read));
                                _read_signal_connected = true;
                        }
		} 
	}

	set_state (node);
}

ALSA_SequencerMidiPort::~ALSA_SequencerMidiPort ()
{
        _all_ports.erase (port_id);

	if (decoder) {
		snd_midi_event_free (decoder);
	}
	if (encoder) {
		snd_midi_event_free (encoder);
	}
	if (port_id >= 0) {
		snd_seq_delete_port (seq, port_id);
	}
}

int 
ALSA_SequencerMidiPort::selectable () const
{
	struct pollfd pfd[1];
	if (0 <= snd_seq_poll_descriptors (seq, pfd, 1, POLLIN | POLLOUT)) {
		return pfd[0].fd;
	}
	return -1;
}

int 
ALSA_SequencerMidiPort::write (byte *msg, size_t msglen)	
{
	TR_FN ();
	int R;
	int totwritten = 0;
	snd_midi_event_reset_encode (encoder);
	int nwritten = snd_midi_event_encode (encoder, msg, msglen, &SEv);
	TR_VAL (nwritten);
	while (0 < nwritten) {
		if (0 <= (R = snd_seq_event_output (seq, &SEv))  &&
		    0 <= (R = snd_seq_drain_output (seq))) {
			bytes_written += nwritten;
			totwritten += nwritten;
			if (output_parser) {
				output_parser->raw_preparse (*output_parser, msg, nwritten);
				for (int i = 0; i < nwritten; i++) {
					output_parser->scanner (msg[i]);
				}
				output_parser->raw_postparse (*output_parser, msg, nwritten);
			}
		} else {
			TR_VAL(R);
			return R;
		}

		msglen -= nwritten;
		msg += nwritten;
		if (msglen > 0) {
			nwritten = snd_midi_event_encode (encoder, msg, msglen, &SEv);
			TR_VAL(nwritten);
		}
		else {
			break;
		}
	}

	return totwritten;
}

void
ALSA_SequencerMidiPort::prepare_read ()
{
        _read_done = false;
}

int 
ALSA_SequencerMidiPort::read (byte *buf, size_t max)
{
        if (!_read_done) {
                read_all_ports (buf, max);
                _read_done = true;
        }
        return 0;
}

int 
ALSA_SequencerMidiPort::read_all_ports (byte *buf, size_t max)
{
	TR_FN();
	int err;
	snd_seq_event_t *ev;

	err = snd_seq_event_input (seq, &ev);

	if (err > 0) {

                AllPorts::iterator p = _all_ports.find (ev->dest.port);
                
                if (p == _all_ports.end()) {
                        /* no error reading but port does not exist */
                        return 0;
                }
                
                return p->second->read_self (buf, max, ev);
        } 

	return -ENOENT == err ? 0 : err;
}

int
ALSA_SequencerMidiPort::read_self (byte* buf, size_t max, snd_seq_event_t* ev)
{
        int evsize = snd_midi_event_decode (decoder, buf, max, ev);

        bytes_read += evsize;

        if (input_parser) {
                input_parser->raw_preparse (*input_parser, buf, evsize);

                for (int i = 0; i < evsize; i++) {
                        input_parser->scanner (buf[i]);
                }	
                input_parser->raw_postparse (*input_parser, buf, evsize);
        }

        return 0;
}

int 
ALSA_SequencerMidiPort::create_ports (const Port::Descriptor& desc)
{
	int err;
	unsigned int caps = 0;

	if (desc.mode == O_WRONLY  ||  desc.mode == O_RDWR)
		caps |= SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE;
	if (desc.mode == O_RDONLY  ||  desc.mode == O_RDWR)
		caps |= SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ;

	if (0 <= (err = snd_seq_create_simple_port (seq, desc.tag.c_str(), caps, 
						    (SND_SEQ_PORT_TYPE_MIDI_GENERIC|
						     SND_SEQ_PORT_TYPE_SOFTWARE|
						     SND_SEQ_PORT_TYPE_APPLICATION)))) {
		
		port_id = err;

		snd_seq_ev_clear (&SEv);
		snd_seq_ev_set_source (&SEv, port_id);
		snd_seq_ev_set_subs (&SEv);
		snd_seq_ev_set_direct (&SEv);
		
                _all_ports.insert (make_pair (port_id, this));

		return 0;
	}

	return err;
}

int
ALSA_SequencerMidiPort::init_client (std::string name)
{
	static bool called = false;

	if (called) {
		return -1;
	}

	called = true;

	if (snd_seq_open (&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) >= 0) {
		snd_seq_set_client_name (seq, name.c_str());
		return 0;
	} else {
		warning << "The ALSA MIDI system is not available. No ports based on it will be created"
			<< endmsg;
		return -1;
	}
}

int
ALSA_SequencerMidiPort::discover (vector<PortSet>& ports)
{
	 int n = 0;

	 snd_seq_client_info_t *client_info;
	 snd_seq_port_info_t   *port_info;

	 snd_seq_client_info_alloca (&client_info);
	 snd_seq_port_info_alloca (&port_info);
	 snd_seq_client_info_set_client (client_info, -1);

	 while (snd_seq_query_next_client(seq, client_info) >= 0) {

		 int alsa_client;

		 if ((alsa_client = snd_seq_client_info_get_client(client_info)) <= 0) {
			 break;
		 }

		 snd_seq_port_info_set_client(port_info, alsa_client);
		 snd_seq_port_info_set_port(port_info, -1);

		 char client[256];
		 snprintf (client, sizeof (client), "%d:%s", alsa_client, snd_seq_client_info_get_name(client_info));

		 ports.push_back (PortSet (client));

		 while (snd_seq_query_next_port(seq, port_info) >= 0) {

#if 0
			 int type = snd_seq_port_info_get_type(pinfo);
			 if (!(type & SND_SEQ_PORT_TYPE_PORT)) {
				 continue;
			 }
#endif

			 unsigned int port_capability = snd_seq_port_info_get_capability(port_info);

			 if ((port_capability & SND_SEQ_PORT_CAP_NO_EXPORT) == 0) {

				 int alsa_port = snd_seq_port_info_get_port(port_info);

				 char port[256];
				 snprintf (port, sizeof (port), "%d:%s", alsa_port, snd_seq_port_info_get_name(port_info));

				 std::string mode;

				 if (port_capability & SND_SEQ_PORT_CAP_READ) {
					 if (port_capability & SND_SEQ_PORT_CAP_WRITE) {
						 mode = "duplex";
					 } else {
						 mode = "output";
					 } 
				 } else if (port_capability & SND_SEQ_PORT_CAP_WRITE) {
					 if (port_capability & SND_SEQ_PORT_CAP_READ) {
						 mode = "duplex";
					 } else {
						 mode = "input";
					 } 
				 }

				 XMLNode node (X_("MIDI-port"));
				 node.add_property ("device", client);
				 node.add_property ("tag", port);
				 node.add_property ("mode", mode);
				 node.add_property ("type", "alsa/sequencer");
				 
				 ports.back().ports.push_back (node);
				 ++n;
			 }
		 }
	 }
	 
	 return n;
}

void
ALSA_SequencerMidiPort::get_connections (vector<SequencerPortAddress>& connections, int dir) const
{
	snd_seq_query_subscribe_t *subs;
	snd_seq_addr_t seq_addr;

	snd_seq_query_subscribe_alloca (&subs);

	// Get port connections...
	
	if (dir) {
		snd_seq_query_subscribe_set_type(subs, SND_SEQ_QUERY_SUBS_WRITE);
	} else {
		snd_seq_query_subscribe_set_type(subs, SND_SEQ_QUERY_SUBS_READ);
	}

	snd_seq_query_subscribe_set_index(subs, 0);
	seq_addr.client = snd_seq_client_id (seq);
	seq_addr.port   = port_id;
	snd_seq_query_subscribe_set_root(subs, &seq_addr);

	while (snd_seq_query_port_subscribers(seq, subs) >= 0) {

                seq_addr = *snd_seq_query_subscribe_get_addr (subs);
		
                connections.push_back (SequencerPortAddress (seq_addr.client,
								     seq_addr.port));

		snd_seq_query_subscribe_set_index (subs, snd_seq_query_subscribe_get_index(subs) + 1);
	}
}

XMLNode&
ALSA_SequencerMidiPort::get_state () const
{
	XMLNode& root (Port::get_state ());
	vector<SequencerPortAddress> connections;
	XMLNode* sub = 0;
	char buf[256];

	get_connections (connections, 1);

	if (!connections.empty()) {
		if (!sub) {
			sub = new XMLNode (X_("connections"));
		}
		for (vector<SequencerPortAddress>::iterator i = connections.begin(); i != connections.end(); ++i) {
			XMLNode* cnode = new XMLNode (X_("read"));
			snprintf (buf, sizeof (buf), "%d:%d", i->first, i->second);
			cnode->add_property ("dest", buf);
			sub->add_child_nocopy (*cnode);
		}
	}
	
	connections.clear ();
	get_connections (connections, 0);

	if (!connections.empty()) {
		if (!sub) {
			sub = new XMLNode (X_("connections"));
		}
		for (vector<SequencerPortAddress>::iterator i = connections.begin(); i != connections.end(); ++i) {
			XMLNode* cnode = new XMLNode (X_("write"));
			snprintf (buf, sizeof (buf), "%d:%d", i->first, i->second);
			cnode->add_property ("dest", buf);
			sub->add_child_nocopy (*cnode);
		}
	}

	if (sub) {
		root.add_child_nocopy (*sub);
	}

	return root;
}

void
ALSA_SequencerMidiPort::set_state (const XMLNode& node)
{
	Port::set_state (node);

	XMLNodeList children (node.children());
	XMLNodeIterator iter;

	for (iter = children.begin(); iter != children.end(); ++iter) {

		if ((*iter)->name() == X_("connections")) {

			XMLNodeList gchildren ((*iter)->children());
			XMLNodeIterator gciter;

			for (gciter = gchildren.begin(); gciter != gchildren.end(); ++gciter) {
				XMLProperty* prop;

				if ((prop = (*gciter)->property ("dest")) != 0) {
					int client;
					int port;

					if (sscanf (prop->value().c_str(), "%d:%d", &client, &port) == 2) {

						snd_seq_port_subscribe_t *sub;
						snd_seq_addr_t seq_addr;
						
						snd_seq_port_subscribe_alloca(&sub);

						if ((*gciter)->name() == X_("write")) {
							
							seq_addr.client = snd_seq_client_id (seq);
							seq_addr.port   = port_id;
							snd_seq_port_subscribe_set_sender(sub, &seq_addr);
							
							seq_addr.client = client;
							seq_addr.port   = port;
							snd_seq_port_subscribe_set_dest(sub, &seq_addr);

						} else {
							
							seq_addr.client = snd_seq_client_id (seq);
							seq_addr.port   = port_id;
							snd_seq_port_subscribe_set_dest(sub, &seq_addr);
							
							seq_addr.client = client;
							seq_addr.port   = port;
							snd_seq_port_subscribe_set_sender(sub, &seq_addr);
						}

						snd_seq_subscribe_port (seq, sub);
					}
				}
			}

			break;
		}
	}
}
