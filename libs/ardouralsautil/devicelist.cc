/*
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <alsa/asoundlib.h>
#include <glib.h>

#include "pbd/string_convert.h"
#include "ardouralsautil/devicelist.h"

using namespace std;

void
ARDOUR::get_alsa_audio_device_names (std::map<std::string, std::string>& devices, AlsaDuplex duplex)
{
	snd_ctl_t *handle;
	snd_ctl_card_info_t *info;
	snd_pcm_info_t *pcminfo;
	snd_ctl_card_info_alloca(&info);
	snd_pcm_info_alloca(&pcminfo);
	string devname;
	int cardnum = -1;
	int device = -1;
	const char* fixed_name;

	if ((fixed_name = g_getenv ("ARDOUR_ALSA_DEVICE"))) {
		devices.insert (make_pair<string,string> (fixed_name, fixed_name));
		return;
	}

	assert (duplex > 0);

	while (snd_card_next (&cardnum) >= 0 && cardnum >= 0) {

		devname = "hw:";
		devname += PBD::to_string (cardnum);

		if (snd_ctl_open (&handle, devname.c_str(), 0) >= 0 && snd_ctl_card_info (handle, info) >= 0) {

			if (snd_ctl_card_info (handle, info) < 0) {
				continue;
			}

			string card_name = snd_ctl_card_info_get_name (info);
			bool have_multiple_subdevices = false;

			while (snd_ctl_pcm_next_device (handle, &device) >= 0 && device >= 0) {

				/* only detect duplex devices here. more
				 * complex arrangements are beyond our scope
				 */

				snd_pcm_info_set_device (pcminfo, device);
				snd_pcm_info_set_subdevice (pcminfo, 0);
				snd_pcm_info_set_stream (pcminfo, SND_PCM_STREAM_CAPTURE);

				if (snd_ctl_pcm_info (handle, pcminfo) < 0 && (duplex & HalfDuplexIn)) {
					continue;
				}

				snd_pcm_info_set_device (pcminfo, device);
				snd_pcm_info_set_subdevice (pcminfo, 0);
				snd_pcm_info_set_stream (pcminfo, SND_PCM_STREAM_PLAYBACK);

				if (snd_ctl_pcm_info (handle, pcminfo) < 0 && (duplex & HalfDuplexOut)) {
					continue;
				}

				/* prefer hardware ID (not card/device number) */
				string hwname = "hw:";
				hwname += snd_ctl_card_info_get_id (info);
				hwname += ',';
				hwname += PBD::to_string (device);

				if (false /* list first subdevice only */) {
					devices.insert (std::make_pair (card_name, hwname));
					continue;
				}

				string uniq_name = card_name;

				if (have_multiple_subdevices) {
					uniq_name += " (" + hwname + ")";
				}

				std::pair<std::map<std::string, std::string>::iterator, bool> rv;
				rv = devices.insert (std::make_pair (uniq_name, hwname));

				if (!rv.second) {
					assert (!have_multiple_subdevices);
					have_multiple_subdevices = true;

					uniq_name += " (" + PBD::to_string (device) + ")";
					devices.insert (std::make_pair (uniq_name, hwname));
#if 0 // disabled (blame the_CLA's laptop)
					/* It may happen that the soundcard has multiple sub-devices for playback
					 * but none for recording.
					 *
					 * In that case the playback device-name has a suffix "(0)" while
					 * the capture device has none.
					 *
					 * This causes issues for backends that use
					 *  ::match_input_output_devices_or_none()
					 *
					 * (the alternative would be to always add a suffix,
					 * and the proper solution would be to compare the hw:name)
					 */
					/* remname the previous entry */
					hwname = devices[card_name];
					devices.erase (devices.find (card_name));
					size_t se = hwname.find_last_of (',');
					assert (se != string::npos);

					uniq_name = card_name + " (" + hwname.substr (se + 1) + ")";
					devices.insert (std::make_pair (uniq_name, hwname));
#endif
				}
			}

			snd_ctl_close(handle);
		}
	}
}

static void
insert_unique_device_name (std::map<std::string, std::string>& devices, std::string const& card_name, std::string const& devname, int caps)
{
	assert (caps != 0);
	std::pair<std::map<std::string, std::string>::iterator, bool> rv;
	char cnt = '2';
	std::string cn = card_name;
	/* Add numbers first this is be independent of physical ID (sequencer vs rawmidi).
	 * If this fails (>= 10 devices) add the device-name for uniqness
	 *
	 * XXX: Perhaps this is a bad idea, and `devname` should always be added if
	 * there is more than one device with the same name.
	 */
	do {
		cn += " (";
		if (caps & SND_SEQ_PORT_CAP_READ) cn += "I";
		if (caps & SND_SEQ_PORT_CAP_WRITE) cn += "O";
		cn += ")";
		rv = devices.insert (std::make_pair (cn, devname));
		cn = card_name + " [" + cnt + "]";
	} while (!rv.second && ++cnt <= '9');

	if (!rv.second) {
		cn = card_name + " [" + devname + "] (";
		if (caps & SND_SEQ_PORT_CAP_READ) cn += "I";
		if (caps & SND_SEQ_PORT_CAP_WRITE) cn += "O";
		cn += ")";
		rv = devices.insert (std::make_pair (cn, devname));
		assert (rv.second == true);
	}
}

