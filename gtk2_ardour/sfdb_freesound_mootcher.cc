/* sfdb_freesound_mootcher.cpp **********************************************************************

	Adapted for Ardour by Ben Loftis, March 2008
	Updated to new Freesound API by Colin Fletcher, November 2011
	Updated to Freesound API v2 by Colin Fletcher, May 2022

	Mootcher 23-8-2005

	Mootcher Online Access to thefreesoundproject website
	http://freesound.org/

	GPL 2005 Jorn Lemon
	mail for questions/remarks: mootcher@twistedlemon.nl
	or go to the freesound website forum

	-----------------------------------------------------------------

	Includes:
		curl.h    (version 7.14.0)
	Libraries:
		libcurl.lib

	-----------------------------------------------------------------
	Licence GPL:

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.


*************************************************************************************/

#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>

#include <glib.h>
#include "pbd/gstdio_compat.h"

#include "pbd/error.h"
#include "pbd/pthread_utils.h"
#include "pbd/openuri.h"
#include "pbd/xml++.h"

#include "ardour/audio_library.h"
#include "ardour/debug.h"
#include "ardour/filesystem_paths.h"

#include "widgets/prompter.h"

#include "ardour_dialog.h"
#include "ardour_http.h"
#include "sfdb_freesound_mootcher.h"
#include "gui_thread.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace PBD;

// freesound API URLs are always https://, and don't include the www. subdomain
static const std::string base_url = "https://freesound.org/apiv2";

// Ardour 7
static const std::string default_token = "t3TjQ67WNh6zJLZRnWmArSiZ8bKlgTc2aEsV1cP7";
static const std::string client_id = "yesyr1g4StTtg2F50KT1";

static const std::string fields = "id,name,duration,filesize,samplerate,license,download,previews";

//------------------------------------------------------------------------
Mootcher::Mootcher(const std::string &the_token)
	: curl(curl_easy_init())
{
	DEBUG_TRACE(PBD::DEBUG::Freesound, "Created new Mootcher, oauth_token =\"" + the_token + "\"\n");
	custom_headers = NULL;
	oauth_token = the_token;

	cancel_download_btn.set_label (_("Cancel"));
	progress_hbox.pack_start (progress_bar, true, true);
	progress_hbox.pack_end (cancel_download_btn, false, false);
	progress_bar.show();
	cancel_download_btn.show();
	cancel_download_btn.signal_clicked().connect(sigc::mem_fun (*this, &Mootcher::cancelDownload));
};
//------------------------------------------------------------------------
Mootcher:: ~Mootcher()
{
	curl_easy_cleanup(curl);
	if (custom_headers) {
		curl_slist_free_all (custom_headers);
	}
	DEBUG_TRACE(PBD::DEBUG::Freesound, "Destroyed Mootcher\n");
}

//------------------------------------------------------------------------

void Mootcher::ensureWorkingDir ()
{
	std::string const& p = UIConfiguration::instance ().get_freesound_dir ();

	DEBUG_TRACE(PBD::DEBUG::Freesound, "ensureWorkingDir() - " + p + "\n");
	if (!Glib::file_test (p, Glib::FILE_TEST_IS_DIR)) {
		if (g_mkdir_with_parents (p.c_str(), 0775) != 0) {
			PBD::error << "Unable to create Mootcher working dir" << endmsg;
		}
	}
	basePath = p;
#ifdef PLATFORM_WINDOWS
	std::string replace = "/";
	size_t pos = basePath.find("\\");
	while( pos != std::string::npos ){
		basePath.replace(pos, 1, replace);
		pos = basePath.find("\\");
	}
#endif
}


//------------------------------------------------------------------------
size_t Mootcher::WriteMemoryCallback(void *ptr, size_t size, size_t nmemb, void *data)
{
	int realsize = (int)(size * nmemb);
	struct SfdbMemoryStruct *mem = (struct SfdbMemoryStruct *)data;

	mem->memory = (char *)realloc(mem->memory, mem->size + realsize + 1);

	if (mem->memory) {
		memcpy(&(mem->memory[mem->size]), ptr, realsize);
		mem->size += realsize;
		mem->memory[mem->size] = 0;
	}
	return realsize;
}


