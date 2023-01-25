/*
 * Copyright (C) 1999-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2012-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
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

#include <unistd.h>
#include <cstdio> /* for snprintf, grrr */

#include <glib.h>
#include "pbd/gstdio_compat.h"
#include <glibmm/miscutils.h>

#include "pbd/xml++.h"
#include "pbd/file_utils.h"
#include "pbd/replace_all.h"

#include "temporal/types_convert.h"

#include "ardour/audioengine.h"
#include "ardour/disk_reader.h"
#include "ardour/disk_writer.h"
#include "ardour/control_protocol_manager.h"
#include "ardour/filename_extensions.h"
#include "ardour/filesystem_paths.h"
#include "ardour/port.h"
#include "ardour/rc_configuration.h"
#include "ardour/session_metadata.h"
#include "ardour/transport_master_manager.h"
#include "ardour/types_convert.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace std;
using namespace PBD;

/* this is global so that we do not have to indirect through an object pointer
   to reference it.
*/

namespace ARDOUR {
    float speed_quietning = 0.251189; // -12dB reduction for ffwd or rewind
}

static const char* user_config_file_name = "config";
static const char* system_config_file_name = "system_config";

RCConfiguration::RCConfiguration ()
	:
/* construct variables */
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL
#define CONFIG_VARIABLE(Type,var,name,value) var (name,value),
#define CONFIG_VARIABLE_SPECIAL(Type,var,name,value,mutator) var (name,value,mutator),
#include "ardour/rc_configuration_vars.h"
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL
	_control_protocol_state (0)
      , _transport_master_state (0)
{
	/* build map */

#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL
#define CONFIG_VARIABLE(Type,var,name,value) _my_variables.insert (std::make_pair ((name), &(var)));
#define CONFIG_VARIABLE_SPECIAL(Type,var,name,value,mutator) _my_variables.insert (std::make_pair ((name), &(var)));
#include "ardour/rc_configuration_vars.h"
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL

	build_metadata ();
}

RCConfiguration::~RCConfiguration ()
{
	delete _control_protocol_state;
	delete _transport_master_state;
}

int
RCConfiguration::load_state ()
{
	std::string rcfile;
	GStatBuf statbuf;

	/* load system configuration first */

	if (find_file (ardour_config_search_path(), system_config_file_name, rcfile)) {

		/* stupid XML Parser hates empty files */

		if (g_stat (rcfile.c_str(), &statbuf)) {
			return -1;
		}

		if (statbuf.st_size != 0) {
			info << string_compose (_("Loading system configuration file %1"), rcfile) << endmsg;

			XMLTree tree;
			if (!tree.read (rcfile.c_str())) {
				error << string_compose(_("%1: cannot read system configuration file \"%2\""), PROGRAM_NAME, rcfile) << endmsg;
				return -1;
			}

			if (set_state (*tree.root(), Stateful::current_state_version)) {
				error << string_compose(_("%1: system configuration file \"%2\" not loaded successfully."), PROGRAM_NAME, rcfile) << endmsg;
				return -1;
			}
		} else {
			error << string_compose (_("Your system %1 configuration file is empty. This probably means that there was an error installing %1"), PROGRAM_NAME) << endmsg;
		}
	}

	/* now load configuration file for user */

	if (find_file (ardour_config_search_path(), user_config_file_name, rcfile)) {

		/* stupid XML parser hates empty files */

		if (g_stat (rcfile.c_str(), &statbuf)) {
			return -1;
		}

		if (statbuf.st_size != 0) {
			info << string_compose (_("Loading user configuration file %1"), rcfile) << endmsg;

			XMLTree tree;
			if (!tree.read (rcfile)) {
				error << string_compose(_("%1: cannot read configuration file \"%2\""), PROGRAM_NAME, rcfile) << endmsg;
				return -1;
			}

			if (set_state (*tree.root(), Stateful::current_state_version)) {
				error << string_compose(_("%1: user configuration file \"%2\" not loaded successfully."), PROGRAM_NAME, rcfile) << endmsg;
				return -1;
			}
		} else {
			warning << string_compose (_("your %1 configuration file is empty. This is not normal."), PROGRAM_NAME) << endmsg;
		}
	}

	return 0;
}