void
ARDOUR::get_alsa_rawmidi_device_names (std::map<std::string, std::string>& devices)
{
	int cardnum = -1;
	snd_ctl_card_info_t *cinfo;
	snd_ctl_card_info_alloca (&cinfo);
	while (snd_card_next (&cardnum) >= 0 && cardnum >= 0) {
		snd_ctl_t *handle;
		std::string devname = "hw:";
		devname += PBD::to_string (cardnum);
		if (snd_ctl_open (&handle, devname.c_str (), 0) >= 0 && snd_ctl_card_info (handle, cinfo) >= 0) {
			int device = -1;
			while (snd_ctl_rawmidi_next_device (handle, &device) >= 0 && device >= 0) {
				snd_rawmidi_info_t *info;
				snd_rawmidi_info_alloca (&info);
				snd_rawmidi_info_set_device (info, device);

				int subs_in, subs_out;

				snd_rawmidi_info_set_stream (info, SND_RAWMIDI_STREAM_INPUT);
				if (snd_ctl_rawmidi_info (handle, info) >= 0) {
					subs_in = snd_rawmidi_info_get_subdevices_count (info);
				} else {
					subs_in = 0;
				}

				snd_rawmidi_info_set_stream (info, SND_RAWMIDI_STREAM_OUTPUT);
				if (snd_ctl_rawmidi_info (handle, info) >= 0) {
					subs_out = snd_rawmidi_info_get_subdevices_count (info);
				} else {
					subs_out = 0;
				}

				const int subs = subs_in > subs_out ? subs_in : subs_out;
				if (!subs) {
					continue;
				}

				for (int sub = 0; sub < subs; ++sub) {
					snd_rawmidi_info_set_stream (info, sub < subs_in ?
							SND_RAWMIDI_STREAM_INPUT :
							SND_RAWMIDI_STREAM_OUTPUT);

					snd_rawmidi_info_set_subdevice (info, sub);
					if (snd_ctl_rawmidi_info (handle, info) < 0) {
						continue;
					}

					const char *sub_name = snd_rawmidi_info_get_subdevice_name (info);
					if (sub == 0 && sub_name[0] == '\0') {
						devname = "hw:";
						devname += snd_ctl_card_info_get_id (cinfo);
						devname += ",";
						devname += PBD::to_string (device);

						std::string card_name = snd_rawmidi_info_get_name (info);

						int caps = 0;
						if (sub < subs_in) caps |= SND_SEQ_PORT_CAP_READ;
						if (sub < subs_out) caps |= SND_SEQ_PORT_CAP_WRITE;

						insert_unique_device_name (devices, card_name, devname, caps);
						break;
					} else {
						devname = "hw:";
						devname += snd_ctl_card_info_get_id (cinfo);
						devname += ",";
						devname += PBD::to_string (device);
						devname += ",";
						devname += PBD::to_string (sub);

						int caps = 0;
						if (sub < subs_in) caps |= SND_SEQ_PORT_CAP_READ;
						if (sub < subs_out) caps |= SND_SEQ_PORT_CAP_WRITE;
						insert_unique_device_name (devices, sub_name, devname, caps);
					}
				}
			}
			snd_ctl_close (handle);
		}
	}
}

void
ARDOUR::get_alsa_sequencer_names (std::map<std::string, std::string>& devices)
{
	snd_seq_t *seq= NULL;
	snd_seq_client_info_t *cinfo;
	snd_seq_port_info_t *pinfo;

	snd_seq_client_info_alloca (&cinfo);
	snd_seq_port_info_alloca (&pinfo);

	if (snd_seq_open (&seq, "hw", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
		return;
	}

	snd_seq_client_info_set_client(cinfo, -1);
	while (snd_seq_query_next_client (seq, cinfo) >= 0) {
		int client = snd_seq_client_info_get_client (cinfo);
		if (client == SND_SEQ_CLIENT_SYSTEM) {
			continue;
		}
		if (!strcmp (snd_seq_client_info_get_name(cinfo), "Midi Through")) {
			continue;
		}
		snd_seq_port_info_set_client (pinfo, client);
		snd_seq_port_info_set_port (pinfo, -1);

		while (snd_seq_query_next_port (seq, pinfo) >= 0) {
			int caps = snd_seq_port_info_get_capability(pinfo);
			if (0 == (caps & (SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_WRITE))) {
				continue;
			}
			if (caps & SND_SEQ_PORT_CAP_NO_EXPORT) {
				continue;
			}
			std::string card_name;
			card_name = snd_seq_port_info_get_name (pinfo);

			std::string devname;
			devname = PBD::to_string(snd_seq_port_info_get_client (pinfo));
			devname += ":";
			devname += PBD::to_string(snd_seq_port_info_get_port (pinfo));
			insert_unique_device_name (devices, card_name, devname, caps);
		}
	}
	snd_seq_close (seq);
}

int
ARDOUR::card_to_num(const char* device_name)
{
	char* ctl_name;
	const char * comma;
	snd_ctl_t* ctl_handle;
	int i = -1;

	if (strncasecmp(device_name, "plughw:", 7) == 0) {
		device_name += 4;
	}
	if (!(comma = strchr(device_name, ','))) {
		ctl_name = strdup(device_name);
	} else {
		ctl_name = strndup(device_name, comma - device_name);
	}

	if (snd_ctl_open (&ctl_handle, ctl_name, 0) >= 0) {
		snd_ctl_card_info_t *card_info;
		snd_ctl_card_info_alloca (&card_info);
		if (snd_ctl_card_info(ctl_handle, card_info) >= 0) {
			i = snd_ctl_card_info_get_card(card_info);
		}
		snd_ctl_close(ctl_handle);
	}
	free(ctl_name);
	return i;
}
