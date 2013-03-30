/*
    Copyright (C) 2010-2013 Paul Davis
    Author: Robin Gareus <robin@gareus.org>

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

*/
#ifdef WITH_VIDEOTIMELINE

#include <cstdio>
#include <cmath>

#include <sigc++/bind.h>
#include <curl/curl.h>

#include "pbd/error.h"
#include "pbd/convert.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/rgb_macros.h"
#include "ardour/session_directory.h"
#include "ardour/profile.h"
#include "ardour/template_utils.h"
#include "ardour/session.h"
#include "ardour_ui.h"

#include "utils.h"
#include "add_video_dialog.h"
#include "utils_videotl.h"
#include "i18n.h"

using namespace Gtk;
using namespace std;
using namespace PBD;
using namespace ARDOUR;

#define PREVIEW_WIDTH (240)
#define PREVIEW_HEIGHT (180)

#ifndef MIN
#define MIN(a,b) ( (a) < (b) ? (a) : (b) )
#endif

AddVideoDialog::AddVideoDialog (Session* s)
	: ArdourDialog (_("Set Video Track"))
	, seek_slider (0,1000,1)
	, preview_path ("")
	, pi_duration ("-", Gtk::ALIGN_RIGHT, Gtk::ALIGN_CENTER, false)
	, pi_aspect ("-", Gtk::ALIGN_RIGHT, Gtk::ALIGN_CENTER, false)
	, pi_fps ("-", Gtk::ALIGN_RIGHT, Gtk::ALIGN_CENTER, false)
	, chooser (FILE_CHOOSER_ACTION_OPEN)
	, xjadeo_checkbox (_("Launch External Video Monitor"))
	, set_session_fps_checkbox (_("Adjust Session Framerate to Match Video Framerate"))
	, harvid_path ("")
	, harvid_reset (_("Reload docroot"))
	, harvid_list (ListStore::create(harvid_list_columns))
	, harvid_list_view (harvid_list)
{
	set_session (s);
	set_name ("AddVideoDialog");
	set_position (Gtk::WIN_POS_MOUSE);
	set_modal (true);
	set_skip_taskbar_hint (true);
	set_resizable (true);
	set_size_request (800, -1);

	harvid_initialized = false;
	std::string dstdir = video_dest_dir(_session->session_directory().video_path(), video_get_docroot(Config));

	if (Config->get_video_advanced_setup()) {

		/* Harvid Browser */
		harvid_list_view.append_column("", pixBufRenderer);
		harvid_list_view.append_column(_("Filename"), harvid_list_columns.filename);

		harvid_list_view.get_column(0)->set_alignment(0.5);
		harvid_list_view.get_column(0)->add_attribute(pixBufRenderer, "stock-id", harvid_list_columns.id);
		harvid_list_view.get_column(1)->set_expand(true);
		harvid_list_view.get_column(1)->set_sort_column(harvid_list_columns.filename);
		harvid_list_view.set_enable_search(true);
		harvid_list_view.set_search_column(1);

		harvid_list_view.get_selection()->set_mode (SELECTION_SINGLE);

		harvid_list_view.get_selection()->signal_changed().connect(sigc::mem_fun(*this, &AddVideoDialog::harvid_list_view_selected));
		harvid_list_view.signal_row_activated().connect (sigc::mem_fun (*this, &AddVideoDialog::harvid_list_view_activated));

		VBox* vbox = manage (new VBox);
		Gtk::ScrolledWindow *scroll = manage(new ScrolledWindow);
		scroll->add(harvid_list_view);
		scroll->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);

		HBox* hbox = manage (new HBox);
		harvid_path.set_alignment (0, 0.5);
		hbox->pack_start (harvid_path, true, true);
		hbox->pack_start (harvid_reset, false, false);

		vbox->pack_start (*hbox, false, false);
		vbox->pack_start (*scroll, true, true);

		notebook.append_page (*vbox, _("VideoServerIndex"));
	} else {
		/* dummy entry */
		VBox* vbox = manage (new VBox);
		notebook.append_page (*vbox, _("VideoServerIndex"));
	}

	/* file chooser */
	chooser.set_border_width (4);
#ifdef GTKOSX
	/* some broken redraw behaviour - this is a bandaid */
	chooser.signal_selection_changed().connect (mem_fun (chooser, &Widget::queue_draw));
