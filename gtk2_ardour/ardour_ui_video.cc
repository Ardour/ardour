/*
 * Copyright (C) 2005-2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2005-2017 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2005-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Karsten Wiese <fzuuzf@googlemail.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2008-2010 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2012-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013-2015 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2013-2016 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2013-2016 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2014-2018 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015 André Nusser <andre.nusser@googlemail.com>
 * Copyright (C) 2016-2018 Len Ovens <len@ovenwerks.net>
 * Copyright (C) 2017 Johannes Mueller <github@johannes-mueller.org>
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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#include "gtk2ardour-version.h"
#endif

#include "pbd/gstdio_compat.h"

#include <gtkmm/stock.h>

#include "pbd/error.h"
#include "pbd/openuri.h"

#include "ardour/ltc_file_reader.h"
#include "ardour/session_directory.h"

#include "add_video_dialog.h"
#include "ardour_ui.h"
#include "export_video_infobox.h"
#include "export_video_dialog.h"
#include "public_editor.h"
#include "utils_videotl.h"
#include "transcode_video_dialog.h"
#include "video_server_dialog.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace std;

void
ARDOUR_UI::stop_video_server (bool ask_confirm)
{
	if (!video_server_process && ask_confirm) {
		warning << string_compose (_("Video-Server was not launched by %1. The request to stop it is ignored."), PROGRAM_NAME) << endmsg;
	}
	if (video_server_process) {
		if(ask_confirm) {
			ArdourDialog confirm (_("Stop Video-Server"), true);
			Label m (_("Do you really want to stop the Video Server?"));
			confirm.get_vbox()->pack_start (m, true, true);
			confirm.add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
			confirm.add_button (_("Yes, Stop It"), Gtk::RESPONSE_ACCEPT);
			confirm.show_all ();
			switch (confirm.run()) {
				case RESPONSE_ACCEPT:
					break;
				default:
					return;
			}
		}
		delete video_server_process;
		video_server_process =0;
	}
}

void
ARDOUR_UI::start_video_server_menu (Gtk::Window* float_window)
{
  ARDOUR_UI::start_video_server( float_window, true);
}

bool
ARDOUR_UI::start_video_server (Gtk::Window* float_window, bool popup_msg)
{
	if (!_session) {
		return false;
	}
	if (popup_msg) {
		if (ARDOUR_UI::instance()->video_timeline->check_server()) {
			if (video_server_process) {
				popup_error(_("The Video Server is already started."));
			} else {
				popup_error(_("An external Video Server is configured and can be reached. Not starting a new instance."));
			}
		}
	}

	int firsttime = 0;
	while (!ARDOUR_UI::instance()->video_timeline->check_server()) {
		if (firsttime++) {
			warning << _("Could not connect to the Video Server. Start it or configure its access URL in Preferences.") << endmsg;
		}
		VideoServerDialog *video_server_dialog = new VideoServerDialog (_session);
		if (float_window) {
			video_server_dialog->set_transient_for (*float_window);
		}

		if (!Config->get_show_video_server_dialog() && firsttime < 2) {
			video_server_dialog->hide();
		} else {
			ResponseType r = (ResponseType) video_server_dialog->run ();
			video_server_dialog->hide();
			if (r != RESPONSE_ACCEPT) { return false; }
			if (video_server_dialog->show_again()) {
				Config->set_show_video_server_dialog(false);
			}
		}

		std::string icsd_exec = video_server_dialog->get_exec_path();
		std::string icsd_docroot = video_server_dialog->get_docroot();
#ifndef PLATFORM_WINDOWS
		if (icsd_docroot.empty()) {
			icsd_docroot = VideoUtils::video_get_docroot (Config);
		}
#endif

		GStatBuf sb;
#ifdef PLATFORM_WINDOWS
		if (VideoUtils::harvid_version >= 0x000802 && icsd_docroot.empty()) {
			/* OK, allow all drive letters */
		} else
#endif
		if (g_lstat (icsd_docroot.c_str(), &sb) != 0 || !S_ISDIR(sb.st_mode)) {
			warning << _("Specified docroot is not an existing directory.") << endmsg;
			continue;
		}
#ifndef PLATFORM_WINDOWS
		if ( (g_lstat (icsd_exec.c_str(), &sb) != 0)
		     || (sb.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)) == 0 ) {
			warning << _("Given Video Server is not an executable file.") << endmsg;
			continue;
		}
#else
		if ( (g_lstat (icsd_exec.c_str(), &sb) != 0)
		     || (sb.st_mode & (S_IXUSR)) == 0 ) {
			warning << _("Given Video Server is not an executable file.") << endmsg;
			continue;
		}