//------------------------------------------------------------------------

std::string Mootcher::sortMethodString(enum sortMethod sort)
{
// given a sort type, returns the string value to be passed to the API to
// sort the results in the requested way.

	switch (sort) {
		case sort_duration_descending:  return "duration_desc";
		case sort_duration_ascending:   return "duration_asc";
		case sort_created_descending:   return "created_desc";
		case sort_created_ascending:    return "created_asc";
		case sort_downloads_descending: return "downloads_desc";
		case sort_downloads_ascending:  return "downloads_asc";
		case sort_rating_descending:    return "rating_desc";
		case sort_rating_ascending:     return "rating_asc";
		default:                        return "";
	}
}

//------------------------------------------------------------------------
void Mootcher::setcUrlOptions()
{
	// some servers don't like requests that are made without a user-agent field, so we provide one
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
	// setup curl error buffer
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
	// Allow redirection
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

	// Allow connections to time out (without using signals)
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30);

	/* set ca-certificates to use for bundled versions of ardour */
	ArdourCurl::HttpGet::ca_setopt (curl);
}

std::string Mootcher::doRequest(std::string uri, std::string params)
{
	std::string result;
	struct SfdbMemoryStruct xml_page;
	xml_page.memory = NULL;
	xml_page.size = 0;

	setcUrlOptions();
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &xml_page);

	// the url to get
	std::string url = base_url + uri + "?";
	if (params != "") {
		url += params + "&token=" + default_token + "&format=xml";
	} else {
		url += "token=" + default_token + "&format=xml";
	}

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str() );

	DEBUG_TRACE(PBD::DEBUG::Freesound, "doRequest() " + url + "\n");

	// perform online request
	CURLcode res = curl_easy_perform(curl);
	if( res != 0 ) {
		std::string errmsg = string_compose (_("curl error %1 (%2)"), res, curl_easy_strerror(res));
		error << errmsg << endmsg;
		DEBUG_TRACE(PBD::DEBUG::Freesound, errmsg + "\n");
		return "";
	}

	// free the memory
	if (xml_page.memory) {
		result = xml_page.memory;
	}

	free (xml_page.memory);
	xml_page.memory = NULL;
	xml_page.size = 0;

	DEBUG_TRACE(PBD::DEBUG::Freesound, result + "\n");
	return result;
}


std::string Mootcher::searchSimilar(std::string id)
{
	std::string params = "";

	params += "&fields=" + fields;
	params += "&num_results=100";
	// XXX should we filter out MP3s here, too?
	// XXX and what if there are more than 100 similar sounds?

	return doRequest("/sounds/" + id + "/similar/", params);
}

//------------------------------------------------------------------------


void
Mootcher::report_login_error(const std::string &msg)
{
	DEBUG_TRACE(PBD::DEBUG::Freesound, "Login failed:" + msg + "\n");
	error << "Freesound login failed: " << msg << endmsg;
}

bool
Mootcher::get_oauth_token()
{
	std::string oauth_url = base_url + "/oauth2/authorize/?client_id="+client_id+"&response_type=code&state=hello";
	std::string auth_code;

	/* use the user's default browser to get an authorization token */
	if (!PBD::open_uri (oauth_url)) {
		report_login_error ("cannot open " + oauth_url);
		return false;
	}
	ArdourWidgets::Prompter token_entry(true);
	token_entry.set_prompt(_("Please log in to Freesound in the browser window that's just been opened, and paste the authorization code here"));
	token_entry.set_title(_("Authorization Code"));

	token_entry.set_name ("TokenEntryWindow");
	// token_entry.set_size_request (250, -1);
	token_entry.set_position (Gtk::WIN_POS_MOUSE);
	token_entry.add_button (Gtk::Stock::OK, Gtk::RESPONSE_ACCEPT);
	token_entry.show ();

	if (token_entry.run () != Gtk::RESPONSE_ACCEPT)
		return false;

	token_entry.get_result(auth_code);
	if (auth_code == "")
		return false;

	oauth_token = auth_code_to_oauth_token(auth_code);
	setcUrlOptions();

	return oauth_token != "";;
}