int
RCConfiguration::save_state()
{
	const std::string rcfile = Glib::build_filename (user_config_directory(), user_config_file_name);
	const std::string tmp = rcfile + temp_suffix;

	XMLTree tree;
	tree.set_root (&get_state());
	if (!tree.write (tmp.c_str())){
		error << string_compose (_("Config file %1 not saved"), rcfile) << endmsg;
		if (g_remove (tmp.c_str()) != 0) {
			error << string_compose(_("Could not remove temporary config file at path \"%1\" (%2)"), tmp, g_strerror (errno)) << endmsg;
		}
		return -1;
	}

	if (::g_rename (tmp.c_str(), rcfile.c_str()) != 0) {
		error << string_compose (_("Could not rename temporary config file %1 to %2 (%3)"), tmp, rcfile, g_strerror(errno)) << endmsg;
		if (g_remove (tmp.c_str()) != 0) {
			error << string_compose(_("Could not remove temporary config file at path \"%1\" (%2)"), tmp, g_strerror (errno)) << endmsg;
		}
		return -1;
	}

	return 0;
}

void
RCConfiguration::add_instant_xml(XMLNode& node)
{
	Stateful::add_instant_xml (node, user_config_directory ());
}

XMLNode*
RCConfiguration::instant_xml(const string& node_name)
{
	return Stateful::instant_xml (node_name, user_config_directory ());
}


XMLNode&
RCConfiguration::get_state () const
{
	XMLNode* root;

	root = new XMLNode("Ardour");

	root->add_child_nocopy (get_variables ());

	root->add_child_nocopy (SessionMetadata::Metadata()->get_user_state());

	if (_extra_xml) {
		root->add_child_copy (*_extra_xml);
	}

	root->add_child_nocopy (ControlProtocolManager::instance().get_state());

	if (TransportMasterManager::exists()) {
		root->add_child_nocopy (TransportMasterManager::instance().get_state());
	}

	return *root;
}

XMLNode&
RCConfiguration::get_variables () const
{
	XMLNode* node;

	node = new XMLNode ("Config");

#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL
#define CONFIG_VARIABLE(type,var,Name,value) \
	var.add_to_node (*node);
#define CONFIG_VARIABLE_SPECIAL(type,var,Name,value,mutator) \
	var.add_to_node (*node);
#include "ardour/rc_configuration_vars.h"
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL

	return *node;
}

int
RCConfiguration::set_state (const XMLNode& root, int version)
{
	if (root.name() != "Ardour") {
		return -1;
	}

	XMLNodeList nlist = root.children();
	XMLNodeConstIterator niter;
	XMLNode *node;

	Stateful::save_extra_xml (root);

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {

		node = *niter;

		if (node->name() == "Config") {
			set_variables (*node);
		} else if (node->name() == "Metadata") {
			SessionMetadata::Metadata()->set_state (*node, version);
		} else if (node->name() == ControlProtocolManager::state_node_name) {
			_control_protocol_state = new XMLNode (*node);
		} else if (node->name() == TransportMasterManager::state_node_name) {
			_transport_master_state = new XMLNode (*node);
		}
	}

	DiskReader::set_chunk_samples (minimum_disk_read_bytes.get() / sizeof (Sample));
	DiskWriter::set_chunk_samples (minimum_disk_write_bytes.get() / sizeof (Sample));

	return 0;
}

void
RCConfiguration::set_variables (const XMLNode& node)
{
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL
#define CONFIG_VARIABLE(type,var,name,value) \
  if (var.set_from_node (node)) {            \
    ParameterChanged (name);                 \
  }

#define CONFIG_VARIABLE_SPECIAL(type,var,name,value,mutator) \
  if (var.set_from_node (node)) {                            \
    ParameterChanged (name);                                 \
  }

#include "ardour/rc_configuration_vars.h"
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL

}
void
RCConfiguration::map_parameters (boost::function<void (std::string)>& functor)
{
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL
#define CONFIG_VARIABLE(type,var,name,value)                 functor (name);
#define CONFIG_VARIABLE_SPECIAL(type,var,name,value,mutator) functor (name);
#include "ardour/rc_configuration_vars.h"
#undef  CONFIG_VARIABLE
#undef  CONFIG_VARIABLE_SPECIAL
}

