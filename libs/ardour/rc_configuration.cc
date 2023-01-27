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

#include "pbd/convert.h"
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

	root->add_child_nocopy (get_variables (X_("Config")));

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
RCConfiguration::get_variables (std::string const & node_name) const
{
	XMLNode* node;

	node = new XMLNode (node_name);

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
	using namespace PBD;

#define VAR_META(name,...)  { char const * _x[] { __VA_ARGS__ }; Configuration::all_metadata.insert (std::make_pair<std::string,Metadata> ((name), internationalize_and_upcase (PACKAGE, _x))); }

	VAR_META (X_("afl-position"),
	          NULL
		);
	VAR_META (X_("all-safe"),
	          NULL
		);
	VAR_META (X_("allow-special-bus-removal"),
	          NULL
		);
	VAR_META (X_("ask-replace-instrument"),
	          NULL
		);
	VAR_META (X_("ask-setup-instrument"),
	          NULL
		);
	VAR_META (X_("auditioner-output-left"),
	          NULL
		);
	VAR_META (X_("auditioner-output-right"),
	          NULL
		);
	VAR_META (X_("auto-analyse-audio"),
	          NULL
		);
	VAR_META (X_("auto-connect-standard-busses"),
	          NULL
		);
	VAR_META (X_("auto-input-does-talkback"),
	          NULL
		);
	VAR_META (X_("auto-return-after-rewind-ffwd"),
	          NULL
		);
	VAR_META (X_("auto-return-target-list"),
	          NULL
		);
	VAR_META (X_("automation-follows-regions"),
	          NULL
		);
	VAR_META (X_("automation-interval-msecs"),
	          NULL
		);
	VAR_META (X_("automation-thinning-factor"),
	          NULL
		);
	VAR_META (X_("buffering-preset"),
	          NULL
		);
	VAR_META (X_("capture-buffer-seconds"),
	          NULL
		);
	VAR_META (X_("click-emphasis-sound"),
	          NULL
		);
	VAR_META (X_("click-gain"),
	          NULL
		);
	VAR_META (X_("click-record-only"),
	          NULL
		);
	VAR_META (X_("click-sound"),
	          NULL
		);
	VAR_META (X_("clicking"),
	          NULL
		);
	VAR_META (X_("clip-library-dir"),
	          NULL
		);
	VAR_META (X_("conceal-lv1-if-lv2-exists"),
	          NULL
		);
	VAR_META (X_("conceal-vst2-if-vst3-exists"),
	          NULL
		);
	VAR_META (X_("copy-demo-sessions"),
	          NULL
		);
	VAR_META (X_("cpu-dma-latency"),
	          NULL
		);
	VAR_META (X_("create-xrun-marker"),
	          NULL
		);
	VAR_META (X_("default-automation-time-domain"),
	          NULL
		);
	VAR_META (X_("default-fade-shape"),
	          NULL
		);
	VAR_META (X_("default-session-parent-dir"),
	          NULL
		);
	VAR_META (X_("default-trigger-input-port"),
	          NULL
		);
	VAR_META (X_("denormal-model"),
	          NULL
		);
	VAR_META (X_("denormal-protection"),
	          NULL
		);
	VAR_META (X_("deprecated-hiding-groups-deactivates-groups"),
	          NULL
		);
	VAR_META (X_("disable-disarm-during-roll"),
	          NULL
		);
	VAR_META (X_("discover-plugins-on-start"),
	          NULL
		);
	VAR_META (X_("disk-choice-space-threshold"),
	          NULL
		);
	VAR_META (X_("display-first-midi-bank-as-zero"),
	          NULL
		);
	VAR_META (X_("donate-url"),
	          NULL
		);
	VAR_META (X_("edit-mode"),
	          NULL
		);
	VAR_META (X_("exclusive-solo"),
	          NULL
		);
	VAR_META (X_("export-preroll"),
	          NULL
		);
	VAR_META (X_("export-silence-threshold"),
	          NULL
		);
	VAR_META (X_("feedback-interval-ms"),
	          NULL
		);
	VAR_META (X_("group-override-inverts"),
	          NULL
		);
	VAR_META (X_("hide-dummy-backend"),
	          NULL
		);
	VAR_META (X_("history-depth"),
	          NULL
		);
	VAR_META (X_("initial-program-change"),
	          NULL
		);
	VAR_META (X_("input-auto-connect"),
	          NULL
		);
	VAR_META (X_("inter-scene-gap-samples"),
	          NULL
		);
	VAR_META (X_("interview-editing"),
	          NULL
		);
	VAR_META (X_("latched-record-enable"),
	          NULL
		);
	VAR_META (X_("layer-model"),
	          NULL
		);
	VAR_META (X_("limit-n-automatables"),
	          NULL
		);
	VAR_META (X_("link-send-and-route-panner"),
	          NULL
		);
	VAR_META (X_("listen-position"),
	          NULL
		);
	VAR_META (X_("locate-while-waiting-for-sync"),
	          NULL
		);
	VAR_META (X_("loop-fade-choice"),
	          NULL
		);
	VAR_META (X_("loop-is-mode"),
	          NULL
		);
	VAR_META (X_("ltc-output-port"),
	          NULL
		);
	VAR_META (X_("ltc-output-volume"),
	          NULL
		);
	VAR_META (X_("ltc-send-continuously"),
	          NULL
		);
	VAR_META (X_("max-gain"),
	          NULL
		);
	VAR_META (X_("max-recent-sessions"),
	          NULL
		);
	VAR_META (X_("max-recent-templates"),
	          NULL
		);
	VAR_META (X_("max-transport-speed"),
	          NULL
		);
	VAR_META (X_("meter-falloff"),
	          NULL
		);
	VAR_META (X_("meter-type-bus"),
	          NULL
		);
	VAR_META (X_("meter-type-master"),
	          NULL
		);
	VAR_META (X_("meter-type-track"),
	          NULL
		);
	VAR_META (X_("midi-audition-synth-uri"),
	          NULL
		);
	VAR_META (X_("midi-clock-sets-tempo"),
	          NULL
		);
	VAR_META (X_("midi-feedback"),
	          NULL
		);
	VAR_META (X_("midi-input-follows-selection"),
	          NULL
		);
	VAR_META (X_("midi-track-buffer-seconds"),
	          NULL
		);
	VAR_META (X_("minimum-disk-read-bytes"),
	          NULL
		);
	VAR_META (X_("minimum-disk-write-bytes"),
	          NULL
		);
	VAR_META (X_("mmc-control"),
	          NULL
		);
	VAR_META (X_("mmc-receive-device-id"),
	          NULL
		);
	VAR_META (X_("mmc-send-device-id"),
	          NULL
		);
	VAR_META (X_("monitor-bus-preferred-bundle"),
	          NULL
		);
	VAR_META (X_("monitoring-model"),
	          NULL
		);
	VAR_META (X_("mtc-qf-speed-tolerance"),
	          NULL
		);
	VAR_META (X_("mute-affects-control-outs"),
	          NULL
		);
	VAR_META (X_("mute-affects-main-outs"),
	          NULL
		);
	VAR_META (X_("mute-affects-post-fader"),
	          NULL
		);
	VAR_META (X_("mute-affects-pre-fader"),
	          NULL
		);
	VAR_META (X_("new-plugins-active"),
	          NULL
		);
	VAR_META (X_("osc-port"),
	          NULL
		);
	VAR_META (X_("output-auto-connect"),
	          NULL
		);
	VAR_META (X_("periodic-safety-backup-interval"),
	          NULL
		);
	VAR_META (X_("periodic-safety-backups"),
	          NULL
		);
	VAR_META (X_("pfl-position"),
	          NULL
		);
	VAR_META (X_("pingback-url"),
	          NULL
		);
	VAR_META (X_("playback-buffer-seconds"),
	          NULL
		);
	VAR_META (X_("plugin-cache-version"),
	          NULL
		);
	VAR_META (X_("plugin-path-lxvst"),
	          NULL
		);
	VAR_META (X_("plugin-path-vst"),
	          NULL
		);
	VAR_META (X_("plugin-path-vst3"),
	          NULL
		);
	VAR_META (X_("plugin-scan-timeout"),
	          NULL
		);
	VAR_META (X_("plugins-stop-with-transport"),
	          NULL
		);
	VAR_META (X_("port-resampler-quality"),
	          NULL
		);
	VAR_META (X_("preroll-seconds"),
	          NULL
		);
	VAR_META (X_("processor-usage"),
	          NULL
		);
	VAR_META (X_("quieten-at-speed"),
	          NULL
		);
	VAR_META (X_("range-location-minimum"),
	          NULL
		);
	VAR_META (X_("range-selection-after-split"),
	          NULL
		);
	VAR_META (X_("recording-resets-xrun-count"),
	          NULL
		);
	VAR_META (X_("reference-manual-url"),
	          NULL
		);
	VAR_META (X_("region-boundaries-from-onscreen_tracks"),
	          NULL
		);
	VAR_META (X_("region-boundaries-from-selected-tracks"),
	          NULL
		);
	VAR_META (X_("region-equivalency"),
	          NULL
		);
	VAR_META (X_("region-selection-after-split"),
	          NULL
		);
	VAR_META (X_("replicate-missing-region-channels"),
	          NULL
		);
	VAR_META (X_("reset-default-speed-on-stop"),
	          NULL
		);
	VAR_META (X_("resource-index-url"),
	          NULL
		);
	VAR_META (X_("rewind-ffwd-like-tape-decks"),
	          NULL
		);
	VAR_META (X_("ripple-mode"),
	          NULL
		);
	VAR_META (X_("run-all-transport-masters-always"),
	          NULL
		);
	VAR_META (X_("sample-lib-path"),
	          NULL
		);
	VAR_META (X_("save-history"),
	          NULL
		);
	VAR_META (X_("save-history-depth"),
	          NULL
		);
	VAR_META (X_("send-ltc"),
	          NULL
		);
	VAR_META (X_("send-midi-clock"),
	          NULL
		);
	VAR_META (X_("send-mmc"),
	          NULL
		);
	VAR_META (X_("send-mtc"),
	          NULL
		);
	VAR_META (X_("show-solo-mutes"),
	          NULL
		);
	VAR_META (X_("show-video-server-dialog"),
	          NULL
		);
	VAR_META (X_("show-vst3-micro-edit-inline"),
	          NULL
		);
	VAR_META (X_("shuttle-max-speed"),
	          NULL
		);
	VAR_META (X_("shuttle-speed-factor"),
	          NULL
		);
	VAR_META (X_("shuttle-speed-threshold"),
	          NULL
		);
	VAR_META (X_("shuttle-units"),
	          NULL
		);
	VAR_META (X_("skip-playback"),
	          NULL
		);
	VAR_META (X_("solo-control-is-listen-control"),
	          NULL
		);
	VAR_META (X_("solo-mute-gain"),
	          NULL
		);
	VAR_META (X_("solo-mute-override"),
	          NULL
		);
	VAR_META (X_("stop-at-session-end"),
	          NULL
		);
	VAR_META (X_("stop-recording-on-xrun"),
	          NULL
		);
	VAR_META (X_("strict-io"),
	          NULL
		);
	VAR_META (X_("timecode-sync-frame-rate"),
	          NULL
		);
	VAR_META (X_("trace-midi-input"),
	          NULL
		);
	VAR_META (X_("trace-midi-output"),
	          NULL
		);
	VAR_META (X_("tracks-auto-naming"),
	          NULL
		);
	VAR_META (X_("transient-sensitivity"),
	          NULL
		);
	VAR_META (X_("transport-masters-just-roll-when-sync-lost"),
	          NULL
		);
	VAR_META (X_("try-autostart-engine"),
	          NULL
		);
	VAR_META (X_("tutorial-manual-url"),
	          NULL
		);
	VAR_META (X_("updates-url"),
	          NULL
		);
	VAR_META (X_("use-audio-units"),
	          NULL
		);
	VAR_META (X_("use-click-emphasis"),
	          NULL
		);
	VAR_META (X_("use-lxvst"),
	          NULL
		);
	VAR_META (X_("use-macvst"),
	          NULL
		);
	VAR_META (X_("use-master-volume"),
	          NULL
		);
	VAR_META (X_("use-monitor-bus"),
	          NULL
		);
	VAR_META (X_("use-osc"),
	          NULL
		);
	VAR_META (X_("use-plugin-own-gui"),
	          NULL
		);
	VAR_META (X_("use-tranzport"),
	          NULL
		);
	VAR_META (X_("use-vst3"),
	          NULL
		);
	VAR_META (X_("use-windows-vst"),
	          NULL
		);
	VAR_META (X_("verbose-plugin-scan"),
	          NULL
		);
	VAR_META (X_("verify-remove-last-capture"),
	          NULL
		);
	VAR_META (X_("video-advanced-setup"),
	          NULL
		);
	VAR_META (X_("video-server-docroot"),
	          NULL
		);
	VAR_META (X_("video-server-url"),
	          NULL
		);
	VAR_META (X_("work-around-jack-no-copy-optimization"),
	          NULL
		);
	VAR_META (X_("xjadeo-binary"), NULL);
}