std::string Mootcher::auth_code_to_oauth_token(const std::string &auth_code)
{
	struct SfdbMemoryStruct json_page;
	json_page.memory = NULL;
	json_page.size = 0;
	CURLcode res;

	setcUrlOptions();
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &json_page);

	std::string oauth_url = base_url + "/oauth2/access_token/";
	std::string new_oauth_token;

	curl_easy_setopt(curl, CURLOPT_URL, oauth_url.c_str());
	curl_easy_setopt(curl, CURLOPT_POST, 5);
	curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS,
			("client_id=" + client_id +
			"&client_secret=" + default_token +
			"&grant_type=authorization_code" +
			"&code=" + auth_code).c_str());

	progress_bar.set_text(_("Fetching Access Token (auth_code=") + auth_code + "...");
	while (gtk_events_pending()) gtk_main_iteration (); // allow the progress bar text to update

	res = curl_easy_perform (curl);
	if (res != CURLE_OK) {
		if (res != CURLE_ABORTED_BY_CALLBACK) {
			report_login_error (string_compose ("curl failed: %1, error=%2", oauth_url, res));
		}
		return "";
	}

	if (!json_page.memory) {
		report_login_error (string_compose ("curl returned nothing, url=%1!", oauth_url));
		return "";
	}

	std::string access_token_json_str = json_page.memory;
	free (json_page.memory);
	json_page.memory = NULL;
	json_page.size = 0;

	DEBUG_TRACE(PBD::DEBUG::Freesound, access_token_json_str);

	// one of these days ardour's gonna need a proper JSON parser...
	size_t token_pos = access_token_json_str.find ("access_token");
	oauth_token = access_token_json_str.substr (token_pos + 16, 30);

	// we've set a bunch of curl options - reset the important ones now
	curl_easy_setopt(curl, CURLOPT_POST, 0);

	DEBUG_TRACE(PBD::DEBUG::Freesound, "oauth_token is :" + oauth_token + "\n");
	return oauth_token;
}

std::string Mootcher::searchText(std::string query, int page, std::string filter, enum sortMethod sort)
{
	std::string params = "";
	char buf[24];

	if (page > 1) {
		snprintf(buf, 23, "page=%d&", page);
		params += buf;
	}

	char *eq = curl_easy_escape(curl, query.c_str(), query.length());
	params += "query=\"" + std::string(eq) + "\"";
	curl_free(eq);

	if (filter != "") {
		char *ef = curl_easy_escape(curl, filter.c_str(), filter.length());
		params += "&filter=" + std::string(ef);
		curl_free(ef);
	}

	if (sort)
		params += "&sort=" + sortMethodString(sort);

	params += "&fields=" + fields;
	params += "&page_size=100";

	return doRequest("/search/text/", params);
}

//------------------------------------------------------------------------

std::string Mootcher::getSoundResourceFile(std::string ID)
{
	/* get the resource file for the sound with given ID.
	 * return the file name of the sound
	 */
	std::string originalSoundURI;
	std::string audioFileName;
	std::string xml;

	DEBUG_TRACE(PBD::DEBUG::Freesound, "getSoundResourceFile(" + ID + ")\n");

	// download the xmlfile into xml_page
	xml = doRequest("/sounds/" + ID + "/", "");

	XMLTree doc;
	doc.read_buffer( xml.c_str() );
	XMLNode *freesound = doc.root();

	// if the page is not a valid xml document with a 'freesound' root
	if (freesound == NULL) {
		error << _("getSoundResourceFile: There is no valid root in the xml file") << endmsg;
		return "";
	}

	if (strcmp(doc.root()->name().c_str(), "root") != 0) {
		error << string_compose (_("getSoundResourceFile: root = %1, != \"root\""), doc.root()->name()) << endmsg;
		return "";
	}

	XMLNode *name = freesound->child("name");

	// get the file name and size from xml file
	// assert (name);
	if (name) {

		audioFileName = Glib::build_filename (basePath, ID + "-" + name->child("text")->content());

		//store all the tags in the database
		XMLNode *tags = freesound->child("tags");
		if (tags) {
			XMLNodeList children = tags->children();
			XMLNodeConstIterator niter;
			std::vector<std::string> strings;
			for (niter = children.begin(); niter != children.end(); ++niter) {
				XMLNode *node = *niter;
				if( strcmp( node->name().c_str(), "list-item") == 0 ) {
					XMLNode *text = node->child("text");
					if (text) {
						// std::cerr << "tag: " << text->content() << std::endl;
						strings.push_back(text->content());
					}
				}
			}
			ARDOUR::Library->set_tags (std::string("//")+audioFileName, strings);
			ARDOUR::Library->save_changes ();
		}
	}

	return audioFileName;
}