void
RCConfiguration::build_metadata ()
{
#define VAR_META(name,...)  { char const * _x[] { __VA_ARGS__ }; all_metadata.insert (std::make_pair<std::string,Metadata> ((name), PBD::internationalize (PACKAGE, _x))); }

	VAR_META (X_("afl-position"),
	          "foo", "bar"
		);
	VAR_META (X_("all-safe"),
	          ""
		);
	VAR_META (X_("allow-special-bus-removal"),
	          ""
		);
	VAR_META (X_("ask-replace-instrument"),
	          ""
		);
	VAR_META (X_("ask-setup-instrument"),
	          ""
		);
	VAR_META (X_("auditioner-output-left"),
	          ""
		);
	VAR_META (X_("auditioner-output-right"),
	          ""
		);
	VAR_META (X_("auto-analyse-audio"),
	          ""
		);
	VAR_META (X_("auto-connect-standard-busses"),
	          ""
		);
	VAR_META (X_("auto-input-does-talkback"),
	          ""
		);
	VAR_META (X_("auto-return-after-rewind-ffwd"),
	          ""
		);
	VAR_META (X_("auto-return-target-list"),
	          ""
		);
	VAR_META (X_("automation-follows-regions"),
	          ""
		);
	VAR_META (X_("automation-interval-msecs"),
	          ""
		);
	VAR_META (X_("automation-thinning-factor"),
	          ""
		);
	VAR_META (X_("buffering-preset"),
	          ""
		);
	VAR_META (X_("capture-buffer-seconds"),
	          ""
		);
	VAR_META (X_("click-emphasis-sound"),
	          ""
		);
	VAR_META (X_("click-gain"),
	          ""
		);
	VAR_META (X_("click-record-only"),
	          ""
		);
	VAR_META (X_("click-sound"),
	          ""
		);
	VAR_META (X_("clicking"),
	          ""
		);
	VAR_META (X_("clip-library-dir"),
	          ""
		);
	VAR_META (X_("conceal-lv1-if-lv2-exists"),
	          ""
		);
	VAR_META (X_("conceal-vst2-if-vst3-exists"),
	          ""
		);
	VAR_META (X_("copy-demo-sessions"),
	          ""
		);
	VAR_META (X_("cpu-dma-latency"),
	          ""
		);
	VAR_META (X_("create-xrun-marker"),
	          ""
		);
	VAR_META (X_("default-automation-time-domain"),
	          ""
		);
	VAR_META (X_("default-fade-shape"),
	          ""
		);
	VAR_META (X_("default-session-parent-dir"),
	          ""
		);
	VAR_META (X_("default-trigger-input-port"),
	          ""
		);
	VAR_META (X_("denormal-model"),
	          ""
		);
	VAR_META (X_("denormal-protection"),
	          ""
		);
	VAR_META (X_("deprecated-hiding-groups-deactivates-groups"),
	          ""
		);
	VAR_META (X_("disable-disarm-during-roll"),
	          ""
		);
	VAR_META (X_("discover-plugins-on-start"),
	          ""
		);
	VAR_META (X_("disk-choice-space-threshold"),
	          ""
		);
	VAR_META (X_("display-first-midi-bank-as-zero"),
	          ""
		);
	VAR_META (X_("donate-url"),
	          ""
		);
	VAR_META (X_("edit-mode"),
	          ""
		);
	VAR_META (X_("exclusive-solo"),
	          ""
		);
	VAR_META (X_("export-preroll"),
	          ""
		);
	VAR_META (X_("export-silence-threshold"),
	          ""
		);
	VAR_META (X_("feedback-interval-ms"),
	          ""
		);
	VAR_META (X_("group-override-inverts"),
	          ""
		);
	VAR_META (X_("hide-dummy-backend"),
	          ""
		);
	VAR_META (X_("history-depth"),
	          ""
		);
	VAR_META (X_("initial-program-change"),
	          ""
		);
	VAR_META (X_("input-auto-connect"),
	          ""
		);
	VAR_META (X_("inter-scene-gap-samples"),
	          ""
		);
	VAR_META (X_("interview-editing"),
	          ""
		);
	VAR_META (X_("latched-record-enable"),
	          ""
		);
	VAR_META (X_("layer-model"),
	          ""
		);
	VAR_META (X_("limit-n-automatables"),
	          ""
		);
	VAR_META (X_("link-send-and-route-panner"),
	          ""
		);
	VAR_META (X_("listen-position"),
	          ""
		);
	VAR_META (X_("locate-while-waiting-for-sync"),
	          ""
		);
	VAR_META (X_("loop-fade-choice"),
	          ""
		);
	VAR_META (X_("loop-is-mode"),
	          ""
		);
	VAR_META (X_("ltc-output-port"),
	          ""
		);
	VAR_META (X_("ltc-output-volume"),
	          ""
		);
	VAR_META (X_("ltc-send-continuously"),
	          ""
		);
	VAR_META (X_("max-gain"),
	          ""
		);
	VAR_META (X_("max-recent-sessions"),
	          ""
		);
	VAR_META (X_("max-recent-templates"),
	          ""
		);
	VAR_META (X_("max-transport-speed"),
	          ""
		);
	VAR_META (X_("meter-falloff"),
	          ""
		);
	VAR_META (X_("meter-type-bus"),
	          ""
		);
	VAR_META (X_("meter-type-master"),
	          ""
		);
	VAR_META (X_("meter-type-track"),
	          ""
		);
	VAR_META (X_("midi-audition-synth-uri"),
	          ""
		);
	VAR_META (X_("midi-clock-sets-tempo"),
	          ""
		);
	VAR_META (X_("midi-feedback"),
	          ""
		);
	VAR_META (X_("midi-input-follows-selection"),
	          ""
		);
	VAR_META (X_("midi-track-buffer-seconds"),
	          ""
		);
	VAR_META (X_("minimum-disk-read-bytes"),
	          ""
		);
	VAR_META (X_("minimum-disk-write-bytes"),
	          ""
		);
	VAR_META (X_("mmc-control"),
	          ""
		);
	VAR_META (X_("mmc-receive-device-id"),
	          ""
		);
	VAR_META (X_("mmc-send-device-id"),
	          ""
		);
	VAR_META (X_("monitor-bus-preferred-bundle"),
	          ""
		);
	VAR_META (X_("monitoring-model"),
	          ""
		);
	VAR_META (X_("mtc-qf-speed-tolerance"),
	          ""
		);
	VAR_META (X_("mute-affects-control-outs"),
	          ""
		);
	VAR_META (X_("mute-affects-main-outs"),
	          ""
		);
	VAR_META (X_("mute-affects-post-fader"),
	          ""
		);
	VAR_META (X_("mute-affects-pre-fader"),
	          ""
		);
	VAR_META (X_("new-plugins-active"),
	          ""
		);
	VAR_META (X_("osc-port"),
	          ""
		);
	VAR_META (X_("output-auto-connect"),
	          ""
		);
	VAR_META (X_("periodic-safety-backup-interval"),
	          ""
		);
	VAR_META (X_("periodic-safety-backups"),
	          ""
		);
	VAR_META (X_("pfl-position"),
	          ""
		);
	VAR_META (X_("pingback-url"),
	          ""
		);
	VAR_META (X_("playback-buffer-seconds"),
	          ""
		);
	VAR_META (X_("plugin-cache-version"),
	          ""
		);
	VAR_META (X_("plugin-path-lxvst"),
	          ""
		);
	VAR_META (X_("plugin-path-vst"),
	          ""
		);
	VAR_META (X_("plugin-path-vst3"),
	          ""
		);
	VAR_META (X_("plugin-scan-timeout"),
	          ""
		);
	VAR_META (X_("plugins-stop-with-transport"),
	          ""
		);
	VAR_META (X_("port-resampler-quality"),
	          ""
		);
	VAR_META (X_("preroll-seconds"),
	          ""
		);
	VAR_META (X_("processor-usage"),
	          ""
		);
	VAR_META (X_("quieten-at-speed"),
	          ""
		);
	VAR_META (X_("range-location-minimum"),
	          ""
		);
	VAR_META (X_("range-selection-after-split"),
	          ""
		);
	VAR_META (X_("recording-resets-xrun-count"),
	          ""
		);
	VAR_META (X_("reference-manual-url"),
	          ""
		);
	VAR_META (X_("region-boundaries-from-onscreen_tracks"),
	          ""
		);
	VAR_META (X_("region-boundaries-from-selected-tracks"),
	          ""
		);
	VAR_META (X_("region-equivalency"),
	          ""
		);
	VAR_META (X_("region-selection-after-split"),
	          ""
		);
	VAR_META (X_("replicate-missing-region-channels"),
	          ""
		);
	VAR_META (X_("reset-default-speed-on-stop"),
	          ""
		);
	VAR_META (X_("resource-index-url"),
	          ""
		);
	VAR_META (X_("rewind-ffwd-like-tape-decks"),
	          ""
		);
	VAR_META (X_("ripple-mode"),
	          ""
		);
	VAR_META (X_("run-all-transport-masters-always"),
	          ""
		);
	VAR_META (X_("sample-lib-path"),
	          ""
		);
	VAR_META (X_("save-history"),
	          ""
		);
	VAR_META (X_("save-history-depth"),
	          ""
		);
	VAR_META (X_("send-ltc"),
	          ""
		);
	VAR_META (X_("send-midi-clock"),
	          ""
		);
	VAR_META (X_("send-mmc"),
	          ""
		);
	VAR_META (X_("send-mtc"),
	          ""
		);
	VAR_META (X_("show-solo-mutes"),
	          ""
		);
	VAR_META (X_("show-video-server-dialog"),
	          ""
		);
	VAR_META (X_("show-vst3-micro-edit-inline"),
	          ""
		);
	VAR_META (X_("shuttle-max-speed"),
	          ""
		);
	VAR_META (X_("shuttle-speed-factor"),
	          ""
		);
	VAR_META (X_("shuttle-speed-threshold"),
	          ""
		);
	VAR_META (X_("shuttle-units"),
	          ""
		);
	VAR_META (X_("skip-playback"),
	          ""
		);
	VAR_META (X_("solo-control-is-listen-control"),
	          ""
		);
	VAR_META (X_("solo-mute-gain"),
	          ""
		);
	VAR_META (X_("solo-mute-override"),
	          ""
		);
	VAR_META (X_("stop-at-session-end"),
	          ""
		);
	VAR_META (X_("stop-recording-on-xrun"),
	          ""
		);
	VAR_META (X_("strict-io"),
	          ""
		);
	VAR_META (X_("timecode-sync-frame-rate"),
	          ""
		);
	VAR_META (X_("trace-midi-input"),
	          ""
		);
	VAR_META (X_("trace-midi-output"),
	          ""
		);
	VAR_META (X_("tracks-auto-naming"),
	          ""
		);
	VAR_META (X_("transient-sensitivity"),
	          ""
		);
	VAR_META (X_("transport-masters-just-roll-when-sync-lost"),
	          ""
		);
	VAR_META (X_("try-autostart-engine"),
	          ""
		);
	VAR_META (X_("tutorial-manual-url"),
	          ""
		);
	VAR_META (X_("updates-url"),
	          ""
		);
	VAR_META (X_("use-audio-units"),
	          ""
		);
	VAR_META (X_("use-click-emphasis"),
	          ""
		);
	VAR_META (X_("use-lxvst"),
	          ""
		);
	VAR_META (X_("use-macvst"),
	          ""
		);
	VAR_META (X_("use-master-volume"),
	          ""
		);
	VAR_META (X_("use-monitor-bus"),
	          ""
		);
	VAR_META (X_("use-osc"),
	          ""
		);
	VAR_META (X_("use-plugin-own-gui"),
	          ""
		);
	VAR_META (X_("use-tranzport"),
	          ""
		);
	VAR_META (X_("use-vst3"),
	          ""
		);
	VAR_META (X_("use-windows-vst"),
	          ""
		);
	VAR_META (X_("verbose-plugin-scan"),
	          ""
		);
	VAR_META (X_("verify-remove-last-capture"),
	          ""
		);
	VAR_META (X_("video-advanced-setup"),
	          ""
		);
	VAR_META (X_("video-server-docroot"),
	          ""
		);
	VAR_META (X_("video-server-url"),
	          ""
		);
	VAR_META (X_("work-around-jack-no-copy-optimization"),
	          ""
		);
	VAR_META (X_("xjadeo-binary"), "");
}