#endif

		char **argp;
		argp=(char**) calloc(9,sizeof(char*));
		argp[0] = strdup(icsd_exec.c_str());
		argp[1] = strdup("-P");
		argp[2] = (char*) calloc(16,sizeof(char)); snprintf(argp[2], 16, "%s", video_server_dialog->get_listenaddr().c_str());
		argp[3] = strdup("-p");
		argp[4] = (char*) calloc(6,sizeof(char)); snprintf(argp[4], 6, "%i", video_server_dialog->get_listenport());
		argp[5] = strdup("-C");
		argp[6] = (char*) calloc(6,sizeof(char)); snprintf(argp[6], 6, "%i", video_server_dialog->get_cachesize());
		argp[7] = strdup(icsd_docroot.c_str());
		argp[8] = 0;
		stop_video_server();

#ifdef PLATFORM_WINDOWS
		if (VideoUtils::harvid_version >= 0x000802 && icsd_docroot.empty()) {
			/* OK, allow all drive letters */
		} else
#endif
		if (icsd_docroot == X_("/") || icsd_docroot == X_("C:\\")) {
			Config->set_video_advanced_setup(false);
		} else {
			std::string url_str = "http://127.0.0.1:" + to_string(video_server_dialog->get_listenport()) + "/";
			Config->set_video_server_url(url_str);
			Config->set_video_server_docroot(icsd_docroot);
			Config->set_video_advanced_setup(true);
		}

		if (video_server_process) {
			delete video_server_process;
		}

		video_server_process = new ARDOUR::SystemExec(icsd_exec, argp);
		if (video_server_process->start()) {
			warning << _("Cannot launch the video-server") << endmsg;
			continue;
		}
		int timeout = 120; // 6 sec
		while (!ARDOUR_UI::instance()->video_timeline->check_server()) {
			Glib::usleep (50000);
			gui_idle_handler();
			if (--timeout <= 0 || !video_server_process->is_running()) break;
		}
		if (timeout <= 0) {
			warning << _("Video-server was started but does not respond to requests...") << endmsg;
		} else {
			if (!ARDOUR_UI::instance()->video_timeline->check_server_docroot()) {
				delete video_server_process;
				video_server_process = 0;
			}
		}
	}
	return true;
}