int audioFileWrite(void *buffer, size_t size, size_t nmemb, void *file)
{
	return (int)fwrite(buffer, size, nmemb, (FILE*) file);
};

//------------------------------------------------------------------------

void *
Mootcher::threadFunc() {
CURLcode res;

	DEBUG_TRACE(PBD::DEBUG::Freesound, "threadFunc\n");
	res = curl_easy_perform (curl);
	fclose (theFile);
	curl_easy_setopt (curl, CURLOPT_NOPROGRESS, 1); // turn off the progress bar

	if (res != CURLE_OK) {
		/* it's not an error if the user pressed the stop button */
		if (res != CURLE_ABORTED_BY_CALLBACK) {
			error <<  string_compose (_("curl error %1 (%2)"), res, curl_easy_strerror(res)) << endmsg;
		}
		remove ( (audioFileName+".part").c_str() );
	} else {
		DEBUG_TRACE(PBD::DEBUG::Freesound, string_compose("renaming %1.part to %1\n", audioFileName));
		int r = rename ( (audioFileName+".part").c_str(), audioFileName.c_str() );
		if (r != 0) {
			const char *err = strerror(errno);
			DEBUG_TRACE(PBD::DEBUG::Freesound, string_compose("rename() failed: %1\n", err));
			assert(0);
		} else {
			// now download the tags &c.
			getSoundResourceFile(ID);
		}
	}

	return (void *) res;
}

void
Mootcher::doneWithMootcher()
{
	// update the sound info pane if the selection in the list box is still us
	sfb->refresh_display(ID, audioFileName);

	delete this; // this should be OK to do as long as Progress and Finished signals are always received in the order in which they are emitted
}

static void *
freesound_download_thread_func(void *arg)
{
	Mootcher *thisMootcher = (Mootcher *) arg;
	void *res;

	DEBUG_TRACE(PBD::DEBUG::Freesound, string_compose("freesound_download_thread_func(%1)\n", arg));
	res = thisMootcher->threadFunc();
	DEBUG_TRACE(PBD::DEBUG::Freesound, string_compose("threadFunc returns %1\n", res));
	thisMootcher->Finished(); /* EMIT SIGNAL */
	DEBUG_TRACE(PBD::DEBUG::Freesound, string_compose("returning from freesound_download_thread_func()\n", res));
	return res;
}


//------------------------------------------------------------------------

bool Mootcher::checkAudioFile(std::string originalFileName, std::string theID)
{
	// return true if file already exists locally and is larger than 256
	// bytes, false otherwise
	DEBUG_TRACE(PBD::DEBUG::Freesound, string_compose("checkAudiofile(%1, %2)\n", originalFileName, theID));
	ensureWorkingDir();
	ID = theID;
	audioFileName = Glib::build_filename (basePath, ID + "-" + originalFileName);

	// check to see if audio file already exists
	FILE *testFile = g_fopen(audioFileName.c_str(), "r");
	if (testFile) {
		fseek (testFile , 0 , SEEK_END);
		if (ftell (testFile) > 256) {
			fclose (testFile);
			DEBUG_TRACE(PBD::DEBUG::Freesound, "checkAudiofile() - found " + audioFileName + "\n");
			return true;
		}

		// else file was small, probably an error, delete it
		DEBUG_TRACE(PBD::DEBUG::Freesound, "checkAudiofile() - " + audioFileName + " <= 256 bytes, removing it\n");
		fclose (testFile);
		// remove (audioFileName.c_str() );
		rename (audioFileName.c_str(), (audioFileName + ".bad").c_str() );
	}
	DEBUG_TRACE(PBD::DEBUG::Freesound, "checkAudiofile() - not found " + audioFileName + "\n");
	return false;
}