#endif
	chooser.set_current_folder (dstdir);

	Gtk::FileFilter video_filter;
	Gtk::FileFilter matchall_filter;
	video_filter.add_custom (FILE_FILTER_FILENAME, mem_fun(*this, &AddVideoDialog::on_video_filter));
	video_filter.set_name (_("Video files"));

	matchall_filter.add_pattern ("*.*");
	matchall_filter.set_name (_("All files"));

	chooser.add_filter (video_filter);
	chooser.add_filter (matchall_filter);
	chooser.set_select_multiple (false);

	VBox* vboxfb = manage (new VBox);
	vboxfb->pack_start (chooser, true, true, 0);

	if (video_get_docroot(Config).size() > 0 &&
			Config->get_video_advanced_setup()) {
		notebook.append_page (*vboxfb, _("Browse Files"));
	}

	/* Global Options*/
	Gtk::Label* l;
	VBox* options_box = manage (new VBox);

	l = manage (new Label (_("<b>Options</b>"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	l->set_use_markup ();

	options_box->pack_start (*l, false, true, 4);
	options_box->pack_start (xjadeo_checkbox, false, true, 2);
	options_box->pack_start (set_session_fps_checkbox, false, true, 2);

	/* preview pane */
	VBox* previewpane = manage (new VBox);
	Gtk::Table *table = manage(new Table(4,2));

	table->set_row_spacings(2);
	table->set_col_spacings(4);

	l = manage (new Label (_("<b>Video Information</b>"), Gtk::ALIGN_CENTER, Gtk::ALIGN_CENTER, false));
	l->set_use_markup ();
	table->attach (*l, 0, 2, 0, 1, FILL, FILL);
	l = manage (new Label (_("Duration:"), Gtk::ALIGN_RIGHT, Gtk::ALIGN_CENTER, false));
	table->attach (*l, 0, 1, 1, 2, FILL, FILL);
	table->attach (pi_duration, 1, 2, 1, 2, FILL, FILL);
	l = manage (new Label (_("Frame rate:"), Gtk::ALIGN_RIGHT, Gtk::ALIGN_CENTER, false));
	table->attach (*l, 0, 1, 2, 3, FILL, FILL);
	table->attach (pi_fps, 1, 2, 2, 3, FILL, FILL);
	l = manage (new Label (_("Aspect Ratio:"), Gtk::ALIGN_RIGHT, Gtk::ALIGN_CENTER, false));
	table->attach (*l, 0, 1, 3, 4, FILL, FILL);
	table->attach (pi_aspect, 1, 2, 3, 4, FILL, FILL);

	preview_image = manage(new Gtk::Image);

	imgbuf = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, true, 8, PREVIEW_WIDTH, PREVIEW_HEIGHT);
	imgbuf->fill(RGBA_TO_UINT(127,0,0,255));
	preview_image->set(imgbuf);
	seek_slider.set_draw_value(false);

	HBox* hbox = manage (new HBox);
	hbox->pack_start (*table, true, false);

	Gtk::Alignment *al = manage(new Gtk::Alignment());
	al->set_size_request(-1, 20);

	previewpane->pack_start (*preview_image, false, false);
	previewpane->pack_start (seek_slider, false, false);
	previewpane->pack_start (*al, false, false);
	previewpane->pack_start (*hbox, true, true, 6);

	/* Overall layout */
	hbox = manage (new HBox);
	if (Config->get_video_advanced_setup()) {
		hbox->pack_start (notebook, true, true);
	} else {
		hbox->pack_start (*vboxfb, true, true);
	}
	hbox->pack_start (*previewpane, false, false);

	get_vbox()->set_spacing (4);
	get_vbox()->pack_start (*hbox, true, true);
	get_vbox()->pack_start (*options_box, false, false);


	/* xjadeo checkbox */
	if (ARDOUR_UI::instance()->video_timeline->found_xjadeo()
			/* TODO xjadeo setup w/ xjremote */
			&& video_get_docroot(Config).size() > 0) {
		xjadeo_checkbox.set_active(true);  /* set in ardour_ui.cpp ?! */
	} else {
		printf("xjadeo was not found or video-server docroot is unset (remote video-server)\n");
		xjadeo_checkbox.set_active(false);
		xjadeo_checkbox.set_sensitive(false);
	}

	/* FPS checkbox */
	set_session_fps_checkbox.set_active(true);

	/* Buttons */
	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	ok_button = add_button (Stock::OK, RESPONSE_ACCEPT);
	//ok_button->set_sensitive(false);
	set_action_ok(false);

	/* connect signals after eveything has been initialized */
	chooser.signal_selection_changed().connect (mem_fun (*this, &AddVideoDialog::file_selection_changed));
	chooser.signal_file_activated().connect (mem_fun (*this, &AddVideoDialog::file_activated));
	//chooser.signal_update_preview().connect(sigc::mem_fun(*this, &AddVideoDialog::update_preview));
	notebook.signal_switch_page().connect (sigc::hide_return (sigc::hide (sigc::hide (sigc::mem_fun (*this, &AddVideoDialog::page_switch)))));
	seek_slider.signal_value_changed().connect(sigc::mem_fun(*this, &AddVideoDialog::seek_preview));
	harvid_reset.signal_clicked().connect (sigc::mem_fun (*this, &AddVideoDialog::harvid_load_docroot));

	show_all_children ();
}

AddVideoDialog::~AddVideoDialog ()
{
}

void
AddVideoDialog::on_show ()
{
	Dialog::on_show ();
}

static bool check_video_file_extension(std::string file)
{
	const char* suffixes[] = {
		".avi"     , ".AVI"     ,
		".mov"     , ".MOV"     ,
		".ogg"     , ".OGG"     ,
		".ogv"     , ".OGV"     ,
		".mpg"     , ".MPG"     ,
		".mov"     , ".MOV"     ,
		".mp4"     , ".MP4"     ,
		".mkv"     , ".MKV"     ,
		".vob"     , ".VOB"     ,
		".asf"     , ".ASF"     ,
		".avs"     , ".AVS"     ,
		".dts"     , ".DTS"     ,
		".flv"     , ".FLV"     ,
		".m4v"     , ".M4V"     ,
		".matroska", ".MATROSKA",
		".h264"    , ".H264"    ,
		".dv"      , ".DV"      ,
		".dirac"   , ".DIRAC"   ,
		".webm"    , ".WEBM"    ,
	};

	for (size_t n = 0; n < sizeof(suffixes)/sizeof(suffixes[0]); ++n) {
		if (file.rfind (suffixes[n]) == file.length() - strlen (suffixes[n])) {
			return true;
		}
	}

	return false;
}

bool
AddVideoDialog::on_video_filter (const FileFilter::Info& filter_info)
{
	return check_video_file_extension(filter_info.filename);
}

std::string
AddVideoDialog::file_name (bool &local_file)
{
	int n = notebook.get_current_page ();
	if (n == 1 || ! Config->get_video_advanced_setup()) {
		local_file = true;
		return chooser.get_filename();
	} else {
		local_file = false;
		Gtk::TreeModel::iterator iter = harvid_list_view.get_selection()->get_selected();
		if(!iter) return "";

		std::string uri = (*iter)[harvid_list_columns.uri];
		std::string video_server_url = video_get_server_url(Config);

		/* check if video server is running locally */
		if (video_get_docroot(Config).size() > 0
				&& !video_server_url.compare(0, 16, "http://localhost"))
		{
			/* check if the file can be accessed */
			int plen;
			CURL *curl;
			curl = curl_easy_init();
			char *ue = curl_easy_unescape(curl, uri.c_str(), uri.length(), &plen);
			std::string path = video_get_docroot(Config) + ue;
			if (!::access(path.c_str(), R_OK)) {
				uri = path;
				local_file = true;
			}
			curl_easy_cleanup(curl);
			curl_free(ue);
		}
		return uri;
	}
}

enum VtlImportOption
AddVideoDialog::import_option ()
{
	int n = notebook.get_current_page ();
	if (n == 0 && Config->get_video_advanced_setup()) { return VTL_IMPORT_NONE; }
	return VTL_IMPORT_TRANSCODE;
}

bool
AddVideoDialog::launch_xjadeo ()
{
	return xjadeo_checkbox.get_active();
}

bool
AddVideoDialog::auto_set_session_fps ()
{
	return set_session_fps_checkbox.get_active();
}

void
AddVideoDialog::set_action_ok (bool yn)
{
	if (yn) {
		ok_button->set_sensitive(true);
	} else {
		preview_path = "";
		pi_duration.set_text("-");
		pi_aspect.set_text("-");
		pi_fps.set_text("-");
		ok_button->set_sensitive(false);
		imgbuf->fill(RGBA_TO_UINT(0,0,0,255));
		video_draw_cross(imgbuf);
		preview_image->set(imgbuf);
		preview_image->show();
	}
}

void
AddVideoDialog::file_selection_changed ()
{
	if (chooser.get_filename().size() > 0) {
		std::string path = chooser.get_filename();
		bool ok =
				check_video_file_extension(path)
				&&  Glib::file_test(path.c_str(), Glib::FILE_TEST_IS_REGULAR | Glib::FILE_TEST_IS_SYMLINK)
				&& !Glib::file_test(path.c_str(), Glib::FILE_TEST_IS_DIR);
		set_action_ok(ok);
		if (ok) {
			request_preview(video_map_path(video_get_docroot(Config), path));
		}
	} else {
		set_action_ok(false);
	}
}

void
AddVideoDialog::file_activated ()
{
	if (chooser.get_filename().size() > 0) {
		std::string path = chooser.get_filename();
		// TODO check docroot -> set import options
		bool ok =
				check_video_file_extension(path)
				&&  Glib::file_test(path.c_str(), Glib::FILE_TEST_IS_REGULAR | Glib::FILE_TEST_IS_SYMLINK)
				&& !Glib::file_test(path.c_str(), Glib::FILE_TEST_IS_DIR);
		if (ok) {
			Gtk::Dialog::response(RESPONSE_ACCEPT);
		}
	}
}

/**** Tree List Interaction ***/

void
AddVideoDialog::harvid_list_view_selected () {
	Gtk::TreeModel::iterator iter = harvid_list_view.get_selection()->get_selected();
	// TODO check docroot -> set import options, xjadeo
	if(!iter) {
		set_action_ok(false);
		return;
	}
	if ((std::string)((*iter)[harvid_list_columns.id]) == Stock::DIRECTORY.id) {
		set_action_ok(false);
	} else {
		set_action_ok(true);
		request_preview((*iter)[harvid_list_columns.uri]);
	}
}

void
AddVideoDialog::harvid_list_view_activated (const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn*) {
	Gtk::TreeModel::iterator iter = harvid_list->get_iter(path);
	if (!iter) return;
	std::string type = (*iter)[harvid_list_columns.id];
	std::string url = (*iter)[harvid_list_columns.uri];

#if 0
	printf ("A: %s %s %s\n",
			((std::string)((*iter)[harvid_list_columns.id])).c_str(),
			((std::string)((*iter)[harvid_list_columns.uri])).c_str(),
			((std::string)((*iter)[harvid_list_columns.filename])).c_str());
#endif

	if (type == Gtk::Stock::DIRECTORY.id) {
		harvid_request(url.c_str());
	} else {
		Gtk::Dialog::response(RESPONSE_ACCEPT);
	}
}

void
AddVideoDialog::harvid_load_docroot() {
	set_action_ok(false);

	std::string video_server_url = video_get_server_url(Config);
	char url[2048];
	snprintf(url, sizeof(url), "%s%sindex/"
		, video_server_url.c_str()
		, (video_server_url.length()>0 && video_server_url.at(video_server_url.length()-1) == '/')?"":"/");
	harvid_request(url);
	harvid_initialized = true;
}

bool
AddVideoDialog::page_switch() {
	if (notebook.get_current_page () == 1 || Config->get_video_advanced_setup()) {
		file_selection_changed();
		return true;
	}

	if (harvid_initialized) {
		harvid_list_view_selected();
	} else {
		harvid_load_docroot();
	}
	return true;
}

/**** Harvid HTTP interface ***/
void
AddVideoDialog::harvid_request(std::string u)
{
	char url[2048];
	int status;
	snprintf(url, sizeof(url), "%s?format=csv", u.c_str());

	harvid_list->clear();

	char *res = curl_http_get(url, &status);
	if (status != 200) {
		printf("request failed\n"); // XXX
		harvid_path.set_text(" - request failed -");
		free(res);
		return;
	}

	/* add up-to-parent */
	size_t se = u.find_last_of("/", u.size()-2);
	size_t ss = u.find("/index/");
	if (se != string::npos && ss != string::npos && se > ss) {
		TreeModel::iterator new_row = harvid_list->append();
		TreeModel::Row row = *new_row;
		row[harvid_list_columns.id      ] = Gtk::Stock::DIRECTORY.id;
		row[harvid_list_columns.uri     ] = u.substr(0, se + 1);
		row[harvid_list_columns.filename] = X_("..");
	}
	if (se != string::npos) {
		int plen;
		std::string path = u.substr(ss + 6);
		CURL *curl;
		curl = curl_easy_init();
		char *ue = curl_easy_unescape(curl, path.c_str(), path.length(), &plen);
		harvid_path.set_text(std::string(ue));
		curl_easy_cleanup(curl);
		curl_free(ue);
	} else {
		harvid_path.set_text(" ??? ");
	}

	if (!res) return;

	std::vector<std::vector<std::string> > lines;
	ParseCSV(std::string(res), lines);
	for (std::vector<std::vector<std::string> >::iterator i = lines.begin(); i != lines.end(); ++i) {
		TreeModel::iterator new_row = harvid_list->append();
		TreeModel::Row row = *new_row;

		if (i->at(0) == X_("D")) {
			row[harvid_list_columns.id      ] = Gtk::Stock::DIRECTORY.id;
			row[harvid_list_columns.uri     ] = i->at(1).c_str();
			row[harvid_list_columns.filename] = i->at(2).c_str();
		} else {
			row[harvid_list_columns.id      ] = Gtk::Stock::MEDIA_PLAY.id;
			row[harvid_list_columns.uri     ] = i->at(2).c_str();
			row[harvid_list_columns.filename] = i->at(3).c_str();
		}
	}

	free(res);
}

void
AddVideoDialog::seek_preview()
{
	if (preview_path.size() > 0)
		request_preview(preview_path);
}

void
AddVideoDialog::request_preview(std::string u)
{
	std::string video_server_url = video_get_server_url(Config);

	double video_file_fps;
	long long int video_duration;
	double video_start_offset;
	double video_aspect_ratio;

	int clip_width = PREVIEW_WIDTH;
	int clip_height = PREVIEW_HEIGHT;
	int clip_xoff, clip_yoff;

	if (!video_query_info(video_server_url, u,
			video_file_fps, video_duration, video_start_offset, video_aspect_ratio))
	{
		printf("image preview info request failed\n");
		// set_action_ok(false); // XXX only if docroot mismatch
		preview_path = "";
		pi_duration.set_text("-");
		pi_aspect.set_text("-");
		pi_fps.set_text("-");
		return;
	}

	if ((PREVIEW_WIDTH / (double)PREVIEW_HEIGHT) > video_aspect_ratio ) {
		clip_width = MIN(PREVIEW_WIDTH, rint(clip_height * video_aspect_ratio));
	} else {
		clip_height = MIN(PREVIEW_HEIGHT, rint(clip_width / video_aspect_ratio));
	}

	pi_duration.set_text(string_compose("%1 sec", video_duration / video_file_fps));
	pi_aspect.set_text(string_compose("%1", video_aspect_ratio));
	pi_fps.set_text(string_compose("%1 fps", video_file_fps));

	clip_xoff = (PREVIEW_WIDTH - clip_width)/2;
	clip_yoff = (PREVIEW_HEIGHT - clip_height)/2;

	char url[2048];
	snprintf(url, sizeof(url), "%s%s?frame=%lli&w=%d&h=%di&file=%s&format=rgb"
		, video_server_url.c_str()
		, (video_server_url.length()>0 && video_server_url.at(video_server_url.length()-1) == '/')?"":"/"
		, (long long) (video_duration * seek_slider.get_value() / 1000.0)
		, clip_width, clip_height, u.c_str());

	char *data = curl_http_get(url, NULL);
	if (!data) {
		printf("image preview request failed %s\n", url);
		imgbuf->fill(RGBA_TO_UINT(0,0,0,255));
		video_draw_cross(imgbuf);
		preview_path = "";
	} else {
		Glib::RefPtr<Gdk::Pixbuf> tmp;
		tmp = Gdk::Pixbuf::create_from_data ((guint8*) data, Gdk::COLORSPACE_RGB, false, 8, clip_width, clip_height, clip_width*3);
		if (clip_width != PREVIEW_WIDTH || clip_height != PREVIEW_HEIGHT) {
			imgbuf->fill(RGBA_TO_UINT(0,0,0,255));
		}
		tmp->copy_area (0, 0, clip_width, clip_height, imgbuf, clip_xoff, clip_yoff);
		preview_path = u;
		free(data);
	}
	preview_image->set(imgbuf);
	preview_image->show();
}

#endif /* WITH_VIDEOTIMELINE */