void
ARDOUR_UI::add_video (Gtk::Window* float_window)
{
	if (!_session) {
		return;
	}

	if (!start_video_server(float_window, false)) {
		warning << _("Could not connect to the Video Server. Start it or configure its access URL in Preferences.") << endmsg;
		return;
	}

	if (float_window) {
		add_video_dialog->set_transient_for (*float_window);
	}

	if (add_video_dialog->is_visible()) {
		/* we're already doing this */
		return;
	}

	ResponseType r = (ResponseType) add_video_dialog->run ();
	add_video_dialog->hide();
	if (r != RESPONSE_ACCEPT) { return; }

	bool local_file, orig_local_file;
	std::string path = add_video_dialog->file_name(local_file);

	std::string orig_path = path;
	orig_local_file = local_file;

	bool auto_set_session_fps = add_video_dialog->auto_set_session_fps();

	if (local_file && !Glib::file_test(path, Glib::FILE_TEST_EXISTS)) {
		warning << string_compose(_("could not open %1"), path) << endmsg;
		return;
	}
	if (!local_file && path.length() == 0) {
		warning << _("no video-file selected") << endmsg;
		return;
	}

	std::string audio_from_video;
	bool detect_ltc = false;

	switch (add_video_dialog->import_option()) {
		case VTL_IMPORT_TRANSCODE:
			{
				TranscodeVideoDialog *transcode_video_dialog;
				transcode_video_dialog = new TranscodeVideoDialog (_session, path);
				ResponseType r = (ResponseType) transcode_video_dialog->run ();
				transcode_video_dialog->hide();
				if (r != RESPONSE_ACCEPT) {
					delete transcode_video_dialog;
					return;
				}

				audio_from_video = transcode_video_dialog->get_audiofile();

				if (!audio_from_video.empty() && transcode_video_dialog->detect_ltc()) {
					detect_ltc = true;
				}
				else if (!audio_from_video.empty()) {
					editor->embed_audio_from_video(
							audio_from_video,
							video_timeline->get_offset(),
							(transcode_video_dialog->import_option() != VTL_IMPORT_NO_VIDEO)
							);
				}
				switch (transcode_video_dialog->import_option()) {
					case VTL_IMPORT_TRANSCODED:
						path = transcode_video_dialog->get_filename();
						local_file = true;
						break;
					case VTL_IMPORT_REFERENCE:
						break;
					default:
						delete transcode_video_dialog;
						return;
				}
				delete transcode_video_dialog;
			}
			break;
		default:
		case VTL_IMPORT_NONE:
			break;
	}

	/* strip _session->session_directory().video_path() from video file if possible */
	if (local_file && !path.compare(0, _session->session_directory().video_path().size(), _session->session_directory().video_path())) {
		 path=path.substr(_session->session_directory().video_path().size());
		 if (path.at(0) == G_DIR_SEPARATOR) {
			 path=path.substr(1);
		 }
	}

	video_timeline->set_update_session_fps(auto_set_session_fps);

	if (video_timeline->video_file_info(path, local_file)) {
		XMLNode* node = new XMLNode(X_("Videotimeline"));
		node->set_property (X_("Filename"), path);
		node->set_property (X_("AutoFPS"), auto_set_session_fps);
		node->set_property (X_("LocalFile"), local_file);
		if (orig_local_file) {
			node->set_property (X_("OriginalVideoFile"), orig_path);
		} else {
			node->remove_property (X_("OriginalVideoFile"));
		}
		_session->add_extra_xml (*node);
		_session->set_dirty ();

		if (!audio_from_video.empty() && detect_ltc) {
			std::vector<LTCFileReader::LTCMap> ltc_seq;

			try {
				/* TODO ask user about TV standard (LTC alignment if any) */
				LTCFileReader ltcr (audio_from_video, video_timeline->get_video_file_fps());
				/* TODO ASK user which channel:  0 .. ltcr->channels() - 1 */

				ltc_seq = ltcr.read_ltc (/*channel*/ 0, /*max LTC samples to decode*/ 15);

				/* TODO seek near end of file, and read LTC until end.
				 * if it fails to find any LTC samples, scan complete file
				 *
				 * calculate drift of LTC compared to video-duration,
				 * ask user for reference (timecode from start/mid/end)
				 */
			} catch (...) {
				// LTCFileReader will have written error messages
			}

			::g_unlink(audio_from_video.c_str());

			if (ltc_seq.size() == 0) {
				PBD::error << _("No LTC detected, video will not be aligned.") << endmsg;
			} else {
				/* the very first TC in the file is somteimes not aligned properly */
				int i = ltc_seq.size() -1;
				ARDOUR::sampleoffset_t video_start_offset =
					_session->nominal_sample_rate() * (ltc_seq[i].timecode_sec - ltc_seq[i].framepos_sec);
				PBD::info << string_compose (_("Align video-start to %1 [samples]"), video_start_offset) << endmsg;
				video_timeline->set_offset(video_start_offset);
			}
		}

		_session->maybe_update_session_range (
			timepos_t (std::max(video_timeline->get_offset(), (sampleoffset_t) 0)),
			timepos_t (std::max(video_timeline->get_offset() + video_timeline->get_duration(), (sampleoffset_t) 0)));


		if (add_video_dialog->launch_xjadeo() && local_file) {
			editor->set_xjadeo_sensitive(true);
			editor->toggle_xjadeo_proc(1);
		} else {
			editor->toggle_xjadeo_proc(0);
		}
		editor->toggle_ruler_video(true);
	}
}

void
ARDOUR_UI::remove_video ()
{
	video_timeline->close_session();
	editor->toggle_ruler_video(false);

	/* reset state */
	video_timeline->set_offset_locked(false);
	video_timeline->set_offset(0);

	/* delete session state */
	XMLNode* node = new XMLNode(X_("Videotimeline"));
	_session->add_extra_xml(*node);
	node = new XMLNode(X_("Videomonitor"));
	_session->add_extra_xml(*node);
	node = new XMLNode(X_("Videoexport"));
	_session->add_extra_xml(*node);
	stop_video_server();
}

void
ARDOUR_UI::flush_videotimeline_cache (bool localcacheonly)
{
	if (localcacheonly) {
		video_timeline->vmon_update();
	} else {
		video_timeline->flush_cache();
	}
	editor->queue_visual_videotimeline_update();
}

void
ARDOUR_UI::export_video (bool range)
{
	if (ARDOUR::Config->get_show_video_export_info()) {
		ExportVideoInfobox infobox (_session);
		Gtk::ResponseType rv = (Gtk::ResponseType) infobox.run();
		if (infobox.show_again()) {
			ARDOUR::Config->set_show_video_export_info(false);
		}
		switch (rv) {
			case RESPONSE_YES:
				PBD::open_uri (ARDOUR::Config->get_reference_manual_url() + "/video-timeline/operations/#export");
				break;
			default:
				break;
		}
	}
	export_video_dialog->set_session (_session);
	export_video_dialog->apply_state(editor->get_selection().time, range);
	export_video_dialog->run ();
	export_video_dialog->hide ();
}