bool
Mootcher::fetchAudioFile(std::string originalFileName, std::string theID, std::string audioURL, SoundFileBrowser *caller, std::string &token)
{

	DEBUG_TRACE(PBD::DEBUG::Freesound, string_compose("fetchAudiofile(%1, %2, %3, ...)\n", originalFileName, theID, audioURL));

	ensureWorkingDir();
	ID = theID;
	audioFileName = Glib::build_filename (basePath, ID + "-" + originalFileName);

	if (!curl) {
		return false;
	}

	Gtk::VBox *freesound_vbox = dynamic_cast<Gtk::VBox *> (caller->notebook.get_nth_page(2));
	freesound_vbox->pack_start(progress_hbox, Gtk::PACK_SHRINK);

	cancel_download = false;
	curl_easy_setopt (curl, CURLOPT_NOPROGRESS, 0); // turn on the progress bar
	curl_easy_setopt (curl, CURLOPT_PROGRESSFUNCTION, progress_callback);
	curl_easy_setopt (curl, CURLOPT_PROGRESSDATA, this);

	if (oauth_token == "") {
		if (!get_oauth_token()) {
			DEBUG_TRACE(PBD::DEBUG::Freesound, "get_oauth_token() failed!\n");
			return false;
		}
	}

	token = oauth_token;

	// now download the actual file
	theFile = g_fopen( (audioFileName + ".part").c_str(), "wb" );

	if (!theFile) {
		DEBUG_TRACE(PBD::DEBUG::Freesound, "Can't open file for writing:" + audioFileName + ".part\n");
		return false;
	}

	// create the download url
	audioURL += "?token=" + default_token;

	setcUrlOptions();
	std::string auth_header = "Authorization: Bearer " + oauth_token;
	DEBUG_TRACE(PBD::DEBUG::Freesound, "auth_header = " + auth_header + "\n");
	custom_headers = curl_slist_append (custom_headers, auth_header.c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, custom_headers);
	curl_easy_setopt(curl, CURLOPT_URL, audioURL.c_str() );
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, audioFileWrite);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, theFile);

	DEBUG_TRACE(PBD::DEBUG::Freesound, "Downloading audio from " + audioURL + " into " + audioFileName + ".part\n");
	std::string prog;
	prog = string_compose (_("%1"), originalFileName);
	progress_bar.set_text(prog);
	progress_hbox.show();

	sfb = caller;

	Progress.connect(*this, invalidator (*this), boost::bind(&Mootcher::updateProgress, this, _1, _2), gui_context());
	Finished.connect(*this, invalidator (*this), boost::bind(&Mootcher::doneWithMootcher, this), gui_context());
	pthread_t freesound_download_thread;
	pthread_create_and_store("freesound_import", &freesound_download_thread, freesound_download_thread_func, this);

	return true;
}

//---------

void
Mootcher::updateProgress(double dlnow, double dltotal)
{
	if (dltotal > 0) {
		double fraction = dlnow / dltotal;
		// std::cerr << "progress idle: " << progress_bar.get_text() << ". " << dlnow << " / " << dltotal << " = " << fraction << std::endl;
		if (fraction > 1.0) {
			fraction = 1.0;
		} else if (fraction < 0.0) {
			fraction = 0.0;
		}
		progress_bar.set_fraction(fraction);
	}
}

int
Mootcher::progress_callback(void *caller, double dltotal, double dlnow, double /*ultotal*/, double /*ulnow*/)
{
	// It may seem curious to pass a pointer to an instance of an object to a static
	// member function, but we can't use a normal member function as a curl progress callback,
	// and we want access to some private members of Mootcher.

	Mootcher *thisMootcher = (Mootcher *) caller;

	if (thisMootcher->cancel_download) {
		return -1;
	}

	thisMootcher->Progress(dlnow, dltotal); /* EMIT SIGNAL */
	return 0;
}

